#include <stdexcept>

#include <boost/format.hpp>

#include <SFCGAL/Geometry.h>
#include <SFCGAL/Point.h>
#include <SFCGAL/LineString.h>
#include <SFCGAL/Triangle.h>
#include <SFCGAL/Polygon.h>
#include <SFCGAL/GeometryCollection.h>
#include <SFCGAL/MultiPoint.h>
#include <SFCGAL/MultiLineString.h>
#include <SFCGAL/MultiPolygon.h>
#include <SFCGAL/PolyhedralSurface.h>
#include <SFCGAL/TriangulatedSurface.h>
#include <SFCGAL/Solid.h>

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
    case SFCGAL::TYPE_MULTISOLID:
	return COLLECTIONTYPE;
    case SFCGAL::TYPE_GEOMETRYCOLLECTION:
	return COLLECTIONTYPE;
	//    case SFCGAL::TYPE_CIRCULARSTRING:
	//	return CIRCSTRINGTYPE;
	//    case SFCGAL::TYPE_COMPOUNDCURVE:
	//	return COMPOUNDTYPE;
	//    case SFCGAL::TYPE_CURVEPOLYGON:
	//	return CURVEPOLYTYPE;
	//    case SFCGAL::TYPE_MULTICURVE:
	//	return MULTICURVETYPE;
	//    case SFCGAL::TYPE_MULTISURFACE:
	//	return MULTISURFACETYPE;
	//    case SFCGAL::TYPE_CURVE:
	// Unknown LWGEOM type
	//	return 0;
	//    case SFCGAL::TYPE_SURFACE:
	// Unknown LWGEOM type
	//	return 0;
    case SFCGAL::TYPE_POLYHEDRALSURFACE:
	return POLYHEDRALSURFACETYPE;
    case SFCGAL::TYPE_TRIANGULATEDSURFACE:
	return TINTYPE;
    case SFCGAL::TYPE_TRIANGLE:
	return TRIANGLETYPE;
    };
    return 0;
}

