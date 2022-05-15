#include "HTEC.hh"

#include <assert.h>
#include <exception>
#include <iomanip>

int retryLimit = 10;

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

    unsigned long int comb[4];
    for (unsigned long int i = 1, total = 1; i <= _n; i++) {
        total *= i;
        if (i == _m) { comb[0] = total; }
        if (i == _k) { comb[1] = total; }
        if (i == _n) { comb[2] = total; }
    }
    comb[3] = comb[2] / comb[0] / comb[1] * _m * _w;
    //cout << "Comb " << comb[0]  << " " << comb[1] << " " << comb[2]  << " " << comb[3] << endl;
    if (_q < comb[3]) {
        cerr << "warning: guaranteed lower bound on field size for an MDS code is " << comb[3] << " which is larger than 32.\n";
    } else if (comb[3] <= 16) {
        _q = 16;
    }

    // coding optimization option
    _opt = opt;

    // HTEC-specific variables
    _numGroups = (_k + _m - 1) / _m;
    _groupSize = _m;
    _portion = (_w + _m - 1) / _m;

    cout << "HTEC: n = " << _n << " k = " << _k << " alpha = " << _w << " group size = " << _groupSize << " portion = " << _portion << endl;

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
                _paritySourcePackets[i][j]->at(l) = l * _w + j;
                _paritySourcePacketsD[i][j]->at(l) = l * _w + j;
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

        pair<int, int> np = FindPartition(step, run, validPartitions, (dataNodeId % _groupSize == 1? -1 : (dataNodeId - 1) / _groupSize * _groupSize));
        if (np.first == -1 && np.second == -1) {
            cerr << "Failed to find a valid partition for data node " << dataNodeId << " with step = " << step << " and run = " << run << endl;
            assert(!validPartitions.empty());
            // reuse the last found partition (giving up on Condition 2)
            np = validPartitions.at(validPartitions.size() - 1);
            //throw std::exception();
        } else {
            cout << "(1st phase) Find partition for data node " << dataNodeId << " with step = " << np.first << " (" << step << ") and run = " << run << " (" << run << ")\n";
            if (dataNodeId % _m == 0) GetPartition(np).Print();

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
            cerr << "(2nd phase) Failed to find the partition for data node " << dataNodeId << endl;
            assert(!validPartitions.empty());
            // reuse the last found partition (giving up on Condition 2)
            np = validPartitions.at(validPartitions.size() - 1);
            //throw std::exception();
        } else {
            cout << "Find partition for data node " << dataNodeId << "(" << np.first << "," << np.second << ")\n";
        }
        validPartitions.emplace_back(np);
        if (dataNodeId == _k) GetPartition(np).Print();
        FillParityIndices(v, GetPartition(np), dataNodeId);
    }

    PrintParityInfo();

    DestroyPartitionSearchMaps();

    CondenseParityInfo();

    PrintParityInfo(/* dense */ true);

    FillParityCoefficients();
}

