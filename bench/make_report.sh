#!/bin/sh

if [ -z "$1" ]; then
    echo "Argument: report file"
    exit
fi

reportfile=$1
points="4 5 10 20 50 100 200"
ngeoms="1000"
algos="intersects_polygon_polygon intersects_point_polygon intersects_ls_poly_h intersects_ls_ls intersection_polygon_polygon intersection_poly_poly_h intersection_ls_ls area_polygon convexhull_multipoint"

for algo in $algos; do
    nalgos="$nalgos -x $algo"
done
for point in $points; do
    npoints="$npoints -p $point"
done
cmd="python ./sfcgal_bench.py -r $reportfile $npoints $nalgos -n $ngeoms"
echo $cmd
$cmd

