#ifndef _ECBASE_HH_
#define _ECBASE_HH_

#include "../inc/include.hh"

#include "ECDAG.hh"

using namespace std;

class ECBase {
  public:
    int _n, _k, _w;
    //bool _locality;
    int _opt;

    ECBase();
    ECBase(int n, int k, int w, int opt, vector<string> param);
    
    virtual ECDAG* Encode() = 0;
    virtual ECDAG* Decode(vector<int> from, vector<int> to) = 0;
    virtual void Place(vector<vector<int>>& group) = 0;

    /**
     * additional functions
     * Note: It's impure virtual so as not to implement them for every code
     */

    /**
     * @brief Get symbols from a node
     * 
     * @param nodeid 
     * @return vector<int> 
     */
    virtual vector<int> GetNodeSymbols(int nodeid);
    
    /**
     * @brief get node layout
     * 
     * @return vector<vector<int>> 
     */
    virtual vector<vector<int>> GetLayout();
};

#endif
