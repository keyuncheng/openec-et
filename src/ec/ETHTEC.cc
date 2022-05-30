#include "ETHTEC.hh"

#include <iomanip>
extern "C" {
#include "../util/jerasure.h"  // jerasure_invert_matrix()
}

const char *ETHTEC::_useLargerBaseKey = "lb";

ETHTEC::ETHTEC(int n, int k, int alpha, int opt, vector<string> param) {

    // coding parameters
    _n = n;
    _k = k;
    _m = _n - _k;
    _w = alpha;
    _q = 8;

    // transformation parameter
    _e = 2;

    // ET-HTEC specific parameters
    _useLargerBase = false;
    for (const auto &s : param) {
        if (s == _useLargerBaseKey) { _useLargerBase = true; }
    }

    if (_n <= 0 || k <= 0 || _w <= 0) {
        throw invalid_argument("error: n, k, w must be positive.");
    }

    // should it be _n -_k + 1 to meet the supported sub-packetization range?
    if (_n < _k) {
        throw invalid_argument("error: n must be larger than k.");
    }

    // limit the supported sub-packetization
    if (_w < 4) {
        throw invalid_argument("error: w must be larger than 4.");
    }

    // determine the number of base code instances and the base code sub-packetization
    _numInstances = 2;
    _baseW = _w / 2;
    if (_useLargerBase) {
        for (int i = 2; i <= _w / 2; i++) { if (_w % i == 0) { _numInstances = i; break; } }
        _baseW = _w / _numInstances;
    } else {
        for (int i = 2; i <= _w / 2; i++) { if (_w % i == 0 && _w / i <= _m) { _baseW = i; break; } }
        _numInstances = _w / _baseW;
    }

    if (_w % _baseW != 0) {
        throw invalid_argument("error: w must not be a prime.");
    }

    cout << "ET-HTEC: n = " << _n << " k = " << _k << " alpha = " << _w << " base w = " << _baseW << " number of instances = " << _numInstances << " use larger base = " << _useLargerBase << endl;

    // initialize the instances (let each instance be awared of its offset)
    _instances = new HTEC*[_numInstances];
    for (int i = 0; i < _numInstances; i++) {
        vector<string> params;
        params.push_back(string(HTEC::_targetWKey).append(to_string(_w)));
        params.push_back(string(HTEC::_preceedWKey).append(to_string(_baseW * i)));
        _instances[i] = new HTEC(_n, _k, _baseW, opt, params);
    }

    // initialize the transformation maps
    _transformationSourcePackets = new vector<int>** [_m];
    _transformationMatrix = new vector<int>** [_m];
    _reverseTransformationSourcePackets = new vector<int>** [_m];
    _reverseTransformationMatrix = new vector<int>** [_m];
    for (int i = 0; i < _m; i++) {
        _transformationSourcePackets[i] = new vector<int>* [_w];
        _transformationMatrix[i] = new vector<int>* [_w];
        _reverseTransformationSourcePackets[i] = new vector<int>* [_w];
        _reverseTransformationMatrix[i] = new vector<int>* [_w];
        for (int j = 0; j < _w; j++) {
            _transformationSourcePackets[i][j] = new vector<int>();
            _transformationMatrix[i][j] = new vector<int>();
            _reverseTransformationSourcePackets[i][j] = new vector<int>();
            _reverseTransformationMatrix[i][j] = new vector<int>();
        }
    }

    FillTransformationMaps();

    //PrintParityInfo();
}

ETHTEC::~ETHTEC() {
    delete [] _instances;
    delete [] _transformationSourcePackets;
    delete [] _transformationMatrix;
    delete [] _reverseTransformationSourcePackets;
    delete [] _reverseTransformationMatrix;
}

void ETHTEC::FillTransformationMaps() {
    // group-based permutation, pair-coupling
    FillMapWithPermutedIndices();
    CouplePairsInParities();
    // construct the decoding map for original parities (mainly for data decoding and supplement parity decoding)
    ConstructDecodingMap();

    //PrintSourcePackets(/* transform */ true);
    //PrintSourcePackets(/* transform */ false);
}

