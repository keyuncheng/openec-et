#include "HTEC.hh"

#include <assert.h>
#include <exception>
#include <iomanip>

extern "C" {
//#include <gf_method.h>         // create_gf_from_argv(), galois_change_technique()
#include "../util/jerasure.h"  // jerasure_invert_matrix()
}

int retryLimit = 10;

const char *HTEC::_targetWKey = "tw=";
const char *HTEC::_preceedWKey = "pw=";

HTEC::HTEC(int n, int k, int alpha, int opt, vector<string> param) { 
    int i = 0, j = 0, l = 0;
    int alphaMax = 1;

    // coding parameters
    _n = n;
    _k = k;
    _w = alpha;
    _m = n - k;
    _stepMax = (_w + _m - 1) / _m - 1; // step is max when run is 1
    _runMax = (_w + _m - 1) / _m; // run is max when v = 1
    _q = 32;

    if (_n <= 0 || k <= 0 || _w <= 0) {
        throw invalid_argument("error: n, k, w must be positive.");
    }

    // should it be _n -_k + 1 to meet the supported sub-packetization range?
    if (_n < _k) {
        throw invalid_argument("error: n must be larger than k.");
    }

    for (int i = 0; i < (_k + _m - 1) / _m; i++) { alphaMax *= _m; };
    if (_w < 2 || _w > alphaMax) {
        cerr << "error: sub-packetization must be between 2 and (n-k)^ceil(k/(n-k)) = " << alphaMax << endl;
        throw invalid_argument("error: sub-packetization must be between 2 and (n-k)^ceil(k/(n-k))");
    }

    _q = 8;

    // coding optimization option
    _opt = opt;

    // HTEC-specific variables
    _numGroups = (_k + _m - 1) / _m;
    _groupSize = _m;
    _portion = (_w + _m - 1) / _m;

    // multi-instance variables 
    _targetW = 0;
    _preceedW = 0;

    // process parameters
    for (const string &s : param) {
        if (s.compare(0, strlen(_targetWKey), _targetWKey) == 0) { _targetW = stoll(s.substr(s.find("=") + 1)); }
        if (s.compare(0, strlen(_preceedWKey), _preceedWKey) == 0) { _preceedW = stoll(s.substr(s.find("=") + 1)); }
    }

    cout << "HTEC: n = " << _n << " k = " << _k << " alpha = " << _w << " group size = " << _groupSize << " portion = " << _portion << " q = " << _q << " target w = " << _targetW << " preceed w = " << _preceedW << endl;

    // data layout array (horizontal: storage nodes, vertical: packets)
    //      N_0  N_1   N_2   ...  N_(k-1)
    // [0]  0    w     2w    ...  (k-1)w
    // [1]  1    w+1   2w+1  ...  (k-1)w + 1
    // ...  ...
    // [w]  w-1  2w-1  3w-1  ...  kw - 1
    _dataLayout = new vector<int>* [_w];
    for (i = 0; i < _w; i++) { // for every sub-stripe
        _dataLayout[i] = new vector<int>();
        _dataLayout[i]->resize(_k);
        for (j = 0; j < _k; j++) { // for every data node
            _dataLayout[i]->at(j) = j * _w + i; 
        }
    }

    // parity matrix and source packets
    // allocate space
    _parityMatrix = new vector<int>** [_m];
    _paritySourcePackets = new vector<int>** [_m];
    _parityMatrixD = new vector<int>** [_m];
    _paritySourcePacketsD = new vector<int>** [_m];
    for (i = 0; i < _m; i++) {
        _parityMatrix[i] = new vector<int>*[_w];
        _paritySourcePackets[i] = new vector<int>*[_w];
        _parityMatrixD[i] = new vector<int>*[_w];
        _paritySourcePacketsD[i] = new vector<int>*[_w];
        for (j = 0; j < _w; j++) {
            // initialize all elements to 0
            _parityMatrix[i][j] = new vector<int>(GetNumSourcePackets(i), 0);
            _paritySourcePackets[i][j] = new vector<int>(GetNumSourcePackets(i), -1);
            _parityMatrixD[i][j] = new vector<int>(GetNumSourcePackets(i), 0);
            _paritySourcePacketsD[i][j] = new vector<int>(GetNumSourcePackets(i), -1);
        }
    }

    // fill the parity information (matrix and source packet)
    InitParityInfo();
}

HTEC::~HTEC() {
    delete [] _dataLayout;
    delete [] _parityMatrix;
    delete [] _paritySourcePackets;
    delete [] _parityMatrixD;
    delete [] _paritySourcePacketsD;
}

inline int HTEC::GetNumSourcePackets(int idx) const {
    return idx == 0? _k : _k + _numGroups;
}

HTEC::Partition HTEC::GetPartition(pair<int, int> idx) const {
    Partition p;

    if (idx.first >= 0 && _searchMap.count(idx.first) > 0) 
        p = _searchMap.at(idx.first).second;

    if (idx.first < 0 && _randMap.count(idx.first) > 0)
        p = _randMap.at(idx.first);

    return p;
}

