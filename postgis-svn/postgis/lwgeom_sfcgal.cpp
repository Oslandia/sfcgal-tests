#include <stdexcept>
#include <vector>
#include <string>
#include <iostream>
#include <fstream>

#include <SFCGAL/Geometry.h>
#include <SFCGAL/TriangulatedSurface.h>
#include <SFCGAL/Solid.h>
#include <SFCGAL/tools/Log.h>
#include <SFCGAL/algorithm/triangulate.h>
#include <SFCGAL/algorithm/intersects.h>
#include <SFCGAL/algorithm/covers.h>
#include <SFCGAL/algorithm/intersection.h>
#include <SFCGAL/algorithm/convexHull.h>
#include <SFCGAL/algorithm/area.h>
#include <SFCGAL/algorithm/extrude.h>
#include <SFCGAL/algorithm/plane.h>
#include <SFCGAL/transform/ForceZOrderPoints.h>
#include <SFCGAL/algorithm/collectionExtract.h>
#include <SFCGAL/io/wkt.h>
#include <SFCGAL/io/Serialization.h>

/* TODO: we probaby don't need _all_ these pgsql headers */
extern "C" {
#include "postgres.h"
#include "fmgr.h"
#include "miscadmin.h"
#include "utils/array.h"
#include "utils/builtins.h"
#include "utils/hsearch.h"
#include "utils/memutils.h"
#include "executor/spi.h"
#include "funcapi.h"

#include "../postgis_config.h"
#include "lwgeom_functions_analytic.h" /* for point_in_polygon */
#include "lwgeom_cache.h"
#include "liblwgeom_internal.h"
#include "lwgeom_rtree.h"

}

#include "lwgeom_sfcgal.h"

std::auto_ptr<SFCGAL::Geometry> POSTGIS2SFCGAL(GSERIALIZED *pglwgeom)
{
	LWGEOM *lwgeom = lwgeom_from_gserialized(pglwgeom);
	if ( ! lwgeom )
	{
		throw std::runtime_error("POSTGIS2SFCGAL: unable to deserialize input");
	}
	std::auto_ptr<SFCGAL::Geometry> g(LWGEOM2SFCGAL(lwgeom));
	lwgeom_free(lwgeom);
	//	lwnotice( "POSTGIS2SFCGAL serialized: %p lwgeom: %p SFCGAL::Geometry: %p (%d)", pglwgeom, lwgeom, g.get(), g->geometryTypeId() );
	return g;
}

GSERIALIZED* SFCGAL2POSTGIS(const SFCGAL::Geometry& geom, bool force3D, int SRID )
{
	LWGEOM* lwgeom = SFCGAL2LWGEOM( &geom, force3D, SRID );
	if ( lwgeom_needs_bbox(lwgeom) == LW_TRUE )
	{
		lwgeom_add_bbox(lwgeom);
	}

	GSERIALIZED* result = geometry_serialize(lwgeom);
	lwgeom_free(lwgeom);

	//	lwnotice( "SFCGAL2POSTGIS result: %p SFCGAL::Geometry: %p (%d)", result, &geom, geom.geometryTypeId() );
	return result;
}


namespace SFCGAL {
	typedef bool (*BinaryPredicate) ( const Geometry&, const Geometry& );
	typedef std::auto_ptr<Geometry> (*UnaryConstruction) ( const Geometry& );
	typedef std::auto_ptr<Geometry> (*BinaryConstruction) ( const Geometry&, const Geometry& );
}

///
/// Generic processing of a binary predicate
Datum sfcgal_binary_predicate(PG_FUNCTION_ARGS, const char* name, SFCGAL::BinaryPredicate predicate_ptr )
{
	GSERIALIZED *geom1;
	GSERIALIZED *geom2;

	geom1 = (GSERIALIZED *)PG_DETOAST_DATUM(PG_GETARG_DATUM(0));
	geom2 = (GSERIALIZED *)PG_DETOAST_DATUM(PG_GETARG_DATUM(1));

	std::auto_ptr<SFCGAL::Geometry> g1;
	try {
		g1 = POSTGIS2SFCGAL( geom1 );
	}
	catch ( std::exception& e ) {
		lwerror("First argument geometry could not be converted to SFCGAL: %s", e.what() );
		PG_RETURN_NULL();
	}
	std::auto_ptr<SFCGAL::Geometry> g2;
	try {
		g2 = POSTGIS2SFCGAL( geom2 );
	}
	catch ( std::exception& e ) {
		lwerror("Second argument geometry could not be converted to SFCGAL: %s", e.what() );
		PG_RETURN_NULL();
	}
	
	bool result = false;
	try {
		result = (*predicate_ptr)( *g1, *g2 );
	}
	catch ( std::exception& e ) {
		lwnotice("geom1: %s", g1->asText().c_str());
		lwnotice("geom2: %s", g2->asText().c_str());
		lwnotice(e.what());
		lwerror("Error during execution of %s()", name);
		PG_RETURN_NULL();
	}

	PG_FREE_IF_COPY(geom1, 0);
	PG_FREE_IF_COPY(geom2, 1);

	PG_RETURN_BOOL(result);
}

