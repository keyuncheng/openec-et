#!/bin/bash

if [ $# -ne 5 ]
    then
        echo "usage: sh offline_write.sh k inputfile saveas poolid sizeinMB"
        exit 1
fi

k=$1
inputfile=$2
saveas=$3
poolid=$4
sizeinMB=$5

for i in `seq 0 $(($k-1))`
    do ./OECClient write $inputfile ${saveas}_${i} $poolid offline $sizeinMB;
    sleep 2
done