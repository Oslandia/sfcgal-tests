#ifndef _SFCGAL_POLYGON_H_
#define _SFCGAL_POLYGON_H_

#include <vector>
#include <boost/assert.hpp>

#include <SFCGAL/LineString.h>
#include <SFCGAL/Surface.h>


namespace SFCGAL {

	/**
	 * A Polygon in SFA with holes
	 */
	class Polygon : public Surface {
	public:
		/**
		 * Empty Polygon constructor
		 */
		Polygon() ;
		/**
		 * Constructor with an exterior ring
		 */
		Polygon( LineString exteriorRing ) ;

		/**
		 * Copy constructor
		 */
		Polygon( Polygon const& other ) ;

		/**
		 * assign operator
		 */
		Polygon& operator = ( const Polygon & other ) ;

		/**
		 * destructor
		 */
		~Polygon() ;

		//-- SFCGAL::Geometry
		virtual Polygon *   clone() const ;

		//-- SFCGAL::Geometry
		virtual std::string    geometryType() const ;
		//-- SFCGAL::Geometry
		virtual GeometryType   geometryTypeId() const ;
		//-- SFCGAL::Geometry
		virtual int            coordinateDimension() const ;
		//-- SFCGAL::Geometry
		virtual bool           isEmpty() const ;
		//-- SFCGAL::Geometry
		virtual bool           is3D() const ;


		/**
		 * [OGC/SFA]returns the exterior ring
		 */
		inline const LineString &    exteriorRing() const { return _exteriorRing; }
		/**
		 * [OGC/SFA]returns the exterior ring
		 */
		inline LineString &          exteriorRing() { return _exteriorRing; }

		/**
		 * [OGC/SFA]returns the exterior ring
		 */
		inline size_t                numInteriorRings() const { return _interiorRings.size(); }
		/**
		 * [OGC/SFA]returns the exterior ring
		 */
		inline const LineString &    interiorRingN( const size_t & n ) const { return _interiorRings[n]; }
		/**
		 * [OGC/SFA]returns the exterior ring
		 */
		inline LineString &          interiorRingN( const size_t & n ) { return _interiorRings[n]; }


	private:
		/**
		 * exterior ring of the polygon
		 */
		LineString                _exteriorRing ;
		/**
		 * holes in the polygon
		 */
		std::vector< LineString > _interiorRings ;
	};


}

#endif
