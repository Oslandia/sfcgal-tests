#include "lwgeom_sfcgal.h"

int SFCGAL_type_to_lwgeom_type( sfcgal_geometry_type_t type );

int SFCGAL_type_to_lwgeom_type( sfcgal_geometry_type_t type )
{
    switch ( type )
    {
    case SFCGAL_TYPE_POINT:
	return POINTTYPE;
    case SFCGAL_TYPE_LINESTRING:
	return LINETYPE;
    case SFCGAL_TYPE_POLYGON:
	return POLYGONTYPE;
    case SFCGAL_TYPE_MULTIPOINT:
	return MULTIPOINTTYPE;
    case SFCGAL_TYPE_MULTILINESTRING:
	return MULTILINETYPE;
    case SFCGAL_TYPE_MULTIPOLYGON:
	return MULTIPOLYGONTYPE;
    case SFCGAL_TYPE_MULTISOLID:
	return COLLECTIONTYPE;
    case SFCGAL_TYPE_GEOMETRYCOLLECTION:
	return COLLECTIONTYPE;
	//    case SFCGAL_TYPE_CIRCULARSTRING:
	//	return CIRCSTRINGTYPE;
	//    case SFCGAL_TYPE_COMPOUNDCURVE:
	//	return COMPOUNDTYPE;
	//    case SFCGAL_TYPE_CURVEPOLYGON:
	//	return CURVEPOLYTYPE;
	//    case SFCGAL_TYPE_MULTICURVE:
	//	return MULTICURVETYPE;
	//    case SFCGAL_TYPE_MULTISURFACE:
	//	return MULTISURFACETYPE;
	//    case SFCGAL_TYPE_CURVE:
	// Unknown LWGEOM type
	//	return 0;
	//    case SFCGAL_TYPE_SURFACE:
	// Unknown LWGEOM type
	//	return 0;
    case SFCGAL_TYPE_POLYHEDRALSURFACE:
	return POLYHEDRALSURFACETYPE;
    case SFCGAL_TYPE_TRIANGULATEDSURFACE:
	return TINTYPE;
    case SFCGAL_TYPE_TRIANGLE:
	return TRIANGLETYPE;
    default:
	return 0;
    };
    return 0;
}

POINTARRAY* ptarray_from_SFCGAL( const sfcgal_geometry_t* geom, int force3D );
POINTARRAY* ptarray_from_SFCGAL( const sfcgal_geometry_t* geom, int force3D )
{
    POINTARRAY* pa = 0;
    POINT4D point;
    int want3d = force3D || sfcgal_geometry_is_3d( geom );
    
    switch ( sfcgal_geometry_type_id( geom ) )
    {
    case SFCGAL_TYPE_POINT:
	{
	    pa = ptarray_construct( want3d, 0, 1 );
	    point.x = sfcgal_point_x( geom );
	    point.y = sfcgal_point_y( geom );
	    if ( sfcgal_geometry_is_3d( geom ) ) {
		point.z = sfcgal_point_z( geom );
	    }
	    else if ( force3D ) {
		point.z = 0.0;
	    }	    
	    point.m = 0.0;
	    ptarray_set_point4d( pa, 0, &point );
	    break;
	}
    case SFCGAL_TYPE_LINESTRING:
	{
	    //	    pa = ptarray_construct( ls->is3D() ? 1 : 0, 0, ls->numPoints() );
	    size_t num_points = sfcgal_linestring_num_points( geom );
	    size_t i;
	    pa = ptarray_construct( want3d, 0, num_points );
	    
	    for ( i = 0; i < num_points; i++ )
	    {
		const sfcgal_geometry_t* pt = sfcgal_linestring_point_n( geom, i );
		point.x = sfcgal_point_x( pt );
		point.y = sfcgal_point_y( pt );
		if ( sfcgal_geometry_is_3d( geom ) ) {
		    point.z = sfcgal_point_z( pt );
		}
		else if ( force3D ) {
		    point.z = 0.0;
		}
		point.m = 0.0;
		ptarray_set_point4d( pa, i, &point );		
	    }
	    break;
	}
    case SFCGAL_TYPE_TRIANGLE:
	{
	    size_t i;

	    pa = ptarray_construct( want3d, 0, 4 );
	    
	    for ( i = 0; i < 4; i++ )
	    {
		const sfcgal_geometry_t* pt = sfcgal_triangle_vertex( geom, (i%3) );
		point.x = sfcgal_point_x( pt );
		point.y = sfcgal_point_y( pt );
		if ( sfcgal_geometry_is_3d( geom ) ) {
		    point.z = sfcgal_point_z( pt );
		}
		else if ( force3D ) {
		    point.z = 0.0;
		}
		point.m = 0.0;
		ptarray_set_point4d( pa, i, &point );		
	    }
	    break;
	}
	// These other types should not be called directly ...
    case SFCGAL_TYPE_POLYGON:
    case SFCGAL_TYPE_MULTIPOINT:
    case SFCGAL_TYPE_MULTILINESTRING:
    case SFCGAL_TYPE_MULTIPOLYGON:
    case SFCGAL_TYPE_GEOMETRYCOLLECTION:
	//    case SFCGAL::TYPE_CIRCULARSTRING:
	//    case SFCGAL::TYPE_COMPOUNDCURVE:
	//    case SFCGAL::TYPE_CURVEPOLYGON:
	//    case SFCGAL::TYPE_MULTICURVE:
	//    case SFCGAL::TYPE_MULTISURFACE:
	//    case SFCGAL::TYPE_CURVE:
	//    case SFCGAL::TYPE_SURFACE:
    case SFCGAL_TYPE_POLYHEDRALSURFACE:
    case SFCGAL_TYPE_TRIANGULATEDSURFACE:
    default:
	// FIXME
	//	    throw std::runtime_error( "ptarray_from_SFCGAL: Unsupported SFCGAL geometry of type " + geom->geometryType() );
	break;
    }
    return pa;
}