void ETHTEC::FillMapWithPermutedIndices() {
    int p = 0, pkt = 0;
    int groupParities = _m % _numInstances == 0? _numInstances : _m; // form square matrix whenever possible
    int groupStripes = _numInstances; // each group takes one stripe from each code instance

    cout << "group size: row = " << groupStripes << " col = " << groupParities << endl;

    for (pkt = 0; pkt < _w; pkt++) {
        for (p = 0; p < _m; p++) {
            // find the new parity node for the target parity packet
            int pInGroup = p % groupParities;
            int pGroupOffset = p / groupParities * groupParities;
            int pi = pGroupOffset + (pInGroup - (pkt / _baseW) + groupParities) % groupParities;
            // the original parity packet and its target packet position
            int src = p * _w + pkt, target = pi * _w + pkt;
            _transformationSourcePackets[pi][pkt]->emplace_back(ParityToVirtualNode(LocalToGlobal(src)));
            _transformationMatrix[pi][pkt]->emplace_back(1);
            // add a reverse mapping from target position back to the original parity
            _reverseTransformationSourcePackets[p][pkt]->emplace_back(target);
        }
    }
}

void ETHTEC::CouplePairsInParities() {
    int p = 0, pkt = 0;
    int groupParities = _m % _numInstances == 0? _numInstances : _m; // form square matrix whenever possible
    int groupStripes = _numInstances; // each group takes one stripe from each instances
    int groupParityOffsetStep = _m % _numInstances == 0? _numInstances : _m % _numInstances;

    cout << "group size: row = " << groupStripes << " col = " << groupParities << " group parity step = " << groupParityOffsetStep << endl;

    for (int groupParityOffset = 0; groupParityOffset + groupStripes <= _m; groupParityOffset += groupParityOffsetStep) { // groups along the parities
        for (pkt = 0; pkt < _w; pkt++) { // along the code instances
            int instanceId = pkt / _baseW;
            int pktInInstance = pkt % _baseW;
            for (p = instanceId; p < groupStripes; p++) { // group base coupling, square matrix

                // check only packets in the 'lower left triangle' in each group (with parity as rows and sub-stripe as columns)
                // i and j are the (parity) row index and (packet) column index in the group
                int i = p;
                int j = instanceId;

                // skip diagonal packets
                if (i == j) { continue; }

                int srcPi = groupParityOffset + p;
                int srcPkti = pkt;
                int targetPi = groupParityOffset + j;
                int targetPkti = i * _baseW + pktInInstance;

                //int srcNode = VirtualNodeToParity(_transformationSourcePackets[groupParityOffset + p][pkt]->at(0));
                //int targetNode = VirtualNodeToParity(_transformationSourcePackets[groupParityOffset + j][i * _baseW + pktInInstance]->at(0));
                //cout << "Coupling (" << (srcNode % _w) + 1<< "," << (srcNode / _w) + 1 << ") against (" << (targetNode % _w) + 1 << "," << (targetNode / _w) + 1 << ")\n";

                // couple entries
                vector<int> *src = _transformationSourcePackets[srcPi][srcPkti];
                vector<int> *srcC = _transformationMatrix[srcPi][srcPkti];
                vector<int> *target = _transformationSourcePackets[targetPi][targetPkti];
                vector<int> *targetC = _transformationMatrix[targetPi][targetPkti];

                int numSrc = src->size();
                int numTarget = target->size();

                // (add the composite of source to target)
                for (auto it = src->begin(), cit = srcC->begin(); it != src->end(); it++, cit++) {
                    // add source entries to target
                    target->emplace_back(*it);
                    int pi = GlobalToLocal(VirtualNodeToParity(*it));
                    int spi = pi / _w;
                    int spkti = pi % _w;
                    // add source coefficients
                    targetC->emplace_back(*cit);
                    // also update the decouping map
                    for (int ti = 0; ti < numTarget; ti++) {
                        int t = GlobalToLocal(VirtualNodeToParity(target->at(ti)));
                        int tpi = t / _w;
                        int tpkti = t % _w;
                        //cout << " add to " << t << " (" << tpi + 1 << "," << tpkti + 1 << ")\n";
                        _reverseTransformationSourcePackets[spi][spkti]->emplace_back(_reverseTransformationSourcePackets[tpi][tpkti]->at(0));
                    }
                }

                // (update the coefficients of source)
                for (int i = 0; i < numSrc; i++) { srcC->at(i) = galois_single_multiply(srcC->at(i), _e, _q); }

                vector<int>::iterator targetEnd = target->begin();
                targetEnd += numTarget;
                // (add the composite of target to source)
                for (int ti = 0; ti < numTarget; ti++) {
                    // add target entries to source
                    int t = target->at(ti);
                    src->emplace_back(t);
                    int pi = GlobalToLocal(VirtualNodeToParity(t));
                    int spi = pi / _w;
                    int spkti = pi % _w;
                    // add target coefficients
                    srcC->emplace_back(targetC->at(ti));
                }
            }
        }
    }
}