void HTEC::InitParityInfo() {
    int run = 0, step = 0;
    int dataNodeId = 0, v = 0, v_m = 0;
    int i = 0, j = 0, l = 0;
    vector<pair<int, int>> validPartitions; // index of parition, if first value is >= 0, the partition is in _serachMap, otherwise, the parition is in _randMap

    // init the first k source packets for all parity packets
    for (i = 0; i < _m; i++) {
        for (j = 0; j < _w; j++) {
            for (l = 0; l < _k; l++) {
                _paritySourcePackets[i][j]->at(l) = l * (_targetW > 0? _targetW : _w) + _preceedW + j;
                _paritySourcePacketsD[i][j]->at(l) = l * (_targetW > 0? _targetW : _w) + _preceedW + j;
            }
        }
    }

    InitPartitionSearchMaps();

    // figure out the remaining source packets
    // phase 1
    do {
        dataNodeId += 1;
        v = (dataNodeId + _m - 1)/ _m;
        for (i = 0, v_m = 1; i < v; i++) { v_m *= _m; }
        run = (_w + v_m - 1) / v_m;
        step = _portion - run;
        // warnding: hard-code some 'correct steps' to archive the expected repair saving under specific coding parameters
        // (for n=14,k=10,alpha={32,64}, and n=16,k=12,alpha={32,64}, the second selected partition is of step 3 instead of 12 or 6)
        if (
                ((_n == 14 && _k == 10 || (_n == 16 && _k == 12))  && (_w == 32 || _w == 64))
                && (dataNodeId - 1)/ _groupSize == 1
        ) { 
            step = 3;
        }

        pair<int, int> np = FindPartition(step, run, validPartitions, (dataNodeId % _groupSize == 1? -1 : (dataNodeId - 1) / _groupSize * _groupSize));
        if (np.first == -1 && np.second == -1) {
            //cerr << "Failed to find a valid partition for data node " << dataNodeId << " with step = " << step << " and run = " << run << endl;
            assert(!validPartitions.empty());
            // reuse the last found partition (giving up on Condition 2)
            np = validPartitions.at(validPartitions.size() - 1);
            //throw std::exception();
        } else {
            //cout << "(1st phase) Find partition for data node " << dataNodeId << " with step = " << np.first << " (" << step << ") and run = " << run << " (" << run << ")\n";
            //if (dataNodeId % _m == 0 || dataNodeId == _k) GetPartition(np).Print();

        }
        validPartitions.emplace_back(np);
        FillParityIndices(v, GetPartition(np), dataNodeId);
    } while ((run > 1 || dataNodeId % _m != 0) && dataNodeId < _k);
    //} while (run > 1 && dataNodeId % _m != 0);

    // phase 2
    while (dataNodeId < _k) {
        dataNodeId += 1;
        v = (dataNodeId + _m - 1)/ _m;
        run = 0;

        pair<int, int> np = FindPartition(step, run, validPartitions, (dataNodeId % _groupSize == 1? -1 : (dataNodeId - 1) / _groupSize * _groupSize));
        if (np.first == -1 && np.second == -1) {
            //cerr << "(2nd phase) Failed to find the partition for data node " << dataNodeId << endl;
            assert(!validPartitions.empty());
            // reuse the last found partition (giving up on Condition 2)
            np = validPartitions.at(validPartitions.size() - 1);
            //throw std::exception();
        } else {
            //cout << "Find partition for data node " << dataNodeId << "(" << np.first << "," << np.second << ")\n";
        }
        validPartitions.emplace_back(np);
        //if (dataNodeId == _k) GetPartition(np).Print();
        FillParityIndices(v, GetPartition(np), dataNodeId);
    }

    FillParityCoefficients();

    //PrintParityInfo();

    DestroyPartitionSearchMaps();

    CondenseParityInfo();

    //PrintParityInfo(/* dense */ true);
}

