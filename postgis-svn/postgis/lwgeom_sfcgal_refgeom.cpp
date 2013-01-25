/**********************************************************************
 *
 * PostGIS - Spatial Types for PostgreSQL
 * http://postgis.refractions.net
 *
 * Wrapper around SFCGAL for 3D and exact geometries
 *
 * Copyright 2012-2013 Oslandia <contact@oslandia.com>
 *
 * This is free software; you can redistribute and/or modify it under
 * the terms of the GNU General Public Licence. See the COPYING file.
 *
 **********************************************************************/

#include "lwgeom_sfcgal_wrapper.h"

#include <vector>
#include <map>

#include <SFCGAL/Geometry.h>
#include <SFCGAL/PreparedGeometry.h>
#include <SFCGAL/TriangulatedSurface.h>
#include <SFCGAL/Solid.h>
#include <SFCGAL/algorithm/triangulate.h>
#include <SFCGAL/algorithm/extrude.h>
#include <SFCGAL/algorithm/intersects.h>
#include <SFCGAL/algorithm/intersection.h>
#include <SFCGAL/algorithm/convexHull.h>
#include <SFCGAL/algorithm/distance.h>
#include <SFCGAL/algorithm/distance3d.h>
#include <SFCGAL/algorithm/area.h>
#include <SFCGAL/algorithm/plane.h>
#include <SFCGAL/transform/ForceZOrderPoints.h>
#include <SFCGAL/io/wkt.h>
#include <SFCGAL/io/ewkt.h>
#include <SFCGAL/io/Serialization.h>

extern "C" {
#include "postgres.h"
#include "fmgr.h"
#include "utils/memutils.h"
#include "nodes/execnodes.h"

#include "../postgis_config.h"
#include "lwgeom_pg.h"
}

/**
 * Only used for debugging purpose
 */
static void display_children( MemoryContext context, int level, MemoryContext current )
{
	char indent[level*2+1];
	memset( indent, ' ', level*2 );
	indent[level*2] = 0;

	if ( context == current && level > 0 ) {
		indent[0] = '>';
	}
	
	lwnotice( "%s%s %p", indent, context->name, context );

	MemoryContext child = context->firstchild;
	while ( child != 0 ) {
		display_children( child, level + 2, current );
		child = child->nextchild;
	}
}

/**
 * Only used for debugging purpose
 */
static void display_contexts( MemoryContext refContext )
{
	MemoryContext c = refContext;
	while ( c != TopMemoryContext ) {
		c = c->parent;
	}

	display_children( c, 0, refContext );
}

/**
 * Global pool of referenced geometries.
 *
 * A list of pointers to geometry can be associated to a MemoryContext
 * They are deleted when the MemoryContext is deleted or reset
 */
class GeometryPool
{
public:
	static void init( MemoryContext context )
	{
	}

	static bool isEmpty( MemoryContext context )
	{
		/* mandatory implementation */
		return false;
	}

	static void check( MemoryContext context )
	{
	}

	static void stats( MemoryContext context, int level )
	{
		fprintf(stderr, "%s: Ref geometry context\n", context->name);
	}

	/**
	 * Is the given Geometry still existing ?
	 */
	static bool isReferenced( SFCGAL::PreparedGeometry* geometry )
	{
		// this has to be fast, the geometry is then the key of the pool_ map
		return pool_.find( geometry ) != pool_.end();
	}

