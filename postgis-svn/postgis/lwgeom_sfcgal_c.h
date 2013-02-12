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

#ifndef _LWGEOM_SFCGAL_C_H_
#define _LWGEOM_SFCGAL_C_H_

#include "../liblwgeom/lwgeom_sfcgal_c.h"

/**
 * Conversion from GSERIALIZED* to SFCGAL::Geometry
 */
sfcgal_geometry_t* POSTGIS2SFCGALGeometry(GSERIALIZED *pglwgeom);

/**
 * Conversion from GSERIALIZED* to SFCGAL::PreparedGeometry
 */
sfcgal_prepared_geometry_t* POSTGIS2SFCGALPreparedGeometry(GSERIALIZED *pglwgeom);

/**
 * Conversion from SFCGAL::Geometry to GSERIALIZED*
 */
GSERIALIZED* SFCGALGeometry2POSTGIS( const sfcgal_geometry_t* geom, int force3D, int SRID );

/**
 * Conversion from SFCGAL::PreparedGeometry to GSERIALIZED*
 */
GSERIALIZED* SFCGALPreparedGeometry2POSTGIS( const sfcgal_prepared_geometry_t* geom, int force3D );


/**
 * Initialize sfcgal with PostGIS error handlers
 */
void sfcgal_postgis_init();

#endif