POINTARRAY* ptarray_from_SFCGAL( const SFCGAL::Geometry* geom, bool force3D = false)
{
    POINTARRAY* pa = 0;
    POINT4D point;
    bool want3d = force3D || geom->is3D();

    switch ( geom->geometryTypeId() )
    {
    case SFCGAL::TYPE_POINT:
	{
	    const SFCGAL::Point* pt = static_cast<const SFCGAL::Point*>( geom );
	    pa = ptarray_construct( want3d, 0, 1 );
	    point.x = pt->x();
	    point.y = pt->y();
	    if ( geom->is3D() ) {
		    point.z = pt->z();
	    }
	    else if ( force3D ) {
		    point.z = 0.0;
	    }	    
	    point.m = 0.0;
	    ptarray_set_point4d( pa, 0, &point );
	    break;
	}
    case SFCGAL::TYPE_LINESTRING:
	{
	    const SFCGAL::LineString* ls = static_cast<const SFCGAL::LineString*>( geom );
	    //	    pa = ptarray_construct( ls->is3D() ? 1 : 0, 0, ls->numPoints() );
	    pa = ptarray_construct( want3d, 0, ls->numPoints() );

	    for ( size_t i = 0; i < ls->numPoints(); i++ )
	    {
		const SFCGAL::Point* pt = &ls->pointN( i );
		point.x = pt->x();
		point.y = pt->y();
		if ( geom->is3D() ) {
			point.z = pt->z();
		}
		else if ( force3D ) {
			point.z = 0.0;
		}
		point.m = 0.0;
		ptarray_set_point4d( pa, i, &point );		
	    }
	    break;
	}
    case SFCGAL::TYPE_TRIANGLE:
	{
	    const SFCGAL::Triangle* tri = static_cast<const SFCGAL::Triangle*>( geom );
	    pa = ptarray_construct( want3d, 0, 3 );

	    for ( size_t i = 0; i < 3; i++ )
	    {
		const SFCGAL::Point* pt = &tri->vertex( i );
		point.x = pt->x();
		point.y = pt->y();
		if ( geom->is3D() ) {
			point.z = pt->z();
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
    case SFCGAL::TYPE_POLYGON:
    case SFCGAL::TYPE_MULTIPOINT:
    case SFCGAL::TYPE_MULTILINESTRING:
    case SFCGAL::TYPE_MULTIPOLYGON:
    case SFCGAL::TYPE_GEOMETRYCOLLECTION:
	//    case SFCGAL::TYPE_CIRCULARSTRING:
	//    case SFCGAL::TYPE_COMPOUNDCURVE:
	//    case SFCGAL::TYPE_CURVEPOLYGON:
	//    case SFCGAL::TYPE_MULTICURVE:
	//    case SFCGAL::TYPE_MULTISURFACE:
	//    case SFCGAL::TYPE_CURVE:
	//    case SFCGAL::TYPE_SURFACE:
    case SFCGAL::TYPE_POLYHEDRALSURFACE:
    case SFCGAL::TYPE_TRIANGULATEDSURFACE:
    default:
	throw std::runtime_error( "ptarray_from_SFCGAL: Unsupported SFCGAL geometry of type " + geom->geometryType() );
	break;
    }
    return pa;
}

std::auto_ptr<SFCGAL::Geometry> ptarray_to_SFCGAL( const POINTARRAY* pa, int type )
{
    POINT3DZ point;

    switch ( type )
    {
    case POINTTYPE:
	{
	    getPoint3dz_p( pa, 0, &point );
	    bool is_3d = FLAGS_GET_Z( pa->flags ) != 0;
	    if ( is_3d )
		return std::auto_ptr<SFCGAL::Geometry>(new SFCGAL::Point( point.x, point.y, point.z ));
	    else
		return std::auto_ptr<SFCGAL::Geometry>(new SFCGAL::Point( point.x, point.y ));
	}
    case LINETYPE:
	{
	    SFCGAL::LineString* ret_geom = new SFCGAL::LineString;

	    bool is_3d = FLAGS_GET_Z( pa->flags ) != 0;
	    for ( size_t i = 0; i < pa->npoints; i++ )
	    {
		getPoint3dz_p( pa, i, &point );
		if ( is_3d )
		    ret_geom->addPoint( SFCGAL::Point( point.x, point.y, point.z ) );
		else
		    ret_geom->addPoint( SFCGAL::Point( point.x, point.y ) );
	    }
	    return std::auto_ptr<SFCGAL::Geometry>(ret_geom);
	}
    case TRIANGLETYPE:
	{
	    SFCGAL::Point p, q, r;

	    bool is_3d = FLAGS_GET_Z( pa->flags ) != 0;
	    getPoint3dz_p( pa, 0, &point );
	    if ( is_3d )
		p = SFCGAL::Point( point.x, point.y, point.z );
	    else
		p = SFCGAL::Point( point.x, point.y );
	    getPoint3dz_p( pa, 1, &point );
	    if ( is_3d )
		q = SFCGAL::Point( point.x, point.y, point.z );
	    else
		q = SFCGAL::Point( point.x, point.y );
	    getPoint3dz_p( pa, 2, &point );
	    if ( is_3d )
		r = SFCGAL::Point( point.x, point.y, point.z );
	    else
		r = SFCGAL::Point( point.x, point.y );

	    return std::auto_ptr<SFCGAL::Geometry>(new SFCGAL::Triangle( p, q, r ));
	}
    }
    return std::auto_ptr<SFCGAL::Geometry>(0);
}

LWGEOM* SFCGAL2LWGEOM( const SFCGAL::Geometry* geom, bool force3D )
{
    // default SRID
    int SRID = SRID_UNKNOWN;
    bool want3d = force3D || geom->is3D();

    switch ( geom->geometryTypeId() )
    {
    case SFCGAL::TYPE_POINT:
	{
	    if ( geom->isEmpty() )
		return (LWGEOM*)lwpoint_construct_empty( SRID, want3d, 0 );
	    POINTARRAY* pa = ptarray_from_SFCGAL( geom, force3D );
	    return (LWGEOM*)lwpoint_construct( SRID, /* bbox */ NULL, pa );
	}
    case SFCGAL::TYPE_LINESTRING:
	{
	    if ( geom->isEmpty() )
		return (LWGEOM*)lwline_construct_empty( SRID, want3d, 0 );
	    POINTARRAY* pa = ptarray_from_SFCGAL( geom, force3D );
	    return (LWGEOM*)lwline_construct( SRID, /* bbox */ NULL, pa );
	}
    case SFCGAL::TYPE_TRIANGLE:
	{
	    if ( geom->isEmpty() )
		return (LWGEOM*)lwtriangle_construct_empty( SRID, want3d, 0 );
	    POINTARRAY* pa = ptarray_from_SFCGAL( geom, force3D );
	    return (LWGEOM*)lwtriangle_construct( SRID, /* bbox */ NULL, pa );
	}
    case SFCGAL::TYPE_POLYGON:
	{
	    if ( geom->isEmpty() )
		return (LWGEOM*)lwpoly_construct_empty( SRID, want3d, 0 );

	    const SFCGAL::Polygon* poly = static_cast<const SFCGAL::Polygon*>( geom );
	    size_t n_interiors = poly->numInteriorRings();
	    // allocate for all the rings (including the exterior one)
	    POINTARRAY** pa = (POINTARRAY**) lwalloc( sizeof(POINTARRAY*) * (n_interiors + 1 ) );

	    // write the exterior ring
	    pa[0] = ptarray_from_SFCGAL( &poly->exteriorRing(), force3D );
	    for ( size_t i = 0; i < n_interiors; i++ )
	    {
		    pa[ i+1 ] = ptarray_from_SFCGAL( &poly->interiorRingN( i ), force3D );
	    }
	    return (LWGEOM*)lwpoly_construct( SRID, NULL, n_interiors + 1, pa );
	}
    case SFCGAL::TYPE_MULTIPOINT:
    case SFCGAL::TYPE_MULTILINESTRING:
    case SFCGAL::TYPE_MULTIPOLYGON:
    case SFCGAL::TYPE_MULTISOLID:
    case SFCGAL::TYPE_GEOMETRYCOLLECTION:
	{
	    const SFCGAL::GeometryCollection* collection = static_cast<const SFCGAL::GeometryCollection*>( geom );
	    size_t n_geoms = collection->numGeometries();
	    LWGEOM** geoms = 0;
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
	//    case SFCGAL::TYPE_CIRCULARSTRING:
	//    case SFCGAL::TYPE_COMPOUNDCURVE:
	//    case SFCGAL::TYPE_CURVEPOLYGON:
	//    case SFCGAL::TYPE_MULTICURVE:
	//    case SFCGAL::TYPE_MULTISURFACE:
	//    case SFCGAL::TYPE_CURVE:
	//    case SFCGAL::TYPE_SURFACE:
    case SFCGAL::TYPE_POLYHEDRALSURFACE:
	{
	    const SFCGAL::PolyhedralSurface* collection = static_cast<const SFCGAL::PolyhedralSurface*>( geom );
	    size_t n_geoms = collection->numPolygons();
	    LWGEOM** geoms = 0;
	    if ( n_geoms )
	    {
		geoms = (LWGEOM**)lwalloc( sizeof(LWGEOM*) * n_geoms );
		for ( size_t i = 0; i < n_geoms; i++ )
		{
		    const SFCGAL::Geometry& g = collection->polygonN( i );
		    // recurse call
		    geoms[i] = SFCGAL2LWGEOM( &g );
		}
	    }
	    return (LWGEOM*)lwcollection_construct( POLYHEDRALSURFACETYPE,
						    SRID,
						    NULL,
						    n_geoms,
						    geoms );
	}
    case SFCGAL::TYPE_SOLID:
	{
		// a Solid is a closed PolyhedralSurface
		const SFCGAL::Solid* solid = static_cast<const SFCGAL::Solid*>( geom );
		// compute the number of polyhedral
		size_t n_geoms = 0;
		for ( size_t i = 0; i < solid->numShells(); ++i ) {
			n_geoms += solid->shellN(i).numPolygons();
		}
		LWGEOM** geoms = 0;
		if ( n_geoms )
		{
			geoms = (LWGEOM**)lwalloc( sizeof(LWGEOM*) * n_geoms );
			size_t k = 0;
			for ( size_t i = 0; i < solid->numShells(); i++ )
			{
				for ( size_t j = 0; j < solid->shellN(i).numPolygons(); ++j ) {
					const SFCGAL::Geometry& g = solid->shellN(i).polygonN(j);
					// recurse call
					geoms[k] = SFCGAL2LWGEOM( &g, /* force3D = */ true );
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
    case SFCGAL::TYPE_TRIANGULATEDSURFACE:
	{
	    const SFCGAL::TriangulatedSurface* collection = static_cast<const SFCGAL::TriangulatedSurface*>( geom );
	    size_t n_geoms = collection->numTriangles();
	    LWGEOM** geoms = 0;
	    if ( n_geoms )
	    {
		geoms = (LWGEOM**)lwalloc( sizeof(LWGEOM*) * n_geoms );
		for ( size_t i = 0; i < n_geoms; i++ )
		{
		    const SFCGAL::Geometry& g = collection->triangleN( i );
		    // recurse call
		    geoms[i] = SFCGAL2LWGEOM( &g );
		}
	    }
	    return (LWGEOM*)lwcollection_construct( TINTYPE,
						    SRID,
						    NULL,
						    n_geoms,
						    geoms );
	}
    default:
	throw std::runtime_error( "SFCGAL2LWGEOM: Unsupported SFCGAL geometry of type " + geom->geometryType() );
	break;
    }
}

std::auto_ptr<SFCGAL::Geometry> LWGEOM2SFCGAL( const LWGEOM* geom )
{
    SFCGAL::Geometry* ret_geom = 0;
    POINT3DZ point;

    switch ( geom->type )
    {
    case POINTTYPE:
	{
	    const LWPOINT* lwp = (const LWPOINT*) geom;
	    if ( lwgeom_is_empty( geom ) )
		return std::auto_ptr<SFCGAL::Geometry>(new SFCGAL::Point());

	    return ptarray_to_SFCGAL( lwp->point, POINTTYPE );
	}
	break;
    case LINETYPE:
	{
	    const LWLINE* line = (const LWLINE*) geom;
	    if ( lwgeom_is_empty( geom ) )
		return std::auto_ptr<SFCGAL::Geometry>(new SFCGAL::LineString());

	    return ptarray_to_SFCGAL( line->points, LINETYPE );
	}
	break;
    case TRIANGLETYPE:
	{
	    const LWTRIANGLE* tri = (const LWTRIANGLE*) geom;
	    if ( lwgeom_is_empty( geom ) )
		return std::auto_ptr<SFCGAL::Geometry>(new SFCGAL::Triangle());

	    return ptarray_to_SFCGAL( tri->points, TRIANGLETYPE );
	}
	break;
    case POLYGONTYPE:
	{
	    const LWPOLY* poly = (const LWPOLY*) geom;
	    if ( lwgeom_is_empty( geom ) )
		return std::auto_ptr<SFCGAL::Geometry>(new SFCGAL::Polygon());

	    size_t n_rings = poly->nrings - 1;
	    
	    std::auto_ptr<SFCGAL::Geometry> ext_ring(ptarray_to_SFCGAL( poly->rings[0], LINETYPE ));
	    SFCGAL::Polygon* ret_poly = new SFCGAL::Polygon( *static_cast<SFCGAL::LineString*>(ext_ring.get()) );

	    for ( size_t i = 0; i < n_rings; i++ )
	    {
		std::auto_ptr<SFCGAL::Geometry> ring(ptarray_to_SFCGAL( poly->rings[ i + 1 ], LINETYPE ));
		// takes ownership
		ret_poly->addRing( static_cast<SFCGAL::LineString*>(ring.release()) );
	    }
	    return std::auto_ptr<SFCGAL::Geometry>(ret_poly);
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
		std::auto_ptr<SFCGAL::Geometry> g(LWGEOM2SFCGAL( lwc->geoms[i] ));
		// takes ownership of the pointer
		static_cast<SFCGAL::GeometryCollection*>(ret_geom)->addGeometry( g.release() );
	    }
	    return std::auto_ptr<SFCGAL::Geometry>(ret_geom);
	}
	break;
    case POLYHEDRALSURFACETYPE:
	{
	    const LWPSURFACE* lwp = (const LWPSURFACE*)geom;
	    ret_geom = new SFCGAL::PolyhedralSurface();

	    for ( size_t i = 0; i < lwp->ngeoms; i++ )
	    {
		// recurse call
		std::auto_ptr<SFCGAL::Geometry> g(LWGEOM2SFCGAL( (const LWGEOM*)lwp->geoms[i] ));
		BOOST_ASSERT( g->geometryTypeId() == SFCGAL::TYPE_POLYGON );
		// add the obtained polygon to the surface
		// (pass ownership )
		static_cast<SFCGAL::PolyhedralSurface*>(ret_geom)->addPolygon( static_cast<SFCGAL::Polygon*>(g.release()) );
	    }
	    if ( FLAGS_GET_SOLID( lwp->flags ) ) {
		    // return a Solid
		    // FIXME: we treat polyhedral surface as the only exterior shell, since we do not have
		    // any way to distinguish exterior from interior shells ...
		    return std::auto_ptr<SFCGAL::Geometry>( new SFCGAL::Solid( ret_geom->as<SFCGAL::PolyhedralSurface>() ) );
	    }
	    const SFCGAL::PolyhedralSurface& p = static_cast<const SFCGAL::PolyhedralSurface&>(*ret_geom);
	    return std::auto_ptr<SFCGAL::Geometry>(ret_geom);
	}
    case TINTYPE:
	{
	    const LWTIN* lwp = (const LWTIN*)geom;
	    ret_geom = new SFCGAL::TriangulatedSurface();

	    for ( size_t i = 0; i < lwp->ngeoms; i++ )
	    {
		// recurse call
		std::auto_ptr<SFCGAL::Geometry> g(LWGEOM2SFCGAL( (const LWGEOM*)lwp->geoms[i] ));
		BOOST_ASSERT( g->geometryTypeId() == SFCGAL::TYPE_TRIANGLE );
		// add the obtained polygon to the surface
		static_cast<SFCGAL::TriangulatedSurface*>(ret_geom)->addTriangle( *static_cast<SFCGAL::Triangle*>(g.get()) );
	    }
	    return std::auto_ptr<SFCGAL::Geometry>(ret_geom);
	}
    default:
	throw std::runtime_error( (boost::format( "Unsupported LWGEOM type %1%" ) % geom->type ).str() );
    }
    return std::auto_ptr<SFCGAL::Geometry>(ret_geom);
}

LWGEOM* lwgeom_sfcgal_noop( const LWGEOM* geom_in )
{
    std::auto_ptr<SFCGAL::Geometry> converted = LWGEOM2SFCGAL( geom_in );

    // Noop

    LWGEOM* geom_out = SFCGAL2LWGEOM( converted.get() );

    // copy SRID (SFCGAL does not store the SRID)
    geom_out->srid = geom_in->srid;
    return geom_out;
}
