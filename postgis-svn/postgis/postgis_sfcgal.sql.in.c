---------------------------------------------------------------------------
--
-- PostGIS - SFCGAL functions
-- Copyright 2012-2013 Oslandia <contact@oslandia.com>
--
-- This is free software; you can redistribute and/or modify it under
-- the terms of the GNU General Public Licence. See the COPYING file.
--
---------------------------------------------------------------------------

-----------------------------------------------------------------------
--
-- SFCGAL schema
--
-----------------------------------------------------------------------

DROP SCHEMA IF EXISTS sfcgal CASCADE;
CREATE SCHEMA sfcgal;

CREATE FUNCTION sfcgal.ST_GeomFromText(text)
    RETURNS geometry
    AS 'MODULE_PATHNAME', 'sfcgal_from_text'
    LANGUAGE C IMMUTABLE STRICT;

CREATE OR REPLACE FUNCTION sfcgal.ST_MakeSolid(geometry)
	RETURNS geometry
	AS 'MODULE_PATHNAME','sfcgal_make_solid'
	LANGUAGE 'c' IMMUTABLE STRICT;

CREATE OR REPLACE FUNCTION sfcgal._ST_Intersects(geom1 geometry, geom2 geometry)
	RETURNS boolean
	AS 'MODULE_PATHNAME','sfcgal_intersects'
	LANGUAGE 'c' IMMUTABLE STRICT
	COST 100;

CREATE OR REPLACE FUNCTION sfcgal.ST_3DIntersects(geom1 geometry, geom2 geometry)
	RETURNS boolean
	AS 'MODULE_PATHNAME','sfcgal_intersects3D'
	LANGUAGE 'c' IMMUTABLE STRICT
	COST 100;

CREATE OR REPLACE FUNCTION sfcgal.ST_Intersection(geom1 geometry, geom2 geometry)
	RETURNS geometry
	AS 'MODULE_PATHNAME','sfcgal_intersection'
	LANGUAGE 'c' IMMUTABLE STRICT
	COST 100;

CREATE OR REPLACE FUNCTION sfcgal.ST_3DIntersection(geom1 geometry, geom2 geometry)
	RETURNS geometry
	AS 'MODULE_PATHNAME','sfcgal_intersection3D'
	LANGUAGE 'c' IMMUTABLE STRICT
	COST 100;

CREATE OR REPLACE FUNCTION sfcgal.ST_ConvexHull(geometry)
	RETURNS geometry
	AS 'MODULE_PATHNAME','sfcgal_convexhull'
	LANGUAGE 'c' IMMUTABLE STRICT
	COST 100;

CREATE OR REPLACE FUNCTION sfcgal.ST_3DConvexHull(geometry)
	RETURNS geometry
	AS 'MODULE_PATHNAME','sfcgal_convexhull3D'
	LANGUAGE 'c' IMMUTABLE STRICT
	COST 100;

CREATE OR REPLACE FUNCTION sfcgal.ST_DelaunayTriangles(geometry)
	RETURNS geometry
	AS 'MODULE_PATHNAME','sfcgal_triangulate2D'
	LANGUAGE 'c' IMMUTABLE STRICT
	COST 100;

CREATE OR REPLACE FUNCTION sfcgal.ST_3DDelaunayTriangles(geometry)
	RETURNS geometry
	AS 'MODULE_PATHNAME','sfcgal_triangulate'
	LANGUAGE 'c' IMMUTABLE STRICT
	COST 100;

CREATE OR REPLACE FUNCTION sfcgal.ST_Area(geometry)
	RETURNS FLOAT8
	AS 'MODULE_PATHNAME','sfcgal_area'
	LANGUAGE 'c' IMMUTABLE STRICT;

CREATE OR REPLACE FUNCTION sfcgal.ST_3DArea(geometry)
	RETURNS FLOAT8
	AS 'MODULE_PATHNAME','sfcgal_area3D'
	LANGUAGE 'c' IMMUTABLE STRICT;

CREATE OR REPLACE FUNCTION sfcgal.ST_HasPlane(geometry)
	RETURNS BOOL
	AS 'MODULE_PATHNAME','sfcgal_has_plane'
	LANGUAGE 'c' IMMUTABLE STRICT;

CREATE OR REPLACE FUNCTION sfcgal.ST_Extrude(geometry, float8, float8, float8)
	RETURNS geometry
	AS 'MODULE_PATHNAME','sfcgal_extrude'
	LANGUAGE 'c' IMMUTABLE STRICT;

CREATE OR REPLACE FUNCTION sfcgal.ST_ForceZUp(geometry)
	RETURNS geometry
	AS 'MODULE_PATHNAME','sfcgal_force_z_up'
	LANGUAGE 'c' IMMUTABLE STRICT;

CREATE OR REPLACE FUNCTION sfcgal.ST_PointingUp(geometry)
	RETURNS BOOL
	AS 'MODULE_PATHNAME','sfcgal_pointing_up'
	LANGUAGE 'c' IMMUTABLE STRICT;

-- CREATE OR REPLACE FUNCTION sfcgal.ST_CollectionExtract(geometry, int4)
-- 	RETURNS geometry
-- 	AS 'MODULE_PATHNAME','sfcgal_collection_extract'
-- 	LANGUAGE 'c' IMMUTABLE STRICT;

CREATE OR REPLACE FUNCTION sfcgal.ST_Copy(geometry)
	RETURNS geometry
	AS 'MODULE_PATHNAME','sfcgal_copy'
	LANGUAGE 'c' IMMUTABLE STRICT;

CREATE OR REPLACE FUNCTION sfcgal.ST_Minkowski(geometry, geometry)
        RETURNS geometry
        AS 'MODULE_PATHNAME','sfcgal_minkowski_sum'
        LANGUAGE 'c' IMMUTABLE STRICT;

CREATE OR REPLACE FUNCTION sfcgal.ST_Offset(geometry, float8, int4)
        RETURNS geometry
        AS 'MODULE_PATHNAME','sfcgal_offset_polygon'
        LANGUAGE 'c' IMMUTABLE STRICT;

CREATE OR REPLACE FUNCTION sfcgal.ST_StraightSkeleton(geometry)
        RETURNS geometry
        AS 'MODULE_PATHNAME','sfcgal_straight_skeleton'
        LANGUAGE 'c' IMMUTABLE STRICT;

CREATE OR REPLACE FUNCTION sfcgal.ST_Round(geometry, int4)
        RETURNS geometry
        AS 'MODULE_PATHNAME','sfcgal_round'
        LANGUAGE 'c' IMMUTABLE STRICT;

CREATE OR REPLACE FUNCTION sfcgal.ST_Distance(geometry, geometry)
	RETURNS FLOAT8
	AS 'MODULE_PATHNAME','sfcgal_distance'
	LANGUAGE 'c' IMMUTABLE STRICT;

CREATE OR REPLACE FUNCTION sfcgal.ST_3DDistance(geometry, geometry)
	RETURNS FLOAT8
	AS 'MODULE_PATHNAME','sfcgal_distance3D'
	LANGUAGE 'c' IMMUTABLE STRICT;

