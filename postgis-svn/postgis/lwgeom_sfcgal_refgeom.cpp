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

#include "../postgis_config.h"
#include "lwgeom_pg.h"
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
	static void reference( void* context, SFCGAL::PreparedGeometry* geometry )
	{
		pool_[context].push_back( geometry );
	}

	static void deleteAll( void* context )
	{
		if ( pool_.find( context ) == pool_.end() ) {
			return;
		}
		GeometryList& l = pool_[ context ];
		if ( l.size() == 0 ) {
			return;
		}
		for ( int i = l.size() - 1; i >= 0; --i ) {
			SFCGAL::PreparedGeometry* g = l[i];
			delete g;
			l.pop_back();
		}
	}
private:
	typedef std::vector<SFCGAL::PreparedGeometry*> GeometryList;
	static std::map<void*, GeometryList> pool_;
};
std::map<void*, GeometryPool::GeometryList> GeometryPool::pool_;

/**
 * Memory context destructor
 */
void (*oldDelete) (MemoryContext );
void refDelete( MemoryContext context )
{
	GeometryPool::deleteAll( context );
	(*oldDelete) ( context );
}

/**
 * Memory context destructor used by reset
 */
void (*oldReset) (MemoryContext );
void refReset( MemoryContext context )
{
	GeometryPool::deleteAll( context );
	(*oldReset) ( context );
}

/**
 * Returns the MemoryContext where geometries will be referenced and delete afterwards
 */
MemoryContext get_reference_context()
{
	//
	// returns the memory context used to store SFCGAL::Geometry*
	// Ideally, this would be in the closest parent context of the current function evaluation.
	MemoryContext refContext = MessageContext;

	// overwrite destructors if needed
	if ( refContext->methods->delete_context != refDelete ) {
		oldDelete = refContext->methods->delete_context;
		oldReset = refContext->methods->reset;
		refContext->methods->delete_context = refDelete;
		refContext->methods->reset = refReset;
	}
	return refContext;
}

/**
 * Get a Geometry pointer from function arguments
 */
SFCGAL::PreparedGeometry* get_geometry_arg( FunctionCallInfo fcinfo, size_t n )
{
	void** p = (void**)PG_GETARG_POINTER( n );
	SFCGAL::PreparedGeometry* pp = (SFCGAL::PreparedGeometry*)(*p);
	return pp;
}


/**
 * Prepare a Geometry for return
 */
Datum return_geometry( SFCGAL::PreparedGeometry* geometry )
{
	void** p = (void**)palloc( sizeof(void*) );
	*p = (void*)(geometry);
	return Datum(p);
}


/**
 * Convert a WKT to a referenced geometry
 */
SFCGAL::PreparedGeometry* refGeometryFromCString( char* cstring )
{
	std::auto_ptr<SFCGAL::PreparedGeometry> g;
	try
	{
		g = SFCGAL::io::readEwkt( cstring, strlen(cstring) );
	}
	catch ( std::exception& e )
	{
		lwerror("First argument geometry could not be converted to SFCGAL: %s", e.what() );
		return 0;
	}

	MemoryContext referenceContext = get_reference_context();
	SFCGAL::PreparedGeometry* geo = g.release();
	GeometryPool::reference( referenceContext, geo );
	return geo;
}

extern "C" {
	PG_FUNCTION_INFO_V1(sfcgal_ref_in);
	PG_FUNCTION_INFO_V1(sfcgal_ref_from_text);
}

extern "C" Datum sfcgal_ref_in(PG_FUNCTION_ARGS)
{
	char* cstring = PG_GETARG_CSTRING( 0 );
	SFCGAL::PreparedGeometry* geo = refGeometryFromCString( cstring );
	if (!geo)
		PG_RETURN_NULL();
	return return_geometry( geo );
}

extern "C" Datum sfcgal_ref_from_text(PG_FUNCTION_ARGS)
{
	text *wkttext = PG_GETARG_TEXT_P(0);
	char *cstring = text2cstring(wkttext);
	SFCGAL::PreparedGeometry* geo = refGeometryFromCString( cstring );
	if (!geo)
		PG_RETURN_NULL();
	return return_geometry( geo );
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

    MemoryContext referenceContext = get_reference_context();
    SFCGAL::PreparedGeometry* geo = g1.release();
    GeometryPool::reference( referenceContext, geo );
    return return_geometry( geo );
}


/**
 * Conversion from a ref geometry to a regular PostGIS geometry
 */