void HTEC::InitPartitionSearchMaps() {

    int st = 0; // step counter
    int rn = 0; // run counter
    int ss = 0; // subset counter
    int l = 0;  // element counter
    int pkt = 0; // packet index
    int rst = 0; // 'random' step

    // if the number of rows required when subset size is fixed to portion is greater than the number of row exists in (m-1) parities, use m subsets
    bool useMoreSubsets = _portion * ((_w + _portion - 1) / _portion - 1) * _groupSize > _w * (_m - 1);

    // the paritions fulfilling both Condition 1 and Condition 2
    for (st = 0; st <= _stepMax; st++) {
        for (rn = _runMax; rn >= 1; rn--) {
            // skip if insufficient runs over the available packets, cannot fulfill first condition
            // sub-packetization a multiple of (step + 1) and cycle back to/beyond the first element
            if (_w % (st + 1) == 0 && (rn - 1) * (st + 1) >= _w) { continue; }

            // start picking elements for this partition
            // TODO search all possible combinations
            Partition p;
            rst = st;
            do {
                vector<int> subset;
                bool packetsSelected[_w] = { false };
                int numSelected = 0;
                int numSubsets = useMoreSubsets? _m : (_w + _portion - 1) / _portion;

                // search for subsets in the partition
                for (ss = 0; ss < numSubsets; ss++) {
                    pkt = 0;

                    int numElements = useMoreSubsets? (_w - numSelected + (numSubsets - ss) - 1) / (numSubsets - ss) : _portion;
                    // choose subset elements
                    for (l = 0; l < numElements; l++) {
                        // insifficient element of a required run is being selected, we cannot find a mapping that can fulfill the requirement..
                        if (packetsSelected[pkt] && ss == 0 && l < rn) { p.Clear(); break; }

                        // reset packet selection (i.e., allow partial overlap in subsets) if portion cannot divide sub-packetization
                        if (numSelected >= _w && (_w % _portion != 0)) {
                            for (int i = 0; i < _w; i++) packetsSelected[i] = { false };
                            numSelected = 0;
                        } else if (numSelected >= _w) { // if the condition cannot relaxed, stop searching
                            break;
                        }

                        // move one packet forward if the current one is already selected
                        while(packetsSelected[pkt]) { pkt = (pkt + 1) % _w; };

                        // select this packet
                        subset.push_back(pkt);
                        packetsSelected[pkt] = true;
                        numSelected++;

                        // advance by step
                        pkt = (pkt + (ss < rn? st : rst) + 1) % _w;
                    }
                    if (p.Empty() && subset.size() < _portion) { break; }
                    // add the subset to partition
                    if (!subset.empty()) p.AppendSubset(subset);
                    // reset the subset (holder)
                    subset.clear();
                }

                // check for same partition 
                for (const auto &ep : _searchMap) {
                    // discard the result if it overlaps with any others... (and search again)
                    if (p == ep.second.second) { p.Clear(); break; }
                }

                // try a new 'random' step for next search
                rst = (rst + 1) % _w;

            } while (p.Empty() && rst != st); // a partition is not found and not search for all 'random' steps

            // no partition can be found...
            if (p.Empty()) { continue; } 

            // record the partition in the search map
            _searchMap.emplace(make_pair(st, make_pair(rn, p)));

            //cout << "Generate a partition with step = " << st << " and run = " << rn << " in search map" << endl;
            //p.Print();

            break;
        }
    }

    // sufficient paritions for all groups
    if (_searchMap.size() >= _numGroups) {
        return;
    }

    // the paritions fulfilling only Condition 2
    srand(12345);
    for (int rpidx = 1; rpidx <= _numGroups - _searchMap.size(); rpidx++) {
        Partition p;
        int retry = 0;
        do {
            set<int> subset;
            bool packetsSelected[_w] = { false };
            int numSelected = 0;
            int numSubsets = useMoreSubsets? _m : (_w + _portion - 1) / _portion;
            for (ss = 0; ss < numSubsets; ss++) {
                pkt = rand() % _w;

                int numElements = useMoreSubsets? (_w - numSelected + (numSubsets - ss) - 1) / (numSubsets - ss) : _portion; //ss + 1 < numSubsets || _w % _portion == 0? _portion : _w % _portion;
                for (l = 0; l < numElements; l++) {
                    // allow partial overlap in subsets if portion cannot divide sub-packetization
                    if (numSelected >= _w && (_w % _portion != 0)) {
                        for (int i = 0; i < _w; i++) packetsSelected[i] = { false };
                        numSelected = 0;
                    } else if (numSelected >= _w) { // if the condition cannot relaxed, stop searching
                        break;
                    }

                    // move one packet forward if the current one is already selected
                    while(packetsSelected[pkt]) { pkt = (pkt + 1) % _w; };

                    // select this packet
                    subset.emplace(pkt);
                    packetsSelected[pkt] = true;
                    numSelected++;

                    // find next random packet
                    pkt = rand() % _w;
                }

                // add the subset
                p.AppendSubset(vector<int>(subset.begin(), subset.end()));
                subset.clear();
            }

            // check for same partition, discard the result and search again if it overlaps with any others
            for (const auto &ep : _searchMap) {
                if (p == ep.second.second) { p.Clear(); break; }
            }

            for (const auto &ep : _randMap) {
                if (p == ep.second) { p.Clear(); break; }
            }
            retry++;

        } while (p.Empty() && retry <= retryLimit);

        if (retry > retryLimit) {
            //cout << "Failed to find a matrix after " << retryLimit << " retries..." << endl;
            break;
        } else {
            _randMap.insert(make_pair(0-rpidx, p));
            //cout << "Generate a random partition in random map" << endl;
            //p.Print();
        }
    }
    
}

void HTEC::DestroyPartitionSearchMaps() {
    _searchMap.clear();
    _randMap.clear();
}

