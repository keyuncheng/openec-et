import argparse
import os
import subprocess
import sys



def parse_args(cmd_args):
    arg_parser = argparse.ArgumentParser(description="HDFS3.3.4 generate hdfs-site.xml")
    arg_parser.add_argument("-bs", type=int, required=True, help="EC block size (in Bytes)")
    arg_parser.add_argument("-ps", type=int, required=True, help="EC packet size (in Bytes)")
    arg_parser.add_argument("-useoec", dest="useoec", action="store_true", help="use OEC")
    arg_parser.add_argument("-enable_recovery", dest="enable_recovery", action="store_true", help="enable repair")
    arg_parser.add_argument("-ctr_addr", type=str, required=True, help="controller IP address")
    arg_parser.add_argument("-save_dir", type=str, required=True, help="config file save directory")

    args = arg_parser.parse_args(cmd_args)
    return args

def get_relative_hadoop_home_dir():
    # home dir
    cmd = r'echo ~'
    home_dir_str, stderr = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE).communicate()
    home_dir = home_dir_str.decode().strip()

    # home dir
    cmd = r'echo $HADOOP_HOME'
    hadoop_home_dir_str, stderr = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE).communicate()
    hadoop_home_dir = hadoop_home_dir_str.decode().strip()

    relative_hadoop_home_dir = hadoop_home_dir.replace(home_dir, "")

    return relative_hadoop_home_dir


args = parse_args(sys.argv[1:])
if not args:
    exit()

bs_Bytes = args.bs
ps_Bytes = args.ps
useoec = args.useoec
enable_recovery = args.enable_recovery
ctr_addr = args.ctr_addr
save_dir = args.save_dir


# create config

attr=[]

line="<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
attr.append(line) 

line="<?xml-stylesheet type=\"text/xsl\" href=\"configuration.xsl\"?>\n"
attr.append(line)

line="<configuration>\n"
attr.append(line)

line="<property><name>dfs.replication</name><value>1</value></property>\n"
attr.append(line)

line="<property><name>dfs.blocksize</name><value>"+str(bs_Bytes)+"</value></property>\n"
attr.append(line)

line="<property><name>dfs.block.replicator.classname</name><value>org.apache.hadoop.hdfs.server.blockmanagement.BlockPlacementPolicyOEC</value></property>\n"
attr.append(line)

line="<property><name>link.oec</name><value>"+str(useoec).lower()+"</value></property>\n"
attr.append(line)

line="<property><name>oec.controller.addr</name><value>"+ctr_addr+"</value></property>\n"
attr.append(line)

line="<property><name>oec.local.addr</name><value>"+ctr_addr+"</value></property>\n"
attr.append(line)

line="<property><name>oec.pktsize</name><value>"+str(ps_Bytes)+"</value></property>\n"
attr.append(line)

attr.append("\n")

line="<property><name>dfs.blockreport.intervalMsec</name><value>10</value></property>\n"
attr.append(line)

line="<property><name>dfs.datanode.directoryscan.interval</name><value>10s</value></property>\n"
attr.append(line)

line="<property><name>dfs.client.max.block.acquire.failures</name><value>0</value></property>\n"
attr.append(line)

line="<property><name>dfs.namenode.fs-limits.min-block-size</name><value>1024</value></property>\n"
attr.append(line)

line="<property><name>dfs.client-write-packet-size</name><value>"+str(ps_Bytes)+"</value></property>\n"
attr.append(line)

line="<property><name>io.file.buffer.size</name><value>"+str(ps_Bytes)+"</value></property>\n"
attr.append(line)

attr.append("\n")

line="<property><name>dfs.namenode.datanode.registration.ip-hostname-check</name><value>false</value></property>\n"
attr.append(line)

line="<property><name>enable.recovery</name><value>"+str(enable_recovery).lower()+"</value></property>\n"
attr.append(line)

rack_aware_script = "{}/etc/hadoop/rack_aware.sh".format(get_relative_hadoop_home_dir()[1:])
line="<property><name>net.topology.script.file.name</name><value>{}</value></property>\n".format(rack_aware_script)
attr.append(line)


line="</configuration>"
attr.append(line)

# create save dir
cmd = "mkdir -p {}".format(save_dir)
os.system(cmd)

filename = "{}/hdfs-site.xml".format(save_dir)
f=open(filename, "w")
for line in attr:
    f.write(line)

f.close()