sfcgal_geometry_t* ptarray_to_SFCGAL( const POINTARRAY* pa, int type );
sfcgal_geometry_t* ptarray_to_SFCGAL( const POINTARRAY* pa, int type )
{
    POINT3DZ point;
    
    switch ( type )
    {
    case POINTTYPE:
	{
	    getPoint3dz_p( pa, 0, &point );
	    int is_3d = FLAGS_GET_Z( pa->flags ) != 0;
	    if ( is_3d ) {
		return sfcgal_point_create_from_xyz( point.x, point.y, point.z );
	    }
	    else {
		return sfcgal_point_create_from_xy( point.x, point.y );
	    }
	}
    case LINETYPE:
	{
	    size_t i;
	    sfcgal_geometry_t* ret_geom = sfcgal_linestring_create();
	    
	    int is_3d = FLAGS_GET_Z( pa->flags ) != 0;
	    for ( i = 0; i < pa->npoints; i++ )
	    {
		getPoint3dz_p( pa, i, &point );
		if ( is_3d ) {
		    sfcgal_linestring_add_point( ret_geom, sfcgal_point_create_from_xyz( point.x, point.y, point.z ) );
		}
		else {
		    sfcgal_linestring_add_point( ret_geom, sfcgal_point_create_from_xy( point.x, point.y ) );
		}
	    }
	    return ret_geom;
	}
    case TRIANGLETYPE:
	{
	    sfcgal_geometry_t* pta;
	    sfcgal_geometry_t* ptb;
	    sfcgal_geometry_t* ptc;

	    int is_3d = FLAGS_GET_Z( pa->flags ) != 0;
	    getPoint3dz_p( pa, 0, &point );
	    if ( is_3d ) {
		pta = sfcgal_point_create_from_xyz( point.x, point.y, point.z );
	    }
	    else {
		pta = sfcgal_point_create_from_xy( point.x, point.y );
	    }
	    getPoint3dz_p( pa, 1, &point );
	    if ( is_3d ) {
		ptb = sfcgal_point_create_from_xyz( point.x, point.y, point.z );
	    }
	    else {
		ptb = sfcgal_point_create_from_xy( point.x, point.y );
	    }
	    getPoint3dz_p( pa, 2, &point );
	    if ( is_3d ) {
		ptc = sfcgal_point_create_from_xyz( point.x, point.y, point.z );
	    }
	    else {
		ptc = sfcgal_point_create_from_xy( point.x, point.y );
	    }

	    sfcgal_geometry_t* tri = sfcgal_triangle_create_from_points( pta, ptb, ptc );
	    // delete points
	    sfcgal_geometry_delete( pta );
	    sfcgal_geometry_delete( ptb );
	    sfcgal_geometry_delete( ptc );
	    return tri;
	}
    }
    return NULL;
}

