/**
 * @file ETHHXORPlus.hh
 * @author Keyun Cheng (kycheng@cse.cuhk.edu.hk)
 * @brief 
 * @version 0.1
 * @date 2022-05-10
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#ifndef __ET_HH_XORPlus_HH__
#define __ET_HH_XORPlus_HH__

#include <map>

#include "../inc/include.hh"
#include "Computation.hh"

#include "ECBase.hh"
#include "HHXORPlus.hh"
#include "ETUnit.hh"

using namespace std;

class ETHHXORPlus : public ECBase
{
private:
    
    int _m; // n - k
    int _base_w; // w for base code
    int _num_instances; // number of base code instances

    vector<vector<int>> _uncoupled_layout; // uncoupled layout (w * n)
    vector<vector<int>> _inv_perm_uc_layout; // inverse permutated uncoupled layout (w * n)
    vector<vector<int>> _layout; // layout (w * n)

    vector<HHXORPlus *> _instances; // base RS code instances
    vector<ETUnit *> _parity_et_units; // et units for parity groups
    vector<vector<int>> _parity_et_groups; // parity groups for elastic transformation

    ECDAG *DecodeSingle(vector<int> from, vector<int> to); // single failure decode
    ECDAG *DecodeMultiple(vector<int> from, vector<int> to); // multiple failure decode

public:
    ETHHXORPlus(int n, int k, int w, int opt, vector<string> param);
    ~ETHHXORPlus();

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



#endif // __ET_HH_NON_XOR_HH__