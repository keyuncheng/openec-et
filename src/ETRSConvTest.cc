#include "common/Config.hh" 
#include "ec/ECDAG.hh"
#include "ec/ECPolicy.hh"
#include "ec/ECBase.hh"
#include "ec/ECTask.hh"
#include "ec/ETRSConv.hh"

using namespace std;

void usage() {
    printf("usage: ./ETRSConvTest n k w pktbytes failed_ids \n");
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
    int pktsizeB = atoi(argv[4]);

    vector<int> failed_ids;
    for (int i = 5; i < argc; i++) {
        int failed_id = atoi(argv[i]);
        failed_ids.push_back(failed_id);
    }
    
    string ecid = "ETRSConv_" + string(argv[1]) + "_" + string(argv[2]) + "_" + string(argv[3]);
    
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
    for (int i=0; i<n_data_symbols; i++) encodeBufMap.insert(make_pair(i, databuffers[i]));
    for (int i=0; i<n_code_symbols; i++) encodeBufMap.insert(make_pair(n_data_symbols+i, codebuffers[i]));
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

    cout<< "failed node ";
    for (auto failnode : failed_ids) {
        cout << failnode << " ";
        vector<int> failed_symbols_node = ec1->GetNodeSymbols(failnode);
        for (auto symbol : failed_symbols_node) {
            failsymbols.push_back(symbol);
        }
    }
    cout << endl;

    for(int i=0; i<failsymbols.size(); i++) {
        char* tmpbuf = (char*)calloc(pktsizeB, sizeof(char));
        repairbuf[failsymbols[i]] = tmpbuf;
    }

    vector<int> availsymbols;
    for (int i=0; i<n*w; i++) {
        if (find(failsymbols.begin(), failsymbols.end(), i) == failsymbols.end())
            availsymbols.push_back(i);
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
        if (find(failsymbols.begin(), failsymbols.end(), i) == failsymbols.end())
            decodeBufMap.insert(make_pair(i, databuffers[i]));
        else
            decodeBufMap.insert(make_pair(i, repairbuf[i]));
    }
    for (int i=0; i<n_code_symbols; i++) 
        if (find(failsymbols.begin(), failsymbols.end(), n_data_symbols+i) == failsymbols.end())
            decodeBufMap.insert(make_pair(n_data_symbols+i, codebuffers[i]));
        else
            decodeBufMap.insert(make_pair(i, repairbuf[n_data_symbols+i]));
        
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

        int failed_node = failidx / w;

        int diff = 0;

        if (failed_node < k) {
            diff = memcmp(decodeBufMap[failidx], databuffers[failidx], pktsizeB * sizeof(char));
        } else {
            diff = memcmp(decodeBufMap[failidx], codebuffers[failidx - n_data_symbols], pktsizeB * sizeof(char));
        }
        if (diff != 0) {
            printf("failed to decode data of symbol %d!!!!\n", i);
        } else {
            printf("Decoded data of symbol %d.\n", i);
        }
    }
}
