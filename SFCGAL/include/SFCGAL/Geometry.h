#ifndef _SFCGAL_GEOMETRY_H_
#define _SFCGAL_GEOMETRY_H_

#include <memory>
#include <string>
#include <sstream>

#include <SFCGAL/Coordinate.h>

namespace SFCGAL {

    class Point ;
    class LineString ;
    class Polygon ;

    class GeometryCollection ;
    class MultiPoint ;
    class MultiLineString ;
    class MultiPolygon ;
}

namespace SFCGAL {

    /**
     * [OGC/SFA]8.2.3 "A common list of codes for geometric types"
     *
     * @todo solid and triangles as non OGC/SFA geometric types?
     */
    typedef enum {
       TYPE_GEOMETRY            = 0,
       TYPE_POINT               = 1,
       TYPE_LINESTRING          = 2,
       TYPE_POLYGON             = 3,
       TYPE_MULTIPOINT          = 4,
       TYPE_MULTILINESTRING     = 5,
       TYPE_MULTIPOLYGON        = 6,
       TYPE_GEOMETRYCOLLECTION  = 7,
       TYPE_CIRCULARSTRING      = 8,
       TYPE_COMPOUNDCURVE       = 9,
       TYPE_CURVEPOLYGON        = 10,
       TYPE_MULTICURVE          = 11,
       TYPE_MULTISURFACE        = 12,
       TYPE_CURVE               = 13,
       TYPE_SURFACE             = 14,
       TYPE_POLYHEDRALSURFACE   = 15,
       TYPE_TIN                 = 16
    } GeometryType ;


    /**
     * OGC/SFA base geometry class
     *
     * @todo template with CGAL kernel as parameter?
     */
    class Geometry {
    public:
       virtual ~Geometry();

       /**
        * returns a deep copy of the geometry
        */
       virtual Geometry *  clone() const = 0 ;

       /**
        * [OGC/SFA]returns the geometry type
        * @warning use CamelCase (LineString, not LINESTRING)
        */
       virtual std::string  geometryType() const = 0 ;
       /**
        * Returns a code corresponding to the type
        * @warning not standard
        */
       virtual GeometryType geometryTypeId() const = 0 ;

       /**
        * [OGC/SFA]Dimension of the Geometry ( 0 : ponctual, 1 : curve, ...)
        */
       virtual int          dimension() const = 0 ;
       /**
        * [OGC/SFA]returns the dimension of the coordinates
        */
       virtual int          coordinateDimension() const = 0 ;
       /**
        * [OGC/SFA]test if geometry is empty
        */
       virtual bool         isEmpty() const = 0 ;
       /**
        * [OGC/SFA]test if geometry is 3d
        */
       virtual bool         is3D() const = 0 ;
       /**
        * [OGC/SFA]Indicate if the geometry is simple (~no self-intersections)
        */
       //virtual bool         isSimple() const = 0 ;

       /**
        * [OGC/SFA]returns the WKT string
        * @numDecimals extension specify fix precision output
        */
       std::string          asText( const int & numDecimals = -1 ) const ;

       /**
        * Test if geometry is of "Derived" type given as template parameter
        * @warning not optimized (slow with dynamic_cast)
        */
       template < typename Derived >
       inline bool is() const
       {
    	   return dynamic_cast< Derived const * >(this) != NULL ;
       }


       /**
        * Downcast helper
        */
       template < typename Derived >
       inline const Derived &  as() const {
              return *static_cast< Derived const * >( this );
       }
       /**
        * Downcast helper
        */
       template < typename Derived >
       inline Derived &        as() {
              return *static_cast< Derived * >( this );
       }


    protected:
       Geometry();
       Geometry( Geometry const& other );
    };

}

#endif
