import os
import time
import sys
import json

network_set = 5
oec_packet_size_MB = 1
oec_packet_size = oec_packet_size_MB * 1048572

# k=12
#input_file_size = 768 # MB
#input_file = "input_768"
#input_file_size = 1536 # MB
#input_file = "input_1536"
# k=10
#input_file_size = 640 # MB
#input_file = "input_640"
# k=6
input_file_size = 384 # MB
input_file = "input_384"
out_file = "/test_code_"
read_out_file = "out_64"
result_save_folder = "exp_result/all_node_repair_2022-07-17/network_" + str(network_set) + "G/oec_packet_" + str(oec_packet_size_MB) + "/"
repeat_time = 10

all_codes = [
            #"RSCONV_14_10",
            #"ETRSConv_14_10_2",
            #"ETRSConv_14_10_3",
            #"ETRSConv_14_10_4",
            #"ETRSConv_14_10_8",
            #"ETRSConv_14_10_16",
            #"ETRSConv_14_10_32",
            #"ETRSConv_14_10_64",
            #"ETRSConv_14_10_128",
            #"ETRSConv_14_10_256",
            #"HTEC_14_10_2",
            #"HTEC_14_10_3",
            #"HTEC_14_10_4",
            #"HTEC_14_10_8",
            #"HTEC_14_10_16",
            #"HTEC_14_10_32",
            #"HTEC_14_10_64",
            #"ETHTEC_14_10_128",
            #"ETHTEC_14_10_256",
            #"ETHTEC_14_10_4",
            #"ETHTEC_14_10_6",
            #"ETHTEC_14_10_8",
            #"ETHTEC_14_10_16",
            #"ETHTEC_14_10_64",
            #"Clay_14_10",
            #"WASLRC_14_10",
            #"ETAzureLRC_10_5_2",
            #"ETAzureLRC_10_5_3",
            #"ETAzureLRC_10_5_4",
            #"HHXORPlus_14_10",
            #"ETHHXORPlus_14_10_4",
            #"ETHHXORPlus_14_10_6",
            #"ETHHXORPlus_14_10_8",
            #"HHXORPlus_14_10",
            #"HTEC_14_10_2",
            #"HTEC_14_10_4",
            #"ETHTEC_14_10_256",
            "RSCONV_9_6",
            "ETRSConv_9_6_2",
            "ETRSConv_9_6_3",
            "ETRSConv_9_6_6",
            "ETRSConv_9_6_9",
            "ETRSConv_9_6_18",
            "ETRSConv_9_6_27",
            #"Clay_9_6",
            #"HHXORPlus_9_6",
            #"ETHHXORPlus_9_6_4",
            #"ETHHXORPlus_9_6_6",
            #"WASLRC_11_6",
            #"ETAzureLRC_11_6_2",
            #"ETAzureLRC_11_6_3",
            #"HTEC_9_6_2",
            #"HTEC_9_6_3",
            #"HTEC_9_6_4",
            #"HTEC_9_6_6",
            #"HTEC_9_6_9",
            #"ETHTEC_9_6_18",
            #"ETHTEC_9_6_27",
            #"ETHTEC_9_6_4",
            #"ETHTEC_9_6_6"
            #"RSCONV_16_12",
            #"ETRSConv_16_12_2",
            #"ETRSConv_16_12_3",
            #"ETRSConv_16_12_4",
            #"ETRSConv_16_12_8",
            #"ETRSConv_16_12_16",
            #"ETRSConv_16_12_32",
            #"ETRSConv_16_12_64",
            #"ETRSConv_16_12_128",
            #"ETRSConv_16_12_256",
            #"Clay_16_12",
            #"HHXORPlus_16_12",
            #"ETHHXORPlus_16_12_4",
            #"ETHHXORPlus_16_12_6",
            #"ETHHXORPlus_16_12_8",
            #"HTEC_16_12_2",
            #"HTEC_16_12_4",
            #"HTEC_16_12_8",
            #"HTEC_16_12_16",
            #"HTEC_16_12_32",
            #"HTEC_16_12_64",
            #"ETHTEC_16_12_4",
            #"ETHTEC_16_12_6",
            #"ETHTEC_16_12_8",
            #"ETHTEC_16_12_8",
            #"ETHTEC_16_12_16",
            #"ETHTEC_16_12_64",
            #"ETHTEC_16_12_128",
            #"ETHTEC_16_12_256",
            #'WASLRC_16_10',
            #'ETAzureLRC_16_10_2',
            #'ETAzureLRC_16_10_3',
            #'ETAzureLRC_16_10_4',
            #'WASLRC_18_12',
            #'ETAzureLRC_18_12_2',
            #'ETAzureLRC_18_12_3',
            #'ETAzureLRC_18_12_4',
            ]

