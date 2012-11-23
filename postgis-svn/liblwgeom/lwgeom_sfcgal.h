#ifndef LWGEOM_SFCGAL_H
#define LWGEOM_SFCGAL_H

#include <SFCGAL/Geometry.h>
// auto_ptr
#include <memory>

extern "C"
{

#include "liblwgeom.h"

LWGEOM* lwgeom_sfcgal_noop( const LWGEOM* geom_in );

};

extern "C++" LWGEOM*                         SFCGAL2LWGEOM( const SFCGAL::Geometry* geom, bool force3D = false, int SRID = SRID_UNKNOWN );
extern "C++" std::auto_ptr<SFCGAL::Geometry> LWGEOM2SFCGAL( const LWGEOM* geom );

#endif