void ETHTEC::ConstructDecodingMap() {
    bool done[_m * _w] = { false };

    for (int i = 0; i < _m * _w; i++) {
        // skip the packet if its decoding equation has been found when decoding other packets
        if (done[i]) { continue; }

        int pi = i / _w;
        int pkti = i % _w;
        vector<int> mapped = *_reverseTransformationSourcePackets[pi][pkti];
        int numMapped = mapped.size();

        map<int, int> packetColumnIndex; // packet -> column mapping of the decoding matrix
        int nextColumnIndex = 0;
        int decodeMatrix[_m * _m] = { 0 }, invertedMatrix[_m *_m] = { 0 };

        // copy the coefficients to the decoding matrix
        for (int mi = 0; mi < numMapped; mi++) {
            int t = mapped.at(mi);
            int tpi = t / _w;
            int tpkti = t % _w;

            vector<int> *sources = _transformationSourcePackets[tpi][tpkti];
            vector<int> *cs = _transformationMatrix[tpi][tpkti];
            int numSrc = sources->size();

            for (int si = 0; si < numSrc; si++) {
                int s = sources->at(si);
                if (packetColumnIndex.count(s) == 0) {
                    packetColumnIndex.insert(make_pair(s, nextColumnIndex++));
                }
                int col = packetColumnIndex.at(s);
                decodeMatrix[mi * numMapped + col] = cs->at(si);
            }
        }

        //for (int i = 0; i < numMapped; i++) { for (int j = 0; j < numMapped; j++) { cout << "[" << decodeMatrix[i * numMapped + j] << "]"; } cout << endl; }

        // invert the decoding matrix
        jerasure_invert_matrix(decodeMatrix, invertedMatrix, numMapped, _q); 

        //for (int i = 0; i < numMapped; i++) { for (int j = 0; j < numMapped; j++) { cout << "[" << invertedMatrix[i * numMapped + j] << "]"; } cout << endl; }

        // save the equation to repair original parity packet using transformed parity packets
        for (auto pit = packetColumnIndex.begin(); pit != packetColumnIndex.end(); pit++) {
            int pkt = GlobalToLocal(VirtualNodeToParity(pit->first));
            int ri = pit->second;
            int pi = pkt / _w;
            int pkti = pkt % _w;
            //cout << "Set (" << pkti + 1 << "," << pi + 1 << "): ";
            _reverseTransformationSourcePackets[pi][pkti]->clear();
            for (int mri = 0; mri < numMapped; mri++) {
                // add the source packet and the coefficient
                int src = mapped.at(mri);
                _reverseTransformationSourcePackets[pi][pkti]->emplace_back(LocalToGlobal(src));
                _reverseTransformationMatrix[pi][pkti]->emplace_back(invertedMatrix[ri * numMapped + mri]);
                //cout << "[" << invertedMatrix[ri * numMapped + mri] << "]";
            }
            //cout << endl;
            done[pkt] = true;
        }
    }
}

