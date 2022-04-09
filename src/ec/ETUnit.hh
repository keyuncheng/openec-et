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

    vector<vector<int>> _uncoupled_layout; // uncoupled layout (r * w)
    vector<vector<int>> _perm_uc_layout; // permutated uncoupled layout (r * w)
    vector<vector<int>> _inv_perm_uc_layout; // permutated uncoupled layout (r * w)
    vector<vector<int>> _layout; // layout (r * w)

    void gen_square_pc_matrix(int r, int *pc_matrix);

    void gen_rotation_matrix(int r, int c, int *rotation_matrix);
    
    int *_rotation_matrix;
    int *_pc_matrix;
    int *_inv_pc_matrix;

public:
    ETUnit(int r, int c, int base_w, bool is_parity, vector<vector<int>> uncoupled_layout, vector<vector<int>> layout, int e = 2);
    ~ETUnit();

        /**
     * @brief append encode routine on ecdag
     * 
     * @param ecdag 
     */
    void Encode(ECDAG *ecdag);

    /**
     * @brief append decode routine on ecdag
     * 
     * @param ecdag 
     */
    void Decode(ECDAG *ecdag);

    /**
     * @brief Get Inverse Permutated Uncoupled Layout
     * 
     * @return vector<vector<int>> 
     */
    vector<vector<int>> GetInvPermUCLayout();
};


#endif // __ET_UNIT_HH__