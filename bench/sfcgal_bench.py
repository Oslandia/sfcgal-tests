#!/usr/bin/env python

import subprocess
import sys
import time
from optparse import OptionParser

class PgBench(object):

    def __init__( self, db_name, n_objs, n_pts, quiet ):
        self.db_name = db_name
        self.n_objs = n_objs
        self.n_pts = n_pts
        self.quiet = quiet

    def call_sql( self, query, out_name = '' ):
        fmt_query = query % { 'n_id' : self.n_objs, 'n_pts' : self.n_pts, 'out_name' : out_name }
        start_time = time.time()
        p = subprocess.Popen( ['psql', '-q', '-t', '-d', self.db_name], stdin = subprocess.PIPE, stdout = subprocess.PIPE, stderr = sys.stderr )
        result=p.communicate( fmt_query )
        duration = time.time() - start_time
        return [duration, result]

    def bench_queries( self, queries ):
        for (name, query) in queries.items():

            prepare_query = query[0]
            request = query[1]
            if not self.quiet:
                print "==== %s ====" % name

            self.call_sql( "drop table if exists sfcgal.geoms; create table sfcgal.geoms as " + prepare_query)
            geos = self.call_sql( 'set search_path=public;' + request, 'geos_' + name  )
            geos_result = float(geos[1][0][1:-2])
            if not self.quiet:
                print "GEOS:\t%.3fs result: %.2f" % (geos[0], geos_result)
            # use 'SET search_path' to override the default st_intersects
            sfcgal = self.call_sql( 'set search_path=sfcgal,public;' + request, 'sfcgal_' + name )
            sfcgal_result = float(sfcgal[1][0][1:-2])
            if not self.quiet:
                print "SFCGAL:\t%.3fs result: %.2f" % (sfcgal[0], sfcgal_result)
            if self.quiet:
                print "%d;%.3f;%.2f;%.3f;%.2f" % (self.n_pts, geos[0], geos_result, sfcgal[0], sfcgal_result)

# generate a star-shaped polygon based on random points around a circle
prepare_query="""
-- generate a random linestring ($1: # of points, $2: max radius)
drop function if exists sfcgal.gen_linestring(int, float);
create or replace function sfcgal.gen_linestring( N int, maxr float) returns geometry as $$
  select st_makeline(array(
    select st_makepoint(r*cos(alpha), r*sin(alpha))
    from (
-- cut the circle into equal pieces and take a random point on each radius
      select (f-1)*(2*pi()/$1) as alpha, random()*($2/2) + $2/2 as r
      from generate_series(1,$1) as f
      order by alpha asc
      )
    as t
    )
  ) as line
$$
language SQL;

-- generate a random 3D linestring ($1: # of points, $2: max radius)
drop function if exists sfcgal.gen_linestring3D(int, float);
create or replace function sfcgal.gen_linestring3D( N int, maxr float) returns geometry as $$
  select st_makeline(array(
    select st_makepoint(r*cos(alpha), r*sin(alpha), r*sin(alpha))
    from (
-- cut the circle into equal pieces and take a random point on each radius
      select (f-1)*(2*pi()/$1) as alpha, random()*($2/2) + $2/2 as r
      from generate_series(1,$1) as f
      order by alpha asc
      )
    as t
    )
  ) as line
$$
language SQL;

-- generate a random polygon ($1: # of points, $2: max radius)
drop function if exists sfcgal.gen_poly1(int, float);
create or replace function sfcgal.gen_poly1( N int, maxr float) returns geometry as $$
select st_makepolygon( st_addpoint(tline.line, st_startpoint(tline.line)))
from (
  select sfcgal.gen_linestring($1,$2) as line
) as tline
$$
language SQL;

-- generate a random polygon with a hole ($1: # of points, $2: max radius)
drop function if exists sfcgal.gen_poly_with_hole(int, float);
create or replace function sfcgal.gen_poly_with_hole( N int, maxr float) returns geometry as $$
with gen_poly as (select sfcgal.gen_poly1( $1, $2 - $2/2) as geom),
gen_ls as (select st_exteriorring(geom) as geom from gen_poly)
select st_makepolygon( st_scale(geom, 2, 2), array[geom] ) from gen_ls
$$
language SQL;

-- generate a random triangle ($1: max radius)
-- drop function if exists sfcgal.gen_triangle(float);
-- create or replace function sfcgal.gen_triangle(maxr float) returns geometry as $$
-- with gen_poly as (select sfcgal.gen_poly1( 3, $1 ) as geom)
-- select 
-- $$
-- language SQL;

-- generate a random multipoint ($1: number of points)
drop function if exists sfcgal.gen_mpoints(int);
create or replace function sfcgal.gen_mpoints(N int) returns geometry as $$
select st_collect(array(select st_makepoint(random()*16-8, random()*16-8) from generate_series(1, $1) ))
$$
language SQL;

-- generate a random TIN ($1: # of triangles, $2: max radius)
drop function if exists sfcgal.gen_tin(int, float);
create or replace function sfcgal.gen_tin( N int, maxr float) returns geometry as $$
-- generate a polygon with hole and triangulate it
select sfcgal.st_delaunaytriangles( sfcgal.gen_poly_with_hole($1,$2) )
$$
language SQL;

-- generate a random solid (extrusion from a polygon) ($1: # of triangles, $2: max radius)
drop function if exists sfcgal.gen_solid(int, float);
create or replace function sfcgal.gen_solid( N int, maxr float) returns geometry as $$
-- generate a polygon and extrude it
select sfcgal.st_extrude( st_force_3d(sfcgal.gen_poly1($1,$2)), 0.0, 0.0, $2 )
$$
language SQL;
"""

