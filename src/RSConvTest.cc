#include "common/Config.hh" 
#include "ec/ECDAG.hh"
#include "ec/ECPolicy.hh"
#include "ec/ECBase.hh"
#include "ec/ECTask.hh"
#include "ec/RSConv.hh"

#include <utility>

using namespace std;

void usage() {
    printf("usage: ./RSConvTest n k pktbytes failed_ids\n");
}

double getCurrentTime() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (double)tv.tv_sec * 1e+6 + (double)tv.tv_usec;
}

int main(int argc, char** argv) {

    if (argc < 5) {
        usage();
        return 0;
    }
    int pktsizeB = atoi(argv[3]);

    vector<int> failed_ids;
    for (int i = 4; i < argc; i++) {
        int failed_id = atoi(argv[i]);
        failed_ids.push_back(failed_id);
    }
    
    string ecid = "RSConv_" + string(argv[1]) + "_" + string(argv[2]);
    
    string confpath = "conf/sysSetting.xml";
    Config* conf = new Config(confpath);

    ECPolicy* ecpolicy = conf->_ecPolicyMap[ecid];
    ECBase* ec = ecpolicy->createECClass();

    int n = ec->_n;
    int k = ec->_k;
    int w = ec->_w;

    int n_data_symbols = k * w;
    int n_code_symbols = (n - k) * w;

    // 0. prepare data buffers
    char **databuffers = (char **)calloc(n_data_symbols, sizeof(char *));
    for (int i = 0; i < n_data_symbols; i++) {
        databuffers[i] = (char *)calloc(pktsizeB, sizeof(char));
        char v = i;
        memset(databuffers[i], v, pktsizeB);
    }

    // 1. prepare code buffers
    char **codebuffers = (char **)calloc(n_code_symbols, sizeof(char *));
    for (int i = 0; i < n_code_symbols; i++) {
        codebuffers[i] = (char *)calloc(pktsizeB, sizeof(char));
        memset(codebuffers[i], 0, pktsizeB);
    }

    double initEncodeTime=0, initDecodeTime=0;
    double encodeTime = 0, decodeTime=0;
    initEncodeTime -= getCurrentTime();

    ECDAG* encdag = ec->Encode();
    vector<ECTask*> encodetasks;
    unordered_map<int, char*> encodeBufMap;
    vector<int> toposeq = encdag->toposort();
    for (int i=0; i<toposeq.size(); i++) { 
        ECNode* curnode = encdag->getNode(toposeq[i]);
        curnode->parseForClient(encodetasks);
    }

    vector<vector<int>> _layout = ec->GetLayout();

    for (int i=0; i<n_data_symbols; i++) {
        int sym_w = i % w;
        int sym_node_id = i / w;
        encodeBufMap.insert(make_pair(_layout[sym_w][sym_node_id], databuffers[i]));
    }
    for (int i=0; i<n_code_symbols; i++) {
        int sym_w = (i+n_data_symbols) % w;
        int sym_node_id = (i+n_data_symbols) / w;
        encodeBufMap.insert(make_pair(_layout[sym_w][sym_node_id], codebuffers[i]));
    }
    initEncodeTime += getCurrentTime();

    encodeTime -= getCurrentTime();
    for (int taskid = 0; taskid < encodetasks.size(); taskid++) {
        ECTask* compute = encodetasks[taskid];
        compute->dump();

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
            data[bufIdx] = encodeBufMap[child];
        }
        int codeBufIdx = 0;
        for (auto it: coefMap) {
            int target = it.first;
            char* codebuf; 
            if (encodeBufMap.find(target) == encodeBufMap.end()) {
                codebuf = (char *)calloc(pktsizeB, sizeof(char));
                encodeBufMap.insert(make_pair(target, codebuf));
            } else {
                codebuf = encodeBufMap[target];
            }
            code[codeBufIdx] = codebuf;
            targets.push_back(target);
            vector<int> curcoef = it.second;
            for (int j = 0; j < col; j++) {
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

    // debug encode
    for (int i=0; i<n_data_symbols; i++) {
        char* curbuf = (char*)databuffers[i];
        cout << "dataidx = " << i << ", value = " << (int)curbuf[0] << endl;
    }
    for (int i=0; i<n_code_symbols; i++) {
        char* curbuf = (char*)codebuffers[i];
        cout << "codeidx = " << n_data_symbols+i << ", value = " << (int)curbuf[0] << endl;
    }

    cout << "========================" << endl;

    // decode
    initDecodeTime -= getCurrentTime();

    ECPolicy* ecpolicy1 = conf->_ecPolicyMap[ecid];
    ECBase* ec1 = ecpolicy->createECClass();

    vector<int> failsymbols;
    unordered_map<int, char*> repairbuf;

    for (auto failnode : failed_ids) {
        vector<int> failed_symbols_node = ec1->GetNodeSymbols(failnode);
        for (auto symbol : failed_symbols_node) {
            failsymbols.push_back(symbol);
        }
    }

    for(int i=0; i<failsymbols.size(); i++) {
        char* tmpbuf = (char*)calloc(pktsizeB, sizeof(char));
        repairbuf[failsymbols[i]] = tmpbuf;
    }

    vector<int> availsymbols;
    for (int i=0; i<n*w; i++) {
        int sym_w = i % w;
        int sym_node_id = i / w;
        int symbol = _layout[sym_w][sym_node_id];
        if (find(failsymbols.begin(), failsymbols.end(), symbol) == failsymbols.end())
            availsymbols.push_back(symbol);
    }

    cout << "fail symbols: ";
    for (int i=0; i<failsymbols.size(); i++) {
        cout << failsymbols[i] << " ";
    }
    cout << endl;

    cout << "avail symbols:";
    for(int i=0; i<availsymbols.size(); i++) {
        cout << availsymbols[i] << " ";
    }
    cout << endl;

    ECDAG* decdag = ec1->Decode(availsymbols, failsymbols);
    vector<ECTask*> decodetasks;
    unordered_map<int, char*> decodeBufMap;
    vector<int> dectoposeq = decdag->toposort();
    for (int i=0; i<dectoposeq.size(); i++) { 
        ECNode* curnode = decdag->getNode(dectoposeq[i]);
        curnode->parseForClient(decodetasks);
    }
    for (int i=0; i<n_data_symbols; i++) {
        int sym_w = i % w;
        int sym_node_id = i / w;
        int symbol = _layout[sym_w][sym_node_id];
        if (find(failsymbols.begin(), failsymbols.end(), symbol) == failsymbols.end())
            decodeBufMap.insert(make_pair(symbol, databuffers[i]));
        else
            decodeBufMap.insert(make_pair(symbol, repairbuf[symbol]));
    }
    for (int i=0; i<n_code_symbols; i++) {
        int sym_w = (i+n_data_symbols) % w;
        int sym_node_id = (i+n_data_symbols) / w;
        int symbol = _layout[sym_w][sym_node_id];
        if (find(failsymbols.begin(), failsymbols.end(), symbol) == failsymbols.end())
            decodeBufMap.insert(make_pair(symbol, codebuffers[i]));
        else
            decodeBufMap.insert(make_pair(symbol, repairbuf[symbol]));
    }
    for (auto item : decodeBufMap) {
        printf("%d %d\n", item.first, item.second[0]);
    }
        
    initDecodeTime += getCurrentTime();

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

    for (int taskid = 0; taskid < decodetasks.size(); taskid++) {
        ECTask* compute = decodetasks[taskid];
        compute->dump();

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

            data[bufIdx] = decodeBufMap[child];
        }
        int codeBufIdx = 0;
        for (auto it: coefMap) {
            int target = it.first;
            char* codebuf; 
            if (decodeBufMap.find(target) == decodeBufMap.end()) {
                codebuf = (char*)calloc(pktsizeB, sizeof(char));
                decodeBufMap.insert(make_pair(target, codebuf));
            } else {
                codebuf = decodeBufMap[target];
            }
            code[codeBufIdx] = codebuf;
            targets.push_back(target);
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
    // printf("packets read: %d / %d, norm repair bandwidth: %f\n", sum_packets_read, k * w, norm_repair_bandwidth);

    // debug decode
    for (int i=0; i<failsymbols.size(); i++) {
        int failidx = failsymbols[i];
        char* curbuf  = decodeBufMap[failidx];
        cout << "failidx = " << failidx << ", value = " << (int)curbuf[0] << endl;

        int sym_w = -1;
        int sym_node_id = -1;

        for (int i = 0; i < n; i++) {
            for (int j = 0; j < w; j++) {
                if (failidx == _layout[j][i]) {
                    sym_w = j;
                    sym_node_id = i;
                }
            }
        }

        int offset = sym_node_id * w + sym_w;
        int failed_node = sym_node_id;

        int diff = 0;

        if (failed_node < k) {
            diff = memcmp(decodeBufMap[failidx], databuffers[offset], pktsizeB * sizeof(char));
        } else {
            diff = memcmp(decodeBufMap[failidx], codebuffers[offset - n_data_symbols], pktsizeB * sizeof(char));
        }
        if (diff != 0) {
            printf("failed to decode data!!!!\n");
        }
    }
}