///
/// Generic processing of a unary construction
Datum sfcgal_unary_construction( PG_FUNCTION_ARGS, const char* name, SFCGAL::UnaryConstruction fun_ptr )
{
	GSERIALIZED *geom1;
	GSERIALIZED *result;

	geom1 = (GSERIALIZED *)PG_DETOAST_DATUM(PG_GETARG_DATUM(0));

	std::auto_ptr<SFCGAL::Geometry> g1;
	try {
		g1 = POSTGIS2SFCGAL( geom1 );
	}
	catch ( std::exception& e ) {
		lwerror("Argument geometry could not be converted to SFCGAL: %s", e.what() );
		PG_RETURN_NULL();
	}
	
	std::auto_ptr<SFCGAL::Geometry> inter;
	try {
		inter = ( *fun_ptr )( *g1 );
	}
	catch ( std::exception& e ) {
		lwnotice("geom: %s", g1->asText().c_str());
		lwnotice(e.what());
		lwerror("Error during execution of %s()", name);
		PG_RETURN_NULL();
	}

	if ( inter.get() ) {
	    try {
		    result = SFCGAL2POSTGIS( *inter, /* force 3d */ false, gserialized_get_srid( geom1 ) );
	    }
	    catch ( std::exception& e ) {
		lwerror("Result geometry could not be converted to lwgeom: %s", e.what() );
		PG_RETURN_NULL();
	    }
	}

	PG_FREE_IF_COPY(geom1, 0);

	PG_RETURN_POINTER(result);
}

///
/// Generic processing of a binary construction
Datum sfcgal_binary_construction( PG_FUNCTION_ARGS, const char* name, SFCGAL::BinaryConstruction fun_ptr )
{
	GSERIALIZED *geom1;
	GSERIALIZED *geom2;
	GSERIALIZED *result;

	
	geom1 = (GSERIALIZED *)PG_DETOAST_DATUM(PG_GETARG_DATUM(0));
	geom2 = (GSERIALIZED *)PG_DETOAST_DATUM(PG_GETARG_DATUM(1));

	std::auto_ptr<SFCGAL::Geometry> g1;
	try {
		g1 = POSTGIS2SFCGAL( geom1 );
	}
	catch ( std::exception& e ) {
		lwerror("First argument geometry could not be converted to SFCGAL: %s", e.what() );
		PG_RETURN_NULL();
	}
	std::auto_ptr<SFCGAL::Geometry> g2;
	try {
		g2 = POSTGIS2SFCGAL( geom2 );
	}
	catch ( std::exception& e ) {
		lwerror("Second argument geometry could not be converted to SFCGAL: %s", e.what() );
		PG_RETURN_NULL();
	}

	std::auto_ptr<SFCGAL::Geometry> inter;
	try {
		inter = ( *fun_ptr )( *g1, *g2 );
	}
	catch ( std::exception& e ) {
		lwnotice("geom1: %s", g1->asText().c_str());
		lwnotice("geom2: %s", g2->asText().c_str());
		lwnotice(e.what());
		lwerror("Error during execution of %s()", name);
		PG_RETURN_NULL();
	}

	if ( inter.get() ) {
	    try {
		    result = SFCGAL2POSTGIS( *inter, /* force3D */ false, gserialized_get_srid( geom1 ) );
	    }
	    catch ( std::exception& e ) {
		lwerror("Result geometry could not be converted to lwgeom: %s", e.what() );
		PG_RETURN_NULL();
	    }
	}

	PG_FREE_IF_COPY(geom1, 0);
	PG_FREE_IF_COPY(geom2, 1);

	PG_RETURN_POINTER(result);
}

///
/// Declaration of predicates
///
extern "C" {
	PG_FUNCTION_INFO_V1(sfcgal_intersects);
	PG_FUNCTION_INFO_V1(sfcgal_intersects3D);
	PG_FUNCTION_INFO_V1(sfcgal_covers3D);
}

