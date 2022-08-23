my_ip=$(ifconfig | grep 192.168.10 | head -1 | sed "s/ *inet [addr:]*\([^ ]*\).*/\1/")

sed -i "s%\(<attribute><name>local.addr</name><value>\)[^<]*%\1${my_ip}%" ../conf/sysSetting.xml
sed -i "s%\(<property><name>oec.local.addr</name><value>\)[^<]*%\1${my_ip}%" /home/kycheng/hadoop-3.3.4-src/hadoop-dist/target/hadoop-3.3.4/etc/hadoop/hdfs-site.xml
