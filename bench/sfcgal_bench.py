#!/usr/bin/env python

import matplotlib.pyplot as plt
# for multipage pdfs
from matplotlib.backends.backend_pdf import PdfPages
import subprocess
import sys
import time
import os
from optparse import OptionParser

class PgBench(object):

    def __init__( self, db_name, n_objs, n_pts, quiet ):
        self.db_name = db_name
        self.n_objs = n_objs
        self.n_pts = n_pts
        self.quiet = quiet

    def call_sql( self, query, measure_mem = False ):
        fmt_query = query % { 'n_id' : self.n_objs, 'n_pts' : self.n_pts }
        start_time = time.time()
        p = subprocess.Popen( ['psql', '-q', '-t', '-d', self.db_name ], stdin = subprocess.PIPE, stdout = subprocess.PIPE, stderr = sys.stderr )
        if measure_mem:
            p.stdin.write( "select pg_backend_pid();\n" )
            serverpid = int(p.stdout.readline())
            p.stdout.readline()

        p.stdin.write( fmt_query )
        p.stdin.close()
        maxmem = 0
        while measure_mem and p.poll() is None:
            m = os.popen( "ps -p %d -o rss=" % serverpid ).read()
            if len(m) > 0 and int(m) > maxmem:
                maxmem = int(m)
                time.sleep(0.1)

        result = p.stdout.readline()
        duration = time.time() - start_time
        return [duration, maxmem, result]

    def prepare_query( self, query ):
        self.call_sql( "drop table if exists sfcgal.geoms; create table sfcgal.geoms as " + query)

    def bench_serialization( self, query ):

        cpu = []
        mem = []
        for q in query:
            r = self.call_sql( q, True )
            cpu.append( r[0] )
            mem.append( r[1] )
        print cpu, mem
        return [ cpu, mem ]

    def bench_operation( self, name, query ):
        if not self.quiet:
            print "==== %s ====" % name
            
        geos = self.call_sql( 'set search_path=public;' + query, 'geos_' + name  )
        geos_result = float(geos[2])
        if not self.quiet:
            print "GEOS:\t%.3fs result: %.2f" % (geos[0], geos_result)
                
        # use 'SET search_path' to override the default st_intersects
        sfcgal = self.call_sql( 'set search_path=sfcgal,public;' + query, 'sfcgal_' + name )
        sfcgal_result = float(sfcgal[2])
        
        if not self.quiet:
            print "SFCGAL:\t%.3fs result: %.2f" % (sfcgal[0], sfcgal_result)
        if self.quiet:
            print "%d;%.3f;%.2f;%.3f;%.2f" % (self.n_pts, geos[0], geos_result, sfcgal[0], sfcgal_result)

        return [ geos[0], sfcgal[0] ]

    def bench_queries( self, queries ):
        results = {}
        rtype = 'o'
        for (name, query) in queries.items():

            self.prepare_query( query[0] )

            if isinstance( query[1], list ):
                # it is an array, special query
                rtype = 's'
                results[ name ] = self.bench_serialization( query[1] )
            else:
                rtype = 'o'
                results[ name ] = self.bench_operation( name, query[1] )

        return rtype, results

# generate a star-shaped polygon based on random points around a circle
prepare_query="""
-- generate a random linestring ($1: # of points, $2: max radius)
drop function if exists sfcgal.gen_linestring(int, float);
create or replace function sfcgal.gen_linestring( N int, maxr float) returns geometry as $$
  select st_makeline(array(
    select st_makepoint(round(r*cos(alpha) * (1<<24))/(1<<24), round(r*sin(alpha)*(1<<24))/(1<<24))
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
    select st_makepoint(round(r*cos(alpha)*(1<<24))/(1<<24),
                        round(r*sin(alpha)*(1<<24))/(1<<24),
                        round(r*sin(alpha)*(1<<24))/(1<<24))
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
select sum(st_npoints(st_convexhull(geom1))) from sfcgal.geoms;
"""

triangulate_query="""
select sum(st_numgeometries(st_delaunaytriangles(geom1))) from sfcgal.geoms;
"""

buffer_query="""
select st_area( st_buffer( geom1, 2, 4 ) ) from sfcgal.geoms;
"""

cleaning_query="""
-- drop table sfcgal.geoms;
"""