extern "C" Datum sfcgal_intersects(PG_FUNCTION_ARGS)
{
	return sfcgal_binary_predicate( fcinfo, "intersects", SFCGAL::algorithm::intersects );
}

extern "C" Datum sfcgal_intersects3D(PG_FUNCTION_ARGS)
{
	return sfcgal_binary_predicate( fcinfo, "intersects3D", SFCGAL::algorithm::intersects3D );
}

extern "C" Datum sfcgal_covers3D(PG_FUNCTION_ARGS)
{
	return sfcgal_binary_predicate( fcinfo, "covers3D", SFCGAL::algorithm::covers3D );
}

///
/// Declaration of unary constructions
extern "C" {
	PG_FUNCTION_INFO_V1(sfcgal_convexhull);
	PG_FUNCTION_INFO_V1(sfcgal_convexhull3D);
}

extern "C" Datum sfcgal_convexhull(PG_FUNCTION_ARGS)
{
	return sfcgal_unary_construction( fcinfo, "convexhull", SFCGAL::algorithm::convexHull );
}

extern "C" Datum sfcgal_convexhull3D(PG_FUNCTION_ARGS)
{
	return sfcgal_unary_construction( fcinfo, "convexhull3D", SFCGAL::algorithm::convexHull3D );
}

///
/// Declaration of binary constructions
extern "C" {
	PG_FUNCTION_INFO_V1(sfcgal_intersection);
	PG_FUNCTION_INFO_V1(sfcgal_intersection3D);
}

extern "C" Datum sfcgal_intersection(PG_FUNCTION_ARGS)
{
	return sfcgal_binary_construction( fcinfo, "intersection", SFCGAL::algorithm::intersection );
}

extern "C" Datum sfcgal_intersection3D(PG_FUNCTION_ARGS)
{
	return sfcgal_binary_construction( fcinfo, "intersection3D", SFCGAL::algorithm::intersection3D );
}

extern "C" {
PG_FUNCTION_INFO_V1(sfcgal_area);
PG_FUNCTION_INFO_V1(sfcgal_area3d);
}

extern "C" Datum sfcgal_area(PG_FUNCTION_ARGS)
{
	GSERIALIZED *geom1;

	geom1 = (GSERIALIZED *)PG_DETOAST_DATUM(PG_GETARG_DATUM(0));

	std::auto_ptr<SFCGAL::Geometry> g1;
	try {
		g1 = POSTGIS2SFCGAL( geom1 );
	}
	catch ( std::exception& e ) {
		lwerror("First argument geometry could not be converted to SFCGAL: %s", e.what() );
		PG_RETURN_NULL();
	}
	
	double area = 0.0;
	try {
		area = SFCGAL::algorithm::area2D( *g1 );
	}
	catch ( std::exception& e ) {
		lwnotice("geom1: %s", g1->asText().c_str());
		lwnotice(e.what());
		lwerror("Error during execution of area()");
		PG_RETURN_NULL();
	}

	PG_FREE_IF_COPY(geom1, 0);

	PG_RETURN_FLOAT8(area);
}

extern "C" Datum sfcgal_area3d(PG_FUNCTION_ARGS)
{
	GSERIALIZED *geom1;

	geom1 = (GSERIALIZED *)PG_DETOAST_DATUM(PG_GETARG_DATUM(0));

	std::auto_ptr<SFCGAL::Geometry> g1;
	try {
		g1 = POSTGIS2SFCGAL( geom1 );
	}
	catch ( std::exception& e ) {
		lwerror("First argument geometry could not be converted to SFCGAL: %s", e.what() );
		PG_RETURN_NULL();
	}
	
	double area = 0.0;
	try {
		area = SFCGAL::algorithm::area3D( *g1 );
	}
	catch ( std::exception& e ) {
		lwnotice("geom1: %s", g1->asText().c_str());
		lwnotice(e.what());
		lwerror("Error during execution of area3D()");
		PG_RETURN_NULL();
	}

	PG_FREE_IF_COPY(geom1, 0);

	PG_RETURN_FLOAT8(area);
}

