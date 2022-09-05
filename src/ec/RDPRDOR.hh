#ifndef _RDPRDOR_HH_
#define _RDPRDOR_HH_

#include "../inc/include.hh"
#include "Computation.hh"

#include "ECBase.hh"

using namespace std;

#define RDPBASE_DEBUG_ENABLE true 

class RDPRDOR : public ECBase {
    private:
        int _m;
        int _p;

        unordered_map<int, int> _idx2diag;
        unordered_map<int, vector<int>> _diag2data;
        unordered_map<int, int> _diag2code;

        vector<int> diagRepair(int failnode);

    public:
        RDPRDOR(int n, int k, int w, int opt, vector<string> param);

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

#endif