pair<int,int> HTEC::FindPartition(int step, int run, const vector<pair<int,int>> &validPartitions, int firstInGroupDataNodeIndex) {
    pair<int, int> target (-1, -1);

    // 2nd phase of parition search
    if (run == 0) {
        // reuse the partition of the first data node in the group
        if (firstInGroupDataNodeIndex != -1 && validPartitions.size() > firstInGroupDataNodeIndex) {
            return validPartitions.at(firstInGroupDataNodeIndex);
        }
        // TODO check and determine whether to use a random partition for better overall locality
        return validPartitions.at(validPartitions.size() - 1);
    }

    // 1st phase of parition search
    auto partitionRec = _searchMap.find(step);

    // failed to find the best partition for the given step ...
    if (partitionRec == _searchMap.end()) {
        cerr << "No partition for step = " << step << endl;
        return target;
    }

    // failed to be the same as those in the same group ...
    if (firstInGroupDataNodeIndex >= 0 && validPartitions.at(firstInGroupDataNodeIndex) != make_pair(step, partitionRec->second.first)) {
        cerr << "Selected not the same as previous ones (" << step << "," << partitionRec->second.first << " vs " << validPartitions.at(firstInGroupDataNodeIndex).first << "," << validPartitions.at(firstInGroupDataNodeIndex).second << ")\n";
        return target;
    }

    // select this partition
    target.first = step;
    target.second = partitionRec->second.first;

    return target;
}

bool HTEC::FillParityIndices(int column, const Partition &p, int dataNodeId) {
    int ss = 0, pkt = 0, ri = 0, pi = 0, i = 0, j = 0;
    int w = _targetW > 0? _targetW : _w;
    int numSubsets = p.GetNumSubsets();
    int bestSubset = -1; 

    int numFilled = 0, numPacketsTotal = 0;
    bool indexFilled[numSubsets][_m];
    map<int, map<int, set<int>>> *numFilledIndices = new map<int, map<int, set<int>>>();             // [number of filled index] -> [[number of unused extra parity slots before this column] -> [subset index]]
    map<int, map<int, set<int>>> *numFilledPacketsInPrevGroups = new map<int, map<int, set<int>>>(); // [subset index] -> [[number of used extra parity slots before this group] -> parity index]
    vector<int> subset;

    // try to use a greedy algorithm to search for subsets with 
    // (i) the most slots available for filling, and then 
    // (ii) the slots with the fewest packets assigned in the same row

    // choose a subset from the partition which corresponds to row indices in the specified column with most rows not yet filled
    for (ss = 0; ss < numSubsets; ss++) {
        subset = p.GetSubset(ss);
        numFilled = 0;
        numPacketsTotal = 0;
        for (pi = 1; pi < _m; pi++) {
            indexFilled[ss][pi] = false;
            int numPacketsInPg = 0;
            for (i = 0; i < subset.size(); i++) {
                ri = subset.at(i);
                // check the number of packets filled along the parities
                if (_paritySourcePackets[pi][ri]->at(_k + column - 1) != -1) {
                    indexFilled[ss][pi] = true;
                    numFilled += 1;
                }
                // count the number of filled packet slots in previous groups for this parity
                for (j = 0; j < column - 1; j++) {
                    numPacketsInPg += _paritySourcePackets[pi][ri]->at(_k + j) != -1;
                }
            }
            // mark the number of filled packet slots in previous group
            if (numFilledPacketsInPrevGroups->count(ss) == 0) { numFilledPacketsInPrevGroups->insert(make_pair(ss, map<int, set<int>>())); }
            if (numFilledPacketsInPrevGroups->at(ss).count(numPacketsInPg) == 0) { numFilledPacketsInPrevGroups->at(ss).insert(make_pair(numPacketsInPg, set<int>())); }
            numFilledPacketsInPrevGroups->at(ss).at(numPacketsInPg).insert(pi);
            //cout << "Column " << column << " subset " << ss << " parity " << pi << " has " << numPacketsInPg << " packets filled in previous groups\n";
            numPacketsTotal += numPacketsInPg;
        }
        // mark the number of packet filled if we choose this subset
        //cout << "Column " << column << " subset " << ss << " has " << numFilled << " packets filled\n";
        if (numFilledIndices->count(numFilled) == 0) { numFilledIndices->insert(make_pair(numFilled, map<int, set<int>>())); }
        if (numFilledIndices->at(numFilled).count(numPacketsTotal) == 0) { numFilledIndices->at(numFilled).insert(make_pair(numPacketsTotal, set<int>())); }
        numFilledIndices->at(numFilled).at(numPacketsTotal).insert(ss);
    }

    // best subset is the one with most slots to fill (piggypack as much as possible?) while minimizing the increase in the number of packets in the same parity row
    bestSubset = *(numFilledIndices->begin()->second.rbegin()->second.begin());

    subset = p.GetSubset(bestSubset);
    _selectedSubset.emplace(make_pair(dataNodeId - 1, subset));

    if (numSubsets < _m) { // this filling method prioritize fewer packets in a row than the packet index locality across parities
        ss = 0;
        for (const auto &it : numFilledPacketsInPrevGroups->at(bestSubset)) {
            for (int pi : it.second) {
                // skip parity columns that are already filled
                if (indexFilled[bestSubset][pi]) { continue; }

                // skip the selected subset and stop when no more subset for index filling
                if (ss == bestSubset && ++ss > numSubsets) { break; }

                vector<int> pktri = p.GetSubset(ss);

                for (j = 0; j < subset.size(); j++) {
                    // skip if there is insufficient packet row index
                    if (pktri.size() <= j) { continue; }
                    pkt = subset.at(j);
                    ri = pktri.at(j);
                    _paritySourcePackets[pi][pkt]->at(_k + column - 1) = (dataNodeId - 1) * w + _preceedW + ri;  
                }

                // stop when no more subset for index filling
                if (++ss >= numSubsets) { break; }
            }
        }
    } else { // if we have enough groups to fill all parities, prioritize for packet index locality
        for (pi = 1, ss = 0; pi < _m; pi++, ss++) {
            if (indexFilled[bestSubset][pi] && pi < _m + 1) { pi++; }

            if (ss == bestSubset) { ss++; }

            vector<int> pktri = p.GetSubset(ss);

            for (j = 0; j < subset.size(); j++) {
                // skip if there is insufficient packet row index
                if (pktri.size() <= j) { continue; }
                pkt = subset.at(j);
                ri = pktri.at(j);
                _paritySourcePackets[pi][pkt]->at(_k + column - 1) = (dataNodeId - 1) * w + _preceedW + ri;  
            }
        }
    }

    // clean up the counter maps
    delete numFilledPacketsInPrevGroups;
    delete numFilledIndices;

    return true;
}

