#include "common/Config.hh"
#include "ec/ECDAG.hh"
#include "ec/ECPolicy.hh"
#include "ec/ECBase.hh"
#include "ec/ECTask.hh"
#include "inc/include.hh"

#include "ec/HTEC.hh"
#include "ec/ETHTEC.hh"

#include <utility>

using namespace std;

// Modified from CodeTest

void usage() {
  cout << "Usage: ./HTECTest " << endl;
  cout << "  1, failed node index (from 0 to n-1)" << endl;
  cout << "  2, n" << endl;
  cout << "  3, k" << endl;
  cout << "  4, blocksizeB" << endl;
  cout << "  5, pktsizeB" << endl;
  cout << "  6, transformed" << endl;
}

double getCurrentTime() {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return (double)tv.tv_sec * 1e+6 + (double)tv.tv_usec;
}

int main(int argc, char** argv) {
  if (argc < 6) {
    usage();
    return 0;
  }

  int f = atoi(argv[1]);
  int n = atoi(argv[2]);
  int k = atoi(argv[3]);
  int blocksizeB = atoi(argv[4]);
  int pktsizeB = atoi(argv[5]);
  bool transformed = argc >= 7;

  int w = blocksizeB / pktsizeB;

  if (f < 0 || f >= n) { usage(); return 1; };

  string confpath = "conf/sysSetting.xml";
  Config* conf = new Config(confpath);
  int stripenum = blocksizeB/pktsizeB;
  string ecid = (transformed? "ETHTEC_" : "HTEC_")+to_string(n)+"_"+to_string(k)+"_"+to_string(stripenum);

  // we need to prepare the original data
  // prepare data buffer and code buffer
  uint8_t** databuffers = (uint8_t**)calloc(k, sizeof(uint8_t*));
  for (int i=0; i<k; i++) {
    databuffers[i] = (uint8_t*)calloc(blocksizeB, sizeof(uint8_t));
  }
  uint8_t** codebuffers = (uint8_t**)calloc((n-k), sizeof(uint8_t*));
  for (int i=0; i<(n-k); i++) {
    codebuffers[i] = (uint8_t*)calloc(blocksizeB, sizeof(uint8_t));
  }
  uint8_t* oribuffer = (uint8_t*)calloc(blocksizeB, sizeof(uint8_t));

  ECPolicy* ecpolicy = nullptr;
  ECBase* ec = nullptr;

  double initEncodeTime=0;
  initEncodeTime -= getCurrentTime();

  // prepare for encode
  ECDAG* encdag = nullptr;
  vector<ECTask*> encodetasks;
  unordered_map<int, char*> encodeBufMap;

  ecpolicy = conf->_ecPolicyMap[ecid];
  ec = ecpolicy->createECClass();
  encdag = ec->Encode();
  vector<int> toposeq = encdag->toposort();
  for (int i=0; i<toposeq.size(); i++) {
    ECNode* curnode = encdag->getNode(toposeq[i]);
    curnode->parseForClient(encodetasks);
  }
  for (int i=0; i<k; i++) encodeBufMap.insert(make_pair(i, (char*)databuffers[i]));
  for (int i=0; i<(n-k); i++) encodeBufMap.insert(make_pair(k+i, (char*)codebuffers[i]));

  initEncodeTime += getCurrentTime();

  // prepare for decode
  double initDecodeTime=0;
  initDecodeTime -= getCurrentTime();
  int fidx=f;
  ECDAG* decdag = nullptr;
  vector<ECTask*> decodetasks;
  unordered_map<int, char*> decodeBufMap;
  vector<int> availidx;
  vector<int> torecidx;

  for(int i=0; i<n; i++) {
    for (int j = 0; j<stripenum; j++) {
        if (i == fidx) torecidx.push_back(i * stripenum + j);
        else availidx.push_back(i * stripenum + j);
    }
  }
  decdag = ec->Decode(availidx, torecidx);
  toposeq = decdag->toposort();
  for (int i=0; i<toposeq.size(); i++) {
    ECNode* curnode = decdag->getNode(toposeq[i]);
    curnode->parseForClient(decodetasks);
  }
  initDecodeTime += getCurrentTime();

  //HTEC *htec = static_cast<HTEC *>(ec);
  //htec->PrintParityInfo();

  // test
  double encodeTime = 0, decodeTime = 0;
  srand((unsigned)1234);

  // clean codebuffers
  for (int i=0;i<(n-k); i++) {
    memset(codebuffers[i], 0, blocksizeB);
  }
  // initialize databuffers
  for (int i=0; i<k; i++) {
    for (int j=0; j<blocksizeB; j++) databuffers[i][j] = rand();
  }
  // encode test
  encodeTime -= getCurrentTime();
  cout << "Num of tasks = " << encodetasks.size() << endl;
  for (int taskid = 0; taskid < encodetasks.size(); taskid++) {
    ECTask* compute = encodetasks[taskid];
    vector<int> children = compute->getChildren();
    unordered_map<int, vector<int>> coefMap = compute->getCoefMap();
    int col = children.size();
    int row = coefMap.size();
    vector<int> targets;
    int* matrix = (int*)calloc(row*col, sizeof(int));
    char** data = (char**)calloc(col, sizeof(char*));
    char** code = (char**)calloc(row, sizeof(char*));
    for (int bufIdx=0; bufIdx<children.size(); bufIdx++) {
      int child = children[bufIdx];
      data[bufIdx] = encodeBufMap[child / stripenum] + (child % stripenum) * pktsizeB;
    }
    int codeBufIdx = 0;
    for (auto it: coefMap) {
      int target = it.first;
      char* codebuf;
      if (encodeBufMap.find(target / stripenum) == encodeBufMap.end()) {
        codebuf = (char*)calloc(blocksizeB, sizeof(char));
        encodeBufMap.insert(make_pair(target /stripenum, codebuf));
        codebuf += (target % stripenum) * pktsizeB;
      } else {
        codebuf = encodeBufMap[target / stripenum] + (target % stripenum) * pktsizeB;
      }
      code[codeBufIdx] = codebuf;
      targets.push_back(target);
      //cout << "task = " << taskid << " encode = " << target << endl;
      vector<int> curcoef = it.second;
      for (int j=0; j<col; j++) {
        matrix[codeBufIdx * col + j] = curcoef[j];
      }
      codeBufIdx++;
    }
    Computation::Multi(code, data, matrix, row, col, pktsizeB, "Isal");
    free(matrix);
    free(data);
    free(code);
  }
  encodeTime += getCurrentTime();

  // take out fidx buffer
  uint8_t* cpybuf;
  if (fidx < k) cpybuf = databuffers[fidx];
  else cpybuf = codebuffers[fidx-k];
  memcpy(oribuffer, cpybuf, blocksizeB);

  // prepare avail buffer and torec buffer
  // and decodeBufMap
  decodeBufMap.clear();
  uint8_t* availbuffers[k];
  int aidx=0;
  for (int i=0; i<k && aidx<k; i++) {
    if (i == fidx) continue;
    availbuffers[aidx++] = databuffers[i];
    decodeBufMap.insert(make_pair(i, (char*)databuffers[i]));
  }
  for (int i=0; i<(n-k); i++) {
    if (i+k == fidx) continue;
    availbuffers[aidx++] = codebuffers[i];
    decodeBufMap.insert(make_pair(i+k, (char*)codebuffers[i]));
  }
  uint8_t* toretbuffers[1];
  uint8_t repairbuf[blocksizeB];
  toretbuffers[0] = repairbuf;
  decodeBufMap.insert(make_pair(fidx, (char*)repairbuf));

  // decode
  decodeTime -= getCurrentTime();

  /**
   * @brief record number of disk seeks and number of sub-packets to read for every node
   * @disk_read_pkts_map <node> sub-packets read in each node
   * @disk_read_info_map <node_id, <num_disk_seeks, num_pkts_read>>
   * 
   */
  map<int, vector<int>> disk_read_pkts_map;
  map<int, pair<int, int>> disk_read_info_map;

  int sum_packets_read = 0;
  double norm_repair_bandwidth = 0;

  // init the map
  for (int node_id = 0; node_id < n; node_id++) {
      disk_read_pkts_map[node_id].clear();
      disk_read_info_map[node_id] = make_pair(0, 0);
  }

  cout << "Num of tasks = " << decodetasks.size() << endl;
  for (int taskid=0; taskid<decodetasks.size(); taskid++) {
    ECTask* compute = decodetasks[taskid];
    vector<int> children = compute->getChildren();
    unordered_map<int, vector<int>> coefMap = compute->getCoefMap();
    int col = children.size();
    int row = coefMap.size();
    vector<int> targets;
    int* matrix = (int*)calloc(row*col, sizeof(int));
    char** data = (char**)calloc(col, sizeof(char*));
    char** code = (char**)calloc(row, sizeof(char*));
    for (int bufIdx=0; bufIdx<children.size(); bufIdx++) {
      int child = children[bufIdx];

      if (child < n * w) {
          int node_id = child / w;
          vector<int> &read_pkts = disk_read_pkts_map[node_id];
          if (find(read_pkts.begin(), read_pkts.end(), child) == read_pkts.end()) {
              read_pkts.push_back(child);
          }
      }

      data[bufIdx] = decodeBufMap[child / stripenum] + (child % stripenum) * pktsizeB;
      //cout << "task = " << taskid << " child = " << child << " data = " << (void *) data[bufIdx] << endl;
    }
    int codeBufIdx = 0;
    for (auto it: coefMap) {
      int target = it.first;
      char* codebuf;
      if (decodeBufMap.find(target / stripenum) == decodeBufMap.end()) {
        codebuf = (char*)calloc(blocksizeB, sizeof(char));
        decodeBufMap.insert(make_pair(target / stripenum, codebuf));
        codebuf += (target % stripenum) * pktsizeB;
      } else {
        codebuf = decodeBufMap[target / stripenum] + (target % stripenum) * pktsizeB;
      }
      code[codeBufIdx] = codebuf;
      targets.push_back(target);
      //cout << "task = " << taskid << " repair = " << target << " buf = " << (void *) code[codeBufIdx] << endl;
      vector<int> coef = it.second;
      for (int j=0; j<col; j++) {
        matrix[codeBufIdx * col + j] = coef[j];
      }
      codeBufIdx++;
    }
    Computation::Multi(code, data, matrix, row, col, pktsizeB, "Isal");
    //cout << "task = " << taskid << " buf = " << (void*) code[0] << endl;
    free(matrix);
    free(data);
    free(code);
  }
  decodeTime += getCurrentTime();

  /**
   * @brief record the disk_read_info_map
   */
  for (auto item : disk_read_pkts_map) {
      vector<int> &read_pkts = item.second;
      sort(read_pkts.begin(), read_pkts.end());
  }

  for (int node_id = 0; node_id < n; node_id++) {
      vector<int> &list = disk_read_pkts_map[node_id];

      // we first transfer items in list %w
      vector<int> offset_list;
      for (int i = 0; i < list.size(); i++) {
          offset_list.push_back(list[i]%w); 
      }
      sort(offset_list.begin(), offset_list.end()); // sort in ascending order
      reverse(offset_list.begin(), offset_list.end());
      
      // create consecutive read list
      int num_of_cons_reads = 0;
      vector<int> cons_list;
      vector<vector<int>> cons_read_list; // consecutive read list
      while (offset_list.empty() == false) { 
          int offset = offset_list.back();
          offset_list.pop_back();

          if (cons_list.empty() == true) {
              cons_list.push_back(offset);
          } else {
              // it's consecutive
              if (cons_list.back() + 1 == offset) {
                  cons_list.push_back(offset); // at to the back of prev cons_list
              } else {
                  cons_read_list.push_back(cons_list); // commits prev cons_list
                  cons_list.clear();
                  cons_list.push_back(offset); // at to the back of new cons_list
              }
          }
      }
      if (cons_list.empty() == false) {
          cons_read_list.push_back(cons_list);
      }

      printf("node id: %d, cons_read_list:\n", node_id);
      for (auto cons_list : cons_read_list) {
          for (auto offset : cons_list) {
          printf("%d ", offset);
          }
          printf("\n");
      }

      // update disk_read_info_map
      disk_read_info_map[node_id].first = cons_read_list.size();
      disk_read_info_map[node_id].second = disk_read_pkts_map[node_id].size();

      // update stats
      sum_packets_read += disk_read_pkts_map[node_id].size();
  }

    // calculate norm repair bandwidth (against RS code)
    norm_repair_bandwidth = sum_packets_read * 1.0 / (k * w);

    printf("disk read info:\n");
    for (int node_id = 0; node_id < n; node_id++) {
        printf("node_id: %d, num_disk_seeks: %d, num_pkts_read: %d, pkts: ", node_id, disk_read_info_map[node_id].first, disk_read_info_map[node_id].second);
        // printf("%d, %d\n", disk_read_info_map[node_id].first, disk_read_info_map[node_id].second);
        for (auto pkt : disk_read_pkts_map[node_id]) {
            printf("%d ", pkt);
        }
        printf("\n");
    }
    printf("packets read: %d / %d, norm repair bandwidth: %f\n", sum_packets_read, k * w, norm_repair_bandwidth);


  // check correcness
  bool success = true;
  //for(int i=0; i<pktsizeB; i++) {
  cout << "Packet size = " << pktsizeB << endl;
  for(int i=0; i<blocksizeB; i++) {
    if (oribuffer[i] != repairbuf[i]) {
      cout << "Diff at packet " << i / pktsizeB << ", block byte offset " << i % pktsizeB << " (" << (unsigned int) oribuffer[i] << " vs " << (unsigned int) repairbuf[i] << ")" << endl;
      success = false;
      i = pktsizeB * (i / pktsizeB + 1);
    }
  }
  if (!success) {
    cout << "repair error!" << endl;
  }
  cout << "============================================" << endl;
  cout << "InitEncodeTime: " << initEncodeTime/1000 << " ms" << endl;
  cout << "PureEncodeTime: " << encodeTime/1000 << " ms" << endl;
  cout << "PureEncodeThroughput: " << blocksizeB*k/1.048576/encodeTime << endl;
  cout << "InitDecodeTime: " << initDecodeTime/1000 << " ms" << endl;
  cout << "PureDecodeTime: " << decodeTime/1000 << " ms" << endl;
  cout << "PureDecodeThroughput: " << blocksizeB/1.048576/decodeTime << endl;

  return 0;

}
