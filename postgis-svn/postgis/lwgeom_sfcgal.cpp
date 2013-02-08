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

#include <stdexcept>
#include <vector>
#include <string>
#include <iostream>
#include <fstream>

#include <SFCGAL/Geometry.h>
#include <SFCGAL/PreparedGeometry.h>
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
#include <SFCGAL/algorithm/distance.h>
#include <SFCGAL/algorithm/distance3d.h>
#include <SFCGAL/algorithm/plane.h>
#include <SFCGAL/transform/ForceZOrderPoints.h>
#include <SFCGAL/algorithm/collectionExtract.h>
#include <SFCGAL/io/wkt.h>
#include <SFCGAL/io/ewkt.h>
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
#include "lwgeom_sfcgal_wrapper.h"

/**
 * Conversion from GSERIALIZED* to SFCGAL::PreparedGeometry
 */
std::auto_ptr<SFCGAL::PreparedGeometry> POSTGIS2SFCGALp(GSERIALIZED *pglwgeom)
{
	LWGEOM *lwgeom = lwgeom_from_gserialized(pglwgeom);
	if ( ! lwgeom )
	{
		throw std::runtime_error("POSTGIS2SFCGALp: unable to deserialize input");
	}
	std::auto_ptr<SFCGAL::PreparedGeometry> g(new SFCGAL::PreparedGeometry(LWGEOM2SFCGAL(lwgeom), gserialized_get_srid(pglwgeom)) );
	lwgeom_free(lwgeom);
	return g;
}


/**
 * Conversion from GSERIALIZED* to SFCGAL::Geometry
 */
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

/**
 * Conversion from SFCGAL::Geometry to GSERIALIZED*
 */
GSERIALIZED* SFCGAL2POSTGIS(const SFCGAL::Geometry& geom, bool force3D, int SRID )
{
	LWGEOM* lwgeom = SFCGAL2LWGEOM( &geom, force3D, SRID );
	if ( lwgeom_needs_bbox(lwgeom) == LW_TRUE )
	{
		lwgeom_add_bbox(lwgeom);
	}

	GSERIALIZED* result = geometry_serialize(lwgeom);
	lwgeom_free(lwgeom);

	return result;
}

/**
 * Conversion from SFCGAL::PreparedGeometry to GSERIALIZED*
 */
GSERIALIZED* SFCGAL2POSTGIS( const SFCGAL::PreparedGeometry& geom, bool force3D )
{
	return SFCGAL2POSTGIS( geom.geometry(), force3D, geom.SRID() );
}

/**
 * Conversion from WKT to GSERIALIZED
 */
extern "C" {
	PG_FUNCTION_INFO_V1(sfcgal_from_text);
}
extern "C" Datum sfcgal_from_text(PG_FUNCTION_ARGS)
{
	GSERIALIZED* result;
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
		return 0;
	}
	result = SFCGAL2POSTGIS( *g, false );
	PG_RETURN_POINTER(result);
}

/**
 *
 * Macros needed for arguments of type SFCGAL::Geometry
 *
 */

// Type index, unique for each type
// How to extract a ith argument of type Geometry
#define SFCGAL_TYPE_Geometry_WRAPPER_INPUT( i )				\
	GSERIALIZED* BOOST_PP_CAT(input, i);				\
	BOOST_PP_CAT(input,i) = (GSERIALIZED*)PG_DETOAST_DATUM(PG_GETARG_DATUM(i)); \
	std::auto_ptr<SFCGAL::Geometry> BOOST_PP_CAT(geom,i);		\
	try								\
	{								\
		BOOST_PP_CAT( geom, i ) = POSTGIS2SFCGAL( BOOST_PP_CAT( input, i ) ); \
	}								\
	catch ( std::exception& e ) {					\
		lwerror( "Argument geometry could not be converted to SFCGAL: %s", e.what() ); \
		PG_RETURN_NULL();					\
	}
// Use dereference for std::auto_ptr<Geometry>
#define SFCGAL_TYPE_Geometry_WRAPPER_ACCESS_INPUT( i )  \
	* BOOST_PP_CAT( geom, i )