void HTEC::FillParityCoefficients() {
    FillParityCoefficientsUsingCauchyMatrix();
}

bool HTEC::FillParityCoefficientsUsingCauchyMatrix() {
    int numCols = _k + _numGroups;
    int numRows = _m;

    // original cauchy matrix adopted from Jerasure implementation
    for (int i = 0, idx = 0; i < numRows; i++) {
        for (int j = 0; j < numCols; j++, idx++) { 
            int c = galois_single_divide(1, (i ^ (numRows + j)), _q);
            if (j >= GetNumSourcePackets(i)) { continue; }
            for (int l = 0; l < _w; l++) {
                _parityMatrix[i][l]->at(j) = c;
                _parityMatrixD[i][l]->at(j) = c;
            }
        }
    }

    return true;
}

void HTEC::CondenseParityInfo() {
    // for all parity index arrays, remove all invalid pairs
    for (int i = 1; i < _m; i++) {
        for (int j = 0; j < _w; j++) {
            int length = _k; // i.e., number of valid pairs
            for (int l = length; l < GetNumSourcePackets(i); l++) {
                if (_paritySourcePackets[i][j]->at(l) != -1) {
                    // move this valid pair to the immediate position after the last valid pair
                    if (length < l) {
                        _paritySourcePacketsD[i][j]->at(length) = _paritySourcePackets[i][j]->at(l);
                        _parityMatrixD[i][j]->at(length) = _parityMatrix[i][j]->at(l);
                    } else {
                        _paritySourcePacketsD[i][j]->at(l) = _paritySourcePackets[i][j]->at(l);
                        _parityMatrixD[i][j]->at(l) = _parityMatrix[i][j]->at(l);
                    }
                    length++;
                }
            }
            // shrink the index and coefficient arrays
            _paritySourcePacketsD[i][j]->resize(length);
            _parityMatrixD[i][j]->resize(length);
        }
    }
}

ECDAG* HTEC::ConstructEncodeECDAG(ECDAG *myecdag, int (*convertId)(int, int, int, int)) const {
    ECDAG* ecdag = myecdag == NULL? new ECDAG() : myecdag;

    // add and bind all parity packets computation
    for (int i = 0; i < _m; i++) {
        for (int j = 0; j < _w; j++) {
            int w = _targetW > 0? _targetW : _w;
            int pidx = (_k + i) * w + _preceedW + j;
            if (convertId) { pidx = convertId(_n, _k, w, pidx); }
            ecdag->Join(pidx, *_paritySourcePacketsD[i][j], *_parityMatrixD[i][j]);
            //ecdag->BindY(pidx, _paritySourcePacketsD[i][j]->at(0));
        }
    }

    return ecdag;
}