extern "C" {
	PG_FUNCTION_INFO_V1(sfcgal_geom_from_ref);
}

extern "C" Datum sfcgal_geom_from_ref(PG_FUNCTION_ARGS)
{
	SFCGAL::PreparedGeometry* g = get_geometry_arg( fcinfo, 0 );
	GSERIALIZED* result = SFCGAL2POSTGIS( *g, false );
	PG_RETURN_POINTER( result );
}

/**
 *
 * Macros for ref geometry argument wrapping
 *
 */
#define WRAPPER_TYPE_refGeometry 1
#define WRAPPER_INPUT_refGeometry( i ) \
	SFCGAL::PreparedGeometry* BOOST_PP_CAT( input, i ) = get_geometry_arg( fcinfo, i );

#define WRAPPER_ACCESS_INPUT_refGeometry( i )  \
	BOOST_PP_CAT( input, i )->geometry()

#define WRAPPER_FREE_INPUT_refGeometry( i )  /* */
#define WRAPPER_DECLARE_RETURN_VAR_refGeometry() \
	std::auto_ptr<SFCGAL::Geometry> result
#define WRAPPER_CONVERT_RESULT_refGeometry()   /* */
#define WRAPPER_RETURN_refGeometry() \
	SFCGAL::PreparedGeometry* geo = new SFCGAL::PreparedGeometry( result, input0->SRID() ); \
	GeometryPool::reference( get_reference_context(), geo ); \
	return return_geometry( geo );
#define WRAPPER_TO_CSTR_refGeometry( i )   			\
	"%s", BOOST_PP_CAT( input, i ) ->asEWKT().c_str()

WRAPPER_DECLARE_SFCGAL_FUNCTION( ref_intersects, SFCGAL::algorithm::intersects, bool, (refGeometry)(refGeometry) )
WRAPPER_DECLARE_SFCGAL_FUNCTION( ref_intersects3D, SFCGAL::algorithm::intersects3D, bool, (refGeometry)(refGeometry) )
WRAPPER_DECLARE_SFCGAL_FUNCTION( ref_intersection, SFCGAL::algorithm::intersection, refGeometry, (refGeometry)(refGeometry) )
WRAPPER_DECLARE_SFCGAL_FUNCTION( ref_intersection3D, SFCGAL::algorithm::intersection3D, refGeometry, (refGeometry)(refGeometry) )
WRAPPER_DECLARE_SFCGAL_FUNCTION( ref_convexhull, SFCGAL::algorithm::convexHull, refGeometry, (refGeometry) )
WRAPPER_DECLARE_SFCGAL_FUNCTION( ref_convexhull3D, SFCGAL::algorithm::convexHull3D, refGeometry, (refGeometry) )
WRAPPER_DECLARE_SFCGAL_FUNCTION( ref_area, SFCGAL::algorithm::area2D, double, (refGeometry) )
WRAPPER_DECLARE_SFCGAL_FUNCTION( ref_area3D, SFCGAL::algorithm::area3D, double, (refGeometry) )
WRAPPER_DECLARE_SFCGAL_FUNCTION( ref_hasplane, _sfcgal_hasplane, bool, (refGeometry) )
WRAPPER_DECLARE_SFCGAL_FUNCTION( ref_pointing_up, _sfcgal_pointing_up, bool, (refGeometry) )
WRAPPER_DECLARE_SFCGAL_FUNCTION( ref_triangulate, _sfcgal_triangulate, refGeometry, (refGeometry) )
WRAPPER_DECLARE_SFCGAL_FUNCTION( ref_triangulate2D, _sfcgal_triangulate2D, refGeometry, (refGeometry) )
WRAPPER_DECLARE_SFCGAL_FUNCTION( ref_extrude, _sfcgal_extrude, refGeometry, (refGeometry)(double)(double)(double) )
WRAPPER_DECLARE_SFCGAL_FUNCTION( ref_make_solid, _sfcgal_make_solid, refGeometry, (refGeometry) )
WRAPPER_DECLARE_SFCGAL_FUNCTION( ref_force_z_up, _sfcgal_force_z_up, refGeometry, (refGeometry) )
WRAPPER_DECLARE_SFCGAL_FUNCTION( ref_distance, SFCGAL::algorithm::distance, double, (refGeometry)(refGeometry) )
WRAPPER_DECLARE_SFCGAL_FUNCTION( ref_distance3D, SFCGAL::algorithm::distance3D, double, (refGeometry)(refGeometry) )

