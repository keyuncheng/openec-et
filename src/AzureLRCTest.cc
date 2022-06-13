#include "common/Config.hh" 
#include "ec/ECDAG.hh"
#include "ec/ECPolicy.hh"
#include "ec/ECBase.hh"
#include "ec/ECTask.hh"
#include "ec/AzureLRC.hh"

using namespace std;

void usage() {
    printf("usage: ./AzureLRCTest pktbytes failed_ids\n");
}

double getCurrentTime() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (double)tv.tv_sec * 1e+6 + (double)tv.tv_usec;
}

int main(int argc, char** argv) {

    if (argc < 3) {
        usage();
        return 0;
    }
    int pktsizeB = atoi(argv[1]);

    vector<int> failed_ids;
    for (int i = 2; i < argc; i++) {
        int failed_id = atoi(argv[i]);
        failed_ids.push_back(failed_id);
    }
    
    string ecid = "AzureLRC_14_10";
    
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
