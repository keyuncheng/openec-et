#!/bin/bash

log_folder_name="local_test_nrb"

mkdir -p $log_folder_name

declare -a ETRS_9_6=(2 3 6 9 18 27)
declare -a ETRS_14_10=(2 4 8 16 32 64 128 256)
declare -a ETRS_16_12=(2 4 8 16 32 64 128 256)
declare -a ALRC_16_10=(2 3 4)

rm -f $log_folder_name/*.log


# URL="http://something.com/backup/v/photos/path/to/"
# URI="./$(echo $URL | awk -F'.com/' '{print $2}')"
# echo $URI

n=9
k=6
touch $log_folder_name/ETRS_${n}_${k}_summary.log
for alpha in ${ETRS_9_6[*]}; do
    nrb=0
    touch $log_folder_name/ETRS_${n}_${k}_${alpha}.log
    for i in $(seq 0 $(($n-1))); do
        echo ./ETRSConvTest $n $k $alpha 1 $i 
        nrb_full_str=$(./ETRSConvTest $n $k $alpha 1 $i | grep "norm repair bandwidth")
        echo ${nrb_full_str} >> $log_folder_name/ETRS_${n}_${k}_${alpha}.log
        
        nrb_val_str="$(echo ${nrb_full_str} | awk -F'norm repair bandwidth: ' '{print $2}')"
        nrb=$(echo ${nrb} + ${nrb_val_str} | bc -l)
    done
    nrb=$(echo ${nrb} / ${n} | bc -l)
    echo "ETRS ("${n}","${k}$","${alpha}") norm. bandwidth: " $nrb >> $log_folder_name/ETRS_${n}_${k}_summary.log
done


n=14
k=10
touch $log_folder_name/ETRS_${n}_${k}_summary.log
for alpha in ${ETRS_14_10[*]}; do
    nrb=0
    touch $log_folder_name/ETRS_${n}_${k}_${alpha}.log
    for i in $(seq 0 $(($n-1))); do
        echo ./ETRSConvTest $n $k $alpha 1 $i 
        nrb_full_str=$(./ETRSConvTest $n $k $alpha 1 $i | grep "norm repair bandwidth")
        echo ${nrb_full_str} >> $log_folder_name/ETRS_${n}_${k}_${alpha}.log
        
        nrb_val_str="$(echo ${nrb_full_str} | awk -F'norm repair bandwidth: ' '{print $2}')"
        nrb=$(echo ${nrb} + ${nrb_val_str} | bc -l)
    done
    nrb=$(echo ${nrb} / ${n} | bc -l)
    echo "ETRS ("${n}","${k}$","${alpha}") norm. bandwidth: " $nrb >> $log_folder_name/ETRS_${n}_${k}_summary.log
done


n=16
k=12
touch $log_folder_name/ETRS_${n}_${k}_summary.log
for alpha in ${ETRS_16_12[*]}; do
    nrb=0
    touch $log_folder_name/ETRS_${n}_${k}_${alpha}.log
    for i in $(seq 0 $(($n-1))); do
        echo ./ETRSConvTest $n $k $alpha 1 $i 
        nrb_full_str=$(./ETRSConvTest $n $k $alpha 1 $i | grep "norm repair bandwidth")
        echo ${nrb_full_str} >> $log_folder_name/ETRS_${n}_${k}_${alpha}.log
        
        nrb_val_str="$(echo ${nrb_full_str} | awk -F'norm repair bandwidth: ' '{print $2}')"
        nrb=$(echo ${nrb} + ${nrb_val_str} | bc -l)
    done
    nrb=$(echo ${nrb} / ${n} | bc -l)
    echo "ETRS ("${n}","${k}$","${alpha}") norm. bandwidth: " $nrb >> $log_folder_name/ETRS_${n}_${k}_summary.log
done

n=16
k=10
touch $log_folder_name/ALRC_${n}_${k}_summary.log
for alpha in ${ALRC_16_10[*]}; do
    nrb=0
    touch $log_folder_name/ALRC_${n}_${k}_${alpha}.log
    for i in $(seq 0 $(($n-1))); do
        echo ./ETAzureLRCTest $n $k $alpha 1 $i 
        nrb_full_str=$(./ETAzureLRCTest $n $k $alpha 1 $i | grep "norm repair bandwidth")
        echo ${nrb_full_str} >> $log_folder_name/ALRC_${n}_${k}_${alpha}.log
        
        nrb_val_str="$(echo ${nrb_full_str} | awk -F'norm repair bandwidth: ' '{print $2}')"
        nrb=$(echo ${nrb} + ${nrb_val_str} | bc -l)
    done
    nrb=$(echo ${nrb} / ${n} | bc -l)
    echo "ALRC ("${n}","${k}$","${alpha}") norm. bandwidth: " $nrb >> $log_folder_name/ALRC_${n}_${k}_summary.log
done