int ETHTEC::ParityToVirtualNode(int parityIndex) const {
    return ParityToVirtualNode(_n, _k, _w, parityIndex);
}

int ETHTEC::VirtualNodeToParity(int virtualIndex) const {
    if (virtualIndex < _k * _w) { return virtualIndex; }
    if (virtualIndex < _n * _w || virtualIndex > _n * _w + (_m * _w)) { return -1; }
    return virtualIndex - _m * _w;
}

int ETHTEC::GlobalToLocal(int global) const {
    if (global < _k * _w || global > _n * _w) { return -1; }
    return global - _k * _w;
}

int ETHTEC::LocalToGlobal(int local) const {
    if (local < 0 || local > _m * _w) { return -1; }
    return local + _k * _w;
}

ECDAG* ETHTEC::ConstructEncodeECDAG() const {
    ECDAG *ecdag = new ECDAG();
    // multiple indepdenent instances
    for (int i = 0; i < _numInstances; i++) { _instances[i]->ConstructEncodeECDAG(ecdag); }
    return ecdag;
}

ECDAG* ETHTEC::ConstructDecodeECDAG(vector<int> from, vector<int> to) const {
    ECDAG *ecdag = new ECDAG();
    if (to.size() != _w) { // TODO handle non-single-node failures
        cerr << "error: non-single node failure is not supported in ET-HTEC!";
        return ecdag;
    }

    set<int> packetsUsed;
    // decode multiple independent instances
    vector<int>::iterator st = to.begin(), ed = st + _baseW;
    for (int i = 0; i < _numInstances; i++, st += _baseW, ed += _baseW) {
        _instances[i]->ConstructDecodeECDAG(from, vector<int>(st, ed), ecdag, /* covertId */ NULL, /* set of parities */ NULL, &packetsUsed);
    }

    cout << "Number of packets read = " << packetsUsed.size()
         << fixed
         << "; Normalized repair bandwidth = " << setprecision(3) << packetsUsed.size() * 1.0 / _w / _k
         << endl;

    return ecdag;
}

ECDAG* ETHTEC::ConstructEncodeECDAGWithET() const {
    ECDAG *ecdag = new ECDAG();

    vector<int> *src, *cs;
    for (int i = 0; i < _m * _w; i++) {
        // original parity
        if (i / _w == 0 && i % _baseW == 0) {
            int instanceId = (i % _w) / _baseW;
            _instances[instanceId]->ConstructEncodeECDAG(ecdag, &ETHTEC::ParityToVirtualNode);
        }

        // transformed parity
        int pi = i / _w;
        int pkt = i % _w;
        src = _transformationSourcePackets[pi][pkt];
        cs = _transformationMatrix[pi][pkt];
        int pid = LocalToGlobal(i);
        if (src->size() > 1) {
            ecdag->Join(pid, *src, *cs);
            ecdag->BindY(pid, src->at(0));
        } else {
            // workaround: force ECDAG to compute the node with single source by adding a redundant source (TODO: any fix?)
            vector<int> srct = *src;
            vector<int> cst = *cs;
            srct.emplace_back(srct.at(0));
            cst.emplace_back(0);
            ecdag->Join(pid, srct, cst);
            ecdag->BindY(pid, srct.at(0));
        }
    }

    return ecdag;
}

