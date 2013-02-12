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

#include "postgres.h"
#include "fmgr.h"

#include "lwgeom_sfcgal_c.h"

GSERIALIZED *geometry_serialize(LWGEOM *lwgeom);
char* text2cstring(const text *textptr);

/**
 * Conversion from GSERIALIZED* to SFCGAL::Geometry
 */
sfcgal_geometry_t* POSTGIS2SFCGALGeometry(GSERIALIZED *pglwgeom)
{
	LWGEOM *lwgeom = lwgeom_from_gserialized(pglwgeom);
	sfcgal_geometry_t* g;
	if ( ! lwgeom )
	{
		lwerror("POSTGIS2SFCGALGeometry: unable to deserialize input");
	}
	g = LWGEOM2SFCGAL( lwgeom );
	lwgeom_free(lwgeom);
	return g;
}

/**
 * Conversion from GSERIALIZED* to SFCGAL::PreparedGeometry
 */
sfcgal_prepared_geometry_t* POSTGIS2SFCGALPreparedGeometry(GSERIALIZED *pglwgeom)
{
	LWGEOM *lwgeom = lwgeom_from_gserialized(pglwgeom);
	sfcgal_geometry_t* g;
	if ( ! lwgeom )
	{
		lwerror("POSTGIS2SFCGALPreparedGeometry: unable to deserialize input");
	}
	g = LWGEOM2SFCGAL( lwgeom );
	lwgeom_free(lwgeom);
	return sfcgal_prepared_geometry_create_from_geometry( g, gserialized_get_srid(pglwgeom) );
}

/**
 * Conversion from SFCGAL::Geometry to GSERIALIZED*
 */
GSERIALIZED* SFCGALGeometry2POSTGIS( const sfcgal_geometry_t* geom, int force3D, int SRID )
{
	LWGEOM* lwgeom = SFCGAL2LWGEOM( geom, force3D, SRID );
	GSERIALIZED *result;
	if ( lwgeom_needs_bbox(lwgeom) == LW_TRUE )
	{
		lwgeom_add_bbox(lwgeom);
	}

	result = geometry_serialize(lwgeom);
	lwgeom_free(lwgeom);

	return result;
}

/**
 * Conversion from SFCGAL::PreparedGeometry to GSERIALIZED*
 */
GSERIALIZED* SFCGALPreparedGeometry2POSTGIS( const sfcgal_prepared_geometry_t* geom, int force3D )
{
    return SFCGALGeometry2POSTGIS( sfcgal_prepared_geometry_geometry( geom ), force3D, sfcgal_prepared_geometry_srid( geom ) );
}

static int __sfcgal_init = 0;

void sfcgal_postgis_init()
{
    if ( ! __sfcgal_init ) {
	sfcgal_init();
	sfcgal_set_error_handlers( (sfcgal_error_handler_t)lwnotice, (sfcgal_error_handler_t)lwerror );
	__sfcgal_init = 1;
    }
}

/**
 * Conversion from WKT to GSERIALIZED
 */
Datum sfcgal_from_text(PG_FUNCTION_ARGS);
PG_FUNCTION_INFO_V1(sfcgal_from_text);
Datum sfcgal_from_text(PG_FUNCTION_ARGS)
{
	GSERIALIZED* result;
	sfcgal_prepared_geometry_t* g;
	text *wkttext = PG_GETARG_TEXT_P(0);
	char *cstring = text2cstring(wkttext);

	sfcgal_postgis_init();

	g = sfcgal_io_read_ewkt( cstring, strlen(cstring) );

	result = SFCGALPreparedGeometry2POSTGIS( g, 0 );
	sfcgal_prepared_geometry_delete( g );
	PG_RETURN_POINTER(result);
}


