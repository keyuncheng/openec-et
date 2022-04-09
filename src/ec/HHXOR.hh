/**
 * @file HHXOR.hh
 * @author Keyun Cheng (kycheng@cse.cuhk.edu.hk)
 * @brief Hitchhiker-nonXOR ECDAG
 * @version 0.1
 * @date 2022-03-22
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#ifndef __HH_XOR_HH__
#define __HH_XOR_HH__

#include <map>

#include "Computation.hh"
#include "ECBase.hh"

class HHXOR : public ECBase
{
private:

    int *_rs_encode_matrix; // underlying RS encode matrix
    
    vector<vector<int>> _layout; // layout (w * n)
    vector<vector<int>> _data_layout; // data layout (w * k)
    vector<vector<int>> _code_layout; // code layout (w * (n - k))
    vector<vector<int>> _uncoupled_code_layout; // uncoupled code layout (w * (n - k))
    
    map<int, int> _pid_group_code_map; // parity_id to group code map
    map<int, vector<int>> _pid_group_map; // parity_id to data group map

    int num_symbols; // total number of symbols (k * w + num_groups * w)

    /**
     * @brief generate rows * cols Vandermonde matrix in GF(2^w)
     * 
     * @param matrix 
     * @param rows 
     * @param cols 
     * @param w 
     */
    void generate_vandermonde_matrix(int* matrix, int rows, int cols, int w);

    /**
     * @brief generate rows * cols Cauchy matrix in GF(2^w)
     * 
     * @param matrix 
     * @param rows 
     * @param cols 
     * @param w 
     */
    void generate_cauchy_matrix(int* matrix, int rows, int cols, int w);

public:

    /**
     * @brief Constructor
     * 
     * @param n ec n
     * @param k ec k
     * @param sub-packetization (fixed to 2)
     * @param opt optimization level
     * @param param additional parameters
     */
    HHXOR(int n, int k, int w, int opt, vector<string> param);

    /**
     * @brief Destructor
     * 
     */
    ~HHXOR();

    /**
     * @brief construct Encode ECDAG
     * 
     * @return ECDAG* 
     */
    ECDAG* Encode();

    /**
     * @brief Decode ECDAG
     * 
     * @param from available symbols
     * @param to lost symbols
     * @return ECDAG* 
     */
    ECDAG* Decode(vector<int> from, vector<int> to);

    /**
     * @brief Not implemented
     * 
     * @param group 
     */
    void Place(vector<vector<int>>& group);

    /**
     * @brief Decode ECDAG for single failure
     * 
     * @param from available symbols
     * @param to lost symbols
     * @return ECDAG* 
     */
    ECDAG* DecodeSingle(vector<int> from, vector<int> to);

    /**
     * @brief Decode ECDAG for multiple failures
     * 
     * @param from available symbols
     * @param to lost symbols
     * @return ECDAG* 
     */
    ECDAG* DecodeMultiple(vector<int> from, vector<int> to);

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

#endif // __HH_XOR_HH__