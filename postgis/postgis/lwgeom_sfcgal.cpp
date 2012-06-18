#include <stdexcept>
#include <iostream>

#include <SFCGAL/Geometry.h>
#include <SFCGAL/algorithm/intersects.h>

/* TODO: we probaby don't need _all_ these pgsql headers */
extern "C" {
#include "postgres.h"
#include "fmgr.h"
#include "miscadmin.h"
#include "utils/array.h"
#include "utils/builtins.h"
#include "utils/hsearch.h"
#include "utils/memutils.h"
#include "executor/spi.h"
#include "funcapi.h"

#include "../postgis_config.h"
#include "lwgeom_functions_analytic.h" /* for point_in_polygon */
#include "lwgeom_cache.h"
#include "liblwgeom_internal.h"
#include "lwgeom_rtree.h"

}

#include "lwgeom_sfcgal.h"

std::auto_ptr<SFCGAL::Geometry> POSTGIS2SFCGAL(GSERIALIZED *pglwgeom)
{
	LWGEOM *lwgeom = lwgeom_from_gserialized(pglwgeom);
	if ( ! lwgeom )
	{
		throw std::runtime_error("POSTGIS2SFCGAL: unable to deserialize input");
	}
	std::auto_ptr<SFCGAL::Geometry> g(LWGEOM2SFCGAL(lwgeom));
	lwgeom_free(lwgeom);
	return g;
}

GSERIALIZED* SFCGAL2POSTGIS(const SFCGAL::Geometry& geom)
{
	LWGEOM* lwgeom = SFCGAL2LWGEOM( &geom );
	if ( lwgeom_needs_bbox(lwgeom) == LW_TRUE )
	{
		lwgeom_add_bbox(lwgeom);
	}

	GSERIALIZED* result = geometry_serialize(lwgeom);
	lwgeom_free(lwgeom);

	return result;
}

extern "C" {
PG_FUNCTION_INFO_V1(intersects);
}

extern "C" Datum intersects(PG_FUNCTION_ARGS)
{
    //	elog(NOTICE, "-- intersects with SFCGAL --");

	GSERIALIZED *geom1;
	GSERIALIZED *geom2;

	geom1 = (GSERIALIZED *)PG_DETOAST_DATUM(PG_GETARG_DATUM(0));
	geom2 = (GSERIALIZED *)PG_DETOAST_DATUM(PG_GETARG_DATUM(1));

	std::auto_ptr<SFCGAL::Geometry> g1;
	try {
		g1 = POSTGIS2SFCGAL( geom1 );
	}
	catch ( std::exception& e ) {
		lwerror("First argument geometry could not be converted to SFCGAL: %s", e.what() );
		PG_RETURN_NULL();
	}
	std::auto_ptr<SFCGAL::Geometry> g2;
	try {
		g2 = POSTGIS2SFCGAL( geom2 );
	}
	catch ( std::exception& e ) {
		lwerror("Second argument geometry could not be converted to SFCGAL: %s", e.what() );
		PG_RETURN_NULL();
	}
	
	bool result = SFCGAL::algorithm::intersects( *g1, *g2 );

	PG_FREE_IF_COPY(geom1, 0);
	PG_FREE_IF_COPY(geom2, 1);

	PG_RETURN_BOOL(result);
}

