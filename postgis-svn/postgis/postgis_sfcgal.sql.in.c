---------------------------------------------------------------------------
--
-- PostGIS - Exact geometry types for PostgreSQL
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
	AS 'MODULE_PATHNAME','sfcgal_hasplane'
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

CREATE OR REPLACE FUNCTION sfcgal.ST_CollectionExtract(geometry, int4)
	RETURNS geometry
	AS 'MODULE_PATHNAME','sfcgal_collection_extract'
	LANGUAGE 'c' IMMUTABLE STRICT;

CREATE OR REPLACE FUNCTION sfcgal.ST_Copy(geometry)
	RETURNS geometry
	AS 'MODULE_PATHNAME','sfcgal_copy'
	LANGUAGE 'c' IMMUTABLE STRICT;

CREATE OR REPLACE FUNCTION sfcgal.ST_Buffer(geometry, float8, int4)
        RETURNS geometry
        AS 'MODULE_PATHNAME','sfcgal_buffer'
        LANGUAGE 'c' IMMUTABLE STRICT;

-----------------------------------------------------------------------
--
-- SFCGAL Referenced exact geometries
--
-----------------------------------------------------------------------

CREATE TYPE ref_geometry;

-- convert a wkt to a ref_geometry
CREATE FUNCTION sfcgal.ref_in(cstring)
    RETURNS ref_geometry
    AS 'MODULE_PATHNAME', 'sfcgal_ref_in'
    LANGUAGE C IMMUTABLE STRICT;

-- display a ref_geometry
CREATE FUNCTION sfcgal.ref_out(ref_geometry)
    RETURNS cstring
    AS 'MODULE_PATHNAME', 'sfcgal_ref_out'
    LANGUAGE C IMMUTABLE STRICT;

CREATE TYPE ref_geometry (
    internallength = 8,
    input = sfcgal.ref_in,
    output = sfcgal.ref_out
);

-- geometry to ref_geometry
CREATE FUNCTION sfcgal.ST_RefGeometry( geometry )
    RETURNS ref_geometry
    AS 'MODULE_PATHNAME', 'sfcgal_ref_from_geom'
    LANGUAGE C IMMUTABLE STRICT;

-- exact_geometry to geometry
CREATE FUNCTION sfcgal.ST_Geometry( ref_geometry )
    RETURNS geometry
    AS 'MODULE_PATHNAME', 'sfcgal_geom_from_ref'
    LANGUAGE C IMMUTABLE STRICT;

CREATE FUNCTION sfcgal.ST_RefGeomFromText(text)
    RETURNS ref_geometry
    AS 'MODULE_PATHNAME', 'sfcgal_ref_from_text'
    LANGUAGE C IMMUTABLE STRICT;


CREATE OR REPLACE FUNCTION sfcgal.ST_MakeSolid(ref_geometry)
	RETURNS ref_geometry
	AS 'MODULE_PATHNAME','sfcgal_ref_make_solid'
	LANGUAGE 'c' IMMUTABLE STRICT;

CREATE OR REPLACE FUNCTION sfcgal.ST_Intersects(geom1 ref_geometry, geom2 ref_geometry)
	RETURNS boolean
	AS 'MODULE_PATHNAME','sfcgal_ref_intersects'
	LANGUAGE 'c' IMMUTABLE STRICT
	COST 100;

CREATE OR REPLACE FUNCTION sfcgal.ST_3DIntersects(geom1 ref_geometry, geom2 ref_geometry)
	RETURNS boolean
	AS 'MODULE_PATHNAME','sfcgal_ref_intersects3D'
	LANGUAGE 'c' IMMUTABLE STRICT
	COST 100;

CREATE OR REPLACE FUNCTION sfcgal.ST_Intersection(geom1 ref_geometry, geom2 ref_geometry)
	RETURNS ref_geometry
	AS 'MODULE_PATHNAME','sfcgal_ref_intersection'
	LANGUAGE 'c' IMMUTABLE STRICT
	COST 100;

CREATE OR REPLACE FUNCTION sfcgal.ST_3DIntersection(geom1 ref_geometry, geom2 ref_geometry)
	RETURNS ref_geometry
	AS 'MODULE_PATHNAME','sfcgal_ref_intersection3D'
	LANGUAGE 'c' IMMUTABLE STRICT
	COST 100;

CREATE OR REPLACE FUNCTION sfcgal.ST_ConvexHull(ref_geometry)
	RETURNS ref_geometry
	AS 'MODULE_PATHNAME','sfcgal_ref_convexhull'
	LANGUAGE 'c' IMMUTABLE STRICT
	COST 100;

CREATE OR REPLACE FUNCTION sfcgal.ST_3DConvexHull(ref_geometry)
	RETURNS ref_geometry
	AS 'MODULE_PATHNAME','sfcgal_ref_convexhull3D'
	LANGUAGE 'c' IMMUTABLE STRICT
	COST 100;

