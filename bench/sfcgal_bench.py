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

    def call_sql( self, query ):
        fmt_query = query % { 'n_id' : self.n_objs, 'n_pts' : self.n_pts }
        start_time = time.time()
        p = subprocess.Popen( ['psql', '-q', '-t', '-d', self.db_name], stdin = subprocess.PIPE, stdout = subprocess.PIPE, stderr = sys.stderr )
        result=p.communicate( fmt_query )
        duration = time.time() - start_time
        return [duration, result]

    def bench_queries( self, queries ):
        for (name, query) in queries.items():

            print "==== %s ====" % name

            geos = self.call_sql( 'set search_path=public;' + query )
            geos_result = float(geos[1][0][1:-2])
            print "GEOS:\t%.3fs result: %.2f" % (geos[0], geos_result)
            # use 'SET search_path' to override the default st_intersects
            sfcgal = self.call_sql( 'set search_path=sfcgal,public;' + query )
            sfcgal_result = float(sfcgal[1][0][1:-2])
            print "SFCGAL:\t%.3fs result: %.2f" % (sfcgal[0], sfcgal_result)


# generate a star-shaped polygon based on random points around a circle
# (int, double) : (# of points, max radius)
prepare_query="""
drop function if exists sfcgal.gen_poly1(int, float);
create or replace function sfcgal.gen_poly1( N int, maxr float) returns geometry as $$
select st_makepolygon(st_addpoint(tline.line, st_startpoint(tline.line)))
from (
  select st_makeline(array(
    select st_makepoint(r*cos(alpha), r*sin(alpha))
    from (
      select (f-1)*(2*pi()/$1) as alpha, random()*($2/2) + $2/2 as r
      from generate_series(1,$1) as f
      order by alpha asc
      )
    as t
    )
  ) as line
) as tline
$$
language SQL;

drop table if exists sfcgal.polys;
create table sfcgal.polys as
select id, st_translate(sfcgal.gen_poly1(%(n_pts)d, 10), random()*16-8, random()*16-8) as poly1,
  st_translate(sfcgal.gen_poly1(%(n_pts)d, 10), random()*16-8, random()*16-8) as poly2
from generate_series(1, %(n_id)d) as id;

drop table if exists sfcgal.point_polys;
create table sfcgal.point_polys as
select
st_makepoint(random()*16-8, random()*16-8) as point,
st_translate(sfcgal.gen_poly1(%(n_pts)d, 10), random()*16-8, random()*16-8) as poly
from generate_series(1, %(n_id)d);
"""

intersects_poly_poly_query="""
select sum(case when
_st_intersects( poly1, poly2 )
then 1 else 0 end)
from sfcgal.polys;
"""

intersection_poly_poly_query="""
select sum(st_npoints(sfcgal.st_intersection( poly1, poly2 ))) from sfcgal.polys;
"""

intersects_pt_poly_query="""
select sum(case when
_st_intersects(point, poly) then 1 else 0 end)
from sfcgal.point_polys;
"""

area_poly_query="""
select sum(ST_area(poly1)) from sfcgal.polys;
"""

cleaning_query="""
drop function sfcgal.gen_poly1(int, float);
drop table sfcgal.point_polys;
drop table sfcgal.polys;
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

queries = { 'intersects(point, polygon)': intersects_pt_poly_query,
            'intersects(polygon, polygon)': intersects_poly_poly_query,
            'intersection(polygon, polygon)': intersection_poly_poly_query,
            'area(polygon)' : area_poly_query }

#queries = { 'intersection(polygon, polygon)': intersection_poly_poly_query }

# vary the number of points
bench.bench_queries( queries )

print "Cleaning ..."
bench.call_sql( cleaning_query )

