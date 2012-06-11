#include <stdexcept>

#include <boost/format.hpp>

#include <SFCGAL/Geometry.h>
#include <SFCGAL/Point.h>
#include <SFCGAL/LineString.h>
#include <SFCGAL/Polygon.h>
#include <SFCGAL/GeometryCollection.h>
#include <SFCGAL/MultiPoint.h>
#include <SFCGAL/MultiLineString.h>
#include <SFCGAL/MultiPolygon.h>

extern "C"
{
#include "lwgeom_sfcgal.h"
}

// FIXMe debug
using std::cout;
using std::endl;

int SFCGAL_type_to_lwgeom_type( SFCGAL::GeometryType type )
{
    switch ( type )
    {
    case SFCGAL::TYPE_POINT:
	return POINTTYPE;
    case SFCGAL::TYPE_LINESTRING:
	return LINETYPE;
    case SFCGAL::TYPE_POLYGON:
	return POLYGONTYPE;
    case SFCGAL::TYPE_MULTIPOINT:
	return MULTIPOINTTYPE;
    case SFCGAL::TYPE_MULTILINESTRING:
	return MULTILINETYPE;
    case SFCGAL::TYPE_MULTIPOLYGON:
	return MULTIPOLYGONTYPE;
    case SFCGAL::TYPE_GEOMETRYCOLLECTION:
	return COLLECTIONTYPE;
    case SFCGAL::TYPE_CIRCULARSTRING:
	return CIRCSTRINGTYPE;
    case SFCGAL::TYPE_COMPOUNDCURVE:
	return COMPOUNDTYPE;
    case SFCGAL::TYPE_CURVEPOLYGON:
	return CURVEPOLYTYPE;
    case SFCGAL::TYPE_MULTICURVE:
	return MULTICURVETYPE;
    case SFCGAL::TYPE_MULTISURFACE:
	return MULTISURFACETYPE;
    case SFCGAL::TYPE_CURVE:
	// Unknown LWGEOM type
	return 0;
    case SFCGAL::TYPE_SURFACE:
	// Unknown LWGEOM type
	return 0;
    case SFCGAL::TYPE_POLYHEDRALSURFACE:
	return POLYHEDRALSURFACETYPE;
    case SFCGAL::TYPE_TIN:
	return TINTYPE;
    };
    // what about TRIANGLETYPE ?
    return 0;
}

POINTARRAY* ptarray_from_SFCGAL( const SFCGAL::Geometry* geom )
{
    POINTARRAY* pa = 0;
    POINT4D point;

    switch ( geom->geometryTypeId() )
    {
    case SFCGAL::TYPE_POINT:
	{
	    const SFCGAL::Point* pt = dynamic_cast<const SFCGAL::Point*>( geom );
	    pa = ptarray_construct( pt->is3D(), 0, 1 );
	    point.x = pt->x();
	    point.y = pt->y();
	    if ( pt->is3D() )
		point.z = pt->z();
	    point.m = 0.0;
	    ptarray_set_point4d( pa, 0, &point );
	    break;
	}
    case SFCGAL::TYPE_LINESTRING:
	{
	    const SFCGAL::LineString* ls = dynamic_cast<const SFCGAL::LineString*>( geom );
	    pa = ptarray_construct( ls->is3D(), 0, ls->numPoints() );

	    for ( size_t i = 0; i < ls->numPoints(); i++ )
	    {
		const SFCGAL::Point* pt = &ls->pointN( i );
		point.x = pt->x();
		point.y = pt->y();
		if ( pt->is3D() )
		    point.z = pt->z();
		point.m = 0.0;
		ptarray_set_point4d( pa, i, &point );		
	    }
	    break;
	}

	// These other types should not be called directly ...
    case SFCGAL::TYPE_POLYGON:
    case SFCGAL::TYPE_MULTIPOINT:
    case SFCGAL::TYPE_MULTILINESTRING:
    case SFCGAL::TYPE_MULTIPOLYGON:
    case SFCGAL::TYPE_GEOMETRYCOLLECTION:
    case SFCGAL::TYPE_CIRCULARSTRING:
    case SFCGAL::TYPE_COMPOUNDCURVE:
    case SFCGAL::TYPE_CURVEPOLYGON:
    case SFCGAL::TYPE_MULTICURVE:
    case SFCGAL::TYPE_MULTISURFACE:
    case SFCGAL::TYPE_CURVE:
    case SFCGAL::TYPE_SURFACE:
    case SFCGAL::TYPE_POLYHEDRALSURFACE:
    case SFCGAL::TYPE_TIN:
    default:
	throw std::runtime_error( "ptarray_from_SFCGAL: Unsupported SFCGAL geometry of type " + geom->geometryType() );
	break;
    }
    return pa;
}