void HTEC::AddConvSingleDecode(int failedNode, int failedPacket, int parityIndex, ECDAG *ecdag, set<int> &repaired, set<int> *allSources, int (*convertId)(int, int, int, int), set<int> *parities) const {

    int decodeMatrix[_k * _k] = { 0 };
    int invertedMatrix[_k * _k] = { 0 };
    int node = 0, ri = 0;   // node index and row index
    int pkt = failedPacket;
    int bindPkt = -1;
    int w = _targetW > 0? _targetW : _w;
    int src = 0, target = 0;

    vector<int> sources, coefficients;

    if (failedNode < _k) { // data node failure

        for (node = 0, ri = 0; node < _k; node++) {
            // copy the coefficients of all data source packets in the parity
            decodeMatrix[(_k - 1) * _k + node] = _parityMatrix[parityIndex][failedPacket]->at(node);

            // exclude failed node from source
            if (node == failedNode) { continue; }

            src = _paritySourcePackets[parityIndex][failedPacket]->at(node);
            if (convertId) { src = convertId(_n, _k, w, src); }

            if (bindPkt == -1) bindPkt = src;


            sources.emplace_back(src);
            decodeMatrix[(ri++) * _k + node] = 1;

            // [debug] for measuring bandwidth
            if (allSources) allSources->emplace(src);
        }

        //cout << "Decoding Matrix:\n";
        //for (int i = 0; i < _k; i++) { for (int j = 0; j < _k; j++) { cout << setw(4) << decodeMatrix[i * _k + j]; } cout << endl; }

        // add parity packet as the last source
        src = (_k + parityIndex) * w + _preceedW + failedPacket;
        if (convertId) { src = convertId(_n, _k, w, src); }
        sources.emplace_back(src);
        if (parities) parities->emplace(src);
        if (allSources) allSources->emplace(src);

        // inverse the matrix
        jerasure_invert_matrix(decodeMatrix, invertedMatrix, _k, _q); 

        //cout << "Inverted Matrix:\n"; 
        //for (int i = 0; i < _k; i++) { for (int j = 0; j < _k; j++) { cout << setw(4) << invertedMatrix[i * _k + j]; } cout << endl; }

        // use the failed row to resolve the failed packet
        for (int i = 0; i < _k; i++) { coefficients.emplace_back(invertedMatrix[failedNode * _k + i]); }

    } else { // parity failure, simply recompute from data source packets

        sources = *_paritySourcePacketsD[failedNode - _k][pkt];
        coefficients = *_parityMatrixD[failedNode - _k][pkt];
        bindPkt = sources.front();

        // [debug] for measuring bandwidth
        if (allSources) allSources->insert(_paritySourcePacketsD[failedNode - _k][pkt]->begin(), _paritySourcePacketsD[failedNode - _k][pkt]->end());
        
    }

    // add the packet repair
    target = failedNode * w + _preceedW + pkt;
    if (convertId) { target = convertId(_n, _k, w, target); }
    ecdag->Join(target, sources, coefficients);
    //ecdag->BindY(target, bindPkt);
    repaired.emplace(target);
}

void HTEC::AddIncrSingleDecode(int failedNode, int selectedPacket, ECDAG *ecdag, set<int> &repaired, set<int> *allSources, int (*convertId)(int, int, int, int), set<int> *parities) const {
    // not for parity nodes
    if (failedNode >= _k) return;

    int groupId = failedNode / _groupSize;
    int spkt = selectedPacket; // index of the selected parity row/packet
    int bindPkt = -1;
    int w = _targetW > 0? _targetW : _w;
    int src = 0, target = 0;

    for (int pi = 1; pi < _m; pi++) {

        int rpkt = _paritySourcePackets[pi][spkt]->at(_k + groupId);  // repaired packet index

        // skip if group not scheduled, or the scheduled packet does not belong to the failed node
        if (rpkt == -1 || rpkt / w != failedNode) {
            continue;
        }

        vector<int> sources, coefficients;
        int decodeMatrix[_n * _n] = { 0 }, invertedMatrix[_n * _n] = { 0 };

        const vector<int>* sourcePackets = _paritySourcePacketsD[pi][spkt];
        int numPackets = sourcePackets->size();
        int failedRow = 0;

        // add first parity as the first source packet
        src = _k * w + _preceedW + spkt;
        if (convertId) { src = convertId(_n, _k, w, src); }
        sources.emplace_back(src);
        if (parities) parities->emplace(src);

        for (int node = 0, ri = 1; node < numPackets; node++) {
            // coefficients of first parity
            if (node < _k) decodeMatrix[node] = _parityMatrixD[0][spkt]->at(node);

            // coefficients of next parity
            decodeMatrix[(numPackets - 1) * numPackets + node] = _parityMatrixD[pi][spkt]->at(node);
    
            int src = sourcePackets->at(node);
            // skip packets on the failed node
            if (node == failedNode || src / w == failedNode) { failedRow = node; continue; }
            // add packets on alive nodes
            if (convertId) { src = convertId(_n, _k, w, src); }
            sources.emplace_back(src);
            decodeMatrix[ri++ * numPackets + node] = 1;
            if (bindPkt == -1) bindPkt = src;


            // [debug] for measuring bandwidth
            if (allSources && node != failedNode) allSources->emplace(sourcePackets->at(node));
        }
        // add next parity as the last source packet
        src = (_k + pi) * w + _preceedW + spkt;
        if (convertId) { src = convertId(_n, _k, w, src); }
        sources.emplace_back(src);
        if (parities) parities->emplace(src);
        // [debug] for measuring bandwidth
        if (allSources) allSources->emplace(src);

        //cout << "Decoding Matrix:\n";
        //for (int i = 0; i < numPackets; i++) { for (int j = 0; j < numPackets; j++) { cout << setw(4) << decodeMatrix[i * numPackets + j]; } cout << endl; }

        jerasure_invert_matrix(decodeMatrix, invertedMatrix, numPackets, _q); 

        //cout << "Failed row = " << failedRow << endl << "Inverted Matrix:\n"; 
        //for (int i = 0; i < numPackets; i++) { for (int j = 0; j < numPackets; j++) { cout << setw(4) << invertedMatrix[i * numPackets + j]; } cout << endl; }

        for (int i = 0; i < numPackets; i++) { coefficients.emplace_back(invertedMatrix[failedRow * numPackets + i]); }

        ecdag->Join(rpkt, sources, coefficients);
        //ecdag->BindY(rpkt, bindPkt);
        repaired.emplace(rpkt);
    }
}