ECDAG* ETHTEC::ConstructDecodeECDAGWithET(vector<int> from, vector<int> to) const {
    ECDAG *ecdag = new ECDAG();
    if (to.size() != _w) { // TODO handle non-single-node failures
        cerr << "error: non-single node failure is not supported in ET-HTEC!";
        return ecdag;
    }

    set<int> parities, packetsUsed;
    vector<int>::iterator st = to.begin(), ed = to.begin();
    ed += _baseW;

    int failedNode = to.at(0) / _w;

    if (failedNode < _k) { // repair a data node
        // original parity
        for (int i = 0; i < _numInstances; i++, st+=_baseW, ed+=_baseW) {
            _instances[i]->ConstructDecodeECDAG(from, vector<int>(st, ed), ecdag, &ETHTEC::ParityToVirtualNode, &parities, &packetsUsed);
        }

        // transformed parity
        set<int> tparities;

        // decode for the original parities from the transformed ones
        for (const auto &p : parities) {
            vector<int> src, cs;
            int lp = GlobalToLocal(VirtualNodeToParity(p));
            int pi = lp / _w;
            int pkti = lp % _w;
            int numSrc = _reverseTransformationSourcePackets[pi][pkti]->size();
            for (int i = 0; i < numSrc; i++) {
                int c = _reverseTransformationMatrix[pi][pkti]->at(i);
                if (c == 0) { continue; }
                int s = _reverseTransformationSourcePackets[pi][pkti]->at(i);
                src.emplace_back(s);
                cs.emplace_back(c);
                tparities.insert(s);
            }
            int pid = ParityToVirtualNode(LocalToGlobal(lp));
            if (numSrc == 1) {
                // workaround: force ECDAG to compute the node with single source by adding a redundant source (TODO: any fix?)
                src.emplace_back(src.at(0));   
                cs.emplace_back(0);
            }
            ecdag->Join(pid, src, cs);
            ecdag->BindY(pid, src.at(0));
        }

        // number of transformed parities required should be the same as the original ones
        if (tparities.size() != parities.size()) { cout << "warning: reading " << tparities.size() << " transformed parities instead of " << parities.size() << endl; }

    } else { // repair a parity node
        set<int> dataSubStripeRead;
        map<int, set<int>> numSourcePackets;
        // order the lost packets by the number of coupled entries
        for (const auto &t : to) {
            int tl = GlobalToLocal(t);
            int pi = tl / _w;
            int pkti = tl % _w;
            vector<int> *tsrc = _transformationSourcePackets[pi][pkti];
            int numSrc = tsrc->size();
            numSourcePackets.emplace(make_pair(numSrc, set<int>()));
            numSourcePackets.at(numSrc).emplace(t);
        }

        // repair from the lost packets with fewest number of coupled entries
        for (auto mit = numSourcePackets.begin(); mit != numSourcePackets.end(); mit++) {
            for (const auto &t : mit->second) {
                vector<int> *src, *cs;

                int tl = GlobalToLocal(t);
                int pkti = tl % _w;
                int pi = tl / _w;
                vector<int> *tsrc = _transformationSourcePackets[pi][pkti];
                vector<int> *tcs = _transformationMatrix[pi][pkti];
                set<int> tsrcSet(tsrc->begin(), tsrc->end());
                int numSrc = tsrc->size();

                // no pairing, repair directly from data
                if (numSrc == 1) {
                    int spkt = GlobalToLocal(VirtualNodeToParity(tsrc->at(0)));
                    int instanceId = pkti / _baseW;
                    //cout << "Single decode Read " << spkt << endl;
                    _instances[instanceId]->GetParitySourceAndCoefficient(spkt, &src, &cs);
                    // (use the target parity id instead of virtual node id)
                    ecdag->Join(t, *src, *cs);
                    ecdag->BindY(t, src->at(0));
                    ecdag->Join(tsrc->at(0), *src, *cs);
                    ecdag->BindY(tsrc->at(0), src->at(0));
                    // mark stripes with all data packets read
                    dataSubStripeRead.emplace(pkti);
                    packetsUsed.insert(src->begin(), src->end());
                    continue;
                }

                // repairing of paired packets

                int tspkt = GlobalToLocal(VirtualNodeToParity(tsrc->at(0)));
                int tspi = tspkt / _w;
                int tspkti = tspkt % _w;

                // read the smallest substripe of pairs if insufficient stripes have been read 
                if (dataSubStripeRead.size() < _baseW) {
                    int instanceId = tspkti / _baseW;
                    _instances[instanceId]->GetParitySourceAndCoefficient(tspkt, &src, &cs);
                    //cout << "Paired decode Read " << tsrc->at(0) << endl;
                    // (use virtual node for future construction of transformed parity packets)
                    ecdag->Join(tsrc->at(0), *src, *cs);
                    ecdag->BindY(tsrc->at(0), src->at(0));
                    // mark stripes with all data packets read
                    dataSubStripeRead.emplace(tspkti % _w);
                    packetsUsed.insert(src->begin(), src->end());
                } else {
                    // find the sub-stripe with all data packets read
                    for (int i = 0; i < tsrc->size(); i++) {
                        int spkt = GlobalToLocal(VirtualNodeToParity(tsrc->at(i)));
                        if (dataSubStripeRead.count(spkt % _w)) {
                            tspkt = spkt;
                            tspi = spkt / _w;
                            tspkti = spkt % _w;
                            //cout << "Paired decode Switch to " << tspkt << endl;
                            break;
                        }
                    }
                }

                // resolve the equations among paried entries (check the paired entries using the reverse mapping for repair original parities)
                vector<int> *rsrc = _reverseTransformationSourcePackets[tspi][tspkti];
                int numRsrc = rsrc->size();
                map<int, int> packetColumnIndex;
                int nextColumnIndex = 0;
                vector<int> dsrc;

                // construct the decoding matrix
                int decodeMatrix[_m * _m] = { 0 }, invertedMatrix[_m * _m] = { 0 };
                for (int si = 0; si < numRsrc; si++) {
                    int spkt = GlobalToLocal(rsrc->at(si));
                    // for the line corresponding to the packet to repair, put the original parity packet repaired with all data packets in the sub-stripe
                    if (spkt == tl) { 
                        int col = 0;
                        int pkt = ParityToVirtualNode(LocalToGlobal(tspkt));
                        auto cit = packetColumnIndex.find(pkt);
                        if (cit == packetColumnIndex.end()) {
                            col = nextColumnIndex;
                            packetColumnIndex.emplace(make_pair(pkt, nextColumnIndex++));
                        } else {
                            col = cit->second;
                        }
                        decodeMatrix[si * numRsrc + col] = 1;
                        dsrc.emplace_back(pkt);
                        //cout << "Put single packet " << pkt << " to column " << col << endl;
                        continue;
                    }
                    // for other lines, copy their corresponding equations to the decoding matrix
                    int spkti = spkt % _w;
                    int spi = spkt / _w;
                    src = _transformationSourcePackets[spi][spkti];
                    cs = _transformationMatrix[spi][spkti];
                    for (int di = 0; di < src->size(); di++) {
                        int col = 0;
                        int dpkt = src->at(di);
                        auto cit = packetColumnIndex.find(dpkt);
                        if (cit == packetColumnIndex.end()) {
                            col = nextColumnIndex;
                            packetColumnIndex.emplace(make_pair(dpkt, nextColumnIndex++));
                        } else {
                            col = cit->second;
                        }
                        //cout << "decode src " << dpkt << " at column = " << col << " c = " << cs->at(di) << endl;
                        decodeMatrix[si * numRsrc + col] = cs->at(di);
                    }
                    dsrc.emplace_back(rsrc->at(si));
                }

                //for (auto pci : packetColumnIndex) { cout << "packet " << pci.first << " on column " << pci.second << endl; };
                //for (int i = 0; i < numRsrc; i++) { for (int j = 0; j < numRsrc; j++) { cout << "[" << decodeMatrix[i * numRsrc + j] << "]"; } cout << endl; }
                //cout << endl;

                // invert the matrix
                jerasure_invert_matrix(decodeMatrix, invertedMatrix, numRsrc, _q);

                //for (int i = 0; i < numRsrc; i++) { for (int j = 0; j < numRsrc; j++) { cout << "[" << invertedMatrix[i * numRsrc + j] << "]"; } cout << endl; }
                //cout << endl;

                // add the equation for the individual original parities
                for (const auto rpkt : packetColumnIndex) {
                    int pkt = rpkt.first;
                    int col = rpkt.second;
                    vector<int> rsrc, rcs;

                    
                    if (tsrcSet.count(pkt) == 0) { continue; }

                    for (int i = 0; i < numRsrc; i++) {
                        int c = invertedMatrix[col * numRsrc + i];
                        if (c == 0) { continue; }
                        rsrc.emplace_back(dsrc.at(i));
                        rcs.emplace_back(c);
                    }
                    // compute the original parity packets
                    if (rsrc.size() > 1) {
                        // use a combinatioin of transformed and original parity packets
                        ecdag->Join(pkt, rsrc, rcs);
                        ecdag->BindY(pkt, rsrc.at(0));
                        packetsUsed.insert(rsrc.begin(), rsrc.end());
                    } else {
                        // use the original packet reconstructed from data packets 
                        int instanceId = (pkt % _w) / _baseW;
                        _instances[instanceId]->GetParitySourceAndCoefficient(GlobalToLocal(VirtualNodeToParity(pkt)), &src, &cs);
                        ecdag->Join(pkt, *src, *cs);
                        ecdag->BindY(pkt, src->at(0));
                        packetsUsed.insert(src->begin(), src->end());
                    }
                }

                // add the transformed parity equation
                ecdag->Join(t, *tsrc, *tcs);
                ecdag->BindY(t, tsrc->at(0));
            }
        }
    }

    cout << "Number of packets read = " << packetsUsed.size()
         << fixed
         << "; Normalized repair bandwidth = " << setprecision(3) << packetsUsed.size() * 1.0 / _w / _k
         << endl;

    return ecdag;
}