	/**
	 * Save a reference to the given Geometry.
	 * It is associated with a MemoryContext with a proper life cycle.
	 */
	static void reference( SFCGAL::PreparedGeometry* geometry )
	{
		//
		// Find the memory context used to store SFCGAL::Geometry*
		// Ideally, this would be in the closest parent context of the current function evaluation.
		//
		// If the current context is a not descendant of a PortalMemory, it means this context is volatile and will be
		// reset between calls within the same tuple access. In this case, attach our context to the MessageContext
		// else, use the current context
		MemoryContext parentContext = MessageContext;
		
		MemoryContext c = CurrentMemoryContext;
		while ( c != TopMemoryContext ) {
			// If it's a child of a Portal[Heap]Memory
			if ( !strncmp( c->name, "Portal", 6 ) ) {
				parentContext = CurrentMemoryContext;
				break;
			}
			c = c->parent;
		}
		
		MemoryContext childContext = 0;
		// If a child context already exists, use it
		ChildContextMap::const_iterator child_it = childContext_.find( parentContext );
		if ( child_it != childContext_.end() ) {
			childContext = child_it->second;
		}
		else {
			// Else, create a new one.
			// We won't allocate anything in the new context. It is only used for its destruction callbacks.
			// It is thus allocated with the minimum required size
			char contextName[] = "SFCGAL";
			childContext = MemoryContextCreate( T_AllocSetContext, sizeof(MemoryContextData) + strlen(contextName) + 1,
							    &GeometryPool::contextMethods,
							    parentContext,
							    contextName );

			// associate this new context to its parent
			childContext_[ parentContext ] = childContext;
		}
		
		// force next reset. When the parent will be reset, reset will be called on this child
		childContext->isReset = false;
		
		// now reference the geometry
		pool_[ geometry ] = childContext;
	}
	
	static void deleteContext( MemoryContext context )
	{
		for ( GeometryReference::iterator it = pool_.begin(); it != pool_.end(); ++it ) {
			if ( it->second == context ) {
				delete it->first;
				pool_.erase( it );
			}
		}

		// delete the ChildContextMap part
		for ( ChildContextMap::iterator it = childContext_.begin(); it != childContext_.end(); ++it ) {
			if ( it->second == context ) {
				childContext_.erase( it );
				break;
			}
		}
	}

	static MemoryContextMethods contextMethods;
private:
	typedef std::map<SFCGAL::PreparedGeometry*, MemoryContext> GeometryReference;
	static GeometryReference pool_;

	// memory context => child memory context
	typedef std::map<MemoryContext, MemoryContext> ChildContextMap;
	static ChildContextMap childContext_;
};
GeometryPool::GeometryReference GeometryPool::pool_;
GeometryPool::ChildContextMap GeometryPool::childContext_;

MemoryContextMethods GeometryPool::contextMethods = {
	NULL, // alloc
	NULL, // free
	NULL, // realloc
	GeometryPool::init,
	GeometryPool::deleteContext, // reset
	GeometryPool::deleteContext,
	NULL, // get_chunk_space
	GeometryPool::isEmpty,
	GeometryPool::stats
#ifdef MEMORY_CONTEXT_CHECKING
	, GeometryPool::check
#endif
};

/**
 * Get a Geometry pointer from function arguments
 */
static SFCGAL::PreparedGeometry* get_geometry_arg( FunctionCallInfo fcinfo, size_t n )
{
	void** p = (void**)PG_GETARG_POINTER( n );
	SFCGAL::PreparedGeometry* pp = (SFCGAL::PreparedGeometry*)(*p);
	if ( ! GeometryPool::isReferenced( pp ) ) {
		// The geometry has been deleted
		return 0;
	}
	return pp;
}

/**
 * Get a Geometry pointer from function arguments, returns error if the geometry does not exist anymore
 */
static SFCGAL::PreparedGeometry* get_geometry_arg_secure( FunctionCallInfo fcinfo, size_t n )
{
	void** p = (void**)PG_GETARG_POINTER( n );
	SFCGAL::PreparedGeometry* pp = (SFCGAL::PreparedGeometry*)(*p);
	if ( ! GeometryPool::isReferenced( pp ) ) {
		lwerror( "Unable to access deleted geometry !" );
		return 0;
	}
	return pp;
}

/**
 * Prepare a Geometry for return
 */
Datum prepare_for_return( SFCGAL::PreparedGeometry* geo )
{
	GeometryPool::reference( geo );

	// allocate space for a pointer
	void** p = (void**)palloc( sizeof(void*) );
	*p = (void *)geo;
	return Datum(p);
}

extern "C" {
	PG_FUNCTION_INFO_V1(sfcgal_ref_in);
	PG_FUNCTION_INFO_V1(sfcgal_ref_from_text);
}

