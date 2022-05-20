#ifndef _HTEC_HH_
#define _HTEC_HH_

#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "ECBase.hh"
#include "ECDAG.hh"

extern "C" {
#include "../util/galois.h"
}

using namespace std;

class HTEC : public ECBase {

/**
 * Implementation of HashTag erasure code
 *
 * "HashTag Erasure Codes: From Theory to Practice", IEEE Transactions on Big Data, 2018 Oct-Dec.
 *
 * https://doi.org/10.1109/TBDATA.2017.2749255
 **/

protected:

    /**
     * Partition D_dj where 1 <= j <= k in the paper, comprising multiple subsets (groups)
     */
    class Partition {
            vector<vector<int>> indices;

        public:
            Partition();
            ~Partition();

            /**
             * Add a subset of index to this partition if not exist
             *
             * @param set            subset of indices to add
             *
             * @return if the set is already in partition, return -1; otherwise, the current number of subsets in this partition
             **/
            int AppendSubset(vector<int> set);

            /**
             * Tell if this partition is the same as a given partition
             *
             * @return true if the two partitions are the same, false otherwise
             **/
            bool operator == (const Partition &rhs) const;

            /**
             * Tell if any subset in a partition is the same as one in a given partition
             *
             * @return true if one subset in this partition also appears in the other, false otherwise
             **/
            bool ContainsEqualSubset(const Partition &rhs) const;

            /**
             * Get the number of subset in this partition
             **/
            size_t GetNumSubsets() const;

            /**
             * Get certain subset in this partition 
             *
             * @param index     subset index (starting from 0)
             * 
             * @return a valid subset if index is less than the number of subsets, otherwise, an empty subset
             **/
            vector<int> GetSubset(int index) const;

            /**
             * Remove all subset in this partition
             **/
            void Clear();

            /**
             * Check if this partition is empty
             *
             * @return true if this partition is empty, false otherwise
             **/
            bool Empty() const;

            /**
             * Print partition information
             **/
            void Print() const;

    };

    int _m;                                                // number of parities, also 'r' in the paper
    int _q;                                                // field size
    int _numGroups;                                        // number of groups (division of data nodes)
    int _groupSize;                                        // size of groups (division of data nodes)
    int _portion;                                          // portion (division of packets in a data/parity)
    int _stepMax;                                          // max possible step given the n, k, w configuration
    int _runMax;                                           // max possible run given the n, k, w configuration

    vector<int>** _dataLayout = NULL;                      // data packet indices, each vector is a sub-stripe
    vector<int>*** _paritySourcePackets = NULL;            // parity source packets (each vector corresponds to a parity packet)
    vector<int>*** _parityMatrix = NULL;                   // parity matrix (each vector corresponds to a parity packet)
    vector<int>*** _paritySourcePacketsD = NULL;           // parity source packets, dense version for encoding
    vector<int>*** _parityMatrixD = NULL;                  // parity matrix, dense version for encoding

    map<int, pair<int, Partition>> _searchMap;             // search map for partition finding step -> <run, Partition>
                                                           // partition that fit the first condition at the best run given the step 
                                                           // the HTEC partitioning algorithm only visit a particular step for data nodes in the same group and requires same partitioning within a group
    map<int, Partition> _randMap;
    map<int, vector<int>> _selectedSubset;                 // selected partition subset by data node, data node index -> subset

    const static unsigned char _coefficients_n9_k6_w6[][6+2];  // specific coefficients from paper
    const static unsigned char _coefficients_n9_k6_w9[][6+2];  // specific coefficients from paper
    gf_t *_gf16;
    gf_t *_gf32;

    /**
     * Get the number of source packets for a parity packet
     *
     * @param idx     the parity index which the packet lies in
     *
     * @return the number of source packets for this parity packet
     **/ 
    inline int GetNumSourcePackets(int idx) const;

    /**
     * Get the partition by index
     *
     * @param index   partition index
     *
     * @return the indexed partition; if the index has no partition, an empty partition is returned
     **/ 
    Partition GetPartition(pair<int, int> idx) const;