#define SFCGAL_TYPE_Geometry_WRAPPER_CONVERT_RESULT()			\
	GSERIALIZED* gresult;						\
	if ( result.get() ) {						\
		try {							\
			gresult = SFCGAL2POSTGIS( *result, result->is3D(), gserialized_get_srid(input0) ); \
		}							\
		catch ( std::exception& e ) {				\
			lwerror("Result geometry could not be converted to lwgeom: %s", e.what() ); \
			PG_RETURN_NULL();				\
		}							\
	}

#define SFCGAL_TYPE_Geometry_WRAPPER_RETURN()	\
	PG_RETURN_POINTER( gresult )

#define SFCGAL_TYPE_Geometry_WRAPPER_TO_CSTR( i )	\
	"%s", BOOST_PP_CAT( geom, i )->asText().c_str()

#define SFCGAL_TYPE_Geometry_WRAPPER_FREE_INPUT( i )	\
	PG_FREE_IF_COPY( BOOST_PP_CAT( input, i ), i )

#define SFCGAL_TYPE_Geometry_WRAPPER_DECLARE_RETURN_VAR()	\
	std::auto_ptr<SFCGAL::Geometry> result

SFCGAL_WRAPPER_DECLARE_FUNCTION( intersects, SFCGAL::algorithm::intersects, bool, (Geometry)(Geometry) )
SFCGAL_WRAPPER_DECLARE_FUNCTION( intersects3D, SFCGAL::algorithm::intersects3D, bool, (Geometry)(Geometry) )
SFCGAL_WRAPPER_DECLARE_FUNCTION( intersection, SFCGAL::algorithm::intersection, Geometry, (Geometry)(Geometry) )
SFCGAL_WRAPPER_DECLARE_FUNCTION( intersection3D, SFCGAL::algorithm::intersection3D, Geometry, (Geometry)(Geometry) )
SFCGAL_WRAPPER_DECLARE_FUNCTION( convexhull, SFCGAL::algorithm::convexHull, Geometry, (Geometry) )
SFCGAL_WRAPPER_DECLARE_FUNCTION( convexhull3D, SFCGAL::algorithm::convexHull3D, Geometry, (Geometry) )
SFCGAL_WRAPPER_DECLARE_FUNCTION( area, SFCGAL::algorithm::area, double, (Geometry) )
SFCGAL_WRAPPER_DECLARE_FUNCTION( area3D, SFCGAL::algorithm::area3D, double, (Geometry) )
SFCGAL_WRAPPER_DECLARE_FUNCTION( hasplane, _sfcgal_hasplane, bool, (Geometry) )
SFCGAL_WRAPPER_DECLARE_FUNCTION( pointing_up, _sfcgal_pointing_up, bool, (Geometry) )
SFCGAL_WRAPPER_DECLARE_FUNCTION( triangulate, _sfcgal_triangulate, Geometry, (Geometry) )
SFCGAL_WRAPPER_DECLARE_FUNCTION( triangulate2D, _sfcgal_triangulate2D, Geometry, (Geometry) )
SFCGAL_WRAPPER_DECLARE_FUNCTION( extrude, _sfcgal_extrude, Geometry, (Geometry)(double)(double)(double) )
SFCGAL_WRAPPER_DECLARE_FUNCTION( make_solid, _sfcgal_make_solid, Geometry, (Geometry) )
SFCGAL_WRAPPER_DECLARE_FUNCTION( force_z_up, _sfcgal_force_z_up, Geometry, (Geometry) )
SFCGAL_WRAPPER_DECLARE_FUNCTION( distance, SFCGAL::algorithm::distance, double, (Geometry)(Geometry) )
SFCGAL_WRAPPER_DECLARE_FUNCTION( distance3D, SFCGAL::algorithm::distance3D, double, (Geometry)(Geometry) )
SFCGAL_WRAPPER_DECLARE_FUNCTION( copy, _sfcgal_copy, Geometry, (Geometry) )
SFCGAL_WRAPPER_DECLARE_FUNCTION( buffer, _sfcgal_buffer2D, Geometry, (Geometry)(double)(int) )
SFCGAL_WRAPPER_DECLARE_FUNCTION( round, _sfcgal_round, Geometry, (Geometry)(int) )
SFCGAL_WRAPPER_DECLARE_FUNCTION( offset, _sfcgal_offset, Geometry, (Geometry)(double)(int) )

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


