#ifndef __ET_RSCONV_HH__
#define __ET_RSCONV_HH__

#include <map>
#include <utility>

#include "../inc/include.hh"
#include "Computation.hh"

#include "ECBase.hh"
#include "RSMultiIns.hh"
#include "ETUnit.hh"

using namespace std;

/**
 * NOTE: in this implementation, we provide one possible construction of elastic transformation to extend the sub-packetization of RS-ET to be larger than (n - k). The core idea is to factorize the final sub-packetization into prime factors, and do elastic transformation for all sub-packets in multiple rounds, determined by the number of prime factors. In each round, we apply elastic transformation to cover *all nodes*.
 * 
 * For example, for RS-ET(9,6) with sub-packetization = 27 = 3 * 3 * 3, we first transform to RS-ET-3 from RS code by covering all nodes; then extend to RS-ET-9 from RS-ET-3 by covering all nodes again; finally extend to RS-ET-27 from RS-ET-9. Note that to cover all nodes, we may need more than one transformation arrays.
 * 
 * There exists more than one possible constructions with the assistance of elastic transformation. For example, in each round, we can apply elastic transformation to *a subset of storage nodes*, and cover the remaining nodes in the remaining rounds. For example, to construct RS-ET(9,6) with sub-packetization 27, we can transform to RS-ET-3 by covering the first 3 nodes 1,2,3, then extend to RS-ET-9 by covering the next 3 nodes 4,5,6 from RS-ET-3; finally extend to RS-ET-27 to cover the remaining 3 nodes 7,8,9 from RS-ET-9. In each round of transformation, we can customize the usage of transformation array, by utilizing the square / non-square arrays in a flexible way. There may exist minor differences of the repair bandwidth between different constructions at the same sub-packetization level.
 * 
 */

class ETRSConv : public ECBase {
private:
    
    int _m; // n - k
    vector<int> _data_base_w;     // base sub-packetization of each data group
    vector<int> _parity_base_w;   // base sub-packetization of each parity group
    vector<int> _data_num_instances; // number of instances of each data group
    vector<int> _parity_num_instances; // number of instances of each parity group

    vector<vector<int>> _uncoupled_layout; // uncoupled layout (w * n)
    vector<vector<int>> _inv_perm_uc_layout; // inverse permutated uncoupled layout (w * n)
    vector<vector<int>> _layout; // layout (w * n)

    vector<RSMultiIns *> _instances; // base RS code instances
    vector<vector<ETUnit *>> _data_et_units; // et units for data groups [group idx]-> units
    vector<vector<ETUnit *>> _parity_et_units; // et units for parity groups [group idx]-> units
    vector<vector<int>> _data_et_groups; // data groups for elastic transformation
    vector<vector<int>> _parity_et_groups; // parity groups for elastic transformation

    static const char *_better_parity_repair_key; // perfer a construction with better parity repair
    static const char *_smooth_parity_repair_key; // perfer a construction with smooth parity repair (if better parity repair is not enabled)

    ECDAG *DecodeSingle(vector<int> from, vector<int> to); // single failure decode
    ECDAG *DecodeMultiple(vector<int> from, vector<int> to); // multiple failure decode

public:
    ETRSConv(int n, int k, int w, int opt, vector<string> param);
    ~ETRSConv();
 
    ECDAG* Encode();
    ECDAG* Decode(vector<int> from, vector<int> to);
    void Place(vector<vector<int>>& group);

    /**
     * @brief Get symbols in nodeid
     * 
     * @param nodeid 
     * @return vector<int> 
     */
    vector<int> GetNodeSymbols(int nodeid);

    /**
     * @brief Get code layout
     * 
     * 0 2 4 6 8 ...
     * 1 3 5 7 9 ... 
     * 
     * @return vector<vector<int>> 
     */
    vector<vector<int>> GetLayout();
};

#endif // __ET_RSCONV_HH__
