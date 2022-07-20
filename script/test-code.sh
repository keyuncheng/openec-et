#!/bin/bash

# coding parameters
n=9
k=6

alphas=(2)

# coding scheme
scheme_prefix="ETRSConv_${n}_${k}_" # elastic transformed RS code

# file input
input_file="384MB_file"
input_file_size_mb=384

# file output
output_file_prefix="/testfile_"
log_file=repair_time_log.out

# HDFS
hdfs_data_dir="${HADOOP_HOME}/dfs/data/current/BP-*/current/finalized/subdir0/subdir0/"

usage() {
  echo "Usage: $0 [encode|repair]"
  exit 1
}

encode() {
    # write all the files of target code into OpenEC
    for a in ${alphas[@]}; do
      scheme=${scheme_prefix}${a}
      ./OECClient write ${input_file} ${output_file_prefix}${a} ${scheme} online ${input_file_size_mb}
      sleep 2
    done
}

repair() {
  echo "" >> ${log_file}
  echo ">> New Repair log $(date)" | tee -a ${log_file}

  rm ${hdfs_data_dir}/*[^.meta]
  for a in ${alphas[@]}; do
    echo "alpha = ${a}" | tee -a ${log_file}
    # drop all cache on storage node
    for i in {1..14}; do ssh openec-dn${i} sudo ./drop_cache.sh; done

    ./OECClient read ${output_file_prefix}${a}_0 /dev/null | tee -a ${log_file}
  done
}

if [ $# -ne 1 ]; then
  usage
fi

if [ $1 == "encode" ]; then
  encode
elif [ $1 == "repair" ]; then
  repair
else
  usage
fi

