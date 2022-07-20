#ifndef __ET_RSCONV_HH__
#define __ET_RSCONV_HH__

#include <map>
#include <utility>

#include "../inc/include.hh"
#include "Computation.hh"

#include "ECBase.hh"
#include "RSConv.hh"
#include "ETUnit.hh"

using namespace std;

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

    vector<RSConv *> _instances; // base RS code instances
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