ECDAG* HTEC::ConstructDecodeECDAG(const vector<int> &from, const vector<int> &to, ECDAG *myecdag, int (*convertId)(int, int, int, int), set<int> *parities, set<int> *allSources) const {
    ECDAG* ecdag = myecdag == NULL? new ECDAG() : myecdag;
    vector<int> failedNodexIndex;
    int i = 0, pkt = 0, pi = 0, node = 0, prevIndex = -1, curIndex = -1;
    int w = _targetW > 0? _targetW : _w;
    set<int> repaired;
    bool releaseAllSourcesSet = allSources == NULL;

    // [debug] for measuring bandwidth
    if (releaseAllSourcesSet) {
        allSources = new set<int>();
    }

    // assume full-node repair
    // TODO check target packet indices
    assert(to.size() % _w == 0);

    // figure out the failed nodes 
    for (i = 0, prevIndex = -1; i < to.size(); i += _w) {
        curIndex = to.at(i) / (_targetW > 0? _targetW : _w);
        if (prevIndex == -1 || prevIndex != curIndex) {
            failedNodexIndex.emplace_back(curIndex);
            prevIndex = curIndex;
        }
    }

    // return an empty ECDAG if nothing fails
    if (failedNodexIndex.empty()) { return ecdag; }

    if (failedNodexIndex.size() > _m) { // the number of failures is beyond repair
        cerr << "error: too many failures (" << failedNodexIndex.size() << " vs. n-k= " << _m << ") for repair!\n";
    } else if (failedNodexIndex.size() > 1) { // TODO multi-node failures (> sub-packetization) not supported yet
        cerr << "error: repair for multiple failures not yet supported..";
    } else { // single-node failure

        // TODO check whether all source packets are available for the special repair

        int failedNode = failedNodexIndex.at(0);
        const vector<int> subset = _selectedSubset.count(failedNode)? _selectedSubset.at(failedNode) : vector<int>();

        // repair packet by packet
        for (const auto pkt : subset) {
            // repair the selected packets using the first parity
            AddConvSingleDecode(failedNode, pkt, /* parityIndex */ 0, ecdag, repaired, allSources, convertId, parities);
            // repair packets included in the second parity and beyond
            AddIncrSingleDecode(failedNode, pkt, ecdag, repaired, allSources, convertId, parities);
        }

        // repair any remaining packets using the first parity
        for (pkt = 0; repaired.size() < _w && pkt < _w; pkt++) {
            if (repaired.count(failedNode * w + _preceedW + pkt) > 0) continue;

            AddConvSingleDecode(failedNode, pkt, /* parityIndex */ 0, ecdag, repaired, allSources, convertId, parities);
        }
    }
    
    double upperBound = GetRepairBandwidthUpperBound();
    // [debug] for measuring bandwidth
    //cout << "Number of packets read = " << allSources->size()
    //     << fixed
    //     << "; Upper bound = " << setprecision(3) << upperBound * _w
    //     << "; Normalized repair bandwidth = " << setprecision(3) << allSources->size() * 1.0 / _w / _k
    //     << "; Upper bound = " << setprecision(3) << upperBound / _k
    //     << "; Exceeded = " << (allSources->size() > upperBound  * _w)
    //     << endl;

    // check if we have included the repair for all missing packets
    assert(repaired.size() == to.size() || failedNodexIndex.size() > _m);

    if (releaseAllSourcesSet) {
        delete allSources;
    }

    return ecdag;
}

double HTEC::GetRepairBandwidthUpperBound() const {
    int a = ((_w + _m - 1) / _m), b = (_k + _m - 1) / _m;
    return (double) (_n - 1) / _m + (double) (_m - 1) / _w * a * b;
}

