#ifndef __ET_AzureLRC_HH__
#define __ET_AzureLRC_HH__

#include <map>

#include "../inc/include.hh"
#include "Computation.hh"

#include "ECBase.hh"
#include "AzureLRC.hh"
#include "ETUnit.hh"

using namespace std;

class ETAzureLRC : public ECBase {
private:
    
    int _m; // n - k
    int _l; // local group
    int _base_w; // w for base code
    int _ins_num;
    int _num_instances; // number of base code instances

    vector<vector<int>> _uncoupled_layout; // uncoupled layout (w * n)
    vector<vector<int>> _inv_perm_uc_layout; // inverse permutated uncoupled layout (w * n)
    vector<vector<int>> _layout; // layout (w * n)

    vector<AzureLRC *> _instances; // base RS code instances
    vector<ETUnit *> _parity_et_units; // et units for parity groups
    vector<vector<int>> _parity_et_groups; // parity groups for elastic transformation

    ECDAG *DecodeSingle(vector<int> from, vector<int> to); // single failure decode
    ECDAG *DecodeMultiple(vector<int> from, vector<int> to); // multiple failure decode

public:
    ETAzureLRC(int n, int k, int w, int opt, vector<string> param);
    ~ETAzureLRC();
 
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

#endif // __ET_ETAzureLRC_HH__