create_poly_poly = """
select id, st_translate(sfcgal.gen_poly1(%(n_pts)d, 10), random()*16-8, random()*16-8) as geom1,
  st_translate(sfcgal.gen_poly1(%(n_pts)d, 10), random()*16-8, random()*16-8) as geom2
from generate_series(1, %(n_id)d) as id;
"""

create_ls_ls = """
select
  id,
  st_translate(sfcgal.gen_linestring(%(n_pts)d, 10), random()*16-8, random()*16-8) as geom1,
  st_translate(sfcgal.gen_linestring(%(n_pts)d, 10), random()*16-8, random()*16-8) as geom2
from generate_series(1, %(n_id)d) as id;
"""

create_ls_ls_3D = """
select
  id,
  st_translate(sfcgal.gen_linestring3D(%(n_pts)d, 10), random()*16-8, random()*16-8) as geom1,
  st_translate(sfcgal.gen_linestring3D(%(n_pts)d, 10), random()*16-8, random()*16-8) as geom2
from generate_series(1, %(n_id)d) as id;
"""

create_ls_poly_h = """
select
  id,
  st_translate(sfcgal.gen_linestring(%(n_pts)d, 10), random()*16-8, random()*16-8) as geom1,
  st_translate(sfcgal.gen_poly_with_hole(%(n_pts)d, 10), random()*16-8, random()*16-8) as geom2
from generate_series(1, %(n_id)d) as id;
"""

create_ls_tin = """
select
  id,
  st_translate(sfcgal.gen_linestring(%(n_pts)d, 10), random()*16-8, random()*16-8) as geom1,
  st_translate(sfcgal.gen_tin(%(n_pts)d, 10), random()*16-8, random()*16-8) as geom2
from generate_series(1, %(n_id)d) as id;
"""

create_poly_h_poly_h = """
select id, st_translate(sfcgal.gen_poly_with_hole(%(n_pts)d, 10), random()*16-8, random()*16-8) as geom1,
  st_translate(sfcgal.gen_poly_with_hole(%(n_pts)d, 10), random()*16-8, random()*16-8) as geom2
from generate_series(1, %(n_id)d) as id;
"""

create_point_poly = """
select
id,
st_makepoint(random()*16-8, random()*16-8) as geom1,
st_translate(sfcgal.gen_poly1(%(n_pts)d, 10), random()*16-8, random()*16-8) as geom2
from generate_series(1, %(n_id)d) as id;
"""

