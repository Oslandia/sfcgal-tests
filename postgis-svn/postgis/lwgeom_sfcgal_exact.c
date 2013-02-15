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

#include "lwgeom_sfcgal.h"

GSERIALIZED *geometry_serialize(LWGEOM *lwgeom);
char* text2cstring(const text *textptr);

/**
 * sfcgal_serialized_geometry varlen structure
 *
 * An exact geometry is a serialized representation of a SFCGAL::Geometry that
 * keeps the arbitrary precision of numbers
 */
typedef struct
{
    uint32_t size;
    char data[1];
} sfcgal_serialized_geometry_t;

sfcgal_prepared_geometry_t* unserialize_exact_geometry( const sfcgal_serialized_geometry_t * serialized_geometry );
sfcgal_serialized_geometry_t* serialize_exact_geometry( const sfcgal_prepared_geometry_t * prepared_geometry );

Datum sfcgal_exact_from_text(PG_FUNCTION_ARGS);
Datum sfcgal_exact_from_geom(PG_FUNCTION_ARGS);
Datum sfcgal_geom_from_exact(PG_FUNCTION_ARGS);
Datum sfcgal_exact_in(PG_FUNCTION_ARGS);
Datum sfcgal_exact_out(PG_FUNCTION_ARGS);

Datum sfcgal_exact_round(PG_FUNCTION_ARGS);
Datum sfcgal_exact_extrude(PG_FUNCTION_ARGS);
Datum sfcgal_exact_offset_polygon(PG_FUNCTION_ARGS);

sfcgal_prepared_geometry_t* unserialize_exact_geometry( const sfcgal_serialized_geometry_t * serialized_geometry )
{
    return sfcgal_io_read_binary_prepared( &serialized_geometry->data[0], VARSIZE( serialized_geometry ) );
}

sfcgal_serialized_geometry_t* serialize_exact_geometry( const sfcgal_prepared_geometry_t * prepared_geometry )
{
    char* buffer;
    size_t len;
    sfcgal_serialized_geometry_t* ret;

    sfcgal_io_write_binary_prepared( prepared_geometry, &buffer, &len );
    ret = (sfcgal_serialized_geometry_t*)lwrealloc( buffer, len + 4 );
    memmove( &ret->data[0], ret, len );

    SET_VARSIZE( ret, len );
    return ret;
}

/**
 * Convert a regular GSERIALIED geometry to a serialized exact geometry
 */
PG_FUNCTION_INFO_V1(sfcgal_exact_from_geom);
Datum sfcgal_exact_from_geom(PG_FUNCTION_ARGS)
{
    GSERIALIZED *input0;
    sfcgal_prepared_geometry_t* pgeom0;
    sfcgal_serialized_geometry_t* ret;
    
    sfcgal_postgis_init();

    input0 = (GSERIALIZED *)PG_DETOAST_DATUM(PG_GETARG_DATUM(0));
    pgeom0 = POSTGIS2SFCGALPreparedGeometry( input0 );

    ret = serialize_exact_geometry( pgeom0 );
    sfcgal_prepared_geometry_delete( pgeom0 );
    PG_RETURN_POINTER( ret );
}

/**
 * Convert a serialized exact geometry to a regular GSERIALIED geometry
 */
PG_FUNCTION_INFO_V1(sfcgal_geom_from_exact);
Datum sfcgal_geom_from_exact(PG_FUNCTION_ARGS)
{
	sfcgal_serialized_geometry_t *input0;
	sfcgal_prepared_geometry_t *pgeom0;
	GSERIALIZED *output;

	sfcgal_postgis_init();

	input0 = (sfcgal_serialized_geometry_t*)PG_DETOAST_DATUM(PG_GETARG_DATUM(0));
	pgeom0 = unserialize_exact_geometry( input0 );
	PG_FREE_IF_COPY( input0, 0 );

	output = SFCGALPreparedGeometry2POSTGIS( pgeom0, 0 );
	sfcgal_prepared_geometry_delete( pgeom0 );
	PG_RETURN_POINTER( output );
}

/**
 * Convert TEXT to exact geometry
 */
