#include "common/Config.hh"
#include "ec/ECDAG.hh"
#include "ec/ECPolicy.hh"
#include "ec/ECBase.hh"
#include "ec/ECTask.hh"
#include "inc/include.hh"

#include "ec/HTEC.hh"
#include "ec/ETHTEC.hh"

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