ECDAG* ETHTEC::Encode() {
    //return ConstructEncodeECDAG();
    return ConstructEncodeECDAGWithET();
}

ECDAG* ETHTEC::Decode(vector<int> from, vector<int> to) {
    //return ConstructDecodeECDAG(from, to);
    return ConstructDecodeECDAGWithET(from, to);
}

void ETHTEC::Place(vector<vector<int>>& group) {
}

void ETHTEC::PrintParityInfo(bool dense) const {
    for (int i = 0; i <_numInstances; i++) {
        _instances[i]->PrintParityInfo(dense);
    }
}

void ETHTEC::PrintSourcePackets(bool transform) const {
    cout << (transform? "Tranformation array:\n" : "Reverse transform array:\n");
    for (int pi = 0; pi < _m; pi++) {
        for (int pkti = 0; pkti < _w; pkti++) {
            vector<int> *pkts = transform? _transformationSourcePackets[pi][pkti] : _reverseTransformationSourcePackets[pi][pkti];
            vector<int> *cs = transform? _transformationMatrix[pi][pkti] : _reverseTransformationMatrix[pi][pkti];
            cout << "(" << pkti + 1 << "," << pi + 1 << "): ";
            for (int l = 0; l < pkts->size(); l++) {
                int pkt = GlobalToLocal(transform? VirtualNodeToParity(pkts->at(l)) : pkts->at(l));
                //int pkt = transform? GlobalToLocal(VirtualNodeToParity(pkts->at(l))) : pkts->at(l);
                int c = cs->at(l);
                cout << setw(2) << right<< c << "p(" << setw(2) << right << (pkt % _w) + 1 << "," << setw(2) << left << pkt / _w + 1 << ") ";
            }
            cout << endl;
        }
        cout << endl;
    }
}

int ETHTEC::ParityToVirtualNode(int n, int k, int w, int parityIndex) {
    if (parityIndex < k * w) { return parityIndex; }
    if (parityIndex > n * w) { return -1; }
    // assign a one-to-one index beyond all data and parity packets
    return parityIndex + (n - k) * w;
}