serialization1_query="""
select sum( st_npoints(
  sfcgal.st_copy( sfcgal.st_copy( sfcgal.st_copy( sfcgal.st_copy( geom1 )))))) from sfcgal.geoms;
"""

serialization2_query="""
select sum( st_npoints(sfcgal.st_geometry(
  sfcgal.st_copy( sfcgal.st_copy( sfcgal.st_copy( sfcgal.st_copy( sfcgal.st_exactgeometry(geom1) ))))))) from sfcgal.geoms;
"""

serialization3_query="""
select sum( st_npoints(sfcgal.st_geometry(
  sfcgal.st_copy( sfcgal.st_copy( sfcgal.st_copy( sfcgal.st_copy( sfcgal.st_refgeometry(geom1) ))))))) from sfcgal.geoms;
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
queries['buffer_poly'] = [ create_poly_poly, buffer_query]

# special query: chaining of serialization / unserialization
queries['serialization'] = [ create_poly_poly, [ serialization1_query,
                                                 serialization2_query,
                                                 serialization3_query ] ]

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

parser.add_option("-r", "--report-file",
                  dest="report_file", default='', type="string",
                  help="Generate a report file" )

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

results = {}
selqueries = {}
for sel in options.selected:
    if not sel in queries:
        print "%s query does not exist" % sel
    else:
        selqueries[sel] = queries[sel]
        results[sel] = [ [], [] ]

for n_pts in options.n_pts:
    bench = PgBench( options.db_name, options.n_objs, n_pts, options.quiet )

    if not options.quiet:
        print "dbname: ", options.db_name, " n_objs: ", options.n_objs, " n_pts: ", n_pts
        print "Preparing ..."

    bench.call_sql( prepare_query )
    if not options.quiet:
        print "Ok"

    ltype, lresults = bench.bench_queries( selqueries )

    # extract results
    for query, r in lresults.items():
        geos = r[0]
        sfcgal = r[1]
        results[query][0].append(geos)
        results[query][1].append(sfcgal)

    if not options.quiet:
        print "Cleaning ..."
    bench.call_sql( cleaning_query )


if options.report_file:
    print "Generating report %s" % options.report_file
    pdf = PdfPages( options.report_file )
    for q, v in results.items():
        if ltype == 'o':
            X = options.n_pts
            plt.clf()
            plt.xlabel( "# of points" )
            plt.ylabel( "Time (s)" )
            
            plt.title( q + ", %d geoms" % options.n_objs )
            # GEOS
            plt.plot( X, v[0], marker='o', label='GEOS' )
            # SFCGAL
            plt.plot( X, v[1], marker='o', label='SFCGAL' )
            plt.legend(loc='upper left')
            pdf.savefig()

            # text report
            print "== %s == " % q
            print "# pts\t",';'.join( str(x) for x in X )
            print "GEOS:\t", ';'.join( str(x) for x in v[0] )
            print "SFCGAL:\t", ';'.join( str(x) for x in v[1] )
        elif ltype == 's':
            X = options.n_pts

            cpu = v[0]
            mem = v[1]
            cpuY = [[],[],[]]
            memY = [[],[],[]]
            for j in range(0,3):
                cpuY[j] = []
                memY[j] = []
                for i in range(0,len(X)):
                    cpuY[j].append(cpu[i][j])
                    memY[j].append(mem[i][j])

            plt.clf()
            plt.xlabel( "# of points" )
            plt.ylabel( "Time (s)" )
            
            plt.title( q + ", %d geoms" % options.n_objs )
            # native
            plt.plot( X, cpuY[0], marker='o', label='Native inexact' )
            # SFCGAL
            plt.plot( X, cpuY[1], marker='o', label='Serialized exact' )
            # Referenced
            plt.plot( X, cpuY[2], marker='o', label='Referenced exact' )
            plt.legend(loc='upper left')
            pdf.savefig()
            
            plt.clf()
            plt.xlabel( "# of points" )
            plt.ylabel( "kB" )
            
            plt.title( q + ", %d geoms, memory usage" % options.n_objs )
            # native
            plt.plot( X, memY[0], marker='o', label='Native inexact' )
            # SFCGAL
            plt.plot( X, memY[1], marker='o', label='Serialized exact' )
            # Referenced
            plt.plot( X, memY[2], marker='o', label='Referenced exact' )
            plt.legend(loc='upper left')
            pdf.savefig()

    pdf.close()