extern "C" {
PG_FUNCTION_INFO_V1(sfcgal_hasplane);
}
extern "C" Datum sfcgal_hasplane(PG_FUNCTION_ARGS)
{
	GSERIALIZED *geom1;

	geom1 = (GSERIALIZED *)PG_DETOAST_DATUM(PG_GETARG_DATUM(0));

	std::auto_ptr<SFCGAL::Geometry> g1;
	try {
		g1 = POSTGIS2SFCGAL( geom1 );
	}
	catch ( std::exception& e ) {
		lwerror("First argument geometry could not be converted to SFCGAL: %s", e.what() );
		PG_RETURN_NULL();
	}
	
	if ( g1->geometryTypeId() != SFCGAL::TYPE_POLYGON )
	{
		lwerror( "hasPlane() cannot be applied to a geometry of type %s", g1->geometryType().c_str() );
		PG_RETURN_NULL();
	}

	bool result = false;
	try {
		result = SFCGAL::algorithm::hasPlane3D< CGAL::Exact_predicates_exact_constructions_kernel >( g1->as< const SFCGAL::Polygon >() );
	}
	catch ( std::exception& e ) {
		lwnotice("geom1: %s", g1->asText().c_str());
		lwnotice(e.what());
		lwerror("Error during execution of hasPlane3D()");
		PG_RETURN_NULL();
	}

	PG_FREE_IF_COPY(geom1, 0);

	PG_RETURN_BOOL(result);
}

extern "C" {
PG_FUNCTION_INFO_V1(sfcgal_pointing_up);
}
extern "C" Datum sfcgal_pointing_up(PG_FUNCTION_ARGS)
{
	GSERIALIZED *geom1;

	geom1 = (GSERIALIZED *)PG_DETOAST_DATUM(PG_GETARG_DATUM(0));

	std::auto_ptr<SFCGAL::Geometry> g1;
	try {
		g1 = POSTGIS2SFCGAL( geom1 );
	}
	catch ( std::exception& e ) {
		lwerror("First argument geometry could not be converted to SFCGAL: %s", e.what() );
		PG_RETURN_NULL();
	}
	
	if ( g1->geometryTypeId() != SFCGAL::TYPE_POLYGON )
	{
		lwerror( "pointingUp() cannot be applied to a geometry of type %s", g1->geometryType().c_str() );
		PG_RETURN_NULL();
	}

	bool result = false;
	try {
		result = g1->as<SFCGAL::Polygon>().isCounterClockWiseOriented();
	}
	catch ( std::exception& e ) {
		lwnotice("geom1: %s", g1->asText().c_str());
		lwnotice(e.what());
		lwerror("Error during execution of pointingUp()");
		PG_RETURN_NULL();
	}

	PG_FREE_IF_COPY(geom1, 0);

	PG_RETURN_BOOL(result);
}

extern "C" {
	PG_FUNCTION_INFO_V1(sfcgal_triangulate);
}

extern "C" Datum sfcgal_triangulate(PG_FUNCTION_ARGS)
{
	GSERIALIZED *geom1;
	GSERIALIZED *result;

	geom1 = (GSERIALIZED *)PG_DETOAST_DATUM(PG_GETARG_DATUM(0));

	std::auto_ptr<SFCGAL::Geometry> g1;
	try {
		g1 = POSTGIS2SFCGAL( geom1 );
	}
	catch ( std::exception& e ) {
		lwerror("First argument geometry could not be converted to SFCGAL: %s", e.what() );
		PG_RETURN_NULL();
	}
	
	SFCGAL::TriangulatedSurface surf;
	try {
		SFCGAL::algorithm::triangulate( *g1, surf );
		result = SFCGAL2POSTGIS( surf, false, gserialized_get_srid( geom1 ) );
	}
	catch ( std::exception& e ) {
		lwnotice("geom1: %s", g1->asText().c_str());
		lwnotice(e.what());
		lwerror("Error during execution of triangulate()");
		PG_RETURN_NULL();
	}

	PG_FREE_IF_COPY(geom1, 0);

	PG_RETURN_POINTER(result);
}

extern "C" {
	PG_FUNCTION_INFO_V1(sfcgal_triangulate2D);
}

extern "C" Datum sfcgal_triangulate2D(PG_FUNCTION_ARGS)
{
	GSERIALIZED *geom1;
	GSERIALIZED *result;

	geom1 = (GSERIALIZED *)PG_DETOAST_DATUM(PG_GETARG_DATUM(0));

	std::auto_ptr<SFCGAL::Geometry> g1;
	try {
		g1 = POSTGIS2SFCGAL( geom1 );
	}
	catch ( std::exception& e ) {
		lwerror("First argument geometry could not be converted to SFCGAL: %s", e.what() );
		PG_RETURN_NULL();
	}
	
	SFCGAL::TriangulatedSurface surf;
	try {
		SFCGAL::algorithm::triangulate2D( *g1, surf );
		result = SFCGAL2POSTGIS( surf, false, gserialized_get_srid( geom1 ) );
	}
	catch ( std::exception& e ) {
		lwnotice("geom1: %s", g1->asText().c_str());
		lwnotice(e.what());
		lwerror("Error during execution of triangulate()");
		PG_RETURN_NULL();
	}

	PG_FREE_IF_COPY(geom1, 0);

	PG_RETURN_POINTER(result);
}