LWGEOM* SFCGAL2LWGEOM( const sfcgal_geometry_t* geom, int force3D, int SRID )
{
    int want3d;

    want3d = force3D || sfcgal_geometry_is_3d( geom );

    switch ( sfcgal_geometry_type_id( geom ) )
    {
    case SFCGAL_TYPE_POINT:
	{
	    if ( sfcgal_geometry_is_empty( geom ) )
		return (LWGEOM*)lwpoint_construct_empty( SRID, want3d, 0 );
	    POINTARRAY* pa = ptarray_from_SFCGAL( geom, force3D );
	    return (LWGEOM*)lwpoint_construct( SRID, /* bbox */ NULL, pa );
	}
    case SFCGAL_TYPE_LINESTRING:
	{
	    if ( sfcgal_geometry_is_empty( geom ) )
		return (LWGEOM*)lwline_construct_empty( SRID, want3d, 0 );
	    POINTARRAY* pa = ptarray_from_SFCGAL( geom, force3D );
	    return (LWGEOM*)lwline_construct( SRID, /* bbox */ NULL, pa );
	}
    case SFCGAL_TYPE_TRIANGLE:
	{
	    if ( sfcgal_geometry_is_empty( geom ) )
		return (LWGEOM*)lwtriangle_construct_empty( SRID, want3d, 0 );
	    POINTARRAY* pa = ptarray_from_SFCGAL( geom, force3D );
	    return (LWGEOM*)lwtriangle_construct( SRID, /* bbox */ NULL, pa );
	}
    case SFCGAL_TYPE_POLYGON:
	{
	    size_t i;
	    if ( sfcgal_geometry_is_empty( geom ) )
		return (LWGEOM*)lwpoly_construct_empty( SRID, want3d, 0 );

	    size_t n_interiors = sfcgal_polygon_num_interior_rings( geom );
	    // allocate for all the rings (including the exterior one)
	    POINTARRAY** pa = (POINTARRAY**) lwalloc( sizeof(POINTARRAY*) * (n_interiors + 1 ) );

	    // write the exterior ring
	    pa[0] = ptarray_from_SFCGAL( sfcgal_polygon_exterior_ring( geom ), force3D );
	    for ( i = 0; i < n_interiors; i++ )
	    {
		pa[ i+1 ] = ptarray_from_SFCGAL( sfcgal_polygon_interior_ring_n( geom, i ), force3D );
	    }
	    return (LWGEOM*)lwpoly_construct( SRID, NULL, n_interiors + 1, pa );
	}
    case SFCGAL_TYPE_MULTIPOINT:
    case SFCGAL_TYPE_MULTILINESTRING:
    case SFCGAL_TYPE_MULTIPOLYGON:
    case SFCGAL_TYPE_MULTISOLID:
    case SFCGAL_TYPE_GEOMETRYCOLLECTION:
	{
	    size_t i;
	    size_t n_geoms = sfcgal_geometry_collection_num_geometries( geom );
	    LWGEOM** geoms = 0;
	    if ( n_geoms )
	    {
		geoms = (LWGEOM**)lwalloc( sizeof(LWGEOM*) * n_geoms );
		size_t j = 0;
		for ( i = 0; i < n_geoms; i++ )
		{
		    const sfcgal_geometry_t* g = sfcgal_geometry_collection_geometry_n( geom, i );
		    if ( ! sfcgal_geometry_is_empty( g ) )
		    {
			// recurse call
			geoms[j++] = SFCGAL2LWGEOM( g, 0, SRID_UNKNOWN );
		    }
		}
		n_geoms = j;
		geoms = (LWGEOM**)lwrealloc( geoms, sizeof(LWGEOM*) * n_geoms );
	    }
	    return (LWGEOM*)lwcollection_construct( SFCGAL_type_to_lwgeom_type( sfcgal_geometry_type_id( geom )),
						    SRID,
						    NULL,
						    n_geoms,
						    geoms );
	}
	//    case SFCGAL_TYPE_CIRCULARSTRING:
	//    case SFCGAL_TYPE_COMPOUNDCURVE:
	//    case SFCGAL_TYPE_CURVEPOLYGON:
	//    case SFCGAL_TYPE_MULTICURVE:
	//    case SFCGAL_TYPE_MULTISURFACE:
	//    case SFCGAL_TYPE_CURVE:
	//    case SFCGAL_TYPE_SURFACE:
    case SFCGAL_TYPE_POLYHEDRALSURFACE:
	{
	    size_t i;
	    size_t n_geoms = sfcgal_polyhedral_surface_num_polygons( geom );
	    LWGEOM** geoms = 0;
	    if ( n_geoms )
	    {
		geoms = (LWGEOM**)lwalloc( sizeof(LWGEOM*) * n_geoms );
		for ( i = 0; i < n_geoms; i++ )
		{
		    const sfcgal_geometry_t* g = sfcgal_polyhedral_surface_polygon_n( geom, i );
		    // recurse call
		    geoms[i] = SFCGAL2LWGEOM( g, 0, SRID_UNKNOWN );
		}
	    }
	    return (LWGEOM*)lwcollection_construct( POLYHEDRALSURFACETYPE,
						    SRID,
						    NULL,
						    n_geoms,
						    geoms );
	}
    case SFCGAL_TYPE_SOLID:
	{
	    // a Solid is a closed PolyhedralSurface
	    // compute the number of polyhedral
	    size_t i;
	    size_t n_geoms = 0;
	    size_t num_shells = sfcgal_solid_num_shells( geom );
	    for ( i = 0; i < num_shells; ++i ) {
		n_geoms += sfcgal_polyhedral_surface_num_polygons( sfcgal_solid_shell_n( geom, i ) );
	    }
	    LWGEOM** geoms = 0;
	    if ( n_geoms )
	    {
		geoms = (LWGEOM**)lwalloc( sizeof(LWGEOM*) * n_geoms );
		size_t k = 0;
		for ( i = 0; i < num_shells; i++ )
		{
		    size_t j;
		    const sfcgal_geometry_t* shell_i = sfcgal_solid_shell_n( geom, i );
		    size_t num_polygons = sfcgal_polyhedral_surface_num_polygons( shell_i );
		    for ( j = 0; j < num_polygons; ++j ) {
			const sfcgal_geometry_t* g = sfcgal_polyhedral_surface_polygon_n( shell_i, j );
			// recurse call
			geoms[k] = SFCGAL2LWGEOM( g, /* force3D = */ 1, SRID_UNKNOWN );
			++k;
		    }
		}
	    }
	    LWGEOM* rgeom =  (LWGEOM*)lwcollection_construct( POLYHEDRALSURFACETYPE,
							      SRID,
							      NULL,
							      n_geoms,
							      geoms );
	    if ( n_geoms ) {
		// set the 'Solid' flag before returning
		FLAGS_SET_SOLID( rgeom->flags, 1 );
	    }
	    return rgeom;
	}
    case SFCGAL_TYPE_TRIANGULATEDSURFACE:
	{
	    size_t i;
	    size_t n_geoms = sfcgal_triangulated_surface_num_triangles( geom );
	    LWGEOM** geoms = 0;
	    if ( n_geoms )
	    {
		geoms = (LWGEOM**)lwalloc( sizeof(LWGEOM*) * n_geoms );
		for ( i = 0; i < n_geoms; i++ )
		{
		    const sfcgal_geometry_t* g = sfcgal_triangulated_surface_triangle_n( geom, i );
		    // recurse call
		    geoms[i] = SFCGAL2LWGEOM( g, 0, SRID_UNKNOWN );
		}
	    }
	    return (LWGEOM*)lwcollection_construct( TINTYPE,
						    SRID,
						    NULL,
						    n_geoms,
						    geoms );
	}
    default:
	// FIXME
	//	throw std::runtime_error( "SFCGAL2LWGEOM: Unsupported SFCGAL geometry of type " + geom->geometryType() );
	break;
    }

    return NULL;
}

