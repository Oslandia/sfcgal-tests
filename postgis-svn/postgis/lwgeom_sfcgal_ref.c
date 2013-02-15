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
#include "utils/hsearch.h"
#include "utils/palloc.h"
#include "access/hash.h"
#include "nodes/memnodes.h"
#include "utils/memutils.h"

GSERIALIZED *geometry_serialize(LWGEOM *lwgeom);
char* text2cstring(const text *textptr);

/**
 * Global pool of referenced geometries.
 *
 * A list of pointers to geometry can be associated to a MemoryContext
 * They are deleted when the MemoryContext is deleted or reset
 */

typedef struct
{
    /* Geometry => MemoryContext */
    HTAB* geometry_context_map;
    /* Memory Context => Child memory context where geometries are stored */
    HTAB* child_context_map;
} geometry_pool_t;

/* Entry of the geometry_context_map */
typedef struct
{
    /* pointer back to geometry (hashtable key) */
    sfcgal_prepared_geometry_t* geometry;
    MemoryContext context;
} geometry_context_entry_t;

/* Entry of the child_context_map */
typedef struct
{
    /* pointer back to the parent context (hashtable key) */
    MemoryContext parent;
    /* child context */
    MemoryContext child;
} child_context_entry_t;

/* global geometry pool */
static geometry_pool_t sfcgal_geometry_pool;
static int __sfcgal_geometry_pool_init = 0;

/* pointer hashing */
uint32 ptr_hash(const void *key, Size keysize);
uint32 ptr_hash(const void *key, Size keysize)
{
	uint32 hashval;
	hashval = DatumGetUInt32(hash_any(key, keysize));
	return hashval;
}

static void geometry_pool_reference_geometry( sfcgal_prepared_geometry_t* geometry );
static void geometry_pool_delete_context( MemoryContext context );
static void geometry_pool_init();
static int  geometry_pool_is_referenced( const sfcgal_prepared_geometry_t* geom );
static void geometry_pool_context_nil( MemoryContext context );
static bool geometry_pool_context_is_empty( MemoryContext context );
static void geometry_pool_context_stats( MemoryContext context, int level );

static void geometry_pool_init()
{
    HASHCTL ctl;

    if ( ! __sfcgal_geometry_pool_init ) {
	/* key: pointer */
	ctl.keysize = sizeof(void*);
	/* entry */
	ctl.entrysize = sizeof( geometry_context_entry_t );
	ctl.hash = ptr_hash;
	
	sfcgal_geometry_pool.geometry_context_map = hash_create( "PostGIS referenced geometry pool 1",
								 32,
								 &ctl,
								 /* set keysize and entrysize, set hash function */ HASH_ELEM | HASH_FUNCTION );
	
	sfcgal_geometry_pool.child_context_map = hash_create( "PostGIS referenced geometry pool 2",
							      32,
							      &ctl,
							      /* set keysize and entrysize, set hash function */ HASH_ELEM | HASH_FUNCTION );
	__sfcgal_geometry_pool_init = 1;
    }
}

static int geometry_pool_is_referenced( const sfcgal_prepared_geometry_t* geom )
{
    // this has to be fast, the geometry is then the key of the pool_ map
    bool found;
    hash_search( sfcgal_geometry_pool.geometry_context_map, &geom, HASH_FIND, &found );
    return found ? 1 : 0;
}

static void geometry_pool_context_nil( MemoryContext context )
{
}

static bool geometry_pool_context_is_empty( MemoryContext context )
{
    return false;
}

static void geometry_pool_context_stats( MemoryContext context, int level )
{
    fprintf( stderr, "%s: ref geometry context\n", context->name );
}

MemoryContextMethods geometry_pool_context_methods = {
    NULL, // alloc
    NULL, // free
    NULL, // realloc
    geometry_pool_context_nil, //init
    geometry_pool_delete_context, //reset
    geometry_pool_delete_context,
    NULL, // get_chunk_space
    geometry_pool_context_is_empty,
    geometry_pool_context_stats,
#ifdef MEMORY_CONTEXT_CHECKING
    , geometry_pool_context_nil // check
#endif
};

/**
 * Save a reference to the given Geometry.
 * It is associated with a MemoryContext with a proper life cycle.
 */
