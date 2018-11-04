#!/bin/bash
base=64
file1=$(tempfile)
file2=$(tempfile)
size=$(( base * 1 ))
./imagegen -s ${size}x${size} --SDL -T -o $file2 -v .5,.5,0 -v .5,.5,0
size=$(( base * 2 ))
./imagegen -s ${size}x${size} --SDL -T -i $file2 -o $file1 -v .6,.4,0 -v .4,.6,0
size=$(( base * 3 ))
./imagegen -s ${size}x${size} --SDL -T -i $file1 -o $file2 -v .7,.3,0 -v .3,.7,0
size=$(( base * 4 ))
./imagegen -s ${size}x${size} --SDL -T -i $file2 -o $file1 -v .8,.2,0 -v .2,.8,0
size=$(( base * 5 ))
./imagegen -s ${size}x${size} --SDL -T -i $file1 -o $file2 -v .9,.1,0 -v .1,.9,0
size=$(( base * 6 ))
./imagegen -s ${size}x${size} --SDL -T --wait 10 -o $file2 -o $file1 -v 1,0,0 -v 0,1,0
echo $file2