extern "C" {
	PG_FUNCTION_INFO_V1(sfcgal_extrude);
}

extern "C" Datum sfcgal_extrude(PG_FUNCTION_ARGS)
{
	GSERIALIZED *geom1;
	double dx, dy, dz;

	GSERIALIZED *result;

	geom1 = (GSERIALIZED *)PG_DETOAST_DATUM(PG_GETARG_DATUM(0));
	dx = PG_GETARG_FLOAT8(1);
	dy = PG_GETARG_FLOAT8(2);
	dz = PG_GETARG_FLOAT8(3);
	// TODO: forbid negative values

	std::auto_ptr<SFCGAL::Geometry> g1;
	try {
		g1 = POSTGIS2SFCGAL( geom1 );
	}
	catch ( std::exception& e ) {
		lwerror("First argument geometry could not be converted to SFCGAL: %s", e.what() );
		PG_RETURN_NULL();
	}
	// force Z
	SFCGAL::transform::ForceZOrderPoints forceZ;
	g1->accept( forceZ );
	
	try {
		std::auto_ptr<SFCGAL::Geometry> gresult = SFCGAL::algorithm::extrude( *g1, dx, dy, dz );
		result = SFCGAL2POSTGIS( *gresult, gresult->is3D(), gserialized_get_srid( geom1 ) );
	}
	catch ( std::exception& e ) {
		lwnotice("geom1: %s", g1->asText().c_str());
		lwnotice("dx: %g", dx );
		lwnotice("dy: %g", dy );
		lwnotice("dz: %g", dz );
		lwnotice(e.what());
		lwerror("Error during execution of extrude()");
		PG_RETURN_NULL();
	}

	PG_FREE_IF_COPY(geom1, 0);

	PG_RETURN_POINTER(result);
}

extern "C" {
	PG_FUNCTION_INFO_V1(sfcgal_make_solid);
}

extern "C" Datum sfcgal_make_solid(PG_FUNCTION_ARGS)
{
    // transform a polyhedral surface into a solid with only one exterior shell
    GSERIALIZED *geom1;
    
    GSERIALIZED *result;
    
    geom1 = (GSERIALIZED *)PG_DETOAST_DATUM(PG_GETARG_DATUM(0));

    std::auto_ptr<SFCGAL::Geometry> g1;
    try {
	g1 = POSTGIS2SFCGAL( geom1 );
    }
    catch ( std::exception& e ) {
	lwerror("First argument geometry could not be converted to SFCGAL: %s", e.what() );
	PG_RETURN_NULL();
    }
    if ( g1->geometryTypeId() == SFCGAL::TYPE_SOLID )
    {
	    // already a solid, return
	    PG_RETURN_POINTER( geom1 );
    }
    if ( g1->geometryTypeId() != SFCGAL::TYPE_POLYHEDRALSURFACE )
    {
	lwerror( "make_solid only applies to polyhedral surfaces" );
	PG_RETURN_NULL();
    }
    
    SFCGAL::Solid* solid = new SFCGAL::Solid( static_cast<const SFCGAL::PolyhedralSurface&>(*g1.get()) );
    try
    {
	    result = SFCGAL2POSTGIS( *solid, /* force3D */ true, gserialized_get_srid( geom1 ) );
    }
    catch ( std::exception& e ) {
	lwnotice("geom1: %s", g1->asText().c_str());
	lwnotice(e.what());
	lwerror("Error during execution of make_solid()");
	PG_RETURN_NULL();
    }
    
    PG_FREE_IF_COPY(geom1, 0);
    
    PG_RETURN_POINTER(result);
}

extern "C" {
	PG_FUNCTION_INFO_V1(sfcgal_force_z_up);
}

