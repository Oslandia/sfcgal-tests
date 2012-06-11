
/**
 * [OGC/SFA]Dimension of the coordinates
 * @warning suppose no coordinate dimension mix
 */
virtual int coordinateDimension() const = 0 ;
/**
 * [OGC/SFA]Dimension of the coordinates
 * @warning suppose no coordinate dimension mix
 */
//virtual int spatialDimension() const = 0 ;
/**
 * [OGC/SFA]Return the name of the Geometry Type
 * @warning use CamelCase (LineString, NOT LINESTRING)
 */
virtual std::string geometryType() const = 0 ;

/**
 * [OGC/SFA]Returns a polygon representing the BBOX of the geometry
 * @todo In order to adapt to 3D, would be better to define an "Envelope type",
 * otherway would lead to Polygon and PolyhedralSurface
 */
//std::auto_ptr< Geometry > envelope() const = 0 ;



/**
 * [OGC/SFA]Indicate if the geometry is measured
 */
//virtual bool isMeasured() const = 0 ;

/**
 * [OGC/SFA]Return the boundary of the geometry
 */
//virtual std::auto_ptr< Geometry > boundary() const = 0 ;

/**
 * [OGC/SFA]Returns the identifier of the spatial reference
 * system (-1 if undefined)
 * @warning not defined here
 */
//inline int SRID() const ;


//asBinary () : Binary
//virtual void writeBinary( std::ostream & s ) const = 0 ;