    /**
     * Initialize the parity matrix and source packets 
     **/
    void InitParityInfo();

    /**
     * Initialize the map of valid partitions
     **/
    void InitPartitionSearchMaps();

    /**
     * Destroy the map of valid partitions
     **/
    void DestroyPartitionSearchMaps();

    /**
     * Find a valid partition
     * 
     * @param step                 step (number of elements in between two selected indices)
     * @param runs                 run (number of consecutive elements separated by step)
     * @param validPartitions      partitions selected so far
     * 
     * @return if no partition is found, return a partition index of (-1,-1); otherwise, the indices of a selected partition
     **/
    pair<int, int> FindPartition(int step, int run, const vector<pair<int, int>> &validPartitions, int firstInGroupDataNodeIndex);

    /**
     * Fill the parity source packet indices according to a partition
     * 
     * @param column      parity column of P_i where 2 <= i <= r to fill in
     * @param p           partition
     * @param dataNodeId  id of the data node corresponded to this fill-in
     * 
     * @return true if parity indices is filled successfully, false otherwise
     **/
    bool FillParityIndices(int column, const Partition &p, int dataNodeId);

    /**
     * Fill the parity coefficients
     **/
    void FillParityCoefficients();

    /**
     * Fill the parity coefficients for special cases
     *
     * @return all coefficients have been filled out
     **/
    bool FillParityCoefficientsForSpecialCases();

    /**
     * Condense the parity info by removing unassigned pairs
     **/
    void CondenseParityInfo();

    /**
     * Construct ECDAG for encoding
     *
     * @return a new ECDAG for encoding
     **/
    ECDAG *ConstructEncodeECDAG() const;

    /**
     * Construct ECDAG for decoding
     *
     * @return if number of failure is 1 return a ECDAG for encoding; otherwise, return an empty ECDAG (no decoding)
     **/

    ECDAG* ConstructDecodeECDAG(const vector<int> &from, const vector<int> &to) const;

    /**
     * Add the conventional repair of a specified packet to the specified ECDAG
     *
     * @param failedNode                    index of the failed node
     * @param failedPacket                  index of the failed packet
     * @param parityIndex                   index of the parity to use for repair
     * @param[in,out] ecdag                 ECDAG to add the repair
     * @param[in,out] repaired              set of repaired packets
     * @param[in,out] totalSources          an optional counter for data packets used
     **/
    void AddConvSingleDecode(int failedNode, int failedPacket, int parityIndex, ECDAG *ecdag, set<int> &repaired, set<int> *totalSources = 0) const;

    /**
     * Add the incremental repair of a specified packet to the specified ECDAG
     *
     * @param failedNode                    index of the failed node
     * @param selectedPacket                index of the select parity row/packet
     * @param[in,out] ecdag                 ECDAG to add the repair
     * @param[in,out] repaired              set of repaired packets
     * @param[in,out] totalSources          an optional counter for data packets used
     **/
    void AddIncrSingleDecode(int failedNode, int selectedPacket, ECDAG *ecdag, set<int> &repaired, set<int> *totalSources = 0) const;

    /**
     * Get the repair bandwidth upper bound proved in the paper
     *
     * @return repair bandwidth upper bound
     **/ 
    double GetRepairBandwidthUpperBound() const;

    static int GetNchooseK(int n, int k);

public:

    HTEC(int n, int k, int alpha, int opt, vector<string> param);
    virtual ~HTEC();

    ECDAG* Encode();
    ECDAG* Decode(vector<int> from, vector<int> to);
    void Place(vector<vector<int>>& group);


    /**
     * Print parity information
     **/
    void PrintParityInfo(bool dense = false) const;
    void PrintParityMatrix(bool dense = false) const;
    void PrintParityIndexArrays(bool dense = false, bool skipFirst = true) const;
    void PrintSelectedSubset() const;

};

#endif //define _HTEC_HH_
