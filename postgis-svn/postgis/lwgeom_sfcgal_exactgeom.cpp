#include "lwgeom_sfcgal_wrapper.h"

#include <SFCGAL/Geometry.h>
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
 * ExactGeometry varlen structure
 *
 * An exact geometry is a serialized representation of a SFCGAL::Geometry that
 * keeps the arbitrary precision of numbers
 */
struct ExactGeometry
{
	uint32_t size;
	char data[1];
};

/**
 * Convert a SFCGAL::Geometry to its binary serialization
 */
ExactGeometry* serializeExactGeometry( const SFCGAL::PreparedGeometry& g1 )
{
    std::string raw = SFCGAL::io::writeBinaryPrepared( g1 );
    ExactGeometry* g = (ExactGeometry*)palloc( raw.size() + 1 + 4 );
    memmove( &g->data[0], raw.data(), raw.size() );
    SET_VARSIZE( g, raw.size() );
    return g;
}

/**
 * Convert a binary serialization of a SFCGAL::Geometry to an instanciation
 */
std::auto_ptr<SFCGAL::PreparedGeometry> unserializeExactGeometry( ExactGeometry* ptr )
{
    uint32_t s = VARSIZE( ptr );
    std::string gstr( &ptr->data[0], s );
    return SFCGAL::io::readBinaryPrepared( gstr );	
}

/**
 * Text (WKT) representation of an exact geometry
 */
extern "C" {
	PG_FUNCTION_INFO_V1(sfcgal_exact_out);
}

extern "C" Datum sfcgal_exact_out(PG_FUNCTION_ARGS)
{
    ExactGeometry *geom1;
    
    geom1 = (ExactGeometry *)PG_GETARG_DATUM(0);

    std::auto_ptr<SFCGAL::PreparedGeometry> g = unserializeExactGeometry( geom1 );

    std::string wkt = g->asEWKT( /* exact */ -1 );
    char * retstr = (char*)palloc( wkt.size() + 1 );
    strncpy( retstr, wkt.c_str(), wkt.size() + 1 );

    PG_RETURN_CSTRING( retstr );
}

/**
 * Conversion from a text representation (WKT) to an exact geometry
 */
extern "C" {
	PG_FUNCTION_INFO_V1(sfcgal_exact_in);
	PG_FUNCTION_INFO_V1(sfcgal_exact_from_text);
}

ExactGeometry* exactFromCString( char* cstring )
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

	ExactGeometry* exactG = serializeExactGeometry( *g );
	return exactG;
}

extern "C" Datum sfcgal_exact_in(PG_FUNCTION_ARGS)
{
	char* cstring = PG_GETARG_CSTRING( 0 );
	ExactGeometry* e =  exactFromCString( cstring );
	if (!e)
		PG_RETURN_NULL();
	PG_RETURN_POINTER( e );
}

extern "C" Datum sfcgal_exact_from_text(PG_FUNCTION_ARGS)
{
	text *wkttext = PG_GETARG_TEXT_P(0);
	char *wkt = text2cstring(wkttext);
	ExactGeometry* e = exactFromCString( wkt );
	if (!e)
		PG_RETURN_NULL();
	PG_RETURN_POINTER( e );
}

/**
 * Conversion from a regular PostGIS geometry to an exact geometry
 */

extern "C" {
	PG_FUNCTION_INFO_V1(sfcgal_exact_from_geom);
}

extern "C" Datum sfcgal_exact_from_geom(PG_FUNCTION_ARGS)
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

    ExactGeometry* exactG = serializeExactGeometry( *g1 );
    PG_RETURN_POINTER( exactG );
}


/**
 * Conversion from an exact geometry to a regular PostGIS geometry
 */

extern "C" {
	PG_FUNCTION_INFO_V1(sfcgal_geom_from_exact);
}

extern "C" Datum sfcgal_geom_from_exact(PG_FUNCTION_ARGS)
{
    ExactGeometry *geom1 = (ExactGeometry *)PG_GETARG_DATUM(0);
    std::auto_ptr<SFCGAL::PreparedGeometry> g = unserializeExactGeometry( geom1 );

    GSERIALIZED* result = SFCGAL2POSTGIS( *g, false );
    PG_RETURN_POINTER( result );
}

