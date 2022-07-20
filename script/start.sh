#!/bin/bash
oec_packetsize=$1
user_dir="/home/openec/"
home_dir="openec-et-anonymous/"
my_ip=$(ifconfig | grep 192.168.10 | sed "s/ *inet addr:\([^ ]*\).*/\1/")
hdfs_blocksize=67108864
num_nodes=9
oec_packetsize=1048576

hdfs_site_config_file=${user_dir}hadoop-3.0.0-src/hadoop-dist/target/hadoop-3.0.0/etc/hadoop/hdfs-site.xml
oec_config_file=${user_dir}${home_dir}conf/sysSetting.xml

# update my IP locally
bash updateIP.sh

# update HDFS configuration
sed -i "s%\(<property><name>oec.controller.addr</name><value>\)[^<]*%\1${my_ip}%" ${hdfs_site_config_file}
sed -i "s%\(<property><name>oec.pktsize</name><value>\)[^<]*%\1${oec_packetsize}%" ${hdfs_site_config_file}
sed -i "s%\(<property><name>dfs.blocksize</name><value>\)[^<]*%\1${hdfs_blocksize}%" ${hdfs_site_config_file}

# update OpenEC configuration
sed -i "s%\(<attribute><name>packet.size</name><value>\)[^<]*%\1${oec_packetsize}%" ${oec_config_file}

# sync all files and configurations of HDFS and OpenEC to data nodes
echo "Sync the all files and configurations to storage nodes"
for i in $(seq 1 ${num_nodes}); do 
#for i in 17 18; do 
	#rsync -avr ${user_dir}hadoop-3.0.0-src openec-dn${i}:~/
	rsync -avr ${hdfs_site_config_file} openec-dn${i}:${user_dir}hadoop-3.0.0-src/hadoop-dist/target/hadoop-3.0.0/etc/hadoop/
	rsync -avr OECAgent OECClient ../conf updateIP.sh openec-dn${i}:${user_dir}${home_dir}
done

echo "Update the local configuration on storage nodes"
for i in $(seq 1 ${num_nodes}); do ssh openec-dn$i "cd ${user_dir}${home_dir}; bash ./updateIP.sh"; done

# start HDFS
start-dfs.sh
while [ 1 == 1 ]; do
        sleep 10
        hdfs dfsadmin -report | grep -e "Live datanodes" -e "Name"
        if [ ! -z "$(hdfs dfsadmin -report | grep -e "Live datanodes" | grep "${num_nodes}")" ]; then
                break;
        fi
done

# start OpenEC coordinator
echo "Start OECCoordinator";
./OECCoordinator &> coor_output &

sleep 10

# start OpenEC agent
for i in $(seq 1 ${num_nodes}); do 
  echo "Start OECAgent on openec-dn$i." && \
  sleep 5 && \
  ssh openec-dn$i "cd ${user_dir}${home_dir}/; ./OECAgent &> agent_output &" && \
  ssh openec-dn$i "ps aux | grep OEC | grep -v grep"
done
