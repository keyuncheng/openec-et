/**
 * @file AzureLRC.hh
 * @author Kaicheng Tang (kctang@cse.cuhk.edu.hk)
 * @brief Revised AzureLRC code implementation
 * @version 0.1
 * @date 2022-05-24
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#ifndef _AzureLRC_HH_
#define _AzureLRC_HH_

#include "../inc/include.hh"
#include "Computation.hh"

#include "ECBase.hh"

using namespace std;

#define RS_N_MAX (32)

class AzureLRC : public ECBase {
  private:
    int _l;
    int _m;
    //int _encode_matrix[RS_N_MAX * RS_N_MAX];   
    int *_encode_matrix = NULL;  

    vector<vector<int>> _layout; // layout (n), including all instances
    vector<int> _symbols; // virtual symbols (n)
    int _num_symbols;

    /**
     * @brief generate rows * cols cauchy matrix in GF(2^w)
     * 
     * @param matrix 
     * @param rows 
     * @param cols 
     * @param w 
     */
    void generate_matrix(int* matrix, int k, int l, int r, int w);

     /**
     * @brief initialize layout and symbols by instance id (out of num_instances)
     * 
     * @param num_instances total number of instances
     * @param instance_id current instance id
     */
    void init_layout(int num_instances, int instance_id);

  public:
    AzureLRC(int n, int k, int cps, int opt, vector<string> param);
    ~AzureLRC();

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

#endif // __AzureLRC_HH__

