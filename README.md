postgis-sfcgal
==============

PostGIS branch that uses CGAL for spatial processing.

Compilation
-----------

Run ./autogen.sh then
./configure --with-cgaldir=... --with-sfcgaldir=...
(see ./configure --help)

make && sudo make install

Usage
-----

Create a PostGIS database :
<pre>
$ createdb test_postgis
$ psql test_postgis &lt; /usr/share/postgresql/9.1/contrib/postgis-2.1/postgis.sql
$ psql test_postgis &lt; /usr/share/postgresql/9.1/contrib/postgis-2.1/spatial_ref_sys.sql
</pre>

Now you can use functions located in the 'sfcgal' schema :
<pre>
$ psql test_postgis
psql (9.1.6)
Type "help" for help.

test_postgis=# select sfcgal.st_intersection( 'LINESTRING(0 0,1 1)'::exact_geometry, 'POINT(1/3 1/3)'::exact_geometry );
st_intersection 
-----------------
POINT(1/3 1/3)
</pre>
 
see postgis/lwgeom_sfcgal_*.{h,cpp}

Benchmarking scripts
--------------------

See bench/sfcgal_bench.py
Current results can be found in bench/report.pdf and bench.report_serialization.pdf
