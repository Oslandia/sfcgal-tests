#!/bin/sh

if [ -z "$1" ]; then
    echo "Argument: report file"
    exit
fi

reportfile=$1
#points="4 5 10 20 50 100 200 500 1500 5000"
points="4 5 10 20 50 100 200"
algos_1000="intersects_ls_ls intersection_polygon_polygon intersection_poly_poly_h intersection_ls_ls"
algos_10000="convexhull_multipoint intersects_ls_poly_h intersects_point_polygon area_polygon intersects_polygon_polygon triangulate_poly"
algos_3d="intersects3D_ls_ls intersects3D_poly_poly"

for algo in $algos_1000; do
    nalgos_1000="$nalgos_1000 -x $algo"
done
for algo in $algos_10000; do
    nalgos_10000="$nalgos_10000 -x $algo"
done
for algo in $algos_3d; do
    nalgos_3d="$nalgos_3d -x $algo"
done
for point in $points; do
    npoints="$npoints -p $point"
done

#cmd="python ./sfcgal_bench.py -r ${reportfile}_part1.pdf $npoints $nalgos_1000 -n 1000"
#echo $cmd
#$cmd
cmd="python ./sfcgal_bench.py -r ${reportfile}_part2.pdf $npoints $nalgos_10000 -n 10000"
echo $cmd
$cmd
#cmd="python ./sfcgal_bench.py -r ${reportfile}_part3.pdf $npoints $nalgos_3d -n 10000"
#echo $cmd
#$cmd

# serialization report
#python ./sfcgal_bench.py -r ${reportfile}_serialization.pdf $npoints -n 1000 -x serialization