static void geometry_pool_reference_geometry( sfcgal_prepared_geometry_t* geometry )
{
    //
    // Find the memory context used to store SFCGAL::Geometry*
    // Ideally, this would be in the closest parent context of the current function evaluation.
    //
    // If the current context is a not descendant of a PortalMemory, it means this context is volatile and will be
    // reset between calls within the same tuple access. In this case, attach our context to the MessageContext
    // else, use the current context
    MemoryContext parent_context = MessageContext;
    MemoryContext c;
    bool found;
    geometry_context_entry_t* geometry_context_entry;
    child_context_entry_t* child_context_entry;
    
    c = CurrentMemoryContext;
    while ( c != TopMemoryContext ) {
	// If it's a child of a Portal[Heap]Memory
	if ( !strncmp( c->name, "Portal", 6 ) ) {
	    parent_context = CurrentMemoryContext;
	    break;
	}
	c = c->parent;
    }
    
    // If a child context already exists, use it

    child_context_entry = (child_context_entry_t *)hash_search( sfcgal_geometry_pool.child_context_map,
								&parent_context,
								HASH_ENTER,
								&found );

    if ( !found ) {
	// Else, create a new one.
	// We won't allocate anything in the new context. It is only used for its destruction callbacks.
	// It is thus allocated with the minimum required size
	char context_name[] = "SFCGAL";
	child_context_entry->child = MemoryContextCreate( T_AllocSetContext, sizeof(MemoryContextData) + strlen(context_name) + 1,
							  &geometry_pool_context_methods,
							  parent_context,
							  context_name );
	
    }

    // force next reset. When the parent will be reset, reset will be called on this child
    child_context_entry->child->isReset = false;
    /* store a reference back to the key */
    child_context_entry->parent = parent_context;

    // now reference the geometry
    geometry_context_entry = (geometry_context_entry_t *) hash_search( sfcgal_geometry_pool.geometry_context_map,
								       &geometry,
								       HASH_ENTER,
								       NULL );
    geometry_context_entry->context = child_context_entry->child;
    /* store a reference back to the key */
    geometry_context_entry->geometry = geometry;
}

static void geometry_pool_delete_context( MemoryContext context )
{
    HASH_SEQ_STATUS h_status;
    geometry_context_entry_t *geometry_context_entry;
    child_context_entry_t *child_context_entry;

    hash_seq_init( &h_status, sfcgal_geometry_pool.geometry_context_map );
    while ( (geometry_context_entry = (geometry_context_entry_t *)hash_seq_search( &h_status ) ) != NULL ) {
	if ( geometry_context_entry->context == context ) {
	    /* delete the associated geometry */
	    sfcgal_prepared_geometry_delete( geometry_context_entry->geometry );
	    /* erase the element from the hashtable */
	    hash_search( sfcgal_geometry_pool.geometry_context_map, &geometry_context_entry->geometry, HASH_REMOVE, NULL );
	}
    }

    /* delete the child_context_map part */
    hash_seq_init( &h_status, sfcgal_geometry_pool.child_context_map );
    while ( (child_context_entry = (child_context_entry_t *)hash_seq_search( &h_status ) ) != NULL ) {
	if ( child_context_entry->child == context ) {
	    /* erase the element from the hashtable */
	    hash_search( sfcgal_geometry_pool.child_context_map, &child_context_entry->parent, HASH_REMOVE, NULL );
	    hash_seq_term( &h_status );
	    break;
	}
    }
}

typedef void* sfcgal_ref_geometry_t;

static sfcgal_ref_geometry_t* serialize_ref_geometry( sfcgal_prepared_geometry_t * pgeom );
static sfcgal_prepared_geometry_t* unserialize_ref_geometry( sfcgal_ref_geometry_t * rgeom );

Datum sfcgal_ref_from_text(PG_FUNCTION_ARGS);
Datum sfcgal_ref_in(PG_FUNCTION_ARGS);
Datum sfcgal_ref_out(PG_FUNCTION_ARGS);

Datum sfcgal_ref_from_geom(PG_FUNCTION_ARGS);
Datum sfcgal_geom_from_ref(PG_FUNCTION_ARGS);

Datum sfcgal_ref_round(PG_FUNCTION_ARGS);
Datum sfcgal_ref_extrude(PG_FUNCTION_ARGS);
Datum sfcgal_ref_offset_polygon(PG_FUNCTION_ARGS);

