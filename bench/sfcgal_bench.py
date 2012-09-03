#!/usr/bin/env python

import subprocess
import sys
import time
from optparse import OptionParser

class PgBench(object):

    def __init__( self, db_name, n_objs, n_pts ):
        self.db_name = db_name
        self.n_objs = n_objs
        self.n_pts = n_pts

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
            print "==== %s ====" % name

            self.call_sql( "drop table if exists sfcgal.geoms; create table sfcgal.geoms as " + prepare_query)

            # geos id = 0
            geos = self.call_sql( 'set search_path=public;' + request, 'geos_' + name  )

            geos_result = float(geos[1][0][1:-2])
            print "GEOS:\t%.3fs result: %.2f" % (geos[0], geos_result)
            # use 'SET search_path' to override the default st_intersects
            sfcgal = self.call_sql( 'set search_path=sfcgal,public;' + request, 'sfcgal_' + name )
            sfcgal_result = float(sfcgal[1][0][1:-2])
            print "SFCGAL:\t%.3fs result: %.2f" % (sfcgal[0], sfcgal_result)


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
select sfcgal.st_triangulate( sfcgal.gen_poly_with_hole($1,$2) )
$$
language SQL;
"""

create_poly_poly = """
select id, st_translate(sfcgal.gen_poly1(%(n_pts)d, 10), random()*16-8, random()*16-8) as geom1,
  st_translate(sfcgal.gen_poly1(%(n_pts)d, 10), random()*16-8, random()*16-8) as geom2
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

cleaning_query="""
-- drop table sfcgal.geoms;
"""

parser = OptionParser()
parser.add_option("-d", "--db", dest="db_name", default="eplu_test",
                  help="Name of the database" )
parser.add_option("-n", "--nobjs",
                  dest="n_objs", default=1000, type="int",
                  help="Number of objects to generate" )

parser.add_option("-p", "--npts",
                  dest="n_pts", default=10, type="int",
                  help="Number of points for each object" )

(options, args) = parser.parse_args()

bench = PgBench( options.db_name, options.n_objs, options.n_pts )

print "dbname: ", options.db_name, " n_objs: ", options.n_objs, " n_pts: ", options.n_pts
print "Preparing ..."
bench.call_sql( prepare_query )

#queries = { 'intersects_point_polygon': [ create_point_poly, intersects_query ],
#            'intersects_polygon_polygon': [ create_poly_poly, intersects_query ],
#            'intersection_polygon_polygon': [ create_poly_poly, intersection_query ],
#            'area_polygon' : [ create_poly_poly, area_poly_query ] }

#queries = { 'convexhull_multipoint': [create_multipoints, convexhull_query] }
#queries = { 'intersection_poly_poly_h': [ create_poly_h_poly_h, intersection_query] }
#queries = { 'intersects_poly_poly': [ create_poly_poly, intersects_query] }
queries = { 'intersection_ls_poly_h': [ create_ls_poly_h, intersection_query] }
#queries = { 'intersection_ls_tin': [ create_ls_tin, intersection_query] }

# vary the number of points
bench.bench_queries( queries )

print "Cleaning ..."
bench.call_sql( cleaning_query )