PG_FUNCTION_INFO_V1(sfcgal_exact_from_text);
Datum sfcgal_exact_from_text(PG_FUNCTION_ARGS)
{
    text *wkttext = PG_GETARG_TEXT_P(0);
    char *wkt = text2cstring(wkttext);
    sfcgal_prepared_geometry_t* pgeom;
    sfcgal_serialized_geometry_t* sgeom;
    
    sfcgal_postgis_init();
    
    pgeom = sfcgal_io_read_ewkt( wkt, strlen(wkt) );
    
    sgeom = serialize_exact_geometry( pgeom );
    sfcgal_prepared_geometry_delete( pgeom );
    
    PG_RETURN_POINTER( sgeom );
}

/**
 * Input an exact geometry
 */
PG_FUNCTION_INFO_V1(sfcgal_exact_in);
Datum sfcgal_exact_in(PG_FUNCTION_ARGS)
{
    char *wkt = PG_GETARG_CSTRING( 0 );
    sfcgal_prepared_geometry_t* pgeom;
    sfcgal_serialized_geometry_t* sgeom;
    
    sfcgal_postgis_init();
    
    pgeom = sfcgal_io_read_ewkt( wkt, strlen(wkt) );
    
    sgeom = serialize_exact_geometry( pgeom );
    sfcgal_prepared_geometry_delete( pgeom );
    
    PG_RETURN_POINTER( sgeom );
}

/**
 * Display an exact geometry
 */
PG_FUNCTION_INFO_V1(sfcgal_exact_out);
Datum sfcgal_exact_out(PG_FUNCTION_ARGS)
{
    sfcgal_serialized_geometry_t *input0;
    sfcgal_prepared_geometry_t *pgeom0;
    char *retstr;
    size_t len;

    sfcgal_postgis_init();
    
    input0 = (sfcgal_serialized_geometry_t*)PG_DETOAST_DATUM(PG_GETARG_DATUM(0));
    pgeom0 = unserialize_exact_geometry( input0 );
    PG_FREE_IF_COPY( input0, 0 );

    sfcgal_prepared_geometry_as_ewkt( pgeom0, /* numDecimals */ -1, &retstr, &len );
    sfcgal_prepared_geometry_delete( pgeom0 );

    PG_RETURN_CSTRING( retstr );
}