sfcgal_geometry_t* LWGEOM2SFCGAL( const LWGEOM* geom )
{
    sfcgal_geometry_t* ret_geom = NULL;

    switch ( geom->type )
    {
    case POINTTYPE:
	{
	    const LWPOINT* lwp = (const LWPOINT*) geom;
	    if ( lwgeom_is_empty( geom ) ) {
		return sfcgal_point_create();
	    }

	    return ptarray_to_SFCGAL( lwp->point, POINTTYPE );
	}
	break;
    case LINETYPE:
	{
	    const LWLINE* line = (const LWLINE*) geom;
	    if ( lwgeom_is_empty( geom ) ) {
		return sfcgal_linestring_create();
	    }

	    return ptarray_to_SFCGAL( line->points, LINETYPE );
	}
	break;
    case TRIANGLETYPE:
	{
	    const LWTRIANGLE* tri = (const LWTRIANGLE*) geom;
	    if ( lwgeom_is_empty( geom ) ) {
		return sfcgal_triangle_create();
	    }

	    return ptarray_to_SFCGAL( tri->points, TRIANGLETYPE );
	}
	break;
    case POLYGONTYPE:
	{
	    size_t i;
	    const LWPOLY* poly = (const LWPOLY*) geom;
	    if ( lwgeom_is_empty( geom ) ) {
		return sfcgal_polygon_create();
	    }

	    size_t n_rings = poly->nrings - 1;
	    
	    sfcgal_geometry_t* ext_ring = ptarray_to_SFCGAL( poly->rings[0], LINETYPE );
	    ret_geom = sfcgal_polygon_create_from_exterior_ring( ext_ring );

	    for ( i = 0; i < n_rings; i++ )
	    {
		sfcgal_geometry_t* ring = ptarray_to_SFCGAL( poly->rings[ i + 1 ], LINETYPE );
		// takes ownership
		sfcgal_polygon_add_interior_ring( ret_geom, ring );
	    }
	    return ret_geom;
	}
	break;
    case MULTIPOINTTYPE:
    case MULTILINETYPE:
    case MULTIPOLYGONTYPE:
    case COLLECTIONTYPE:
	{
	    size_t i;

	    if ( geom->type == MULTIPOINTTYPE ) {
		ret_geom = sfcgal_multi_point_create();
	    }
	    else if ( geom->type == MULTILINETYPE ) {
		ret_geom = sfcgal_multi_linestring_create();
	    }
	    else if ( geom->type == MULTIPOLYGONTYPE ) {
		ret_geom = sfcgal_multi_polygon_create();
	    }
	    else {
		ret_geom = sfcgal_geometry_collection_create();
	    }
	    
	    const LWCOLLECTION* lwc = (const LWCOLLECTION*)geom;
	    for ( i = 0; i < lwc->ngeoms; i++ )
	    {
		// recurse call
		sfcgal_geometry_t* g = LWGEOM2SFCGAL( lwc->geoms[i] );
		// takes ownership of the pointer
		sfcgal_geometry_collection_add_geometry( ret_geom, g );
	    }
	    return ret_geom;
	}
	break;
    case POLYHEDRALSURFACETYPE:
	{
	    size_t i;

	    const LWPSURFACE* lwp = (const LWPSURFACE*)geom;
	    ret_geom = sfcgal_polyhedral_surface_create();

	    for ( i = 0; i < lwp->ngeoms; i++ )
	    {
		// recurse call
		sfcgal_geometry_t* g = LWGEOM2SFCGAL( (const LWGEOM*)lwp->geoms[i] );
		// add the obtained polygon to the surface
		// (pass ownership )
		sfcgal_polyhedral_surface_add_polygon( ret_geom, g );
	    }
	    if ( FLAGS_GET_SOLID( lwp->flags ) ) {
		// return a Solid
		// FIXME: we treat polyhedral surface as the only exterior shell, since we do not have
		// any way to distinguish exterior from interior shells ...
		return sfcgal_solid_create_from_exterior_shell( ret_geom );
	    }
	    return ret_geom;
	}
    case TINTYPE:
	{
	    size_t i;

	    const LWTIN* lwp = (const LWTIN*)geom;
	    ret_geom = sfcgal_triangulated_surface_create();

	    for ( i = 0; i < lwp->ngeoms; i++ )
	    {
		// recurse call
		sfcgal_geometry_t* g = LWGEOM2SFCGAL( (const LWGEOM*)lwp->geoms[i] );
		// add the obtained polygon to the surface
		sfcgal_triangulated_surface_add_triangle( ret_geom, g );
	    }
	    return ret_geom;
	}
    default:
	// FIXME
	//	throw std::runtime_error( (boost::format( "Unsupported LWGEOM type %1%" ) % geom->type ).str() );
	break;
    }
    return ret_geom;
}

LWGEOM* lwgeom_sfcgal_noop( const LWGEOM* geom_in )
{
    sfcgal_geometry_t* converted = LWGEOM2SFCGAL( geom_in );

    // Noop

    LWGEOM* geom_out = SFCGAL2LWGEOM( converted, 0, SRID_UNKNOWN );
    sfcgal_geometry_delete( converted );

    // copy SRID (SFCGAL does not store the SRID)
    geom_out->srid = geom_in->srid;
    return geom_out;
}
