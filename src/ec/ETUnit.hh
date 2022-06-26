/**
 * @file ETUnit.hh
 * @author Keyun Cheng (kycheng@cse.cuhk.edu.hk)
 * @brief Elastic Transformation Unit
 * @version 0.1
 * @date 2022-04-08
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#ifndef __ET_UNIT_HH__
#define __ET_UNIT_HH__

#include <map>
#include <utility>

#include "ECDAG.hh"
#include "Computation.hh"

class ETUnit
{
private:
    /* data */
    int _r; // number of rows (number of instances)
    int _c; // number of columns (number of nodes)
    int _base_w; // sub-packetization of base code
    int _e; // coefficient
    int _eta = 1;
    
    bool _is_parity; // is parity unit

    vector<vector<int>> _uncoupled_layout; // uncoupled layout (r * c)
    vector<vector<int>> _perm_uc_layout; // permutated uncoupled layout (r * c)
    vector<vector<int>> _inv_perm_uc_layout; // permutated uncoupled layout (r * c)
    vector<vector<int>> _layout; // layout (r * w)

    void gen_square_pc_matrix(int r, int *pc_matrix);

    void gen_rotation_matrix(int r, int c, int *rotation_matrix);
    
    int *_rotation_matrix;
    int *_pc_matrix;
    int *_inv_pc_matrix;

public:
    ETUnit(int r, int c, int base_w, vector<vector<int>> uncoupled_layout, vector<vector<int>> layout, int e = 2);
    ~ETUnit();

    /**
     * @brief add coupling routine to ecdag
     * input: all symbols in uncoupled layout
     * output: all symbols in layout
     * 
     * @param ecdag 
     */
    void Coupling(ECDAG *ecdag);

    /**
     * @brief add decoupling routine to ecdag
     * input: all symbols in layout
     * output: all symbols in uncoupled- layout
     * 
     * @param ecdag 
     */
    void Decoupling(ECDAG *ecdag);

    /**
     * @brief 
     * biased coupling: couple the layout symbols in to using 
     * all alive layout symbols and an additional uncoupled layout symbols in alive_ins_id
     * 
     * For each layout packet in to:
     *     input: all alive layout symbols except to[i], uncoupled layout packet for to[i]
     *     output: to[i]
     * 
     * 
     * @param to: layout symbols
     * @param alive_ins_id: alive instance id
     * @param ecdag 
     */
    void BiasedCoupling(vector<int> to, int alive_ins_id, ECDAG *ecdag);

    /**
     * @brief add decoupling routine to ecdag
     * input: all symbols in layout in to
     * output: all symbols in uncoupled- layout
     * 
     * @param vector<int> to
     * @param ecdag 
     */
    void Decoupling(vector<int> to, ECDAG *ecdag);

    /**
     * @brief Get uncoupled layout
     * 
     * @return vector<vector<int>> 
     */
    vector<vector<int>> GetUCLayout();

    /**
     * @brief Get inverse permutated uncoupled layout
     * 
     * @return vector<vector<int>> 
     */
    vector<vector<int>> GetInvPermUCLayout();

    /**
     * @brief Get required uncoupled symbols given index of sub-packetization
     * 
     * @param int sp sub-packetization index
     * @return map<int, vector<int>> (sp, symbols)
     */
    map<int, vector<int>> GetRequiredUCSymbolsForSP(int sp);

    /**
     * @brief Get required uncoupled symbols given failed index in group
     * 
     * @param failed_grp_idx failed node idx in group
     * @return map<int, vector<int>> (sp, symbols)
     */
    map<int, vector<int>> GetRequiedUCSymbolsForNode(int failed_grp_idx);

    /**
     * @brief Get required uncoupled symbols given failed index in group and failed instance id
     * 
     * @param failed_grp_idx failed node idx in group
     * @param failed_ins_id failed instance id
     * @return vector<int> uncoupled symbols
     */
    vector<int> GetUnrelatedUCSymbols(int failed_grp_idx, int failed_ins_id);

    /**
     * @brief Get related uncoupled sybmols for given uncoupled symbol
     * 
     * @param uc_symbol_id 
     * @return vector<int> related uncoupled symbols for uc_symbol_id
     */
    vector<int> GetRelatedUCSymbolsForUCSymbol(int uc_symbol_id);

    /**
     * @brief Get related uncoupled sybmols in the i-th instance for given uncoupled symbol
     * 
     * @param uc_symbol_id 
     * @return vector<int> related uncoupled symbols for uc_symbol_id
     */
    vector<int> GetRelatedUCSymbolsForUCSymbol(int uc_symbol_id, int ins_id);
};


#endif // __ET_UNIT_HH__