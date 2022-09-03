#usage:
#	python createpolicy.py
#		0. K
#		1. M
#		2. PKTBYTE

import argparse
import os
import subprocess
import sys


def parse_args(cmd_args):
    arg_parser = argparse.ArgumentParser(description="HDFS3.3.4 generate user_ec_policies.xml")
    arg_parser.add_argument("-k", type=int, required=True, help="ECN")
    arg_parser.add_argument("-m", type=int, required=True, help="ECK")
    arg_parser.add_argument("-ps", type=int, required=True, help="EC packet size (in Bytes)")
    arg_parser.add_argument("-save_dir", type=str, required=True, help="config file save directory")

    args = arg_parser.parse_args(cmd_args)
    return args

args = parse_args(sys.argv[1:])
if not args:
    exit()

k = args.k
m = args.m
ps_Bytes = args.ps
save_dir = args.save_dir

ps_KB = int(ps_Bytes / 1024)

scheme_id = "RS-{}-{}-{}k".format(k, m, ps_KB)

attr=[]

line="<?xml version=\"1.0\"?>\n"
attr.append(line)

line="<configuration>\n"
attr.append(line)

line="<layoutversion>1</layoutversion>\n"
attr.append(line)

line="<schemas>\n"
attr.append(line)

line="<schema id=\""+scheme_id+"\">\n"
attr.append(line)

line="<codec>rs</codec>\n"
attr.append(line)

line="<k>{}</k>\n".format(k)
attr.append(line)

line="<m>{}</m>\n".format(m)
attr.append(line)

line="<options> </options>\n"
attr.append(line)


line="</schema>\n"
attr.append(line)

line="</schemas>\n"
attr.append(line)

line="<policies>\n"
attr.append(line)

line="<policy>\n"
attr.append(line)

line="<schema>{}</schema>\n".format(scheme_id)
attr.append(line)

line="<cellsize>{}</cellsize>\n".format(ps_Bytes)
attr.append(line)

line="</policy>\n"
attr.append(line)

line="</policies>\n"
attr.append(line)

line="</configuration>\n"
attr.append(line)

filename="{}/user_ec_policies.xml".format(save_dir)
f=open(filename, "w")
for line in attr:
  f.write(line)
f.close()