SFCGAL::Geometry* ptarray_to_SFCGAL( const POINTARRAY* pa, int type )
{
    POINT3DZ point;

    switch ( type )
    {
    case POINTTYPE:
	{
	    getPoint3dz_p( pa, 0, &point );
	    bool is_3d = FLAGS_GET_Z( pa->flags ) != 0;
	    if ( is_3d )
		return new SFCGAL::Point( point.x, point.y, point.z );
	    else
		return new SFCGAL::Point( point.x, point.y );
	}
    case LINETYPE:
	{
	    SFCGAL::LineString* ret_geom = new SFCGAL::LineString();
	    bool is_3d = FLAGS_GET_Z( pa->flags ) != 0;
	    for ( size_t i = 0; i < pa->npoints; i++ )
	    {
		getPoint3dz_p( pa, i, &point );
		if ( is_3d )
		    ret_geom->points().push_back( SFCGAL::Point( point.x, point.y, point.z ) );
		else
		    ret_geom->points().push_back( SFCGAL::Point( point.x, point.y ) );		    
	    }
	    return ret_geom;
	}
    }
    return (SFCGAL::Geometry*)0;
}

LWGEOM* SFCGAL2LWGEOM( const SFCGAL::Geometry* geom )
{
    // default SRID
    int SRID = SRID_UNKNOWN;
    bool want3d = true;

    switch ( geom->geometryTypeId() )
    {
    case SFCGAL::TYPE_POINT:
	{
	    if ( geom->isEmpty() )
		return (LWGEOM*)lwpoint_construct_empty( SRID, want3d, 0 );
	    POINTARRAY* pa = ptarray_from_SFCGAL( geom );
	    return (LWGEOM*)lwpoint_construct( SRID, /* bbox */ NULL, pa );
	}
    case SFCGAL::TYPE_LINESTRING:
	{
	    if ( geom->isEmpty() )
		return (LWGEOM*)lwline_construct_empty( SRID, want3d, 0 );
	    POINTARRAY* pa = ptarray_from_SFCGAL( geom );
	    return (LWGEOM*)lwline_construct( SRID, /* bbox */ NULL, pa );
	}
    case SFCGAL::TYPE_POLYGON:
	{
	    if ( geom->isEmpty() )
		return (LWGEOM*)lwpoly_construct_empty( SRID, want3d, 0 );

	    const SFCGAL::Polygon* poly = dynamic_cast<const SFCGAL::Polygon*>( geom );
	    size_t n_interiors = poly->numInteriorRings();
	    // allocate for all the rings (including the exterior one)
	    POINTARRAY** pa = (POINTARRAY**) lwalloc( sizeof(POINTARRAY*) * (n_interiors + 1 ) );

	    // write the exterior ring
	    pa[0] = ptarray_from_SFCGAL( &poly->exteriorRing() );
	    for ( size_t i = 0; i < n_interiors; i++ )
	    {
		pa[ i+1 ] = ptarray_from_SFCGAL( &poly->interiorRingN( i ) );
	    }
	    return (LWGEOM*)lwpoly_construct( SRID, NULL, n_interiors + 1, pa );
	}
    case SFCGAL::TYPE_MULTIPOINT:
    case SFCGAL::TYPE_MULTILINESTRING:
    case SFCGAL::TYPE_MULTIPOLYGON:
    case SFCGAL::TYPE_GEOMETRYCOLLECTION:
	{
	    const SFCGAL::GeometryCollection* collection = dynamic_cast<const SFCGAL::GeometryCollection*>( geom );
	    size_t n_geoms = collection->numGeometries();
	    LWGEOM** geoms;
	    if ( n_geoms )
	    {
		geoms = (LWGEOM**)lwalloc( sizeof(LWGEOM*) * n_geoms );
		for ( size_t i = 0; i < n_geoms; i++ )
		{
		    const SFCGAL::Geometry& g = collection->geometryN( i );
		    // recurse call
		    geoms[i] = SFCGAL2LWGEOM( &g );
		}
	    }
	    return (LWGEOM*)lwcollection_construct( SFCGAL_type_to_lwgeom_type( geom->geometryTypeId() ),
						    SRID,
						    NULL,
						    n_geoms,
						    geoms );
	}
    case SFCGAL::TYPE_CIRCULARSTRING:
    case SFCGAL::TYPE_COMPOUNDCURVE:
    case SFCGAL::TYPE_CURVEPOLYGON:
    case SFCGAL::TYPE_MULTICURVE:
    case SFCGAL::TYPE_MULTISURFACE:
    case SFCGAL::TYPE_CURVE:
    case SFCGAL::TYPE_SURFACE:
    case SFCGAL::TYPE_POLYHEDRALSURFACE:
    case SFCGAL::TYPE_TIN:
    default:
	throw std::runtime_error( "Unsupported SFCGAL geometry of type " + geom->geometryType() );
	break;
    }
}