extern "C" Datum sfcgal_force_z_up(PG_FUNCTION_ARGS)
{
	// transform a 2d surface to a 3d one, with the normal pointing up
    GSERIALIZED *geom1;
    
    GSERIALIZED *result;
    
    geom1 = (GSERIALIZED *)PG_DETOAST_DATUM(PG_GETARG_DATUM(0));

    std::auto_ptr<SFCGAL::Geometry> g1;
    try {
	g1 = POSTGIS2SFCGAL( geom1 );
    }
    catch ( std::exception& e ) {
	lwerror("First argument geometry could not be converted to SFCGAL: %s", e.what() );
	PG_RETURN_NULL();
    }

    try
    {
	    SFCGAL::transform::ForceZOrderPoints forceZ;
	    g1->accept( forceZ );
	    result = SFCGAL2POSTGIS( *g1, /* force3D */ true, gserialized_get_srid( geom1 ) );
    }
    catch ( std::exception& e ) {
	lwnotice("geom1: %s", g1->asText().c_str());
	lwnotice(e.what());
	lwerror("Error during execution of force_z_up()");
	PG_RETURN_NULL();
    }
    
    PG_FREE_IF_COPY(geom1, 0);
    
    PG_RETURN_POINTER(result);
}

extern "C" {
	PG_FUNCTION_INFO_V1(sfcgal_collection_extract);
}

extern "C" Datum sfcgal_collection_extract(PG_FUNCTION_ARGS)
{
    // transform a polyhedral surface into a solid with only one exterior shell
    GSERIALIZED *geom1;
    
    GSERIALIZED *result;
    
    geom1 = (GSERIALIZED *)PG_DETOAST_DATUM(PG_GETARG_DATUM(0));
    uint32_t extractType = PG_GETARG_INT32(1);

    std::auto_ptr<SFCGAL::Geometry> g1;
    try {
	g1 = POSTGIS2SFCGAL( geom1 );
    }
    catch ( std::exception& e ) {
	lwerror("First argument geometry could not be converted to SFCGAL: %s", e.what() );
	PG_RETURN_NULL();
    }
    if ( extractType != 3 ) {
	    lwerror( "collectionExtract is only supported for polygons(3)");
	    PG_RETURN_NULL();
    }

    try
    {
	    std::auto_ptr<SFCGAL::Geometry> gresult = SFCGAL::algorithm::collectionExtractPolygons( g1 );
	    result = SFCGAL2POSTGIS( *gresult, /* force3D */ true, gserialized_get_srid( geom1 ) );
    }
    catch ( std::exception& e ) {
	lwnotice("geom1: %s", g1->asText().c_str());
	lwnotice(e.what());
	lwerror("Error during execution of collectionExtract()");
	PG_RETURN_NULL();
    }
    
    PG_FREE_IF_COPY(geom1, 0);
    
    PG_RETURN_POINTER(result);
}


void list_children( MemoryContext context, MemoryContext refContext, int depth )
{
	char depthstr[ depth + 1 ];
	memset( depthstr, ' ', depth );
	depthstr[ depth ] = 0;
	if ( context == refContext ) {
		depthstr[0] = '>';
	}

	lwnotice( "%s%s %p", depthstr, context->name, context );

	MemoryContext iter = context->firstchild;
	while ( iter ) {
		list_children( iter, refContext, depth + 2 );
		iter = iter->nextchild;
	}
}
void list_contexts( MemoryContext context )
{
	// find root context
	MemoryContext root = TopMemoryContext;
	list_children( root, context, 0 );
}

class GeometryPool
{
public:
	void reference( void* context, SFCGAL::Geometry* geometry )
	{
		lwnotice( "reference %p in %p", geometry, context );
		//		list_parent_contexts( (MemoryContext)context );
		pool_[context].push_back( geometry );
	}

	void deleteAll( void* context )
	{
		if ( pool_.find( context ) == pool_.end() ) {
			return;
		}
		GeometryList& l = pool_[ context ];
		if ( l.size() == 0 ) {
			return;
		}
		for ( int i = l.size() - 1; i >= 0; --i ) {
			SFCGAL::Geometry* g = l[i];
			lwnotice( "delete %p from %p", g, context );
			delete g;
			l.pop_back();
		}
	}
private:
	typedef std::vector<SFCGAL::Geometry*> GeometryList;
	std::map<void*, GeometryList> pool_;
};

GeometryPool refGeometryPool;

void (*oldDelete) (MemoryContext );
void myDelete( MemoryContext context )
{
	//	lwnotice("myDelete %p, name: %s", context, context->name );
	refGeometryPool.deleteAll( context );
	(*oldDelete) ( context );
}
void (*oldReset) (MemoryContext );
void myReset( MemoryContext context )
{
	//	lwnotice("myReset %p, name: %s", context, context->name );
	refGeometryPool.deleteAll( context );
	(*oldReset) ( context );
}