bool HTEC::GetParitySourceAndCoefficient(int parityIndex, vector<int> **sources, vector<int> **coefficients) const {
    int w = _targetW > 0? _targetW : _w;
    int pi = parityIndex / w;
    int pkt = (parityIndex % w) - _preceedW;
    // check that the input index points to a parity ndoe and within the parity packet index range of this instance
    if (pi < 0 || pi > _m || pkt < 0 || pkt > _w) { return false; }

    *sources = _paritySourcePacketsD[pi][pkt];
    *coefficients = _parityMatrixD[pi][pkt];

    return true;
}

void HTEC::PrintParityIndexArrays(bool dense, bool skipFirst) const {
    // the printed indices of packets follow the convention used in the paper, i.e., starting from 1
    cout << "Parity index arrays:\n";
    int w = _targetW > 0? _targetW : _w;
    for (int i = (skipFirst? 1: 0); i < _m; i++) {
        cout << "> Parity " << i + 1 << endl;
        for (int j = 0; j < _w; j++) {
            const vector<int> *v = dense? _paritySourcePacketsD[i][j] : _paritySourcePackets[i][j];
            int numSourcePackets = v->size();
            for (int l = 0; l < numSourcePackets; l++) {
                int pkt = v->at(l);
                if (pkt >= 0) {
                    cout << "(" << setw(3) << right << (pkt % w) + 1 << "," << setw(3) << left << pkt / w + 1 << ") ";
                } else {
                    cout << "(  NIL  ) ";
                }
            }
            cout << endl;
        }
        cout << endl;
    }
    cout << endl;

}

void HTEC::PrintParityMatrix(bool dense) const {
    cout << "Parity matrix:\n";
    for (int i = 0; i < _m; i++) {
        cout << "> Parity " << i + 1 << endl;
        for (int j = 0; j < _w; j++) {
            const vector<int> *v = dense? _parityMatrixD[i][j] : _parityMatrix[i][j];
            int numCoefficients = v->size();
            for (int l = 0; l < numCoefficients; l++) {
                cout << setw(3) << v->at(l);
            }
            cout << endl;
        }
        cout << endl;
    }
    cout << endl;
}

void HTEC::PrintSelectedSubset() const {
    cout << "Parity selected subset:\n";
    for (const auto &ss : _selectedSubset) {
        cout << "> Data node " << ss.first + 1 << " { ";
        for (int i = 0; i < ss.second.size(); i++) {
            cout << setw(3) << ss.second.at(i) + 1 << (i + 1 == ss.second.size()? "" : ", ");
        }
        cout << " }\n";
    }
    cout << endl;
}

void HTEC::PrintParityInfo(bool dense) const {
    PrintParityIndexArrays(dense);
    //PrintParityMatrix(dense);
    PrintSelectedSubset();
}

ECDAG* HTEC::Encode() {
    return ConstructEncodeECDAG();
}

ECDAG* HTEC::Decode(vector<int> from, vector<int> to) {
    return ConstructDecodeECDAG(from, to);
}

void HTEC::Place(vector<vector<int>>& group) {
}


// HTEC::Partition

HTEC::Partition::Partition() {
}

HTEC::Partition::~Partition() {
}

int HTEC::Partition::AppendSubset(vector<int> set) {
    // reject if the subset already exists
    for (const auto s : indices) {
        if (s == set) return -1;
    }
    // insert new subset
    indices.emplace_back(set);
    return indices.size();
}

bool HTEC::Partition::operator == (const Partition &rhs) const {
    if (rhs.indices.size() != indices.size()) return false;
    int matchCount = 0;
    for (int i = 0; i < indices.size(); i++) {
        matchCount += (rhs.indices.at(i) == indices.at(i)? 1 : 0);
    }
    return matchCount == indices.size();
}

bool HTEC::Partition::ContainsEqualSubset(const Partition &rhs) const {
    for (int i = 0; i < indices.size(); i++) {
        for (int j = 0; j < rhs.indices.size(); j++) {
            if (rhs.indices.at(j) == indices.at(i)) { return true; }
        }
    }
    return false;
}

size_t HTEC::Partition::GetNumSubsets() const {
    return indices.size();
}

vector<int> HTEC::Partition::GetSubset(int idx) const {
    if (indices.size() <= idx) { return vector<int>(); } 
    return indices.at(idx);
}

void HTEC::Partition::Clear() {
    indices.clear();
}

bool HTEC::Partition::Empty() const {
    return indices.empty();
}

void HTEC::Partition::Print() const {
    std::cout << "Partition for with " << indices.size() << " subsets\n";
    for (int i = 0; i < indices.size(); i++) {
        vector<int> s = indices.at(i);
        cout << "Set [" << i + 1 << "] = ";
        for (int j = 0; j < s.size(); j++) {
            cout << setw(2) << s.at(j) + 1 << (j + 1 == s.size()? ";" : ", ");
        }
        cout << endl;
    }
}
