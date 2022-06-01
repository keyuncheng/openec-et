#include "common/Config.hh"
#include "inc/include.hh"
#include "util/hdfs.h"

#define default_alpha 8

void usage() {
  cout << "usage: ./HDFSClient write inputpath saveas sizeinMB" << endl;
  cout << "       ./HDFSClient read hdfspath saveas" << endl;
}

void write(string inputname, string hdfspath, int sizeinMB) {
  string confpath("./conf/sysSetting.xml");
  Config* conf = new Config(confpath);
  vector<string> fsparam = conf->_fsFactory[conf->_fsType];
  string fsIp = fsparam[0];
  int fsPort = atoi(fsparam[1].c_str());
  // connect to hdfs
  struct timeval time1, time2, time3, time4;
  gettimeofday(&time1, NULL);
  hdfsFS fs = hdfsConnect(fsIp.c_str(), fsPort);
  gettimeofday(&time2, NULL);

  // 0. create input file
  FILE* inputfile = fopen(inputname.c_str(), "rb");
 
  // 1. create file
  const char* writePath = hdfspath.c_str();
  hdfsFile writeFile = hdfsOpenFile(fs, writePath, O_WRONLY |O_CREAT, 0, 0, 0);
  if(!writeFile) {
          fprintf(stderr, "Failed to open %s for writing!\n", writePath);
          exit(-1);
  }

  int sizeinBytes = sizeinMB * 1048576;
  int num = sizeinBytes/conf->_pktSize;
  cout << "num = " << num << endl;
  srand((unsigned)time(0));

  for (int i=0; i<num; i++) {
    char* buf = (char*)calloc(conf->_pktSize, sizeof(char));
 
    fread(buf, conf->_pktSize, 1, inputfile);
    tSize num_written_bytes = hdfsWrite(fs, writeFile, (void*)buf, conf->_pktSize);
    free(buf);
    if (hdfsFlush(fs, writeFile)) {
           fprintf(stderr, "Failed to 'flush' %s\n", writePath);
          exit(-1);
    }
  }  
  gettimeofday(&time3, NULL);
  hdfsCloseFile(fs, writeFile);
  gettimeofday(&time4, NULL);

  cout << "HDFSClient::write.connect.hdfs: " << RedisUtil::duration(time1, time2) << endl;
  cout << "HDFSClient::write.write.duration: " << RedisUtil::duration(time2, time3) << endl;
  cout << "HDFSClient::write.overall.duration: " << RedisUtil::duration(time2, time4) << endl;

  delete conf;
}

void read(string filename, string saveas) {
  struct timeval time1, time2, time3, time4, time5;
  gettimeofday(&time1, NULL);

  cout << "read " << filename << " " << saveas << endl;
  string confpath("./conf/sysSetting.xml");
  Config* conf = new Config(confpath);
  vector<string> fsparam = conf->_fsFactory[conf->_fsType];
  string fsIp = fsparam[0];
  int fsPort = atoi(fsparam[1].c_str());
  // connect to dhfs
  hdfsFS fs = hdfsConnect(fsIp.c_str(), fsPort);
  gettimeofday(&time2, NULL);

  hdfsFile hdfsfile = hdfsOpenFile(fs, filename.c_str(), O_RDONLY, conf->_pktSize, 0, 0);
  gettimeofday(&time3, NULL);

  hdfsFileInfo* fileinfo = hdfsGetPathInfo(fs, filename.c_str());
  int filebytes = fileinfo->mSize; 

  ofstream ofs(saveas);
  ofs.close();
  ofs.open(saveas, ios::app);

  cout << "filebytes = " << filebytes << endl;
  int num = filebytes/conf->_pktSize;
  char* buf = (char*)calloc(conf->_pktSize, sizeof(char));
  for (int i=0; i<num; i++) {
    int hasread = 0;
    while(hasread < conf->_pktSize) {
      int len = hdfsRead(fs, hdfsfile, buf + hasread, conf->_pktSize - hasread);
      // int len = hdfsPread(fs, hdfsfile, conf->_pktSize * i + hasread, buf, conf->_pktSize - hasread);
      if (len == 0) break;
      hasread += len;
    }
    ofs.write(buf, conf->_pktSize);
  } 
  free(buf);
  gettimeofday(&time4, NULL);
  ofs.close(); 
  gettimeofday(&time5, NULL);

  cout << "HDFSClient::connect.hdfs: " << RedisUtil::duration(time1, time2) << endl;
  cout << "HDFSClient::filehander: " << RedisUtil::duration(time2, time3) << endl;
  cout << "HDFSClient::read.read.duration: " << RedisUtil::duration(time3, time4) << endl;
  cout << "HDFSClient::read.overall.duration: " << RedisUtil::duration(time2, time5) << endl;
  
  delete conf;
}