create_multipoints = """
select
id,
sfcgal.gen_mpoints(%(n_pts)d) as geom1
from generate_series(1, %(n_id)d) as id;
"""

intersects_query="""
select sum(case when
_st_intersects( geom1, geom2 )
then 1 else 0 end)
from sfcgal.geoms;
"""

intersects3D_query="""
select sum(case when
st_3Dintersects( geom1, geom2 )
then 1 else 0 end)
from sfcgal.geoms;
"""

intersection_query="""
select sum(st_npoints(st_intersection( geom1, geom2 ))) from sfcgal.geoms;
"""

area_poly_query="""
select sum(ST_area(geom1)) from sfcgal.geoms;
"""

convexhull_query="""
-- uncomment this if you want to store the result
--drop table if exists sfcgal.%(out_name)s;
--create table sfcgal.%(out_name)s as
--select id, st_convexhull(geom) as geom from sfcgal.points;
--select sum(st_npoints(geom)) from sfcgal.%(out_name)s;

select sum(st_npoints(st_convexhull(geom1))) from sfcgal.geoms;
"""

triangulate_query="""
select sum(st_numgeometries(st_delaunaytriangles(geom1))) from sfcgal.geoms;
"""

cleaning_query="""
-- drop table sfcgal.geoms;
"""

queries = {}
queries['intersects_point_polygon'] = [ create_point_poly, intersects_query ]
queries['intersects_polygon_polygon'] = [ create_poly_poly, intersects_query ]
queries['intersects_ls_ls'] = [ create_ls_ls, intersects_query ]
queries['intersects_ls_poly_h'] = [ create_ls_poly_h, intersects_query]
queries['intersects3D_ls_ls'] = [ create_ls_ls_3D, intersects3D_query]

queries['intersection_polygon_polygon'] = [ create_poly_poly, intersection_query ]
queries['intersection_poly_poly_h'] = [ create_poly_h_poly_h, intersection_query]
queries['intersection_ls_ls'] = [ create_ls_ls, intersection_query ]
queries['intersection_ls_poly_h'] = [ create_ls_poly_h, intersection_query]
queries['intersection_ls_tin'] = [ create_ls_tin, intersection_query]

queries['area_polygon'] = [ create_poly_poly, area_poly_query ]

queries['convexhull_multipoint'] = [create_multipoints, convexhull_query]

queries['triangulate_poly'] = [ create_poly_poly, triangulate_query]

parser = OptionParser()
parser.add_option("-d", "--db", dest="db_name", default="eplu_test",
                  help="Name of the database" )

parser.add_option("-n", "--nobjs",
                  dest="n_objs", default=1000, type="int",
                  help="Number of objects to generate" )

parser.add_option("-p", "--npts",
                  action="append", dest="n_pts", type="int",
                  help="Number of points for each object" )

parser.add_option("-q", "--quiet",
                  action="store_true", dest="quiet", default=False,
                  help="Quiet mode" )

parser.add_option("-x", 
                  action="append", dest="selected",
                  help="Add a query to execute" )

parser.add_option("-l", "--list-queries",
                  action="store_true", dest="list_queries", default=False,
                  help="List existing queries" )

(options, args) = parser.parse_args()

if options.list_queries:
    print "Available queries:"
    for q in queries.keys():
        print q
    exit(0)

if options.selected is None:
    print "No query selected, aborting"

    print "Available queries:"
    for q in queries.keys():
        print q
    exit(1)

if options.n_pts is None:
    options.n_pts = [10]

for n_pts in options.n_pts:
    bench = PgBench( options.db_name, options.n_objs, n_pts, options.quiet )

    if not options.quiet:
        print "dbname: ", options.db_name, " n_objs: ", options.n_objs, " n_pts: ", n_pts
        print "Preparing ..."

    bench.call_sql( prepare_query )

    selqueries = {}
    for sel in options.selected:
        if not sel in queries:
            print "%s query does not exist" % sel
        else:
            selqueries[sel] = queries[sel]

    bench.bench_queries( selqueries )

    if not options.quiet:
        print "Cleaning ..."
    bench.call_sql( cleaning_query )