def restart_cluster():
    print("Restart the whole cluster...")
    restart_cmd = "ssh namenode \"cd /home/kycheng/openec-et; sh env.sh && sh start.sh;\""
    start = time.time()
    # print(restart_cmd)
    os.system(restart_cmd)
    end = time.time()
    print("Cluster restart finished, time cost: " + str(end - start) + "s")
    time.sleep(2)

def online_write_file(write_index, code_id):
    print("Write " +  code_id + ", file: " + out_file + str(write_index))
    write_cmd = "./OECClient write " + input_file + " " + out_file + str(write_index) + " " + code_id + " online " + str(input_file_size)
    # print(write_cmd)
    write_detail = os.popen(write_cmd).readlines()
    # os.system(write_cmd)
    if len(write_detail) == 2:
        write_sen = write_detail[1]
        begin_p = write_sen.index("duration:") + len("duration:") + 1
        end_p = write_sen.index("\n")
        write_time = float(write_sen[begin_p:end_p])
        print("Write file finished: " + out_file + str(write_index) + ", time cost: " + str(write_time))
    else:
        print("Write file got error output: " + out_file + str(write_index))
        write_time = 0
    time.sleep(1)
    return write_time


def delete_node_block(node_ip, block_id = "*"):
    print("Start to delete block: " + node_ip + ", block: " + block_id)
    delete_cmd = "ssh " + node_ip + " \" rm ~/hadoop-3.0.0-src/hadoop-dist/target/hadoop-3.0.0/dfs/data/current/BP*/current/finalized/*/*/blk_" + block_id + " \" "
    # print(delete_cmd)
    os.system(delete_cmd)
    # delete_detail = os.popen(delete_cmd).readlines()
    # print("Python part: ", delete_detail)
    time.sleep(2)
    print("Delete block finished: " + node_ip + ", block: " + block_id)

def check_hdfs_blocks():
    hdfs_check_blocks_cmd="hdfs fsck -list-corruptfileblocks"
    block_lists = os.popen(hdfs_check_blocks_cmd).readlines()
    print(block_lists)

def read_file_block(file_name):
    print("Start to read file: " + file_name + ", total times: " + str(repeat_time))
    read_time_list = []
    for t in range(repeat_time):
        read_cmd = "./OECClient read " + file_name + " " + read_out_file
        # print(read_cmd)
        # read_detail = os.system(read_cmd)
        read_detail = os.popen(read_cmd).readlines()
        # print("Python part: ", read_detail)
        if len(read_detail) == 3:
            read_sen = read_detail[2]
            begin_p = read_sen.index("duration:") + len("duration:") + 1
            end_p = read_sen.index("\n")
            read_time = float(read_sen[begin_p:end_p])
            print("Read file finished: " + file_name + ", time cost: " + str(read_time))
        else:
            print("Read file got error output: " + file_name)
            read_time = 0
        read_time_list.append(read_time)
        time.sleep(1)
    return read_time_list


def get_breakdown_details():
    print("Start to get breakdown in agent_output")

    # load time record
    loadObj_cmd = "grep \"OECWorker::readOfflineObj loadObj\" agent_output"
    # print(loadObj_cmd)
    loadObj = os.popen(loadObj_cmd).readlines()
    load_time_list = []
    for record in loadObj:
        begin_p = record.index("loadObj =") + len("loadObj =") + 1
        end_p = record.index("\n")
        load_time = float(record[begin_p:end_p])
        load_time_list.append(load_time)

    # compute time record
    compute_cmd = "grep \"OECWorker::readOfflineObj compute\" agent_output"
    # print(compute_cmd)
    compute = os.popen(compute_cmd).readlines()
    compute_time_list = []
    for record in compute:
        begin_p = record.index("compute =") + len("compute =") + 1
        end_p = record.index("\n")
        compute_time = float(record[begin_p:end_p])
        compute_time_list.append(compute_time)

    # write to redis record
    wredis_cmd = "grep \"OECWorker::readOfflineObj write to redis\" agent_output"
    # print(wredis_cmd)
    wredis = os.popen(wredis_cmd).readlines()
    wredis_time_list = []
    for record in wredis:
        begin_p = record.index("redis =") + len("redis =") + 1
        end_p = record.index("\n")
        wredis_time = float(record[begin_p:end_p])
        wredis_time_list.append(wredis_time)

    return load_time_list, compute_time_list, wredis_time_list

