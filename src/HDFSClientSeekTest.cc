#include "common/Config.hh"
#include "inc/include.hh"
#include "util/hdfs.h"

#define default_alpha 8

void usage() {
  // cout << "usage: ./HDFSClient write inputpath saveas sizeinMB" << endl;
  cout << "       ./HDFSClient pRead hdfspath saveas alpha step method[seq/step]" << endl;
}

// seek test
void pRead_seek(string filename, string saveas, int alpha, int step, string method) {
  struct timeval time1, time2, time3;
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

  // ofstream ofs(saveas);
  // ofs.close();
  // ofs.open(saveas, ios::app);

  int pkt_size = conf->_pktSize;
  int num_packets = filebytes / conf->_pktSize;
  int subpkt_size = conf->_pktSize / alpha;
  char* buf = (char*)calloc(pkt_size, sizeof(char)); // pkt size

  double avg_seq_time = 0, avg_step_time = 0;

  for (int pkt_id = 0; pkt_id < num_packets; pkt_id++) {
    // read the first packet
    int pkt_offset = pkt_id * pkt_size;

    // number of subpkts to read
    int num_subpkts_to_read = alpha / step;

    double time_seq = 0;
    struct timeval time_start, time_end;

    // 1. sequential read
    int read_size = num_subpkts_to_read * subpkt_size;
    int read_offset = pkt_offset;

    if (method == "seq") {
      // start time
      gettimeofday(&time_start, NULL);
      int hasread = 0;
      while(hasread < read_size) {
        int len = hdfsPread(fs, hdfsfile, read_offset, buf, read_size - hasread);
        if (len == 0) break;
        hasread += len;
      }
      // ofs.write(buf, read_size);

      // end time
      gettimeofday(&time_end, NULL);
      time_seq = RedisUtil::duration(time_start, time_end);
      avg_seq_time += time_seq;

      printf("seq read pkt (%d) - read_size: %d, read_offset: %d, time: %f\n", pkt_id, read_size, read_offset, time_seq);

    } else if (method == "step") {
      // 2. step read
      double time_step = 0;

      // start time
      gettimeofday(&time_start, NULL);

      for (int i = 0; i < num_subpkts_to_read; i++) {
        struct timeval time_step_start, time_step_end;
        gettimeofday(&time_step_start, NULL);

        read_size = 1 * subpkt_size;
        read_offset = pkt_offset + i * step * subpkt_size;

        int hasread = 0;
        while(hasread < read_size) {
          int len = hdfsPread(fs, hdfsfile, read_offset, buf, read_size - hasread);
          if (len == 0) break;
          hasread += len;
        }
        // ofs.write(buf, read_size);

        gettimeofday(&time_step_end, NULL);

        printf("step read pkt (%d) - read_size: %d, read_offset: %d, time: %f\n", pkt_id, read_size, read_offset, RedisUtil::duration(time_step_start, time_step_end));
      }

      // end time
      gettimeofday(&time_end, NULL);
      time_step = RedisUtil::duration(time_start, time_end);
      avg_step_time += time_step;
    }

  }
  free(buf);
  // ofs.close(); 

  // average the results
  avg_seq_time /= num_packets;
  avg_step_time /= num_packets;

  printf("filename: %s, saveas: %s, alpha: %d, step: %d\n", 
    filename.c_str(), saveas.c_str(), alpha, step);
  printf("HDFSClient::connect.hdfs: %f\n", RedisUtil::duration(time1, time2));
  printf("HDFSClient::filehander: %f\n", RedisUtil::duration(time2, time3));
  printf("seq time: %f, step time: %f\n", avg_seq_time, avg_step_time);

  
  delete conf;
}


int main(int argc, char** argv) {

  if (argc != 7) {
    usage();
    return -1;
  }

  string reqType(argv[1]);
  if (reqType == "pRead") {
    string hdfsfile(argv[2]);
    string saveas(argv[3]);
    int alpha = atoi(argv[4]);
    int step = atoi(argv[5]);
    string method(argv[6]);

    pRead_seek(hdfsfile, saveas, alpha, step, method);

  }

  return 0;
}