static sfcgal_ref_geometry_t* serialize_ref_geometry( sfcgal_prepared_geometry_t * pgeom )
{
    sfcgal_ref_geometry_t* ret;
    geometry_pool_reference_geometry( pgeom );

    ret = (sfcgal_ref_geometry_t*)lwalloc( sizeof(sfcgal_ref_geometry_t) );
    *ret = pgeom;
    return ret;
}

static sfcgal_prepared_geometry_t* unserialize_ref_geometry( sfcgal_ref_geometry_t * rgeom )
{
    sfcgal_prepared_geometry_t* pgeom;
    pgeom = (sfcgal_prepared_geometry_t*)(*rgeom);
    if ( ! geometry_pool_is_referenced( pgeom ) ) {
	lwerror( "Unable to access delete geometry !" );
	return 0;
    }
    return pgeom;
}

/**
 * Convert a WKT to a referenced geometry
 */
PG_FUNCTION_INFO_V1(sfcgal_ref_in);
Datum sfcgal_ref_in(PG_FUNCTION_ARGS)
{
    char* ewkt = PG_GETARG_CSTRING( 0 );
    sfcgal_prepared_geometry_t *pgeom;
    sfcgal_ref_geometry_t* sgeom;
    
    sfcgal_postgis_init();
    geometry_pool_init();
    
    pgeom = sfcgal_io_read_ewkt( ewkt, strlen(ewkt) );
    
    sgeom = serialize_ref_geometry( pgeom );
    
    PG_RETURN_POINTER( sgeom );
}

/**
 * Convert a WKT TEXT to a referenced geometry
 */
PG_FUNCTION_INFO_V1(sfcgal_ref_from_text);
Datum sfcgal_ref_from_text(PG_FUNCTION_ARGS)
{
    text* wkttext = PG_GETARG_TEXT_P(0);
    char* ewkt = text2cstring(wkttext);
    sfcgal_prepared_geometry_t *pgeom;
    sfcgal_ref_geometry_t* sgeom;
    
    sfcgal_postgis_init();
    geometry_pool_init();
    
    pgeom = sfcgal_io_read_ewkt( ewkt, strlen(ewkt) );
    
    sgeom = serialize_ref_geometry( pgeom );
    
    PG_RETURN_POINTER( sgeom );
}

/**
 * Display a referenced geometry
 */
PG_FUNCTION_INFO_V1(sfcgal_ref_out);
Datum sfcgal_ref_out(PG_FUNCTION_ARGS)
{
    sfcgal_ref_geometry_t *input0;
    sfcgal_prepared_geometry_t *pgeom0;
    char *retstr;
    size_t len;

    sfcgal_postgis_init();
    geometry_pool_init();
    
    input0 = (sfcgal_ref_geometry_t*)PG_GETARG_POINTER(0);

    pgeom0 = (sfcgal_prepared_geometry_t*)(*input0);
    if ( ! geometry_pool_is_referenced( pgeom0 ) ) {
	char *retstr;
	char deleted[] = "-deleted-";
	retstr = (char*)lwalloc( strlen(deleted) + 1 );
	strncpy( retstr, deleted, strlen(deleted) );

	lwnotice( "Referenced geometries must not be stored" );
	PG_RETURN_CSTRING( retstr );
    }
    //    pgeom0 = unserialize_ref_geometry( input0 );

    sfcgal_prepared_geometry_as_ewkt( pgeom0, /* numDecimals */ -1, &retstr, &len );

    PG_RETURN_CSTRING( retstr );
}

/**
 * Convert a regular geometry to a ref geometry
 */
PG_FUNCTION_INFO_V1(sfcgal_geom_from_ref);
Datum sfcgal_geom_from_ref(PG_FUNCTION_ARGS)
{
    sfcgal_ref_geometry_t *input0;
    sfcgal_prepared_geometry_t *pgeom0;
    GSERIALIZED* output;

    sfcgal_postgis_init();
    geometry_pool_init();
    
    input0 = (sfcgal_ref_geometry_t*)PG_GETARG_POINTER(0);

    pgeom0 = (sfcgal_prepared_geometry_t*)(*input0);
    pgeom0 = unserialize_ref_geometry( input0 );

    output = SFCGALPreparedGeometry2POSTGIS( pgeom0, 0 );
    PG_RETURN_POINTER( output );
}

/**
 * Convert a ref geometry to a regular geometry
 */