/**
 * Convert a WKT to a referenced geometry
 */
extern "C" Datum sfcgal_ref_in(PG_FUNCTION_ARGS)
{
	char* cstring = PG_GETARG_CSTRING( 0 );

	std::auto_ptr<SFCGAL::PreparedGeometry> g;
	try
	{
		g = SFCGAL::io::readEwkt( cstring, strlen(cstring) );
	}
	catch ( std::exception& e )
	{
		lwerror("First argument geometry could not be converted to SFCGAL: %s", e.what() );
		PG_RETURN_NULL();
	}

	return prepare_for_return( g.release() );
}

/**
 * Convert a WKT to a referenced geometry
 */
extern "C" Datum sfcgal_ref_from_text(PG_FUNCTION_ARGS)
{
	//	display_contexts( CurrentMemoryContext );
	text *wkttext = PG_GETARG_TEXT_P(0);
	char *cstring = text2cstring(wkttext);

	std::auto_ptr<SFCGAL::PreparedGeometry> g;
	try
	{
		g = SFCGAL::io::readEwkt( cstring, strlen(cstring) );
	}
	catch ( std::exception& e )
	{
		lwerror("First argument geometry could not be converted to SFCGAL: %s", e.what() );
		PG_RETURN_NULL();
	}

	return prepare_for_return( g.release() );
}

/**
 * Convert a referenced geometry to its WKT representation
 */

extern "C" {
	PG_FUNCTION_INFO_V1(sfcgal_ref_out);
}

extern "C" Datum sfcgal_ref_out(PG_FUNCTION_ARGS)
{
	SFCGAL::PreparedGeometry* g = get_geometry_arg( fcinfo, 0 );
	if ( !g ) {
		char deleted[] = "-deleted-";
		char* retstr = (char*)palloc( strlen(deleted) + 1 );
		strncpy( retstr, deleted, strlen(deleted) + 1 );

		lwnotice( "Referenced geometries must not be stored" );
		PG_RETURN_CSTRING( retstr );
	}

	std::string wkt = g->asEWKT( /* exact */ -1 );
	char * retstr = (char*)palloc( wkt.size() + 1 );
	strncpy( retstr, wkt.c_str(), wkt.size() + 1 );
	
	PG_RETURN_CSTRING( retstr );
}

/**
 * Conversion from a regular PostGIS geometry to a ref geometry
 */

extern "C" {
	PG_FUNCTION_INFO_V1(sfcgal_ref_from_geom);
}

extern "C" Datum sfcgal_ref_from_geom(PG_FUNCTION_ARGS)
{
	GSERIALIZED *geom1;
	
	geom1 = (GSERIALIZED *)PG_DETOAST_DATUM(PG_GETARG_DATUM(0));
	
	std::auto_ptr<SFCGAL::PreparedGeometry> g1;
	try {
		g1 = POSTGIS2SFCGALp( geom1 );
	}
	catch ( std::exception& e ) {
		lwerror("First argument geometry could not be converted to SFCGAL: %s", e.what() );
		PG_RETURN_NULL();
	}
	
	return prepare_for_return( g1.release() );
}


/**
 * Conversion from a ref geometry to a regular PostGIS geometry
 */

extern "C" {
	PG_FUNCTION_INFO_V1(sfcgal_geom_from_ref);
}

extern "C" Datum sfcgal_geom_from_ref(PG_FUNCTION_ARGS)
{
	SFCGAL::PreparedGeometry* g = get_geometry_arg_secure( fcinfo, 0 );
	if ( !g ) {
		PG_RETURN_NULL();
	}
	GSERIALIZED* result = SFCGAL2POSTGIS( *g, false );
	PG_RETURN_POINTER( result );
}

/**
 *
 * Macros for ref geometry argument wrapping
 *
 */
#define SFCGAL_TYPE_refGeometry_WRAPPER_INPUT( i )			\
	SFCGAL::PreparedGeometry* BOOST_PP_CAT( input, i ) = get_geometry_arg_secure( fcinfo, i ); \
	if ( ! BOOST_PP_CAT( input, i ) ) {				\
		PG_RETURN_NULL();					\
	}