CREATE OR REPLACE FUNCTION sfcgal.ST_DelaunayTriangles(ref_geometry)
	RETURNS ref_geometry
	AS 'MODULE_PATHNAME','sfcgal_ref_triangulate2D'
	LANGUAGE 'c' IMMUTABLE STRICT
	COST 100;

CREATE OR REPLACE FUNCTION sfcgal.ST_3DDelaunayTriangles(ref_geometry)
	RETURNS ref_geometry
	AS 'MODULE_PATHNAME','sfcgal_ref_triangulate'
	LANGUAGE 'c' IMMUTABLE STRICT
	COST 100;

CREATE OR REPLACE FUNCTION sfcgal.ST_Area(ref_geometry)
	RETURNS FLOAT8
	AS 'MODULE_PATHNAME','sfcgal_ref_area'
	LANGUAGE 'c' IMMUTABLE STRICT;

CREATE OR REPLACE FUNCTION sfcgal.ST_3DArea(ref_geometry)
	RETURNS FLOAT8
	AS 'MODULE_PATHNAME','sfcgal_ref_area3D'
	LANGUAGE 'c' IMMUTABLE STRICT;

CREATE OR REPLACE FUNCTION sfcgal.ST_HasPlane(ref_geometry)
	RETURNS BOOL
	AS 'MODULE_PATHNAME','sfcgal_ref_hasplane'
	LANGUAGE 'c' IMMUTABLE STRICT;

CREATE OR REPLACE FUNCTION sfcgal.ST_Extrude(ref_geometry, float8, float8, float8)
	RETURNS ref_geometry
	AS 'MODULE_PATHNAME','sfcgal_ref_extrude'
	LANGUAGE 'c' IMMUTABLE STRICT;

CREATE OR REPLACE FUNCTION sfcgal.ST_ForceZUp(ref_geometry)
	RETURNS ref_geometry
	AS 'MODULE_PATHNAME','sfcgal_ref_force_z_up'
	LANGUAGE 'c' IMMUTABLE STRICT;

CREATE OR REPLACE FUNCTION sfcgal.ST_PointingUp(ref_geometry)
	RETURNS BOOL
	AS 'MODULE_PATHNAME','sfcgal_ref_pointing_up'
	LANGUAGE 'c' IMMUTABLE STRICT;

CREATE OR REPLACE FUNCTION sfcgal.ST_Copy(ref_geometry)
	RETURNS ref_geometry
	AS 'MODULE_PATHNAME','sfcgal_ref_copy'
	LANGUAGE 'c' IMMUTABLE STRICT;

CREATE OR REPLACE FUNCTION sfcgal.ST_Buffer(ref_geometry, float8, int4)
        RETURNS ref_geometry
        AS 'MODULE_PATHNAME','sfcgal_ref_buffer'
        LANGUAGE 'c' IMMUTABLE STRICT;

-----------------------------------------------------------------------
--
-- SFCGAL Serialized exact geometries
--
-----------------------------------------------------------------------

CREATE TYPE exact_geometry;

-- convert a wkt to an exact geometry
CREATE FUNCTION sfcgal.exact_in(cstring)
    RETURNS exact_geometry
    AS 'MODULE_PATHNAME', 'sfcgal_exact_in'
    LANGUAGE C IMMUTABLE STRICT;

-- display an exact_geometry
CREATE FUNCTION sfcgal.exact_out(exact_geometry)
    RETURNS cstring
    AS 'MODULE_PATHNAME', 'sfcgal_exact_out'
    LANGUAGE C IMMUTABLE STRICT;

CREATE TYPE exact_geometry (
    internallength = VARIABLE,
    input = sfcgal.exact_in,
    output = sfcgal.exact_out
);

-- geometry to exact_geometry
CREATE FUNCTION sfcgal.ST_ExactGeometry( geometry )
    RETURNS exact_geometry
    AS 'MODULE_PATHNAME', 'sfcgal_exact_from_geom'
    LANGUAGE C IMMUTABLE STRICT;

-- exact_geometry to geometry
CREATE FUNCTION sfcgal.ST_Geometry( exact_geometry )
    RETURNS geometry
    AS 'MODULE_PATHNAME', 'sfcgal_geom_from_exact'
    LANGUAGE C IMMUTABLE STRICT;

CREATE FUNCTION sfcgal.ST_ExactGeomFromText(text)
    RETURNS exact_geometry
    AS 'MODULE_PATHNAME', 'sfcgal_exact_from_text'
    LANGUAGE C IMMUTABLE STRICT;

CREATE OR REPLACE FUNCTION sfcgal.ST_MakeSolid(exact_geometry)
	RETURNS exact_geometry
	AS 'MODULE_PATHNAME','sfcgal_exact_make_solid'
	LANGUAGE 'c' IMMUTABLE STRICT;