void HTEC::InitPartitionSearchMaps() {

    int st = 0; // step counter
    int rn = 0; // run counter
    int ss = 0; // subset counter
    int l = 0;  // element counter
    int pkt = 0; // packet index
    int rst = 0; // 'random' step

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
                int numSubsets = (_w + _portion - 1) / _portion;

                // search for subsets in the partition
                for (ss = 0; ss < numSubsets; ss++) {
                    pkt = 0;

                    int numElements = _portion; //ss + 1 < numSubsets || _w % _portion == 0? _portion : _w % _portion;
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

                // check for overlap
                for (const auto &ep : _searchMap) {
                    // discard the result if it overlaps with any others... (and search again)
                    if (p.ContainsEqualSubset(ep.second.second)) { p.Clear(); break; }
                }

                // try a new 'random' step for next search
                rst = (rst + 1) % _w;

            } while (p.Empty() && rst != st); // a partition is not found and not search for all 'random' steps

            // no partition can be found...
            if (p.Empty()) { continue; } 

            // record the partition in the search map
            _searchMap.emplace(make_pair(st, make_pair(rn, p)));

            cout << "Generate a partition with step = " << st << " and run = " << rn << " in search map" << endl;
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
            vector<int> subset;
            bool packetsSelected[_w] = { false };
            int numSelected = 0;
            int numSubsets = (_w + _portion - 1) / _portion;
            for (ss = 0; ss < numSubsets; ss++) {
                pkt = rand() % _w;
                int numElements = _portion; //ss + 1 < numSubsets || _w % _portion == 0? _portion : _w % _portion;
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
                    subset.push_back(pkt);
                    packetsSelected[pkt] = true;
                    numSelected++;

                    // find next random packet
                    pkt = rand() % _w;
                }

                // add the subset
                p.AppendSubset(subset);
                subset.clear();
            }

            // check for overlap, discard the result and search again if it overlaps with any others
            for (const auto &ep : _searchMap) {
                if (p.ContainsEqualSubset(ep.second.second)) { p.Clear(); break; }
            }

            for (const auto &ep : _randMap) {
                if (p.ContainsEqualSubset(ep.second)) { p.Clear(); break; }
            }
            retry++;

        } while (p.Empty() && retry <= retryLimit);

        if (retry > retryLimit) {
            cout << "Failed to find a matrix after " << retryLimit << " retries..." << endl;
            break;
        } else {
            _randMap.insert(make_pair(0-rpidx, p));
            cout << "Generate a random partition in random map" << endl;
            p.Print();
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

        size_t numValidPartitions = validPartitions.size();

        // first, use any remaining unselected parition from _searchMap
        if (_searchMap.size() != numValidPartitions / _m) {
            for (const auto &pit : _searchMap) {
                bool found = false;
                for (const pair<int, int>& ep : validPartitions)
                    if (ep.first == pit.first) { found = true; break; }
                if (!found) {
                    target = make_pair(pit.first, pit.second.first);
                    return target;
                }
            }
        } 

        assert(numValidPartitions > 0);

        // then, if nothing left, use parition from _randMap
        const pair<int, int> lastPartition = validPartitions.at(numValidPartitions - 1);
        if (lastPartition.first < 0) {
            assert(_randMap.size() >= 0 - lastPartition.second + 1);
            target = make_pair(lastPartition.first - 1, 0);
        } else if (!_randMap.empty()) {
            target = make_pair(-1, 0);
        }

        return target;
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

    // check if this is has any overlap subset across all selected partitions
    // (skip paritions in the same data group, which are always the same)
    int end = firstInGroupDataNodeIndex == -1? validPartitions.size() : firstInGroupDataNodeIndex;
    for (int i = 0; i < end; i++) {
        auto pidx = validPartitions.at(i);
        pair<int, Partition> pit = _searchMap.at(pidx.first);
        // failed due to overlap
        assert(pit.first == pidx.second);
        if (pit.second.ContainsEqualSubset(partitionRec->second.second)) {
            cerr << "Overlap found with data node " << i << endl;
            return target;
        }
    }

    // select this partition
    target.first = step;
    target.second = partitionRec->second.first;

    return target;
}

bool HTEC::FillParityIndices(int column, const Partition &p, int dataNodeId) {
    int ss = 0, pkt = 0, ri = 0, pi = 0, i = 0, j = 0;
    int numSubsets = p.GetNumSubsets();

    // choose a subset from the partition which corresponds to row indices in the specified column are not yet filled
    for (ss = 0; ss < numSubsets; ss++) {
        vector<int> subset = p.GetSubset(ss);


        // check if any targeted index in this subset has been filled
        int numFilled = 0;
        bool indexFilled[_m] = { false };
        for (i = 1; i < _m; i++) {
            for (j = 0; j < subset.size() && indexFilled; j++) {
                ri = subset.at(j);
                if (_paritySourcePackets[i][ri]->at(_k + column - 1) != -1) {
                    indexFilled[i] = true;
                    numFilled += 1;
                    break;
                }
            }
        }

        // skip if target packets by indices in this subset have been filled in all parity index arrays
        if (numFilled == _m - 1) {
            //cout << "Index filled for ss = " << ss << ", column = " << column << " data node id = " << dataNodeId << endl;
            continue;
        }

        _selectedSubset.emplace(make_pair(dataNodeId - 1, subset));

        // fill out the indices, from the first to the last subset and skip the selected one
        for (pi = 1, i = 0; pi < _m; pi++, i++) {

            while (indexFilled[pi] && pi < _m - 1) pi++;

            if (i == ss) i++;

            vector<int> pktri = p.GetSubset(i);

            // skip if there is insufficient packet row index
            if (subset.size() > pktri.size()) { continue; }

            for (j = 0; j < subset.size(); j++) {
                pkt = subset.at(j);
                ri = pktri.at(j);
                _paritySourcePackets[pi][pkt]->at(_k + column - 1) = (dataNodeId - 1) * _w + ri;  
            }
        }

        return true;
    }

    return false;
}

