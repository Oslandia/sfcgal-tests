#include <SFCGAL/Polygon.h>

namespace SFCGAL {

///
///
///
Polygon::Polygon():
	Surface(),
	_exteriorRing(),
	_interiorRings()
{

}

///
///
///
Polygon::Polygon( LineString exteriorRing_ ):
	Surface(),
	_exteriorRing(exteriorRing_),
	_interiorRings()
{

}

///
///
///
Polygon::Polygon( Polygon const& other ):
	Surface(other),
	_exteriorRing(other._exteriorRing),
	_interiorRings(other._interiorRings)
{

}

///
///
///
Polygon& Polygon::operator = ( const Polygon & other )
{
	_exteriorRing = other._exteriorRing ;
	_interiorRings = other._interiorRings ;
	return *this ;
}

///
///
///
Polygon::~Polygon()
{

}

///
///
///
int Polygon::coordinateDimension() const
{
	return _exteriorRing.coordinateDimension() ;
}


///
///
///
std::string Polygon::geometryType() const
{
	return "Polygon" ;
}

///
///
///
GeometryType Polygon::geometryTypeId() const
{
	return TYPE_POLYGON ;
}

///
///
///
Polygon * Polygon::clone() const
{
	return new Polygon(*this);
}

///
///
///
bool   Polygon::isEmpty() const
{
	return _exteriorRing.isEmpty() ;
}

///
///
///
bool   Polygon:: is3D() const
{
	return _exteriorRing.is3D() ;
}




}//SFCGAL

