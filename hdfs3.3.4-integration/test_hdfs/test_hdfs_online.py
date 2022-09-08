import os
import sys
import time
import subprocess
import argparse
import threading
import time


def parse_args(cmd_args):
    arg_parser = argparse.ArgumentParser(description="test HDFS3.3.4 online write with multiple clients")
    arg_parser.add_argument("-k", type=int, required=True, help="ECN")
    arg_parser.add_argument("-m", type=int, required=True, help="ECK")
    arg_parser.add_argument("-bs", type=int, required=True, help="EC block size (in Bytes)")
    arg_parser.add_argument("-ps", type=int, required=True, help="EC packet size (in Bytes)")
    arg_parser.add_argument("-useoec", dest="useoec", action="store_true", help="use OEC")
    arg_parser.add_argument("-enable_recovery", dest="enable_recovery", action="store_true", help="enable repair")
    arg_parser.add_argument("-num_data_nodes", type=int, required=True, help="Number of data nodes")
    arg_parser.add_argument("-num_clients_in_parallel", type=int, required=True, help="Number of HDFS clients write in parallel")
    arg_parser.add_argument("-num_stripes", type=int, required=True, help="Number of HDFS stripes to write")
    arg_parser.add_argument("-ctr_addr", type=str, required=True, help="controller IP address")
    arg_parser.add_argument("-input_data_dir", type=str, required=True, help="input data directory. Make sure that a file named \"input_<k * bs_MiB>\" is stored in the directory")
    

    args = arg_parser.parse_args(cmd_args)
    return args

def get_home_dir():
    # home dir
    cmd = r'echo ~'
    home_dir_str, stderr = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE).communicate()
    home_dir = home_dir_str.decode().strip()
    return home_dir


class WriteThread(threading.Thread):
    def __init__(self, hostname, input_filename, hdfs_file_list):
        threading.Thread.__init__(self)
        self.hostname = hostname
        self.input_filename = input_filename
        self.hdfs_file_list = hdfs_file_list

    def run(self):
        cmd = r'echo ~'

        for i in range(len(self.hdfs_file_list)):
            cmd="ssh {} \"time -p hdfs dfs -put -d {} {}\"".format(self.hostname, self.input_filename + "_{}".format(i), self.hdfs_file_list[i])
            print(cmd)
            return_str, stderr = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE).communicate()
            home_dir = return_str.decode().strip()
            print(self.hostname, return_str)


def main():
    args = parse_args(sys.argv[1:])
    if not args:
        exit()
    
    k = args.k
    m = args.m
    bs_Bytes = args.bs
    ps_Bytes = args.ps
    useoec = args.useoec
    enable_recovery = args.enable_recovery
    num_clients_in_parallel = args.num_clients_in_parallel
    num_stripes = args.num_stripes
    num_data_nodes = args.num_data_nodes
    ctr_addr = args.ctr_addr
    input_data_dir = args.input_data_dir

    # paths
    home_dir = get_home_dir()
    proj_dir = "{}/openec-et".format(home_dir)
    scripts_dir = "{}/scripts_exp".format(proj_dir)
    hdfs_config_dir = "{}/hdfs3.3.4-integration/test_hdfs/conf".format(proj_dir)

    # clear hdfs data, kill java
    clear_hdfs_script = "clear_hdfs.sh"
    cmd = "cd {}; bash {}; cd -".format(scripts_dir, clear_hdfs_script)
    print(cmd)
    os.system(cmd)

    # create hdfs-site.xml
    cmd = "python3 create_hdfs_site.py -bs {} -ps {} {} {} -ctr_addr {} -save_dir {}".format(
        bs_Bytes, ps_Bytes, 
        "-useoec" if useoec == True else "", 
        "-enable_recovery" if enable_recovery == True else "", 
        ctr_addr, hdfs_config_dir
    )
    print(cmd)
    os.system(cmd)

    cmd = "python3 create_hdfs_ec_policy.py -k {} -m {} -ps {} -save_dir {}".format(
        k, m, ps_Bytes, hdfs_config_dir
    )
    print(cmd)
    os.system(cmd)

    # update configs to data nodes
    update_hdfs_configs_script = "update_hdfs_configs.sh"
    cmd = "bash {}".format(update_hdfs_configs_script)
    print(cmd)
    os.system(cmd)

    # start hdfs
    cmd="hdfs namenode -format -force"
    print(cmd)
    os.system(cmd)
    cmd="start-dfs.sh"
    print(cmd)
    os.system(cmd)

    # create hdfs ec configs
    print("info::create hdfs ec configs")

    user_ec_policies_file = "{}/user_ec_policies.xml".format(hdfs_config_dir)
    cmd = "hdfs ec -addPolicies -policyFile {}".format(user_ec_policies_file)
    print(cmd)
    os.system(cmd)

    ps_KB = int(ps_Bytes / 1024)
    ec_scheme = "RS-{}-{}-{}k".format(k, m, ps_KB)
    cmd = "hdfs ec -enablePolicy -policy {}".format(ec_scheme)
    print(cmd)
    os.system(cmd)

    cmd="hdfs dfs -mkdir /hdfsec"
    print(cmd)
    os.system(cmd)

    cmd="hdfs ec -setPolicy -path /hdfsec -policy {}".format(ec_scheme)
    print(cmd)
    os.system(cmd)


    # write HDFS stripes in parallel

    hdfs_file_lists = []
    for i in range(num_clients_in_parallel):
        hdfs_file_lists.append([])

    # divide stripes into groups
    for i in range(num_stripes):
        client_id = i % num_clients_in_parallel
        hdfs_filename="/hdfsec/RS-{}-{}-{}-{}".format(k, m, ps_KB, i)
        hdfs_file_lists[client_id].append(hdfs_filename)
    
    print(hdfs_file_lists)

    write_threads = []
    for i in range(num_clients_in_parallel):
        # NOTE: the hostname may to be changed
        hostname = "dn{}".format(i + 1)
        fs_MB = int(bs_Bytes / 1048576) * k
        input_filename = "{}/input_{}".format(input_data_dir, fs_MB)
        wt = WriteThread(hostname, input_filename, hdfs_file_lists[i])
        write_threads.append(wt)
    
    start_time = time.time()

    for wt in write_threads:
        wt.start()

    for wt in write_threads:
        wt.join()
        
    # calculate time
    end_time = time.time()
    elapsed_time = end_time - start_time
    print("HDFS encode {} stripes time: ".format(num_stripes), elapsed_time)

    


if __name__ == '__main__':
    main()