void pRead(int alpha, string filename, string saveas) {
  struct timeval time1, time2, time3, time4, time5;
  gettimeofday(&time1, NULL);

  cout << "read " << filename << " " << saveas << endl;
  string confpath("./conf/sysSetting.xml");
  Config* conf = new Config(confpath);
  vector<string> fsparam = conf->_fsFactory[conf->_fsType];
  string fsIp = fsparam[0];
  int fsPort = atoi(fsparam[1].c_str());
  // connect to dhfs
  hdfsFS fs = hdfsConnect(fsIp.c_str(), fsPort);
  gettimeofday(&time2, NULL);

  hdfsFile hdfsfile = hdfsOpenFile(fs, filename.c_str(), O_RDONLY, conf->_pktSize, 0, 0);
  gettimeofday(&time3, NULL);

  hdfsFileInfo* fileinfo = hdfsGetPathInfo(fs, filename.c_str());
  int filebytes = fileinfo->mSize; 

  int slicebytes = conf->_pktSize / alpha;

  ofstream ofs(saveas);
  ofs.close();
  ofs.open(saveas, ios::app);

  cout << "filebytes = " << filebytes << ", slicebytes = " << slicebytes << endl;
  int num_packets = filebytes/conf->_pktSize;
  char* buf = (char*)calloc(slicebytes, sizeof(char)); // slice size
  for (int i=0; i<num_packets; i++) {
    for (int j = 0; j < alpha; j++) {
      if (j != 1) {
        continue;
      }

      int offset = i * conf->_pktSize + j * slicebytes;

      int hasread = 0;
      while(hasread < slicebytes) {
        int len = hdfsPread(fs, hdfsfile, offset, buf, slicebytes - hasread);
        if (len == 0) break;
        hasread += len;
      }
      ofs.write(buf, slicebytes);

    }

  } 
  free(buf);
  gettimeofday(&time4, NULL);
  ofs.close(); 
  gettimeofday(&time5, NULL);

  cout << "HDFSClient::connect.hdfs: " << RedisUtil::duration(time1, time2) << endl;
  cout << "HDFSClient::filehander: " << RedisUtil::duration(time2, time3) << endl;
  cout << "HDFSClient::pRead.read.duration: " << RedisUtil::duration(time3, time4) << endl;
  cout << "HDFSClient::pRead.overall.duration: " << RedisUtil::duration(time2, time5) << endl;
  
  delete conf;
}

void read_sequential(string filename, string saveas, int num_rounds) {

  struct timeval time1, time2;
  gettimeofday(&time1, NULL);

  for (int i = 0; i < num_rounds; i++) {
    // change filename
    string objname = filename + "_" + to_string(i) + "_oecobj_0";
    string save_name = saveas + + "_" + to_string(i) + "_oecobj_0";
    read(objname, save_name);
  }

  gettimeofday(&time2, NULL);

  cout << "HDFSClient::read sequential: " << RedisUtil::duration(time1, time2) << endl;
}

void read_parallel(string filename, string saveas, int num_rounds) {
  struct timeval time1, time2;
  gettimeofday(&time1, NULL);

  vector<thread> read_threads = vector<thread>(num_rounds);
  for (int i = 0; i < num_rounds; i++) {
    // change filename
    string objname = filename + "_" + to_string(i) + "_oecobj_0";
    string save_name = saveas + + "_" + to_string(i) + "_oecobj_0";
    read_threads[i] = thread([=]{read(objname, save_name);});
  }

  for (int i = 0; i < num_rounds; i++) {
    // change filename
    read_threads[i].join();
  }

  gettimeofday(&time2, NULL);

  cout << "HDFSClient::read parallel: " << RedisUtil::duration(time1, time2) << endl;
}

void pRead_sequential(string filename, string saveas, int num_rounds) {

  struct timeval time1, time2;
  gettimeofday(&time1, NULL);

  for (int i = 0; i < num_rounds; i++) {
    // change filename
    string objname = filename + "_" + to_string(i) + "_oecobj_0";
    string save_name = saveas + + "_" + to_string(i) + "_oecobj_0";

    pRead(default_alpha, objname, save_name);
  }

  gettimeofday(&time2, NULL);

  cout << "HDFSClient::pRead sequential: " << RedisUtil::duration(time1, time2) << endl;
}

void pRead_parallel(string filename, string saveas, int num_rounds) {
  struct timeval time1, time2;
  gettimeofday(&time1, NULL);

  vector<thread> read_threads = vector<thread>(num_rounds);
  for (int i = 0; i < num_rounds; i++) {
    // change filename
    string objname = filename + "_" + to_string(i) + "_oecobj_0";
    string save_name = saveas + + "_" + to_string(i) + "_oecobj_0";

    read_threads[i] = thread([=]{pRead(default_alpha, objname, save_name);});
  }

  for (int i = 0; i < num_rounds; i++) {
    // change filename
    read_threads[i].join();
  }

  gettimeofday(&time2, NULL);

  cout << "HDFSClient::pRead parallel: " << RedisUtil::duration(time1, time2) << endl;
}

int main(int argc, char** argv) {

  if (argc < 4) {
    usage();
    return -1;
  }

  string reqType(argv[1]);
  if (reqType == "write") {
    if (argc != 5) {
      usage();
      return -1;
    }
    string inputfile(argv[2]);
    string saveas(argv[3]);
    int size = atoi(argv[4]);
    write(inputfile, saveas, size);
  } else if (reqType == "read") {
    string hdfsfile(argv[2]);
    string saveas(argv[3]);
    // read(hdfsfile, saveas);
    // read_sequential(hdfsfile, saveas, 10);
    // read_parallel(hdfsfile, saveas, 10);
  } else if (reqType == "pRead") {
    string hdfsfile(argv[2]);
    string saveas(argv[3]);
    // pRead(default_alpha, hdfsfile, saveas);
    // pRead_sequential(hdfsfile, saveas, 10);
    pRead_parallel(hdfsfile, saveas, 10);
  }

  return 0;
}
