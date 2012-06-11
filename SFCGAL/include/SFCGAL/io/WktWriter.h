#ifndef _SFCGAL_IO_WKTWRITER_H_
#define _SFCGAL_IO_WKTWRITER_H_

#include <sstream>

#include <SFCGAL/Geometry.h>

namespace SFCGAL {
namespace io {

	/**
	 * Writer for WKT
	 */
	class WktWriter {
	public:
		WktWriter( std::ostream & s ) ;

		/**
		 * @todo replace with visitor dispatch
		 */
		void write( const Geometry & g ) ;

	protected:
		void write( const Coordinate & g );

		void write( const Point & g ) ;
		void writeInner( const Point & g ) ;

		void write( const LineString & g ) ;
		void writeInner( const LineString & g ) ;

		void write( const Polygon & g ) ;
		void writeInner( const Polygon & g ) ;

		void write( const GeometryCollection & g ) ;

		void write( const MultiPoint & g ) ;
		void write( const MultiLineString & g ) ;
		void write( const MultiPolygon & g ) ;
	private:
		std::ostream & _s ;
	};


}//io
}//SFCGAL

#endif
