#include <stdexcept>
#include <vector>
#include <string>
#include <iostream>
#include <fstream>

#include <SFCGAL/Geometry.h>
#include <SFCGAL/TriangulatedSurface.h>
#include <SFCGAL/tools/Log.h>
#include <SFCGAL/algorithm/triangulate.h>
#include <SFCGAL/algorithm/intersects.h>
#include <SFCGAL/algorithm/covers.h>
#include <SFCGAL/algorithm/intersection.h>
#include <SFCGAL/algorithm/convexHull.h>
#include <SFCGAL/algorithm/area.h>

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
	return g;
}

GSERIALIZED* SFCGAL2POSTGIS(const SFCGAL::Geometry& geom)
{
	LWGEOM* lwgeom = SFCGAL2LWGEOM( &geom );
	if ( lwgeom_needs_bbox(lwgeom) == LW_TRUE )
	{
		lwgeom_add_bbox(lwgeom);
	}

	GSERIALIZED* result = geometry_serialize(lwgeom);
	lwgeom_free(lwgeom);

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
		result = SFCGAL2POSTGIS( *inter );
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
		result = SFCGAL2POSTGIS( *inter );
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
		result = SFCGAL2POSTGIS( surf );
	}
	catch ( std::exception& e ) {
		lwnotice("geom1: %s", g1->asText().c_str());
		lwnotice(e.what());
		lwerror("Error during execution of area()");
		PG_RETURN_NULL();
	}

	PG_FREE_IF_COPY(geom1, 0);

	PG_RETURN_POINTER(result);
}
