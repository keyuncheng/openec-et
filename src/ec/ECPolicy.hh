#ifndef _ECPOLICY_HH_
#define _ECPOLICY_HH_

#include "ECBase.hh"
#include "RSCONV.hh"
#include "WASLRC.hh"
#include "HHXORPlus.hh"
#include "RSConv.hh"
#include "AzureLRC.hh"
#include "HTEC.hh"

#include "ETRSConv.hh"
#include "ETAzureLRC.hh"
#include "ETHHXORPlus.hh"
#include "ETHTEC.hh"

#include "../inc/include.hh"

using namespace std;

class ECPolicy {
  private:
    string _id;
    string _classname;
    int _n;
    int _k;
    int _w;
    bool _locality = false;
    int _opt;

    vector<string> _param;
  public:
//    ECPolicy(string id, string classname, int n, int k, int w, bool locality, int opt, vector<string> param);
    ECPolicy(string id, string classname, int n, int k, int w, int opt, vector<string> param);
    ECBase* createECClass();
    string getPolicyId();
    int getN();
    int getK();
    int getW();
    bool getLocality();
    int getOpt();
};

#endif
