#ifndef _ETHTEC_CC_
#define _ETHTEC_CC_

#include "ECBase.hh"
#include "HTEC.hh"

using namespace std;

class ETHTEC : public ECBase {
private:

    // Application of elastic transformation on the parities of HashTag

    HTEC **_instances;                                     // base code instances
    bool _useLargerBase;                                   // whether to use a larger base (and fewer code instances)
    int _numInstances;                                     // number of base code instances
    int _baseW;                                            // base code sub-packetization

    int _m;                                                // n-k
    int _q;                                                // field size
    int _e;                                                // base coefficient for transformation

    static const char *_useLargerBaseKey;                  // key of parameter for using a larger base

    vector<int>*** _transformationSourcePackets;           // variables in equations on how to transform the original parity packets (virtual node index)
    vector<int>*** _transformationMatrix;                  // coefficients in equations on how to transform the original parity packets
    vector<int>*** _reverseTransformationSourcePackets;    // variables in equations on how to reverse the transformed packet into original parity packet (global parity index)
    vector<int>*** _reverseTransformationMatrix;           // coefficients in equations on how to reverse the transformed parity packets

    /**
     * Construct a plain encoding ECDAG for multiple instances without transformation
     *
     * @return a encoding ecdag
     **/
    ECDAG* ConstructEncodeECDAG() const;

    /**
     * Construct a plain decoding ECDAG for multiple instances without transformation
     *
     * @return a decoding ecdag
     **/
    ECDAG* ConstructDecodeECDAG(vector<int> from, vector<int> to) const;

    /**
     * Construct a encoding ECDAG for multiple instances with transformation
     *
     * @return a encoding ecdag
     **/
    ECDAG* ConstructEncodeECDAGWithET() const;

    /**
     * Construct a decoding ECDAG for multiple instances with transformation
     *
     * @param from                          source packets
     * @param to                            packets to repair
     *
     * @return a decoding ecdag
     **/
    ECDAG* ConstructDecodeECDAGWithET(vector<int> from, vector<int> to) const;

    /**
     * Fill out the maps for transformation (forward and backward) 
     **/
    void FillTransformationMaps();

    /**
     * Fill out the forward transformation map with permuted indices 
     **/
    void FillMapWithPermutedIndices();

    /**
     * Couple packet pairs in the forward transformation map (and mark the coupling in the backward transformation map)
     **/
    void CouplePairsInParities();

    /**
     * Fill out the backward transformation map
     **/
    void ConstructDecodingMap();

    /**
     * Convert a parity index to a virtual index beyond the packets to store
     *
     * @param parityIndex             parity index
     *
     * @return the virtual node index if input index is within the range of parity packets [k * w, n * w); the input index if the input index refers to a data packet; -1 otherwise
     **/
    int ParityToVirtualNode(int parityIndex) const;

    /**
     * Convert a virtual node index back to a parity index
     *
     * @param virtualIndex             virtual node index
     *
     * @return the parity node index if input index is within the range of virtual node index [n * w, (k+n) * w); the input index if the input index refers to a data packet; -1 otherwise
     **/
    int VirtualNodeToParity(int virtualIndex) const;

    /**
     * Convert a 'global' index to a 'local' parity-only index which excludes data packets
     *
     * @param local                   global index
     *
     * @return the local index if global index is within the range of parity packets [k * w, n * w); -1 otherwise
     **/
    int GlobalToLocal(int global) const;

    /**
     * Convert a 'local' parity-only index to an 'global' index which includes data packets
     *
     * @param local                   local parity-only index
     *
     * @return the global index if local index is within the number of parity packets [0, (n-k) * w); -1 otherwise
     **/
    int LocalToGlobal(int local) const; 


public:

    ETHTEC(int n, int k, int alpha, int opt, vector<string> param);
    virtual ~ETHTEC();

    ECDAG* Encode();
    ECDAG* Decode(vector<int> from, vector<int> to);
    void Place(vector<vector<int>>& group);

    void PrintParityInfo(bool dense = false) const;
    void PrintSourcePackets(bool transform = true) const;

    /**
     * Convert a parity index to a virtual index beyond the packets to store
     *
     * @param n                       coding parameter n (total number of nodes)
     * @param k                       coding parameter k (number of data nodes)
     * @param w                       coding parameter w (sub-packetization)
     * @param parityIndex             parity index
     *
     * @return the virtual node index if input index is within the range of parity packets [k * w, n * w); the input index if the input index refers to a data packet; -1 otherwise
     **/
    static int ParityToVirtualNode(int n, int k, int w, int parityIndex); 
};

#endif // define _ETHTEC_CC_
