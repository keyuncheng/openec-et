#include "AzureLRC.hh"

AzureLRC::AzureLRC(int n, int k, int w, int opt, vector<string> param) {
    _n = n;
    _k = k;
    _w = w;
    _opt = opt;

    // by default (as base code), num_instances = 1, instance_id = 0
    int num_instances = 1;
    int instance_id = 0;

    if (param.size() > 2) {
        num_instances = atoi(param[2].c_str()); // total number of instances
        instance_id = atoi(param[3].c_str()); // current instance id
    }
    // set default layout
    init_layout(num_instances, instance_id);


    // there should be two other parameters in param
    // 0. l (local parity num)
    // 1. r (global parity num)
    cout << param.size() << endl;
    _l = atoi(param[0].c_str());
    _m = atoi(param[1].c_str());

    _encode_matrix = (int *) malloc(_n * _k * sizeof(int));
    generate_matrix(_encode_matrix, _k, _l, _m, _w);

    printf("AzureLRC::_encode_matrix:\n");
    for (int i = 0; i < _n; i++) {
        for (int j = 0; j < _k; j++) {
            printf("%d ", _encode_matrix[i * _k + j]);
        }
        printf("\n");
    }

}

AzureLRC::~AzureLRC() {
    free(_encode_matrix);
}

void AzureLRC::init_layout(int num_instances, int instance_id) {

    _num_symbols = _n * _w; // number of symbols in one instance
    int num_layout_symbols = _n * _w; // number of symbols in layout of one instance
    int num_additional_symbols = _num_symbols - num_layout_symbols; // number of additional symbols in one instance

    int num_global_symbols = _num_symbols * num_instances;
    int num_global_layout_symbols = num_layout_symbols * num_instances;
    int num_global_additional_symbols = num_additional_symbols * num_instances;
    
    vector<vector<int>> global_symbols(num_instances);
    vector<vector<int>> global_layout(num_instances * _w);

    // assign layout symbols to each instance
    for (int ins_id = 0; ins_id < num_instances; ins_id++) {
        for (int sp = 0; sp < _w; sp++) {
            for (int i = 0; i < _n; i++) {
                int symbol = ins_id * num_layout_symbols + sp * _n + i;
                global_layout[ins_id * _w + sp].push_back(symbol);
                global_symbols[ins_id].push_back(symbol);
            }
        }
    }

    // assign additional symbols to each instance
    int vs_id = num_global_layout_symbols;
    for (int ins_id = 0; ins_id < num_instances; ins_id++) {
        for (int i = 0; i < num_additional_symbols; i++) {
            global_symbols[ins_id].push_back(vs_id++);
        }
    }

    _layout.clear();
    for (int i = 0; i < _w; i++) {
        _layout.push_back(global_layout[instance_id * _w + i]);
    }

    _symbols = global_symbols[instance_id];
}

void AzureLRC::Encode(ECDAG *ecdag) {
    if (ecdag == NULL) {
        printf("error: invalid input ecdag\n");
        return;
    }

    vector<int> data;
    vector<int> code;


    for (int i=0; i<_k; i++) data.push_back(_layout[0][i]); // only modification
    for (int i=_k; i<_n; i++) code.push_back(_layout[0][i]); // only modification
    
    // generate_matrix(_encode_matrix, _k, _l, _m, 8); 
    for (int i=0; i<code.size(); i++) {
        vector<int> coef;
        for (int j=0; j<_k; j++) {
        coef.push_back(_encode_matrix[(i+_k)*_k+j]);
        }
        ecdag->Join(code[i], data, coef);
    } 
    if (code.size() > 1) ecdag->BindX(code);

}

void AzureLRC::Decode(vector<int> from, vector<int> to, ECDAG *ecdag) {
    if (ecdag == NULL) {
            printf("error: invalid input ecdag\n");
            return;
    }  
    if (to.size() == 1) {
        int ridx = to[0];
        vector<int> data;
        vector<int> coef;
        int nr = _k / _l;
        int sidx = find(_layout[0].begin(), _layout[0].end(), ridx) - _layout[0].begin();
        printf("failure sidx = %d\n", sidx);

        if (sidx < _k) {
            // source data
            int gidx = sidx / nr;
            for (int i=0; i < nr; i++) {
                int idxinstripe = gidx * nr + i;
                if (sidx != idxinstripe) {
                    data.push_back(_layout[0][idxinstripe]);
                    coef.push_back(1);
                }
            }
            data.push_back(_layout[0][_k + gidx]);
            coef.push_back(1);
        } else if (sidx < (_k + _l)) {
            // local parity
            int gidx = sidx - _k;
            for (int i = 0; i < nr; i++) {
                int idxinstripe = gidx * nr + i;
                data.push_back(_layout[0][idxinstripe]);
                coef.push_back(1);
            }
        } else {
            // global parity
            generate_matrix(_encode_matrix, _k, _l, _m, 8);
            for (int i=0; i<_k; i++) {
                data.push_back(_layout[0][i]);
                coef.push_back(_encode_matrix[sidx * _k + i]);
            }
        }
        ecdag->Join(ridx, data, coef);
    } else {
        printf("Don't support multiple failures now!\n");
    }
}

void AzureLRC::generate_matrix(int* matrix, int k, int l, int r, int w) {
    int n = k + l + r;
    memset(matrix, 0, n * k * sizeof(int));

    // data blocks: set first k lines
    for (int i = 0; i < k; i ++) {
        matrix[i * k + i] = 1;
    }

    // local parity: set the following l lines as local parity
    int nr = k / l;
    for (int i = 0; i < l; i++) {
        for (int j = 0; j < nr; j ++) {
        matrix[(k + i) * k + i * nr + j] = 1;
        }
    }

    // Cauchy matrix
    int *p = &matrix[(_k + l) * _k];
    for (int i = k + l; i < n; i++) {
        for (int j = 0; j < k; j++) {
            *p++ = galois_single_divide(1, i ^ j, 8);
        }
    }
}


ECDAG* AzureLRC::Encode() {
    ECDAG* ecdag = new ECDAG();

    Encode(ecdag);

    return ecdag;
}

ECDAG* AzureLRC::Decode(vector<int> from, vector<int> to) {
    ECDAG* ecdag = new ECDAG();
    
    Decode(from, to, ecdag);

    return ecdag;
}

void AzureLRC::Place(vector<vector<int>>& group) {
}

vector<int> AzureLRC::GetNodeSymbols(int nodeid) {
    vector<int> symbols;
    for (int i = 0; i < _layout.size(); i++) {
        symbols.push_back(_layout[i][nodeid]);
    }

    return symbols;
}

vector<vector<int>> AzureLRC::GetLayout() {
    return _layout;
}

void AzureLRC::SetLayout(vector<vector<int>> layout) {
    if (layout.size() != _layout.size()) {
        printf("invalid layout\n");
        return;
    }

    _layout = layout;

    printf("AzureLRC::layout: \n");
    for (int sp = 0; sp < _w; sp++) {
        for (int i = 0; i < _n; i++) {
            printf("%d ", _layout[sp][i]);
        }
        printf("\n");
    }

}

void AzureLRC::SetSymbols(vector<int> symbols) {
    if (symbols.size() != _num_symbols) {
        printf("invalid symbols\n");
        return;
    }

    _symbols = symbols;

    printf("AzureLRC::symbol: \n");
    for (int i = 0; i < _num_symbols; i++) {
        printf("%d ", _symbols[i]);
    }
    printf("\n");
}
