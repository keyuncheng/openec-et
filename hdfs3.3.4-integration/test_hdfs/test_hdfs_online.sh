#!/bin/bash

# clear cache
for i in {1..14}; do ssh dn$i "sudo sync; sudo sh -c 'echo 3 > /proc/sys/vm/drop_caches'"; done

python3 test_hdfs_online.py -k 10 -m 4 -bs 268435456 -ps 65536 -num_data_nodes 14 -num_clients_in_parallel 5 -num_stripes 20 -enable_recovery -ctr_addr 192.168.10.21 -input_data_dir /home/kycheng/openec-et