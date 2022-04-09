/**
 * @file ETUnitSquare.hh
 * @author Keyun Cheng (kycheng@cse.cuhk.edu.hk)
 * @brief elastic transformation square unit
 * @version 0.1
 * @date 2022-04-01
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#ifndef __ET_UNIT_SQUARE_HH__
#define __ET_UNIT_SQUARE_HH__

#include "ECDAG.hh"
#include "Computation.hh"

class ETUnitSquare
{
private:
    /* data */
    int _r; // number of rows
    int _e; // coefficient
    int _base_w; // sub-packetization of base code
    int _eta = 1;

    vector<vector<int>> _uncoupled_layout; // uncoupled layout (r * r)
    vector<vector<int>> _layout; // layout (r * r)
    

    int *_encode_matrix;

    void init_square(int r);

public:
    ETUnitSquare(int r, int base_w, vector<vector<int>> uncoupled_layout, vector<vector<int>> layout, int e = 2);
    ~ETUnitSquare();

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
};

#endif // __ET_UNIT_SQUARE_HH__