/**
 *
 * Macros for exact geometry argument wrapping
 *
 */

#define WRAPPER_TYPE_exactGeometry 2
#define WRAPPER_INPUT_exactGeometry( i ) \
	std::auto_ptr<SFCGAL::PreparedGeometry> BOOST_PP_CAT( input, i )  = unserializeExactGeometry( (ExactGeometry*)PG_DETOAST_DATUM(PG_GETARG_DATUM(i)) );

#define WRAPPER_ACCESS_INPUT_exactGeometry( i )  \
	BOOST_PP_CAT( input, i )->geometry()

#define WRAPPER_FREE_INPUT_exactGeometry( i )  /* */
#define WRAPPER_CONVERT_RESULT_exactGeometry()   /* */
#define WRAPPER_DECLARE_RETURN_VAR_exactGeometry() \
	std::auto_ptr<SFCGAL::Geometry> result
#define WRAPPER_RETURN_exactGeometry() \
	SFCGAL::PreparedGeometry pgeom( result, input0->SRID() ); \
	PG_RETURN_POINTER( serializeExactGeometry( pgeom ) );

#define WRAPPER_TO_CSTR_exactGeometry( i )   			\
	"%s", BOOST_PP_CAT( input, i ) ->asEWKT().c_str()

WRAPPER_DECLARE_SFCGAL_FUNCTION( exact_intersects, SFCGAL::algorithm::intersects, bool, (exactGeometry)(exactGeometry) )
WRAPPER_DECLARE_SFCGAL_FUNCTION( exact_intersects3D, SFCGAL::algorithm::intersects3D, bool, (exactGeometry)(exactGeometry) )
WRAPPER_DECLARE_SFCGAL_FUNCTION( exact_intersection, SFCGAL::algorithm::intersection, exactGeometry, (exactGeometry)(exactGeometry) )
WRAPPER_DECLARE_SFCGAL_FUNCTION( exact_intersection3D, SFCGAL::algorithm::intersection3D, exactGeometry, (exactGeometry)(exactGeometry) )
WRAPPER_DECLARE_SFCGAL_FUNCTION( exact_convexhull, SFCGAL::algorithm::convexHull, exactGeometry, (exactGeometry) )
WRAPPER_DECLARE_SFCGAL_FUNCTION( exact_convexhull3D, SFCGAL::algorithm::convexHull3D, exactGeometry, (exactGeometry) )
WRAPPER_DECLARE_SFCGAL_FUNCTION( exact_area, SFCGAL::algorithm::area2D, double, (exactGeometry) )
WRAPPER_DECLARE_SFCGAL_FUNCTION( exact_area3D, SFCGAL::algorithm::area3D, double, (exactGeometry) )
WRAPPER_DECLARE_SFCGAL_FUNCTION( exact_hasplane, _sfcgal_hasplane, bool, (exactGeometry) )
WRAPPER_DECLARE_SFCGAL_FUNCTION( exact_pointing_up, _sfcgal_pointing_up, bool, (exactGeometry) )
WRAPPER_DECLARE_SFCGAL_FUNCTION( exact_triangulate, _sfcgal_triangulate, exactGeometry, (exactGeometry) )
WRAPPER_DECLARE_SFCGAL_FUNCTION( exact_triangulate2D, _sfcgal_triangulate2D, exactGeometry, (exactGeometry) )
WRAPPER_DECLARE_SFCGAL_FUNCTION( exact_extrude, _sfcgal_extrude, exactGeometry, (exactGeometry)(double)(double)(double) )
WRAPPER_DECLARE_SFCGAL_FUNCTION( exact_make_solid, _sfcgal_make_solid, exactGeometry, (exactGeometry) )
WRAPPER_DECLARE_SFCGAL_FUNCTION( exact_force_z_up, _sfcgal_force_z_up, exactGeometry, (exactGeometry) )
WRAPPER_DECLARE_SFCGAL_FUNCTION( exact_distance, SFCGAL::algorithm::distance, double, (exactGeometry)(exactGeometry) )
WRAPPER_DECLARE_SFCGAL_FUNCTION( exact_distance3D, SFCGAL::algorithm::distance3D, double, (exactGeometry)(exactGeometry) )