CREATE OR REPLACE FUNCTION sfcgal.ST_Intersects(geom1 exact_geometry, geom2 exact_geometry)
	RETURNS boolean
	AS 'MODULE_PATHNAME','sfcgal_exact_intersects'
	LANGUAGE 'c' IMMUTABLE STRICT
	COST 100;

CREATE OR REPLACE FUNCTION sfcgal.ST_3DIntersects(geom1 exact_geometry, geom2 exact_geometry)
	RETURNS boolean
	AS 'MODULE_PATHNAME','sfcgal_exact_intersects3D'
	LANGUAGE 'c' IMMUTABLE STRICT
	COST 100;

CREATE OR REPLACE FUNCTION sfcgal.ST_Intersection(geom1 exact_geometry, geom2 exact_geometry)
	RETURNS exact_geometry
	AS 'MODULE_PATHNAME','sfcgal_exact_intersection'
	LANGUAGE 'c' IMMUTABLE STRICT
	COST 100;

CREATE OR REPLACE FUNCTION sfcgal.ST_3DIntersection(geom1 exact_geometry, geom2 exact_geometry)
	RETURNS exact_geometry
	AS 'MODULE_PATHNAME','sfcgal_exact_intersection3D'
	LANGUAGE 'c' IMMUTABLE STRICT
	COST 100;

CREATE OR REPLACE FUNCTION sfcgal.ST_ConvexHull(exact_geometry)
	RETURNS exact_geometry
	AS 'MODULE_PATHNAME','sfcgal_exact_convexhull'
	LANGUAGE 'c' IMMUTABLE STRICT
	COST 100;

CREATE OR REPLACE FUNCTION sfcgal.ST_3DConvexHull(exact_geometry)
	RETURNS exact_geometry
	AS 'MODULE_PATHNAME','sfcgal_exact_convexhull3D'
	LANGUAGE 'c' IMMUTABLE STRICT
	COST 100;

CREATE OR REPLACE FUNCTION sfcgal.ST_DelaunayTriangles(exact_geometry)
	RETURNS exact_geometry
	AS 'MODULE_PATHNAME','sfcgal_exact_triangulate2D'
	LANGUAGE 'c' IMMUTABLE STRICT
	COST 100;

CREATE OR REPLACE FUNCTION sfcgal.ST_3DDelaunayTriangles(exact_geometry)
	RETURNS exact_geometry
	AS 'MODULE_PATHNAME','sfcgal_exact_triangulate'
	LANGUAGE 'c' IMMUTABLE STRICT
	COST 100;

CREATE OR REPLACE FUNCTION sfcgal.ST_Area(exact_geometry)
	RETURNS FLOAT8
	AS 'MODULE_PATHNAME','sfcgal_exact_area'
	LANGUAGE 'c' IMMUTABLE STRICT;

CREATE OR REPLACE FUNCTION sfcgal.ST_3DArea(exact_geometry)
	RETURNS FLOAT8
	AS 'MODULE_PATHNAME','sfcgal_exact_area3D'
	LANGUAGE 'c' IMMUTABLE STRICT;

CREATE OR REPLACE FUNCTION sfcgal.ST_HasPlane(exact_geometry)
	RETURNS BOOL
	AS 'MODULE_PATHNAME','sfcgal_exact_hasplane'
	LANGUAGE 'c' IMMUTABLE STRICT;

CREATE OR REPLACE FUNCTION sfcgal.ST_Extrude(exact_geometry, float8, float8, float8)
	RETURNS exact_geometry
	AS 'MODULE_PATHNAME','sfcgal_exact_extrude'
	LANGUAGE 'c' IMMUTABLE STRICT;

CREATE OR REPLACE FUNCTION sfcgal.ST_ForceZUp(exact_geometry)
	RETURNS exact_geometry
	AS 'MODULE_PATHNAME','sfcgal_exact_force_z_up'
	LANGUAGE 'c' IMMUTABLE STRICT;

CREATE OR REPLACE FUNCTION sfcgal.ST_PointingUp(exact_geometry)
	RETURNS BOOL
	AS 'MODULE_PATHNAME','sfcgal_exact_pointing_up'
	LANGUAGE 'c' IMMUTABLE STRICT;

CREATE OR REPLACE FUNCTION sfcgal.ST_Copy(exact_geometry)
	RETURNS exact_geometry
	AS 'MODULE_PATHNAME','sfcgal_exact_copy'
	LANGUAGE 'c' IMMUTABLE STRICT;

CREATE OR REPLACE FUNCTION sfcgal.ST_Buffer(exact_geometry, float8, int4)
        RETURNS exact_geometry
        AS 'MODULE_PATHNAME','sfcgal_exact_buffer'
        LANGUAGE 'c' IMMUTABLE STRICT;