void HTEC::FillParityCoefficients() {
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

ECDAG* HTEC::ConstructEncodeECDAG() const {
    ECDAG* ecdag = new ECDAG();

    // add and bind all parity packets computation
    for (int i = 0; i < _m; i++) {
        for (int j = 0; j < _w; j++) {
            int pidx = (_k + i) * _w + j;
            ecdag->Join(pidx, *_paritySourcePacketsD[i][j], *_parityMatrixD[i][j]);
            ecdag->BindY(pidx, 0);
        }
    }

    return ecdag;
}

ECDAG* HTEC::ConstructDecodeECDAG(const vector<int> &from, const vector<int> &to) const {
    ECDAG* ecdag = new ECDAG();
    vector<int> failedNodexIndex;
    int i = 0, pkt = 0, pi = 0, node = 0, prevIndex = -1, curIndex = -1;
    set<int> repaired;


    // assume full-node repair
    // TODO check target packet indices
    assert(to.size() % _w == 0);

    // figure out the failed nodes 
    for (i = 0, prevIndex = -1; i < to.size(); i += _w) {
        curIndex = to.at(i) / _w;
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

        vector<int> source;
        vector<int> coefficients;

        // repair packet by packet
        for (const auto pkt : subset) {
            source.clear();
            coefficients.clear();

            // repair the packets included only in the first parity: using all systematic nodes and first parity
            for (node = 0; node < _k + 1; node++) {
                // exclude failed node
                if (node == failedNode) { continue; }

                source.emplace_back(node * _w + pkt);

                // coefficient for parity packet is 1
                int c = node == _k? 1 : _parityMatrix[0][pkt]->at(node);
                coefficients.emplace_back(c);
            }
            // add the packet repair
            ecdag->Join(failedNode * _w + pkt, source, coefficients);
            repaired.emplace(pkt); 

            // repair packets included in the second parity and beyond
            for (pi = 1; pi < _m; pi++) {
                const vector<int>* sourcePackets = _paritySourcePackets[pi][pkt];
                int groupId = failedNode / _groupSize;
                int numExtraGroups = sourcePackets->size() - _k;
                // skip if group not scheduled, or the scheduled packet does not belong to the failed node
                if (numExtraGroups <= groupId || sourcePackets->at(_k + groupId) / _w != failedNode) {
                    continue;
                }
                // add source data packets
                source.clear();
                coefficients.clear();
                for (i = 0; i < sourcePackets->size(); i++) {
                    // skip the group of the failed node
                    if (i == _k + groupId) { continue; }

                    source.emplace_back(sourcePackets->at(i));
                    coefficients.emplace_back(_parityMatrix[pi][pkt]->at(i));
                }
                // add parity packet itself
                source.push_back((_k + pi) * _w + pkt);
                coefficients.push_back(1);

                // add the packet for repair
                int rpkt = sourcePackets->at(_k + groupId);
                ecdag->Join(rpkt, source, coefficients);
                repaired.emplace(rpkt % _w); 
            }
        }

        // repair any remaining packets using the first parity
        for (pkt = 0; repaired.size() < _w && pkt < _w; pkt++) {
            if (repaired.count(pkt) > 0) continue;

            source.clear();
            coefficients.clear();

            for (node = 0; node < _k + 1; node++) {
                // exclude failed node
                if (node == failedNode) { continue; }

                source.emplace_back(node * _w + pkt);

                // coefficient for parity packet is 1
                int c = node == _k? 1 : _parityMatrix[0][pkt]->at(node);
                coefficients.emplace_back(c);
            }

            // the packet repair
            ecdag->Join(failedNode * _w + pkt, source, coefficients);
            repaired.emplace(pkt); 
        }
    }

    // check if we have included the repair for all missing packets
    assert(repaired.size() == to.size() || failedNodexIndex.size() > _m);

    return ecdag;
}

void HTEC::PrintParityIndexArrays(bool dense) const {
    // the printed indices of packets follow the convention used in the paper, i.e., starting from 1
    cout << "Parity index arrays:\n";
    for (int i = 0; i < _m; i++) {
        cout << "> Parity " << i + 1 << endl;
        for (int j = 0; j < _w; j++) {
            const vector<int> *v = dense? _paritySourcePacketsD[i][j] : _paritySourcePackets[i][j];
            int numSourcePackets = v->size();
            for (int l = 0; l < numSourcePackets; l++) {
                int pkt = v->at(l);
                if (pkt >= 0) {
                    cout << "(" << setw(3) << right << (pkt % _w) + 1 << "," << setw(3) << left << pkt / _w + 1 << ") ";
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
    PrintParityMatrix(dense);
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
            cout << s.at(j) + 1 << (j + 1 == s.size()? ";" : ", ");
        }
        cout << endl;
    }
}