#define SFCGAL_TYPE_refGeometry_WRAPPER_ACCESS_INPUT( i )	\
	BOOST_PP_CAT( input, i )->geometry()

#define SFCGAL_TYPE_refGeometry_WRAPPER_FREE_INPUT( i )  /* */
#define SFCGAL_TYPE_refGeometry_WRAPPER_DECLARE_RETURN_VAR() std::auto_ptr<SFCGAL::Geometry> result
#define SFCGAL_TYPE_refGeometry_WRAPPER_CONVERT_RESULT()   /* */
#define SFCGAL_TYPE_refGeometry_WRAPPER_RETURN()			\
	SFCGAL::PreparedGeometry* geo = new SFCGAL::PreparedGeometry( result, input0->SRID() ); \
	return prepare_for_return( geo );

#define SFCGAL_TYPE_refGeometry_WRAPPER_TO_CSTR( i )		\
	"%s", BOOST_PP_CAT( input, i ) ->asEWKT().c_str()

SFCGAL_WRAPPER_DECLARE_FUNCTION( ref_intersects, SFCGAL::algorithm::intersects, bool, (refGeometry)(refGeometry) )
SFCGAL_WRAPPER_DECLARE_FUNCTION( ref_intersects3D, SFCGAL::algorithm::intersects3D, bool, (refGeometry)(refGeometry) )
SFCGAL_WRAPPER_DECLARE_FUNCTION( ref_intersection, SFCGAL::algorithm::intersection, refGeometry, (refGeometry)(refGeometry) )
SFCGAL_WRAPPER_DECLARE_FUNCTION( ref_intersection3D, SFCGAL::algorithm::intersection3D, refGeometry, (refGeometry)(refGeometry) )
SFCGAL_WRAPPER_DECLARE_FUNCTION( ref_convexhull, SFCGAL::algorithm::convexHull, refGeometry, (refGeometry) )
SFCGAL_WRAPPER_DECLARE_FUNCTION( ref_convexhull3D, SFCGAL::algorithm::convexHull3D, refGeometry, (refGeometry) )
SFCGAL_WRAPPER_DECLARE_FUNCTION( ref_area, SFCGAL::algorithm::area, double, (refGeometry) )
SFCGAL_WRAPPER_DECLARE_FUNCTION( ref_area3D, SFCGAL::algorithm::area3D, double, (refGeometry) )
SFCGAL_WRAPPER_DECLARE_FUNCTION( ref_hasplane, _sfcgal_hasplane, bool, (refGeometry) )
SFCGAL_WRAPPER_DECLARE_FUNCTION( ref_pointing_up, _sfcgal_pointing_up, bool, (refGeometry) )
SFCGAL_WRAPPER_DECLARE_FUNCTION( ref_triangulate, _sfcgal_triangulate, refGeometry, (refGeometry) )
SFCGAL_WRAPPER_DECLARE_FUNCTION( ref_triangulate2D, _sfcgal_triangulate2D, refGeometry, (refGeometry) )
SFCGAL_WRAPPER_DECLARE_FUNCTION( ref_extrude, _sfcgal_extrude, refGeometry, (refGeometry)(double)(double)(double) )
SFCGAL_WRAPPER_DECLARE_FUNCTION( ref_make_solid, _sfcgal_make_solid, refGeometry, (refGeometry) )
SFCGAL_WRAPPER_DECLARE_FUNCTION( ref_force_z_up, _sfcgal_force_z_up, refGeometry, (refGeometry) )
SFCGAL_WRAPPER_DECLARE_FUNCTION( ref_distance, SFCGAL::algorithm::distance, double, (refGeometry)(refGeometry) )
SFCGAL_WRAPPER_DECLARE_FUNCTION( ref_distance3D, SFCGAL::algorithm::distance3D, double, (refGeometry)(refGeometry) )
SFCGAL_WRAPPER_DECLARE_FUNCTION( ref_copy, _sfcgal_copy, refGeometry, (refGeometry) )