#define _SFCGAL_EXACT_WRAPPER_UNARY_SCALAR( name, fname, ret_type, return_call ) \
    Datum sfcgal_exact_##name(PG_FUNCTION_ARGS);			\
    PG_FUNCTION_INFO_V1(sfcgal_exact_##name);				\
    Datum sfcgal_exact_##name(PG_FUNCTION_ARGS)				\
    {									\
	sfcgal_serialized_geometry_t *input0;				\
	sfcgal_prepared_geometry_t *pgeom0;				\
	const sfcgal_geometry_t *geom0;					\
	ret_type result;						\
									\
	sfcgal_postgis_init();						\
									\
	input0 = (sfcgal_serialized_geometry_t*)PG_DETOAST_DATUM(PG_GETARG_DATUM(0)); \
	pgeom0 = unserialize_exact_geometry( input0 );			\
	PG_FREE_IF_COPY( input0, 0 );					\
	geom0 = sfcgal_prepared_geometry_geometry( pgeom0 );		\
									\
	result = fname( geom0 );					\
	sfcgal_prepared_geometry_delete( pgeom0 );			\
									\
	return_call( result );						\
    }
    
#define _SFCGAL_EXACT_WRAPPER_UNARY_PREDICATE( name, fname ) \
    _SFCGAL_EXACT_WRAPPER_UNARY_SCALAR( name, fname, int, PG_RETURN_BOOL )
#define _SFCGAL_EXACT_WRAPPER_UNARY_MEASURE( name, fname ) \
    _SFCGAL_EXACT_WRAPPER_UNARY_SCALAR( name, fname, double, PG_RETURN_FLOAT8 )

#define _SFCGAL_EXACT_WRAPPER_BINARY_SCALAR( name, fname, ret_type, return_call ) \
    Datum sfcgal_exact_##name(PG_FUNCTION_ARGS);			\
    PG_FUNCTION_INFO_V1(sfcgal_exact_##name);				\
    Datum sfcgal_exact_##name(PG_FUNCTION_ARGS)				\
    {									\
	sfcgal_serialized_geometry_t *input0, *input1;			\
	sfcgal_prepared_geometry_t *pgeom0, *pgeom1;			\
	const sfcgal_geometry_t *geom0, *geom1;				\
	ret_type result;						\
									\
	sfcgal_postgis_init();						\
									\
	input0 = (sfcgal_serialized_geometry_t*)PG_DETOAST_DATUM(PG_GETARG_DATUM(0)); \
	pgeom0 = unserialize_exact_geometry( input0 );			\
	PG_FREE_IF_COPY( input0, 0 );					\
	geom0 = sfcgal_prepared_geometry_geometry( pgeom0 );		\
									\
	input1 = (sfcgal_serialized_geometry_t*)PG_DETOAST_DATUM(PG_GETARG_DATUM(1)); \
	pgeom1 = unserialize_exact_geometry( input1 );			\
	PG_FREE_IF_COPY( input1, 1 );					\
	geom1 = sfcgal_prepared_geometry_geometry( pgeom1 );		\
									\
	result = fname( geom0, geom1 );					\
	sfcgal_prepared_geometry_delete( pgeom0 );			\
	sfcgal_prepared_geometry_delete( pgeom1 );			\
									\
	return_call( result );						\
    }

#define _SFCGAL_EXACT_WRAPPER_BINARY_PREDICATE( name, fname ) \
    _SFCGAL_EXACT_WRAPPER_BINARY_SCALAR( name, fname, int, PG_RETURN_BOOL )
#define _SFCGAL_EXACT_WRAPPER_BINARY_MEASURE( name, fname ) \
    _SFCGAL_EXACT_WRAPPER_BINARY_SCALAR( name, fname, double, PG_RETURN_FLOAT8 )

#define _SFCGAL_EXACT_WRAPPER_UNARY_CONSTRUCTION( name, fname )		\
    Datum sfcgal_exact_##name(PG_FUNCTION_ARGS);			\
    PG_FUNCTION_INFO_V1(sfcgal_exact_##name);				\
    Datum sfcgal_exact_##name(PG_FUNCTION_ARGS)				\
    {									\
	sfcgal_serialized_geometry_t *input0, *output;			\
	sfcgal_prepared_geometry_t *pgeom0, *presult;			\
	const sfcgal_geometry_t *geom0;					\
	sfcgal_geometry_t *result;					\
	srid_t srid;							\
									\
	sfcgal_postgis_init();						\
									\
	input0 = (sfcgal_serialized_geometry_t*)PG_DETOAST_DATUM(PG_GETARG_DATUM(0)); \
	pgeom0 = unserialize_exact_geometry( input0 );			\
	PG_FREE_IF_COPY( input0, 0 );					\
	geom0 = sfcgal_prepared_geometry_geometry( pgeom0 );		\
	srid = sfcgal_prepared_geometry_srid( pgeom0 );			\
									\
	result = fname( geom0 );					\
	sfcgal_prepared_geometry_delete( pgeom0 );			\
									\
	presult = sfcgal_prepared_geometry_create_from_geometry( result, srid ); \
	output = serialize_exact_geometry( presult );			\
	sfcgal_prepared_geometry_delete( presult );			\
									\
	PG_RETURN_POINTER( output );					\
    }

#define _SFCGAL_EXACT_WRAPPER_BINARY_CONSTRUCTION( name, fname )	\
    Datum sfcgal_exact_##name(PG_FUNCTION_ARGS);			\
    PG_FUNCTION_INFO_V1(sfcgal_exact_##name);				\
    Datum sfcgal_exact_##name(PG_FUNCTION_ARGS)				\
    {									\
	sfcgal_serialized_geometry_t *input0, *input1, *output;		\
	sfcgal_prepared_geometry_t *pgeom0, *pgeom1, *presult;		\
	const sfcgal_geometry_t *geom0, *geom1;				\
	sfcgal_geometry_t *result;					\
	srid_t srid;							\
									\
	sfcgal_postgis_init();						\
									\
	input0 = (sfcgal_serialized_geometry_t*)PG_DETOAST_DATUM(PG_GETARG_DATUM(0)); \
	pgeom0 = unserialize_exact_geometry( input0 );			\
	PG_FREE_IF_COPY( input0, 0 );					\
	geom0 = sfcgal_prepared_geometry_geometry( pgeom0 );		\
	srid = sfcgal_prepared_geometry_srid( pgeom0 );			\
									\
	input1 = (sfcgal_serialized_geometry_t*)PG_DETOAST_DATUM(PG_GETARG_DATUM(1)); \
	pgeom1 = unserialize_exact_geometry( input1 );			\
	PG_FREE_IF_COPY( input1, 1 );					\
	geom1 = sfcgal_prepared_geometry_geometry( pgeom1 );		\
									\
	result = fname( geom0, geom1 );					\
	sfcgal_prepared_geometry_delete( pgeom0 );			\
	sfcgal_prepared_geometry_delete( pgeom1 );			\
									\
	presult = sfcgal_prepared_geometry_create_from_geometry( result, srid ); \
	output = serialize_exact_geometry( presult );			\
	sfcgal_prepared_geometry_delete( presult );			\
									\
	PG_RETURN_POINTER( output );					\
    }

_SFCGAL_EXACT_WRAPPER_UNARY_MEASURE( area, sfcgal_geometry_area )
_SFCGAL_EXACT_WRAPPER_UNARY_MEASURE( area3D, sfcgal_geometry_area_3d )

_SFCGAL_EXACT_WRAPPER_UNARY_PREDICATE( has_plane, sfcgal_geometry_has_plane )
_SFCGAL_EXACT_WRAPPER_UNARY_PREDICATE( pointing_up, sfcgal_geometry_pointing_up )

_SFCGAL_EXACT_WRAPPER_BINARY_PREDICATE( intersects, sfcgal_geometry_intersects )
_SFCGAL_EXACT_WRAPPER_BINARY_PREDICATE( intersects3D, sfcgal_geometry_intersects_3d )

_SFCGAL_EXACT_WRAPPER_BINARY_MEASURE( distance, sfcgal_geometry_distance )
_SFCGAL_EXACT_WRAPPER_BINARY_MEASURE( distance3D, sfcgal_geometry_distance_3d )

_SFCGAL_EXACT_WRAPPER_UNARY_CONSTRUCTION( convexhull, sfcgal_geometry_convexhull )
_SFCGAL_EXACT_WRAPPER_UNARY_CONSTRUCTION( convexhull3D, sfcgal_geometry_convexhull_3d )
_SFCGAL_EXACT_WRAPPER_UNARY_CONSTRUCTION( triangulate, sfcgal_geometry_triangulate )
_SFCGAL_EXACT_WRAPPER_UNARY_CONSTRUCTION( triangulate2D, sfcgal_geometry_triangulate_2d )
_SFCGAL_EXACT_WRAPPER_UNARY_CONSTRUCTION( make_solid, sfcgal_geometry_make_solid )
_SFCGAL_EXACT_WRAPPER_UNARY_CONSTRUCTION( force_z_up, sfcgal_geometry_force_z_up )
_SFCGAL_EXACT_WRAPPER_UNARY_CONSTRUCTION( copy, sfcgal_geometry_copy )
_SFCGAL_EXACT_WRAPPER_UNARY_CONSTRUCTION( straight_skeleton, sfcgal_geometry_straight_skeleton )

_SFCGAL_EXACT_WRAPPER_BINARY_CONSTRUCTION( intersection, sfcgal_geometry_intersection )
_SFCGAL_EXACT_WRAPPER_BINARY_CONSTRUCTION( intersection3D, sfcgal_geometry_intersection_3d )
_SFCGAL_EXACT_WRAPPER_BINARY_CONSTRUCTION( minkowski_sum, sfcgal_geometry_minkowski_sum )

PG_FUNCTION_INFO_V1(sfcgal_exact_extrude);
Datum sfcgal_exact_extrude(PG_FUNCTION_ARGS)
{
    sfcgal_serialized_geometry_t *input0, *output;
    sfcgal_prepared_geometry_t *pgeom0, *presult;
    const sfcgal_geometry_t *geom0;
    sfcgal_geometry_t *result;
    srid_t srid;

    double dx, dy, dz;

    sfcgal_postgis_init();
    
    input0 = (sfcgal_serialized_geometry_t*)PG_DETOAST_DATUM(PG_GETARG_DATUM(0));
    pgeom0 = unserialize_exact_geometry( input0 );
    PG_FREE_IF_COPY( input0, 0 );
    geom0 = sfcgal_prepared_geometry_geometry( pgeom0 );
    srid = sfcgal_prepared_geometry_srid( pgeom0 );

    dx = PG_GETARG_FLOAT8( 1 );
    dy = PG_GETARG_FLOAT8( 2 );
    dz = PG_GETARG_FLOAT8( 3 );
    
    result = sfcgal_geometry_extrude( geom0, dx, dy, dz );
    sfcgal_prepared_geometry_delete( pgeom0 );

    presult = sfcgal_prepared_geometry_create_from_geometry( result, srid );
    output = serialize_exact_geometry( presult );
    sfcgal_prepared_geometry_delete( presult );
    
    PG_RETURN_POINTER( output );
}

PG_FUNCTION_INFO_V1(sfcgal_exact_round);
Datum sfcgal_exact_round(PG_FUNCTION_ARGS)
{
    sfcgal_serialized_geometry_t *input0, *output;
    sfcgal_prepared_geometry_t *pgeom0, *presult;
    const sfcgal_geometry_t *geom0;
    sfcgal_geometry_t *result;
    srid_t srid;
    int scale;

    sfcgal_postgis_init();
    
    input0 = (sfcgal_serialized_geometry_t*)PG_DETOAST_DATUM(PG_GETARG_DATUM(0));
    pgeom0 = unserialize_exact_geometry( input0 );
    PG_FREE_IF_COPY( input0, 0 );
    geom0 = sfcgal_prepared_geometry_geometry( pgeom0 );
    srid = sfcgal_prepared_geometry_srid( pgeom0 );

    scale = PG_GETARG_INT32( 1 );
    
    result = sfcgal_geometry_round( geom0, scale );
    sfcgal_prepared_geometry_delete( pgeom0 );

    presult = sfcgal_prepared_geometry_create_from_geometry( result, srid );
    output = serialize_exact_geometry( presult );
    sfcgal_prepared_geometry_delete( presult );
    
    PG_RETURN_POINTER( output );
}

PG_FUNCTION_INFO_V1(sfcgal_exact_offset_polygon);
Datum sfcgal_exact_offset_polygon(PG_FUNCTION_ARGS)
{
    sfcgal_serialized_geometry_t *input0, *output;
    sfcgal_prepared_geometry_t *pgeom0, *presult;
    const sfcgal_geometry_t *geom0;
    sfcgal_geometry_t *result;
    srid_t srid;
    double offset;

    sfcgal_postgis_init();
    
    input0 = (sfcgal_serialized_geometry_t*)PG_DETOAST_DATUM(PG_GETARG_DATUM(0));
    pgeom0 = unserialize_exact_geometry( input0 );
    PG_FREE_IF_COPY( input0, 0 );
    geom0 = sfcgal_prepared_geometry_geometry( pgeom0 );
    srid = sfcgal_prepared_geometry_srid( pgeom0 );

    offset = PG_GETARG_FLOAT8( 1 );
    
    result = sfcgal_geometry_offset_polygon( geom0, offset );
    sfcgal_prepared_geometry_delete( pgeom0 );

    presult = sfcgal_prepared_geometry_create_from_geometry( result, srid );
    output = serialize_exact_geometry( presult );
    sfcgal_prepared_geometry_delete( presult );
    
    PG_RETURN_POINTER( output );
}