#define _SFCGAL_WRAPPER_UNARY_SCALAR( name, fname, ret_type, return_call )	\
    Datum sfcgal_##name(PG_FUNCTION_ARGS);				\
    PG_FUNCTION_INFO_V1(sfcgal_##name);					\
    Datum sfcgal_##name(PG_FUNCTION_ARGS)				\
    {									\
	GSERIALIZED *input0;						\
	sfcgal_geometry_t *geom0;					\
	ret_type result;						\
									\
	input0 = (GSERIALIZED*)PG_DETOAST_DATUM(PG_GETARG_DATUM(0));	\
	geom0 = POSTGIS2SFCGALGeometry( input0 );			\
									\
	sfcgal_postgis_init();						\
									\
	result = fname( geom0 );					\
	sfcgal_geometry_delete( geom0 );				\
									\
	PG_FREE_IF_COPY( input0, 0 );					\
									\
	return_call( result );						\
    }

#define _SFCGAL_WRAPPER_UNARY_MEASURE( name, fname )	\
    _SFCGAL_WRAPPER_UNARY_SCALAR( name, fname, double, PG_RETURN_FLOAT8 )
#define _SFCGAL_WRAPPER_UNARY_PREDICATE( name, fname )	\
    _SFCGAL_WRAPPER_UNARY_SCALAR( name, fname, int, PG_RETURN_BOOL )

#define _SFCGAL_WRAPPER_BINARY_SCALAR( name, fname, ret_type, return_call )	\
    Datum sfcgal_##name(PG_FUNCTION_ARGS);				\
    PG_FUNCTION_INFO_V1(sfcgal_##name);					\
    Datum sfcgal_##name(PG_FUNCTION_ARGS)				\
    {									\
	GSERIALIZED *input0, *input1;					\
	sfcgal_geometry_t *geom0, *geom1;					\
	ret_type result;						\
									\
	input0 = (GSERIALIZED*)PG_DETOAST_DATUM(PG_GETARG_DATUM(0));	\
	input1 = (GSERIALIZED*)PG_DETOAST_DATUM(PG_GETARG_DATUM(1));	\
	geom0 = POSTGIS2SFCGALGeometry( input0 );			\
	geom1 = POSTGIS2SFCGALGeometry( input1 );			\
									\
	sfcgal_postgis_init();						\
									\
	result = fname( geom0, geom1 );					\
	sfcgal_geometry_delete( geom0 );				\
	sfcgal_geometry_delete( geom1 );				\
									\
	PG_FREE_IF_COPY( input0, 0 );					\
	PG_FREE_IF_COPY( input1, 0 );					\
									\
	return_call( result );						\
    }

#define _SFCGAL_WRAPPER_BINARY_MEASURE( name, fname ) \
    _SFCGAL_WRAPPER_BINARY_SCALAR( name, fname, double, PG_RETURN_FLOAT8 )
#define _SFCGAL_WRAPPER_BINARY_PREDICATE( name, fname ) \
    _SFCGAL_WRAPPER_BINARY_SCALAR( name, fname, int, PG_RETURN_BOOL )

#define _SFCGAL_WRAPPER_UNARY_CONSTRUCTION( name, fname )		\
    Datum sfcgal_##name(PG_FUNCTION_ARGS);				\
    PG_FUNCTION_INFO_V1(sfcgal_##name);					\
    Datum sfcgal_##name(PG_FUNCTION_ARGS)				\
    {									\
	GSERIALIZED *input0, *output;					\
	sfcgal_geometry_t *geom0;					\
	sfcgal_geometry_t *result;					\
									\
	input0 = (GSERIALIZED*)PG_DETOAST_DATUM(PG_GETARG_DATUM(0));	\
	geom0 = POSTGIS2SFCGALGeometry( input0 );			\
									\
	sfcgal_postgis_init();						\
									\
	result = fname( geom0 );					\
	sfcgal_geometry_delete( geom0 );				\
									\
	PG_FREE_IF_COPY( input0, 0 );					\
									\
	output = SFCGALGeometry2POSTGIS( result, 0, gserialized_get_srid( input0 ) ); \
	sfcgal_geometry_delete( result );				\
									\
	PG_RETURN_POINTER( output );					\
    }

#define _SFCGAL_WRAPPER_BINARY_CONSTRUCTION( name, fname )		\
    Datum sfcgal_##name(PG_FUNCTION_ARGS);				\
    PG_FUNCTION_INFO_V1(sfcgal_##name);					\
    Datum sfcgal_##name(PG_FUNCTION_ARGS)				\
    {									\
	GSERIALIZED *input0, *input1, *output;				\
	sfcgal_geometry_t *geom0, *geom1;				\
	sfcgal_geometry_t *result;					\
									\
	input0 = (GSERIALIZED*)PG_DETOAST_DATUM(PG_GETARG_DATUM(0));	\
	input1 = (GSERIALIZED*)PG_DETOAST_DATUM(PG_GETARG_DATUM(1));	\
	geom0 = POSTGIS2SFCGALGeometry( input0 );			\
	geom1 = POSTGIS2SFCGALGeometry( input1 );			\
									\
	sfcgal_postgis_init();						\
									\
	result = fname( geom0, geom1 );					\
	sfcgal_geometry_delete( geom0 );				\
	sfcgal_geometry_delete( geom1 );				\
									\
	PG_FREE_IF_COPY( input0, 0 );					\
	PG_FREE_IF_COPY( input1, 1 );					\
									\
	output = SFCGALGeometry2POSTGIS( result, 0, gserialized_get_srid( input0 ) ); \
	sfcgal_geometry_delete( result );				\
									\
	PG_RETURN_POINTER( output );					\
    }

_SFCGAL_WRAPPER_UNARY_MEASURE( area, sfcgal_geometry_area )
_SFCGAL_WRAPPER_UNARY_MEASURE( area3D, sfcgal_geometry_area_3d )

_SFCGAL_WRAPPER_UNARY_PREDICATE( has_plane, sfcgal_geometry_has_plane )
_SFCGAL_WRAPPER_UNARY_PREDICATE( pointing_up, sfcgal_geometry_pointing_up )

_SFCGAL_WRAPPER_BINARY_PREDICATE( intersects, sfcgal_geometry_intersects )
_SFCGAL_WRAPPER_BINARY_PREDICATE( intersects3D, sfcgal_geometry_intersects_3d )

_SFCGAL_WRAPPER_BINARY_MEASURE( distance, sfcgal_geometry_distance )
_SFCGAL_WRAPPER_BINARY_MEASURE( distance3D, sfcgal_geometry_distance_3d )

_SFCGAL_WRAPPER_UNARY_CONSTRUCTION( convexhull, sfcgal_geometry_convexhull )
_SFCGAL_WRAPPER_UNARY_CONSTRUCTION( convexhull3D, sfcgal_geometry_convexhull_3d )
_SFCGAL_WRAPPER_UNARY_CONSTRUCTION( triangulate, sfcgal_geometry_triangulate )
_SFCGAL_WRAPPER_UNARY_CONSTRUCTION( triangulate2D, sfcgal_geometry_triangulate_2d )
_SFCGAL_WRAPPER_UNARY_CONSTRUCTION( make_solid, sfcgal_geometry_make_solid )
_SFCGAL_WRAPPER_UNARY_CONSTRUCTION( force_z_up, sfcgal_geometry_force_z_up )
_SFCGAL_WRAPPER_UNARY_CONSTRUCTION( copy, sfcgal_geometry_copy )
_SFCGAL_WRAPPER_UNARY_CONSTRUCTION( straight_skeleton, sfcgal_geometry_straight_skeleton )

_SFCGAL_WRAPPER_BINARY_CONSTRUCTION( intersection, sfcgal_geometry_intersection )
_SFCGAL_WRAPPER_BINARY_CONSTRUCTION( intersection3D, sfcgal_geometry_intersection_3d )
_SFCGAL_WRAPPER_BINARY_CONSTRUCTION( minkowski_sum, sfcgal_geometry_minkowski_sum )

Datum sfcgal_extrude(PG_FUNCTION_ARGS);
PG_FUNCTION_INFO_V1(sfcgal_extrude);
Datum sfcgal_extrude(PG_FUNCTION_ARGS)
{
    GSERIALIZED *input0, *output;
    sfcgal_geometry_t *geom0;
    sfcgal_geometry_t *result;
    double dx, dy, dz;

    input0 = (GSERIALIZED*)PG_DETOAST_DATUM(PG_GETARG_DATUM(0));
    geom0 = POSTGIS2SFCGALGeometry( input0 );
    dx = PG_GETARG_FLOAT8( 1 );
    dy = PG_GETARG_FLOAT8( 2 );
    dz = PG_GETARG_FLOAT8( 3 );
    
    sfcgal_postgis_init();
    
    result = sfcgal_geometry_extrude( geom0, dx, dy, dz );
    sfcgal_geometry_delete( geom0 );
    
    PG_FREE_IF_COPY( input0, 0 );
    
    output = SFCGALGeometry2POSTGIS( result, 0, gserialized_get_srid( input0 ) );
    sfcgal_geometry_delete( result );
    
    PG_RETURN_POINTER( output );
}

Datum sfcgal_offset_polygon(PG_FUNCTION_ARGS);
PG_FUNCTION_INFO_V1(sfcgal_offset_polygon);
Datum sfcgal_offset_polygon(PG_FUNCTION_ARGS)
{
    GSERIALIZED *input0, *output;
    sfcgal_geometry_t *geom0;
    sfcgal_geometry_t *result;
    double offset;

    input0 = (GSERIALIZED*)PG_DETOAST_DATUM(PG_GETARG_DATUM(0));
    geom0 = POSTGIS2SFCGALGeometry( input0 );
    offset = PG_GETARG_FLOAT8( 1 );
    
    sfcgal_postgis_init();
    
    result = sfcgal_geometry_offset_polygon( geom0, offset );
    sfcgal_geometry_delete( geom0 );
    
    PG_FREE_IF_COPY( input0, 0 );
    
    output = SFCGALGeometry2POSTGIS( result, 0, gserialized_get_srid( input0 ) );
    sfcgal_geometry_delete( result );
    
    PG_RETURN_POINTER( output );
}

Datum sfcgal_round(PG_FUNCTION_ARGS);
PG_FUNCTION_INFO_V1(sfcgal_round);
Datum sfcgal_round(PG_FUNCTION_ARGS)
{
    GSERIALIZED *input0, *output;
    sfcgal_geometry_t *geom0;
    sfcgal_geometry_t *result;
    int scale;

    input0 = (GSERIALIZED*)PG_DETOAST_DATUM(PG_GETARG_DATUM(0));
    geom0 = POSTGIS2SFCGALGeometry( input0 );
    scale = PG_GETARG_INT32( 1 );
    
    sfcgal_postgis_init();
    
    result = sfcgal_geometry_round( geom0, scale );
    sfcgal_geometry_delete( geom0 );
    
    PG_FREE_IF_COPY( input0, 0 );
    
    output = SFCGALGeometry2POSTGIS( result, 0, gserialized_get_srid( input0 ) );
    sfcgal_geometry_delete( result );
    
    PG_RETURN_POINTER( output );
}