PG_FUNCTION_INFO_V1(sfcgal_ref_from_geom);
Datum sfcgal_ref_from_geom(PG_FUNCTION_ARGS)
{
    GSERIALIZED *input0;
    sfcgal_prepared_geometry_t *pgeom0;
    sfcgal_ref_geometry_t *result;

    sfcgal_postgis_init();
    geometry_pool_init();
    
    input0 = (GSERIALIZED*)PG_DETOAST_DATUM(PG_GETARG_DATUM(0));
    pgeom0 = POSTGIS2SFCGALPreparedGeometry( input0 );
    PG_FREE_IF_COPY( input0, 0 );

    result = serialize_ref_geometry( pgeom0 );
    PG_RETURN_POINTER( result );
}

#define _SFCGAL_REF_WRAPPER_UNARY_SCALAR( name, fname, ret_type, return_call ) \
    Datum sfcgal_ref_##name(PG_FUNCTION_ARGS);			\
    PG_FUNCTION_INFO_V1(sfcgal_ref_##name);				\
    Datum sfcgal_ref_##name(PG_FUNCTION_ARGS)				\
    {									\
	sfcgal_ref_geometry_t *input0;				\
	sfcgal_prepared_geometry_t *pgeom0;				\
	const sfcgal_geometry_t *geom0;					\
	ret_type result;						\
									\
	sfcgal_postgis_init();						\
	geometry_pool_init();						\
									\
	input0 = (sfcgal_ref_geometry_t*)PG_GETARG_POINTER(0);		\
	pgeom0 = unserialize_ref_geometry( input0 );			\
	geom0 = sfcgal_prepared_geometry_geometry( pgeom0 );		\
									\
	result = fname( geom0 );					\
									\
	return_call( result );						\
    }
    
#define _SFCGAL_REF_WRAPPER_UNARY_PREDICATE( name, fname ) \
    _SFCGAL_REF_WRAPPER_UNARY_SCALAR( name, fname, int, PG_RETURN_BOOL )
#define _SFCGAL_REF_WRAPPER_UNARY_MEASURE( name, fname ) \
    _SFCGAL_REF_WRAPPER_UNARY_SCALAR( name, fname, double, PG_RETURN_FLOAT8 )

#define _SFCGAL_REF_WRAPPER_BINARY_SCALAR( name, fname, ret_type, return_call ) \
    Datum sfcgal_ref_##name(PG_FUNCTION_ARGS);				\
    PG_FUNCTION_INFO_V1(sfcgal_ref_##name);				\
    Datum sfcgal_ref_##name(PG_FUNCTION_ARGS)				\
    {									\
	sfcgal_ref_geometry_t *input0, *input1;				\
	sfcgal_prepared_geometry_t *pgeom0, *pgeom1;			\
	const sfcgal_geometry_t *geom0, *geom1;				\
	ret_type result;						\
									\
	sfcgal_postgis_init();						\
	geometry_pool_init();						\
									\
	input0 = (sfcgal_ref_geometry_t*)PG_GETARG_POINTER(0);		\
	pgeom0 = unserialize_ref_geometry( input0 );			\
	geom0 = sfcgal_prepared_geometry_geometry( pgeom0 );		\
									\
	input1 = (sfcgal_ref_geometry_t*)PG_GETARG_POINTER(1);		\
	pgeom1 = unserialize_ref_geometry( input1 );			\
	geom1 = sfcgal_prepared_geometry_geometry( pgeom1 );		\
									\
	result = fname( geom0, geom1 );					\
									\
	return_call( result );						\
    }

#define _SFCGAL_REF_WRAPPER_BINARY_PREDICATE( name, fname ) \
    _SFCGAL_REF_WRAPPER_BINARY_SCALAR( name, fname, int, PG_RETURN_BOOL )
#define _SFCGAL_REF_WRAPPER_BINARY_MEASURE( name, fname ) \
    _SFCGAL_REF_WRAPPER_BINARY_SCALAR( name, fname, double, PG_RETURN_FLOAT8 )

#define _SFCGAL_REF_WRAPPER_UNARY_CONSTRUCTION( name, fname )		\
    Datum sfcgal_ref_##name(PG_FUNCTION_ARGS);				\
    PG_FUNCTION_INFO_V1(sfcgal_ref_##name);				\
    Datum sfcgal_ref_##name(PG_FUNCTION_ARGS)				\
    {									\
	sfcgal_ref_geometry_t *input0, *output;				\
	sfcgal_prepared_geometry_t *pgeom0, *presult;			\
	const sfcgal_geometry_t *geom0;					\
	sfcgal_geometry_t *result;					\
	srid_t srid;							\
									\
	sfcgal_postgis_init();						\
	geometry_pool_init();						\
									\
	input0 = (sfcgal_ref_geometry_t*)PG_GETARG_POINTER(0);		\
	pgeom0 = unserialize_ref_geometry( input0 );			\
	geom0 = sfcgal_prepared_geometry_geometry( pgeom0 );		\
	srid = sfcgal_prepared_geometry_srid( pgeom0 );			\
									\
	result = fname( geom0 );					\
									\
	presult = sfcgal_prepared_geometry_create_from_geometry( result, srid ); \
	output = serialize_ref_geometry( presult );			\
									\
	PG_RETURN_POINTER( output );					\
    }

#define _SFCGAL_REF_WRAPPER_BINARY_CONSTRUCTION( name, fname )	\
    Datum sfcgal_ref_##name(PG_FUNCTION_ARGS);			\
    PG_FUNCTION_INFO_V1(sfcgal_ref_##name);				\
    Datum sfcgal_ref_##name(PG_FUNCTION_ARGS)				\
    {									\
	sfcgal_ref_geometry_t *input0, *input1, *output;		\
	sfcgal_prepared_geometry_t *pgeom0, *pgeom1, *presult;		\
	const sfcgal_geometry_t *geom0, *geom1;				\
	sfcgal_geometry_t *result;					\
	srid_t srid;							\
									\
	sfcgal_postgis_init();						\
	geometry_pool_init();						\
									\
	input0 = (sfcgal_ref_geometry_t*)PG_GETARG_POINTER(0);		\
	pgeom0 = unserialize_ref_geometry( input0 );			\
	geom0 = sfcgal_prepared_geometry_geometry( pgeom0 );		\
	srid = sfcgal_prepared_geometry_srid( pgeom0 );			\
									\
	input1 = (sfcgal_ref_geometry_t*)PG_GETARG_POINTER(1);		\
	pgeom1 = unserialize_ref_geometry( input1 );			\
	geom1 = sfcgal_prepared_geometry_geometry( pgeom1 );		\
									\
	result = fname( geom0, geom1 );					\
									\
	presult = sfcgal_prepared_geometry_create_from_geometry( result, srid ); \
	output = serialize_ref_geometry( presult );			\
									\
	PG_RETURN_POINTER( output );					\
    }

_SFCGAL_REF_WRAPPER_UNARY_MEASURE( area, sfcgal_geometry_area )
_SFCGAL_REF_WRAPPER_UNARY_MEASURE( area3D, sfcgal_geometry_area_3d )

_SFCGAL_REF_WRAPPER_UNARY_PREDICATE( has_plane, sfcgal_geometry_has_plane )
_SFCGAL_REF_WRAPPER_UNARY_PREDICATE( pointing_up, sfcgal_geometry_pointing_up )

_SFCGAL_REF_WRAPPER_BINARY_PREDICATE( intersects, sfcgal_geometry_intersects )
_SFCGAL_REF_WRAPPER_BINARY_PREDICATE( intersects3D, sfcgal_geometry_intersects_3d )

_SFCGAL_REF_WRAPPER_BINARY_MEASURE( distance, sfcgal_geometry_distance )
_SFCGAL_REF_WRAPPER_BINARY_MEASURE( distance3D, sfcgal_geometry_distance_3d )

_SFCGAL_REF_WRAPPER_UNARY_CONSTRUCTION( convexhull, sfcgal_geometry_convexhull )
_SFCGAL_REF_WRAPPER_UNARY_CONSTRUCTION( convexhull3D, sfcgal_geometry_convexhull_3d )
_SFCGAL_REF_WRAPPER_UNARY_CONSTRUCTION( triangulate, sfcgal_geometry_triangulate )
_SFCGAL_REF_WRAPPER_UNARY_CONSTRUCTION( triangulate2D, sfcgal_geometry_triangulate_2d )
_SFCGAL_REF_WRAPPER_UNARY_CONSTRUCTION( make_solid, sfcgal_geometry_make_solid )
_SFCGAL_REF_WRAPPER_UNARY_CONSTRUCTION( force_z_up, sfcgal_geometry_force_z_up )
_SFCGAL_REF_WRAPPER_UNARY_CONSTRUCTION( copy, sfcgal_geometry_copy )
_SFCGAL_REF_WRAPPER_UNARY_CONSTRUCTION( straight_skeleton, sfcgal_geometry_straight_skeleton )

_SFCGAL_REF_WRAPPER_BINARY_CONSTRUCTION( intersection, sfcgal_geometry_intersection )
_SFCGAL_REF_WRAPPER_BINARY_CONSTRUCTION( intersection3D, sfcgal_geometry_intersection_3d )
_SFCGAL_REF_WRAPPER_BINARY_CONSTRUCTION( minkowski_sum, sfcgal_geometry_minkowski_sum )

PG_FUNCTION_INFO_V1(sfcgal_ref_extrude);
Datum sfcgal_ref_extrude(PG_FUNCTION_ARGS)
{
    sfcgal_ref_geometry_t *input0, *output;
    sfcgal_prepared_geometry_t *pgeom0, *presult;
    const sfcgal_geometry_t *geom0;
    sfcgal_geometry_t *result;
    srid_t srid;

    double dx, dy, dz;

    sfcgal_postgis_init();
    geometry_pool_init();
    
    input0 = (sfcgal_ref_geometry_t*)PG_GETARG_POINTER(0);
    pgeom0 = unserialize_ref_geometry( input0 );
    geom0 = sfcgal_prepared_geometry_geometry( pgeom0 );
    srid = sfcgal_prepared_geometry_srid( pgeom0 );

    dx = PG_GETARG_FLOAT8( 1 );
    dy = PG_GETARG_FLOAT8( 2 );
    dz = PG_GETARG_FLOAT8( 3 );
    
    result = sfcgal_geometry_extrude( geom0, dx, dy, dz );

    presult = sfcgal_prepared_geometry_create_from_geometry( result, srid );
    output = serialize_ref_geometry( presult );
    
    PG_RETURN_POINTER( output );
}

PG_FUNCTION_INFO_V1(sfcgal_ref_offset_polygon);
Datum sfcgal_ref_offset_polygon(PG_FUNCTION_ARGS)
{
    sfcgal_ref_geometry_t *input0, *output;
    sfcgal_prepared_geometry_t *pgeom0, *presult;
    const sfcgal_geometry_t *geom0;
    sfcgal_geometry_t *result;
    srid_t srid;

    double offset;

    sfcgal_postgis_init();
    geometry_pool_init();
    
    input0 = (sfcgal_ref_geometry_t*)PG_GETARG_POINTER(0);
    pgeom0 = unserialize_ref_geometry( input0 );
    geom0 = sfcgal_prepared_geometry_geometry( pgeom0 );
    srid = sfcgal_prepared_geometry_srid( pgeom0 );

    offset = PG_GETARG_FLOAT8( 1 );
    
    result = sfcgal_geometry_offset_polygon( geom0, offset );

    presult = sfcgal_prepared_geometry_create_from_geometry( result, srid );
    output = serialize_ref_geometry( presult );
    
    PG_RETURN_POINTER( output );
}

PG_FUNCTION_INFO_V1(sfcgal_ref_round);
Datum sfcgal_ref_round(PG_FUNCTION_ARGS)
{
    sfcgal_ref_geometry_t *input0, *output;
    sfcgal_prepared_geometry_t *pgeom0, *presult;
    const sfcgal_geometry_t *geom0;
    sfcgal_geometry_t *result;
    srid_t srid;

    int scale;

    sfcgal_postgis_init();
    geometry_pool_init();
    
    input0 = (sfcgal_ref_geometry_t*)PG_GETARG_POINTER(0);
    pgeom0 = unserialize_ref_geometry( input0 );
    geom0 = sfcgal_prepared_geometry_geometry( pgeom0 );
    srid = sfcgal_prepared_geometry_srid( pgeom0 );

    scale = PG_GETARG_INT32( 1 );
    
    result = sfcgal_geometry_round( geom0, scale );

    presult = sfcgal_prepared_geometry_create_from_geometry( result, srid );
    output = serialize_ref_geometry( presult );
    
    PG_RETURN_POINTER( output );
}
