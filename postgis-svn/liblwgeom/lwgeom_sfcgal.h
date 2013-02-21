#ifndef LWGEOM_SFCGAL_C_H
#define LWGEOM_SFCGAL_C_H

#include "liblwgeom.h"
#include <SFCGAL/capi/sfcgal_c.h>

LWGEOM* lwgeom_sfcgal_noop( const LWGEOM* geom_in );

LWGEOM*            SFCGAL2LWGEOM( const sfcgal_geometry_t* geom, int force3D, int SRID );
sfcgal_geometry_t* LWGEOM2SFCGAL( const LWGEOM* geom );

#endif
