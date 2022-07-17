#!/bin/bash

alpha=256
declare -a steps=(1 2 4 8 16 32 64 128 256)


for step in ${steps[*]}; do
    rm -f test_seek
    # clean cache, restart hdfs
    sh env.sh

    # write a 64MiB file to hdfs
    hdfs dfs -put input_64 /test_seek

    ./HDFSClientSeekTest pRead /test_seek test_seek ${alpha} $step seq

    echo "finished alpha = $alpha, step = $step, seq"
done

for step in ${steps[*]}; do
    rm -f test_seek
    # clean cache, restart hdfs
    sh env.sh

    # write a 64MiB file to hdfs
    hdfs dfs -put input_64 /test_seek

    ./HDFSClientSeekTest pRead /test_seek test_seek ${alpha} $step step

    echo "finished alpha = $alpha, step = $step, step"
done