def find_block_id_and_ip(node_index):
    find_node_ip_and_block_cmd = "hdfs fsck " + out_file + str(node_index) + "_oecobj_" + str(node_index)+ " -files -blocks -locations | grep Datanode"
    # print(find_node_ip_and_block_cmd)
    find_ip_result = os.popen(find_node_ip_and_block_cmd).readlines()[0]
    #print(find_ip_result)

    block_begin = find_ip_result.index("blk_") + len("blk_")
    block_end = find_ip_result.index("len=")
    block_meta = find_ip_result[block_begin:block_end]
    block_split = block_meta.split("_")
    block_id = block_split[0]

    ip_begin = find_ip_result.index("WithStorage[") + len("WithStorage[")
    ip_end = find_ip_result.index(",DS")
    ip_origin = find_ip_result[ip_begin:ip_end]
    ip_split = ip_origin.split(":")
    node_ip = ip_split[0]
    print(node_ip, block_id)
    return node_ip, block_id

def test_single_code(code_id):
    # use dict to store each breakdown of node repair
    node_time_dict = {}
    code_id_split = code_id.split("_")
    n = int(code_id_split[1])

    print("Start to test the code:" + code_id + ", n = " + str(n))

    start_main = time.time()
    # Step 1: restart the whole cluster
    restart_cluster()

    # Step 2: onlie write n input file
    for node_index in range(n):
        write_time = online_write_file(node_index, code_id)
        print(write_time)
        node_time_dict[node_index] = {}
        node_time_dict[node_index]["encode"] = write_time

        time.sleep(2)
        node_ip = None
        # Step 3: delete the block i for No.i input file
        while node_ip == None:
            try:
                node_ip, block_id = find_block_id_and_ip(node_index)
            except:
                time.sleep(1)
        delete_node_block(node_ip, block_id)

    time.sleep(3)
    check_hdfs_blocks()

    # Step 4: read block i for No.i input file(repeat k times)
    for node_index in range(n):
        read_file_name = out_file + str(node_index) + "_" + str(node_index)
        read_time_list = read_file_block(read_file_name)
        print(read_time_list)
        node_time_dict[node_index]["decode"] = read_time_list

    # Step 5: get breakdown result of agent_output
    loadObj, compute, wredis = get_breakdown_details()
    for node_index in range(n):
        node_time_dict[node_index]["loadObj"] = loadObj[node_index * repeat_time: (node_index + 1) * repeat_time]
        node_time_dict[node_index]["compute"] = compute[node_index * repeat_time: (node_index + 1) * repeat_time]
        node_time_dict[node_index]["wredis"] = wredis[node_index * repeat_time: (node_index + 1) * repeat_time]


    # Step 6: save result into folder
    print(node_time_dict)
    result_save_code_folder = result_save_folder + code_id + "/"
    os.makedirs(result_save_code_folder, exist_ok=True)
    result_save_path = result_save_code_folder + "result.json"
    with open(result_save_path, 'w',encoding='utf8') as f:
        json.dump(node_time_dict, f, indent=1, ensure_ascii = False)

    copy_agent = "cp agent_output " + result_save_code_folder
    os.system(copy_agent)
    copy_coor = "scp namenode:/home/kycheng/openec-et/coor_output " + result_save_code_folder
    os.system(copy_coor)
    print("Save the code results to " + result_save_code_folder)

    end_main = time.time()
    print("Evaluate code " + code_id + " finished, cost time: " + str(end_main - start_main) + "s")

params = sys.argv
if len(params) != 2:
    print("Usage: python eval_code.py code_id[all]")
    exit()

if params[1] == "all":
    for ec_codes in all_codes:
        test_single_code(ec_codes)
else:
    test_single_code(params[1])