SFCGAL::Geometry* LWGEOM2SFCGAL( const LWGEOM* geom )
{
    SFCGAL::Geometry* ret_geom = 0;
    POINT3DZ point;

    switch ( geom->type )
    {
    case POINTTYPE:
	{
	    const LWPOINT* lwp = (const LWPOINT*) geom;
	    if ( lwgeom_is_empty( geom ) )
		return new SFCGAL::Point();

	    return ptarray_to_SFCGAL( lwp->point, POINTTYPE );
	}
	break;
    case LINETYPE:
	{
	    const LWLINE* line = (const LWLINE*) geom;
	    if ( lwgeom_is_empty( geom ) )
		return new SFCGAL::LineString();

	    return ptarray_to_SFCGAL( line->points, LINETYPE );
	}
	break;
    case POLYGONTYPE:
	{
	    const LWPOLY* poly = (const LWPOLY*) geom;
	    if ( lwgeom_is_empty( geom ) )
		return new SFCGAL::Polygon();

	    size_t n_rings = poly->nrings - 1;
	    SFCGAL::Polygon* ret_poly = new SFCGAL::Polygon();
	    
	    SFCGAL::LineString* ext_ring = static_cast<SFCGAL::LineString*>(ptarray_to_SFCGAL( poly->rings[0], LINETYPE ));
	    ret_poly->exteriorRing() = *ext_ring;
	    delete ext_ring;

	    for ( size_t i = 0; i < n_rings; i++ )
	    {
		SFCGAL::LineString* ring = static_cast<SFCGAL::LineString*>(ptarray_to_SFCGAL( poly->rings[ i + 1 ], LINETYPE ));
		ret_poly->interiorRings().push_back( *ring );
		delete ring;
	    }
	    return ret_poly;
	}
	break;
    case MULTIPOINTTYPE:
    case MULTILINETYPE:
    case MULTIPOLYGONTYPE:
    case COLLECTIONTYPE:
	{
	    if ( geom->type == MULTIPOINTTYPE )
		ret_geom = new SFCGAL::MultiPoint();
	    else if ( geom->type == MULTILINETYPE )
		ret_geom = new SFCGAL::MultiLineString();
	    else if ( geom->type == MULTIPOLYGONTYPE )
		ret_geom = new SFCGAL::MultiPolygon();
	    else
		ret_geom = new SFCGAL::GeometryCollection();
	    
	    const LWCOLLECTION* lwc = (const LWCOLLECTION*)geom;
	    for ( size_t i = 0; i < lwc->ngeoms; i++ )
	    {
		// recurse call
		SFCGAL::Geometry* g = LWGEOM2SFCGAL( lwc->geoms[i] );
		// takes ownership of the pointer
		static_cast<SFCGAL::GeometryCollection*>(ret_geom)->addGeometry( g );
	    }
	    return ret_geom;
	}
	break;
    default:
	throw std::runtime_error( (boost::format( "Unsupported LWGEOM type %1%" ) % geom->type ).str() );
    }
    return ret_geom;
}

LWGEOM* lwgeom_sfcgal_noop( const LWGEOM* geom_in )
{
    SFCGAL::Geometry* converted = LWGEOM2SFCGAL( geom_in );

    // Noop

    LWGEOM* geom_out = SFCGAL2LWGEOM( converted );
    delete converted;
    return geom_out;
}