void override_destructors( MemoryContext context )
{
	// overwrite destructors on the current memory context
	if ( context->methods->delete_context != myDelete ) {
		oldDelete = context->methods->delete_context;
		oldReset = context->methods->reset;
		context->methods->delete_context = myDelete;
		context->methods->reset = myReset;
	}
}

SFCGAL::Geometry* get_geometry_arg( FunctionCallInfo fcinfo, size_t n )
{
	void** p = (void**)PG_GETARG_POINTER( n );
	SFCGAL::Geometry* pp = (SFCGAL::Geometry*)(*p);
	return pp;
}

MemoryContext get_parent_context()
{
	//
	// returns the memory context used to store SFCGAL::Geometry*
	// Ideally, this would be in the closest parent context of the current function evaluation.
	return MessageContext;
}

Datum return_geometry( SFCGAL::Geometry* geometry )
{
	void** p = (void**)palloc( sizeof(void*) );
	*p = (void*)(geometry);
	return Datum(p);
}

extern "C" {
	PG_FUNCTION_INFO_V1(sfcgal_ref_in);
}

extern "C" Datum sfcgal_ref_in(PG_FUNCTION_ARGS)
{
	char* cstring = PG_GETARG_CSTRING( 0 );
	std::string rstr( cstring );
	std::auto_ptr<SFCGAL::Geometry> g;
	try
	{
		g = SFCGAL::io::readWkt( rstr );
	}
	catch ( std::exception& e )
	{
		lwerror("First argument geometry could not be converted to SFCGAL: %s", e.what() );
		PG_RETURN_NULL();
	}

	MemoryContext parentContext = get_parent_context();
	override_destructors( parentContext );

	SFCGAL::Geometry* geo = g.release();
	refGeometryPool.reference( parentContext, geo );
	return return_geometry( geo );
}

extern "C" {
	PG_FUNCTION_INFO_V1(sfcgal_ref_out);
}

extern "C" Datum sfcgal_ref_out(PG_FUNCTION_ARGS)
{
	SFCGAL::Geometry* g = get_geometry_arg( fcinfo, 0 );
	//	lwnotice( "ref_out: %p", g );

	std::string wkt = g->asText( /* exact */ -1 );
	char * retstr = (char*)palloc( wkt.size() + 1 );
	strncpy( retstr, wkt.c_str(), wkt.size() + 1 );
	
	PG_RETURN_CSTRING( retstr );
}

extern "C" {
	PG_FUNCTION_INFO_V1(sfcgal_ref_intersects);
}

extern "C" Datum sfcgal_ref_intersects(PG_FUNCTION_ARGS)
{
	SFCGAL::Geometry* ref1 = get_geometry_arg( fcinfo, 0 );
	SFCGAL::Geometry* ref2 = get_geometry_arg( fcinfo, 1 );

	bool result;
	try
	{
		result = SFCGAL::algorithm::intersects( *ref1, *ref2 );
	}
	catch ( std::exception& e )
	{
		lwerror("Problem during sfcgal_ref_intersects: %s", e.what() );
		PG_RETURN_NULL();
	}

	PG_RETURN_BOOL( result );
}

extern "C" {
	PG_FUNCTION_INFO_V1(sfcgal_ref_intersection);
}

extern "C" Datum sfcgal_ref_intersection(PG_FUNCTION_ARGS)
{
	SFCGAL::Geometry* ref1 = get_geometry_arg( fcinfo, 0 );
	SFCGAL::Geometry* ref2 = get_geometry_arg( fcinfo, 1 );

	std::auto_ptr<SFCGAL::Geometry> result;
	try
	{
		result = SFCGAL::algorithm::intersection( *ref1, *ref2 );
	}
	catch ( std::exception& e )
	{
		lwerror("Problem during sfcgal_ref_intersection: %s", e.what() );
		PG_RETURN_NULL();
	}

	SFCGAL::Geometry* geo = result.release();
	refGeometryPool.reference( get_parent_context(), geo );
	return return_geometry( geo );
}

struct ExactGeometry
{
	uint32_t size;
	char data[1];
};

ExactGeometry* serializeExactGeometry( const SFCGAL::Geometry& g1 )
{
    std::string raw = SFCGAL::io::writeBinary( g1 );
    ExactGeometry* g = (ExactGeometry*)palloc( raw.size() + 1 + 4 );
    memmove( &g->data[0], raw.data(), raw.size() );
    SET_VARSIZE( g, raw.size() );
    return g;
}

std::auto_ptr<SFCGAL::Geometry> unserializeExactGeometry( ExactGeometry* ptr )
{
    uint32_t s = VARSIZE( ptr );
    std::string gstr( &ptr->data[0], s );
    return SFCGAL::io::readBinary( gstr );	
}

