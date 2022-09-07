import os
import sys
import time
import subprocess
import argparse
import threading
import time


def parse_args(cmd_args):
    arg_parser = argparse.ArgumentParser(description="test OpenEC atop HDFS3.3.4 online write with multiple clients")
    arg_parser.add_argument("-k", type=int, required=True, help="ECN")
    arg_parser.add_argument("-m", type=int, required=True, help="ECK")
    arg_parser.add_argument("-bs", type=int, required=True, help="EC block size (in Bytes)")
    arg_parser.add_argument("-ps", type=int, required=True, help="EC packet size (in Bytes)")
    arg_parser.add_argument("-num_data_nodes", type=int, required=True, help="Number of data nodes")
    arg_parser.add_argument("-num_clients_in_parallel", type=int, required=True, help="Number of OpenEC clients write in parallel")
    arg_parser.add_argument("-num_stripes", type=int, required=True, help="Number of HDFS stripes to write")
    arg_parser.add_argument("-oec_dir", type=str, required=True, help="OpenEC project directory")
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
    def __init__(self, hostname, oec_dir, input_filename, input_filesize, oec_file_lists, ec_scheme):
        threading.Thread.__init__(self)
        self.hostname = hostname
        self.oec_dir = oec_dir
        self.input_filename = input_filename
        self.input_filesize = input_filesize
        self.oec_file_lists = oec_file_lists
        self.ec_scheme = ec_scheme

    def run(self):
        cmd = r'echo ~'

        for i in range(len(self.oec_file_lists)):
            cmd="ssh {} \"cd {}; ./OECClient write {} {} {} online {}\"".format(self.hostname, self.oec_dir, self.input_filename, self.oec_file_lists[i], self.ec_scheme, self.input_filesize)
            print(cmd)
            # return_str, stderr = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE).communicate()
            # home_dir = return_str.decode().strip()
            # print(self.hostname, return_str)


def main():
    args = parse_args(sys.argv[1:])
    if not args:
        exit()
    
    k = args.k
    m = args.m
    bs_Bytes = args.bs
    ps_Bytes = args.ps
    num_clients_in_parallel = args.num_clients_in_parallel
    num_stripes = args.num_stripes
    num_data_nodes = args.num_data_nodes
    input_data_dir = args.input_data_dir
    oec_dir = args.oec_dir

    # paths
    home_dir = get_home_dir()
    proj_dir = "{}/openec-et".format(home_dir)
    scripts_dir = "{}/scripts_exp".format(proj_dir)
    hdfs_config_dir = "{}/hdfs3.3.4-integration/conf".format(proj_dir)

    # clear hdfs data, kill java
    clear_hdfs_script = "clear_hdfs.sh"
    cmd = "bash {}".format(clear_hdfs_script)
    print(cmd)
    os.system(cmd)

    # NOTE: make sure you update all hdfs config files in hdfs_config_dir and OEC configs in conf/sysSettings.xml

    # update configs to data nodes
    update_configs_script = "update_configs.sh"
    cmd = "bash {}".format(update_configs_script)
    print(cmd)
    os.system(cmd)

    # start OEC
    restart_oec_script = "restart_oec.sh"
    cmd="bash {}".format(restart_oec_script)
    print(cmd)
    os.system(cmd)

    ps_KB = int(ps_Bytes / 1024)
    
    # write OEC stripes in parallel

    oec_file_lists = []
    for i in range(num_clients_in_parallel):
        oec_file_lists.append([])

    # divide stripes into groups
    for i in range(num_stripes):
        client_id = i % num_clients_in_parallel
        oec_filename="/RS-{}-{}-{}-{}".format(k, m, ps_KB, i)
        oec_file_lists[client_id].append(oec_filename)
    
    print(oec_file_lists)

    write_threads = []
    for i in range(num_clients_in_parallel):
        # NOTE: the hostname may to be changed
        hostname = "dn{}".format(i + 1)
        fs_MB = int(bs_Bytes / 1048576) * k
        input_filename = "{}/input_{}".format(input_data_dir, fs_MB)
        ec_scheme = "RSCONV_{}_{}".format(k + m, k)
        wt = WriteThread(hostname, oec_dir, input_filename, fs_MB, oec_file_lists[i], ec_scheme)
        write_threads.append(wt)
    
    start_time = time.time()

    for wt in write_threads:
        wt.start()

    for wt in write_threads:
        wt.join()
        
    # calculate time
    end_time = time.time()
    elapsed_time = end_time - start_time
    print("OEC encode {} stripes time: ".format(num_stripes), elapsed_time)

    


if __name__ == '__main__':
    main()