postgis-sfcgal
==============

PostGIS branch that uses CGAL for spatial processing.

Compilation
-----------

It depends on [SFCGAL](https://github.com/Oslandia/SFCGAL) which must have been installed before. SFCGAL must have been compiled into a dynamic library. Set the cmake option 'SFCGAL_USE_STATIC_LIBS' to OFF for that.

Run
<pre>
./autogen.sh
./configure --with-sfcgal --with-cgaldir=... --with-sfcgaldir=...
</pre>
(see ./configure --help)

then
<pre>
make && sudo make install
</pre>

Features
--------

This PostGIS branch adds the following things:
* a new 'scfcgal' schema. New ST_xxx() functions are created inside this new schema, they use the same signature as the native PostGIS ones, when possible (ST_Intersection, ST_Intersects, etc.)
* a new 'exact_geometry' type. This type can carry exact coordinates and is passed from functions to functions by serialization / deserialization. SFCGAL functions are all overloaded with arguments of this type.
* a new 'ref_geometry' type. This type can also carry exact coordinates and is passed from functions to functions using memory pointers. SFCGAL functions are all overloaded with arguments of this type.
* an extended WKT syntax for exact coordinates, both for reading and writing.

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
