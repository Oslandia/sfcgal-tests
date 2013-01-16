#include <boost/format.hpp>

#include <SFCGAL/Geometry.h>
#include <SFCGAL/TriangulatedSurface.h>
#include <SFCGAL/Solid.h>
#include <SFCGAL/algorithm/triangulate.h>
#include <SFCGAL/algorithm/extrude.h>
#include <SFCGAL/algorithm/intersects.h>
#include <SFCGAL/algorithm/intersection.h>
#include <SFCGAL/algorithm/plane.h>
#include <SFCGAL/transform/ForceZOrderPoints.h>

#include "lwgeom_sfcgal_wrapper.h"

bool _sfcgal_hasplane( const SFCGAL::Geometry& g )
{
	if ( g.geometryTypeId() != SFCGAL::TYPE_POLYGON )
	{
		std::string msg = (boost::format("hasPlane() cannot be applied to a geometry of type %1") % g.geometryType() ).str();
		throw std::runtime_error( msg.c_str() );
	}
	return SFCGAL::algorithm::hasPlane3D< CGAL::Exact_predicates_exact_constructions_kernel >( g.as< const SFCGAL::Polygon >() );
}

bool _sfcgal_pointing_up( const SFCGAL::Geometry& g )
{
	if ( g.geometryTypeId() != SFCGAL::TYPE_POLYGON )
	{
		std::string msg = (boost::format("pointing_up() cannot be applied to a geometry of type %1") % g.geometryType() ).str();
		throw std::runtime_error( msg.c_str() );
	}
	return g.as<SFCGAL::Polygon>().isCounterClockWiseOriented();
}
std::auto_ptr<SFCGAL::Geometry> _sfcgal_triangulate( const SFCGAL::Geometry& g )
{
	SFCGAL::TriangulatedSurface* surf = new SFCGAL::TriangulatedSurface;
	SFCGAL::algorithm::triangulate( g, *surf );
	return std::auto_ptr<SFCGAL::Geometry>( surf );
}
std::auto_ptr<SFCGAL::Geometry> _sfcgal_triangulate2D( const SFCGAL::Geometry& g )
{
	SFCGAL::TriangulatedSurface* surf = new SFCGAL::TriangulatedSurface;
	SFCGAL::algorithm::triangulate2D( g, *surf );
	return std::auto_ptr<SFCGAL::Geometry>( surf );
}

std::auto_ptr<SFCGAL::Geometry> _sfcgal_extrude( SFCGAL::Geometry& g, double dx, double dy, double dz )
{
	SFCGAL::transform::ForceZOrderPoints forceZ;
	g.accept( forceZ );
	return SFCGAL::algorithm::extrude( g, dx, dy, dz );
}

std::auto_ptr<SFCGAL::Geometry> _sfcgal_make_solid( const SFCGAL::Geometry& g )
{
    if ( g.geometryTypeId() == SFCGAL::TYPE_SOLID )
    {
	    // already a solid, return
	    return std::auto_ptr<SFCGAL::Geometry>( g.clone() );
    }
    if ( g.geometryTypeId() != SFCGAL::TYPE_POLYHEDRALSURFACE )
    {
	    throw std::runtime_error( "make_solid only applies to polyhedral surfaces" );
    }
    return std::auto_ptr<SFCGAL::Geometry>( new SFCGAL::Solid( static_cast<const SFCGAL::PolyhedralSurface&>( g ) ) );
}

std::auto_ptr<SFCGAL::Geometry> _sfcgal_force_z_up( SFCGAL::Geometry& g )
{
	    SFCGAL::transform::ForceZOrderPoints forceZ;
	    g.accept( forceZ );
	    return std::auto_ptr<SFCGAL::Geometry>( g.clone() );
}
