/**
 * @file RSMultiIns.hh
 * @brief RS code with multiple instances (for implementation of RS-ET only)
 */

#ifndef __RS_MULTI_INS_HH__
#define __RS_MULTI_INS_HH__

#include "../inc/include.hh"
#include "Computation.hh"

#include "ECBase.hh"

class RSMultiIns : public ECBase {
private:
    int *_encode_matrix = NULL;
    int _m;

    vector<vector<int>> _layout; // layout (n), including all instances
    vector<int> _symbols; // virtual symbols (n)
    int _num_symbols;

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

    /**
     * @brief initialize layout and symbols by instance id (out of num_instances)
     * 
     * @param num_instances total number of instances
     * @param instance_id current instance id
     */
    void init_layout(int num_instances, int instance_id);

public:
    RSMultiIns(int n, int k, int w, int opt, vector<string> param);
    ~RSMultiIns();
 
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

    /**
     * @brief Set code layout 
     * 
     * @param vector<vector<int>> 
     * 0 2 4 6 8 ...
     * 1 3 5 7 9 ... 
     * 
     */
    void SetLayout(vector<vector<int>> layout);
    
    /**
     * @brief Set code layout 
     * 
     * @param vector<int> symbols (size equal to num_symbols)
     * the first few symbols are symbols in the layout, the following are required internal symbols 
     * 0 2 4 6 8 ...
     * 1 3 5 7 9 ... 
     * 
     */
    void SetSymbols(vector<int> symbols);

    /**
     * @brief append encode routine on ecdag
     * 
     * @param ecdag 
     */
    void Encode(ECDAG *ecdag);

    /**
     * @brief append decode routine on ecdag
     * 
     * @param from 
     * @param to 
     * @param ecdag 
     */
    void Decode(vector<int> from, vector<int> to, ECDAG *ecdag);
};

#endif // __RS_MULTI_INS_HH__
