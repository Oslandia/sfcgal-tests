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
#include <SFCGAL/io/wkt.h>

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

	//	lwnotice("context: %p", fcinfo->context);
	//	lwnotice("datum 0: %p, datum 1: %p", PG_GETARG_DATUM(0), PG_GETARG_DATUM(1));
	//	void* p1 = PG_GETARG_POINTER(0);
	//	void* p2 = PG_GETARG_POINTER(1);
	//	lwnotice("p1 = %p", p1);
	//	lwnotice("p2 = %p", p2);
	geom1 = (GSERIALIZED *)PG_DETOAST_DATUM(PG_GETARG_DATUM(0));
	geom2 = (GSERIALIZED *)PG_DETOAST_DATUM(PG_GETARG_DATUM(1));
	//	lwnotice("geom1: %p, geom2: %p", geom1, geom2);

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

	std::auto_ptr<SFCGAL::Geometry> g1;
	try {
		g1 = POSTGIS2SFCGAL( geom1 );
	}
	catch ( std::exception& e ) {
		lwerror("First argument geometry could not be converted to SFCGAL: %s", e.what() );
		PG_RETURN_NULL();
	}
	
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
	PG_FUNCTION_INFO_V1(sfcgal_in);
}

extern "C" Datum sfcgal_in(PG_FUNCTION_ARGS)
{
	lwnotice("sfcgal_in");
	const char *str = PG_GETARG_CSTRING(0);

	std::auto_ptr<SFCGAL::Geometry> g;
	try {
		g = SFCGAL::io::readWkt( str );
	}
	catch ( std::exception& e ) {
		lwerror("ERROR: %s", e.what() );
		PG_RETURN_NULL();
	}
	SFCGAL::Geometry* geom = g.release();
	lwnotice("geom = %p", geom);

	void* retp = palloc( sizeof(void*));
	memmove( retp, &geom, sizeof(void*) );
	PG_RETURN_POINTER(retp);
}

extern "C" {
	PG_FUNCTION_INFO_V1(sfcgal_out);
}

extern "C" Datum sfcgal_out(PG_FUNCTION_ARGS)
{
	lwnotice("sfcgal_out");
	void** p = (void **)PG_GETARG_POINTER(0);
	SFCGAL::Geometry* geom = reinterpret_cast<SFCGAL::Geometry*>(*p);
	lwnotice("geom = %p", geom);

	std::string astxt = geom->asText();

	char *retstr = (char*)palloc( astxt.size() + 1 );
	strncpy( retstr, astxt.c_str(), astxt.size() + 1 );
	delete geom;

	PG_RETURN_CSTRING(retstr);
}

extern "C" {
	PG_FUNCTION_INFO_V1(sfcgal_recv);
}

extern "C" Datum sfcgal_recv(PG_FUNCTION_ARGS)
{
	lwnotice("sfcgal_recv");
}

extern "C" Datum sfcgal_send(PG_FUNCTION_ARGS)
{
	lwnotice("sfcgal_send");
}