extern "C" {
	PG_FUNCTION_INFO_V1(sfcgal_exact_out);
}

extern "C" Datum sfcgal_exact_out(PG_FUNCTION_ARGS)
{
    ExactGeometry *geom1;
    
    geom1 = (ExactGeometry *)PG_GETARG_DATUM(0);

    std::auto_ptr<SFCGAL::Geometry> g = unserializeExactGeometry( geom1 );

    std::string wkt = g->asText( /* exact */ -1 );
    char * retstr = (char*)palloc( wkt.size() + 1 );
    strncpy( retstr, wkt.c_str(), wkt.size() + 1 );

    PG_RETURN_CSTRING( retstr );
}

extern "C" {
	PG_FUNCTION_INFO_V1(sfcgal_exact_in);
}

extern "C" Datum sfcgal_exact_in(PG_FUNCTION_ARGS)
{
	char* cstring = PG_GETARG_CSTRING( 0 );
	std::string rstr( cstring );
	std::auto_ptr<SFCGAL::Geometry> g;
	try
	{
		g = SFCGAL::io::readWkt( rstr );
	}
	catch ( std::exception& e )
	{
		lwerror("First argument geometry could not be converted to SFCGAL: %s", e.what() );
		PG_RETURN_NULL();
	}

	ExactGeometry* exactG = serializeExactGeometry( *g );
	PG_RETURN_POINTER( exactG );
}

extern "C" {
	PG_FUNCTION_INFO_V1(sfcgal_exact);
}

extern "C" Datum sfcgal_exact(PG_FUNCTION_ARGS)
{
    GSERIALIZED *geom1;
    
    geom1 = (GSERIALIZED *)PG_DETOAST_DATUM(PG_GETARG_DATUM(0));

    std::auto_ptr<SFCGAL::Geometry> g1;
    try {
	g1 = POSTGIS2SFCGAL( geom1 );
    }
    catch ( std::exception& e ) {
	lwerror("First argument geometry could not be converted to SFCGAL: %s", e.what() );
	PG_RETURN_NULL();
    }

    ExactGeometry* g = serializeExactGeometry( *g1 );

    PG_FREE_IF_COPY( geom1, 0 );

    PG_RETURN_POINTER( g );
}

extern "C" {
	PG_FUNCTION_INFO_V1(sfcgal_exact_intersects);
}

extern "C" Datum sfcgal_exact_intersects(PG_FUNCTION_ARGS)
{
	ExactGeometry* exact1 = (ExactGeometry*)PG_DETOAST_DATUM(PG_GETARG_DATUM( 0 ));
	ExactGeometry* exact2 = (ExactGeometry*)PG_DETOAST_DATUM(PG_GETARG_DATUM( 1 ));

	std::auto_ptr<SFCGAL::Geometry> g1 = unserializeExactGeometry( exact1 );
	std::auto_ptr<SFCGAL::Geometry> g2 = unserializeExactGeometry( exact2 );

	bool result;
	try
	{
		result = SFCGAL::algorithm::intersects( *g1, *g2 );
	}
	catch ( std::exception& e )
	{
		lwerror("Problem during sfcgal_exact_intersects: %s", e.what() );
		PG_RETURN_NULL();		
	}

	PG_RETURN_BOOL( result );
}

extern "C" {
	PG_FUNCTION_INFO_V1(sfcgal_exact_intersection);
}

extern "C" Datum sfcgal_exact_intersection(PG_FUNCTION_ARGS)
{
	ExactGeometry* exact1 = (ExactGeometry*)PG_DETOAST_DATUM(PG_GETARG_DATUM( 0 ));
	ExactGeometry* exact2 = (ExactGeometry*)PG_DETOAST_DATUM(PG_GETARG_DATUM( 1 ));

	std::auto_ptr<SFCGAL::Geometry> g1 = unserializeExactGeometry( exact1 );
	std::auto_ptr<SFCGAL::Geometry> g2 = unserializeExactGeometry( exact2 );

	ExactGeometry* result;
	try
	{
		std::auto_ptr<SFCGAL::Geometry> rg = SFCGAL::algorithm::intersection( *g1, *g2 );
		result = serializeExactGeometry( *rg );
	}
	catch ( std::exception& e )
	{
		lwerror("Problem during sfcgal_exact_intersection: %s", e.what() );
		PG_RETURN_NULL();		
	}

	PG_RETURN_POINTER( result );
}
