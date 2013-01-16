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

std::auto_ptr<SFCGAL::PreparedGeometry> POSTGIS2SFCGALp(GSERIALIZED *pglwgeom)
{
	LWGEOM *lwgeom = lwgeom_from_gserialized(pglwgeom);
	if ( ! lwgeom )
	{
		throw std::runtime_error("POSTGIS2SFCGAL: unable to deserialize input");
	}
	std::auto_ptr<SFCGAL::PreparedGeometry> g(new SFCGAL::PreparedGeometry(LWGEOM2SFCGAL(lwgeom), gserialized_get_srid(pglwgeom)) );
	lwgeom_free(lwgeom);
	return g;
}


//
// Obsolete
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

GSERIALIZED* SFCGAL2POSTGIS( const SFCGAL::PreparedGeometry& geom, bool force3D )
{
	return SFCGAL2POSTGIS( geom.geometry(), force3D, geom.SRID() );
}

///
///
/// Conversion from WKT to GSERIALIZED
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

WRAPPER_DECLARE_SFCGAL_FUNCTION( intersects, SFCGAL::algorithm::intersects, bool, (Geometry)(Geometry) )
WRAPPER_DECLARE_SFCGAL_FUNCTION( intersects3D, SFCGAL::algorithm::intersects3D, bool, (Geometry)(Geometry) )
WRAPPER_DECLARE_SFCGAL_FUNCTION( intersection, SFCGAL::algorithm::intersection, Geometry, (Geometry)(Geometry) )
WRAPPER_DECLARE_SFCGAL_FUNCTION( intersection3D, SFCGAL::algorithm::intersection3D, Geometry, (Geometry)(Geometry) )
WRAPPER_DECLARE_SFCGAL_FUNCTION( convexhull, SFCGAL::algorithm::convexHull, Geometry, (Geometry) )
WRAPPER_DECLARE_SFCGAL_FUNCTION( convexhull3D, SFCGAL::algorithm::convexHull3D, Geometry, (Geometry) )
WRAPPER_DECLARE_SFCGAL_FUNCTION( area, SFCGAL::algorithm::area2D, double, (Geometry) )
WRAPPER_DECLARE_SFCGAL_FUNCTION( area3D, SFCGAL::algorithm::area3D, double, (Geometry) )
WRAPPER_DECLARE_SFCGAL_FUNCTION( hasplane, _sfcgal_hasplane, bool, (Geometry) )
WRAPPER_DECLARE_SFCGAL_FUNCTION( pointing_up, _sfcgal_pointing_up, bool, (Geometry) )
WRAPPER_DECLARE_SFCGAL_FUNCTION( triangulate, _sfcgal_triangulate, Geometry, (Geometry) )
WRAPPER_DECLARE_SFCGAL_FUNCTION( triangulate2D, _sfcgal_triangulate2D, Geometry, (Geometry) )
WRAPPER_DECLARE_SFCGAL_FUNCTION( extrude, _sfcgal_extrude, Geometry, (Geometry)(double)(double)(double) )
WRAPPER_DECLARE_SFCGAL_FUNCTION( make_solid, _sfcgal_make_solid, Geometry, (Geometry) )
WRAPPER_DECLARE_SFCGAL_FUNCTION( force_z_up, _sfcgal_force_z_up, Geometry, (Geometry) )
WRAPPER_DECLARE_SFCGAL_FUNCTION( distance, SFCGAL::algorithm::distance, double, (Geometry)(Geometry) )
WRAPPER_DECLARE_SFCGAL_FUNCTION( distance3D, SFCGAL::algorithm::distance3D, double, (Geometry)(Geometry) )

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


