#include "RSMultiIns.hh"

RSMultiIns::RSMultiIns(int n, int k, int w, int opt, vector<string> param) {
    _n = n;
    _k = k;
    _w = w;
    _opt = opt;
    _m = _n - _k;

    // by default (as base code), num_instances = 1, instance_id = 0
    int num_instances = 1;
    int instance_id = 0;

    if (param.size() > 0) {
        num_instances = atoi(param[0].c_str()); // total number of instances
        instance_id = atoi(param[1].c_str()); // current instance id
    }

    // set default layout
    init_layout(num_instances, instance_id);

    _encode_matrix = (int *) malloc(_n * _k * sizeof(int));

    generate_cauchy_matrix(_encode_matrix, _n, _k, 8);

    printf("RSMultiIns::_encode_matrix:\n");
    for (int i = 0; i < _n; i++) {
        for (int j = 0; j < _k; j++) {
            printf("%d ", _encode_matrix[i * _k + j]);
        }
        printf("\n");
    }
}

RSMultiIns::~RSMultiIns() {
    free(_encode_matrix);
}


void RSMultiIns::generate_vandermonde_matrix(int* matrix, int rows, int cols, int w) {
    int k = cols;
    int n = rows;
    int m = n - k;

    memset(matrix, 0, rows * cols * sizeof(int));
    for(int i=0; i<k; i++) {
        matrix[i*k+i] = 1;
    }

    // Vandermonde matrix
    for (int i=0; i<m; i++) {
        int tmp = 1;
        for (int j=0; j<k; j++) {
            matrix[(i+k)*cols+j] = tmp;
            tmp = Computation::singleMulti(tmp, i+1, w);
        }
    }
}

void RSMultiIns::generate_cauchy_matrix(int* matrix, int rows, int cols, int w) {
    int k = cols;
    int n = rows;
    int m = n - k;

    memset(matrix, 0, rows * cols * sizeof(int));
    for(int i=0; i<k; i++) {
        matrix[i*k+i] = 1;
    }

    // Cauchy matrix
    int *p = &matrix[_k * _k];
    for (int i = k; i < n; i++) {
        for (int j = 0; j < k; j++) {
            *p++ = galois_single_divide(1, i ^ j, 8);
        }
    }
}

void RSMultiIns::init_layout(int num_instances, int instance_id) {

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

void RSMultiIns::Encode(ECDAG *ecdag) {
    if (ecdag == NULL) {
        printf("error: invalid input ecdag\n");
        return;
    }

    vector<int> data;
    vector<int> code;
//   for (int i=0; i<_k; i++) data.push_back(i);
//   for (int i=_k; i<_n; i++) code.push_back(i);
    for (int i=0; i<_k; i++) data.push_back(_layout[0][i]); // only modification
    for (int i=_k; i<_n; i++) code.push_back(_layout[0][i]); // only modification
  
    generate_cauchy_matrix(_encode_matrix, _n, _k, 8);
    for (int i=0; i<_m; i++) {
        vector<int> coef;
        for (int j=0; j<_k; j++) {
            coef.push_back(_encode_matrix[(i+_k)*_k+j]);
        }
        ecdag->Join(code[i], data, coef);
    }
    if (_m > 1) {
        int vidx = ecdag->BindX(code);
        ecdag->BindY(vidx, _layout[0][0]);
    }
}

void RSMultiIns::Decode(vector<int> from, vector<int> to, ECDAG *ecdag) {
    if (ecdag == NULL) {
        printf("error: invalid input ecdag\n");
        return;
    }

    generate_cauchy_matrix(_encode_matrix, _n, _k, 8);
    vector<int> data;
    int _select_matrix[_k*_k];
    for (int i=0; i<_k; i++) {
        data.push_back(from[i]);
        // int sidx = from[i];
        int sidx = find(_layout[0].begin(), _layout[0].end(), from[i]) - _layout[0].begin();
        memcpy(_select_matrix + i * _k,
            _encode_matrix + sidx * _k,
        sizeof(int) * _k);
    }
    int _invert_matrix[_k*_k];

    jerasure_invert_matrix(_select_matrix, _invert_matrix, _k, 8); 
    for (int i=0; i<to.size(); i++) {
        // int ridx = to[i];
        int ridx = find(_layout[0].begin(), _layout[0].end(), to[i]) - _layout[0].begin();
        
        int _select_vector[_k];
        memcpy(_select_vector,
            _encode_matrix + ridx * _k,
        _k * sizeof(int));
        int* _coef_vector = jerasure_matrix_multiply(
            _select_vector, _invert_matrix, 1, _k, _k, _k, 8);
        vector<int> coef;
        for (int i=0; i<_k; i++) coef.push_back(_coef_vector[i]);
        ecdag->Join(to[i], data, coef);
    }
    if (to.size() > 1) {
        int vidx = ecdag->BindX(to);
        ecdag->BindY(vidx, from[0]);
        for (auto symbol : to) {
            ecdag->BindY(symbol, vidx);
        }
    } else {
        ecdag->BindY(to[0], from[0]);
    }
}

ECDAG* RSMultiIns::Encode() {
    ECDAG* ecdag = new ECDAG();

    Encode(ecdag);

    return ecdag;
}

ECDAG* RSMultiIns::Decode(vector<int> from, vector<int> to) {
    ECDAG* ecdag = new ECDAG();
    
    Decode(from, to, ecdag);

    return ecdag;
}

void RSMultiIns::Place(vector<vector<int>>& group) {
}

vector<int> RSMultiIns::GetNodeSymbols(int nodeid) {
    vector<int> symbols;
    for (int i = 0; i < _layout.size(); i++) {
        symbols.push_back(_layout[i][nodeid]);
    }

    return symbols;
}

vector<vector<int>> RSMultiIns::GetLayout() {
    return _layout;
}

void RSMultiIns::SetLayout(vector<vector<int>> layout) {
    if (layout.size() != _layout.size()) {
        printf("invalid layout\n");
        return;
    }

    _layout = layout;

    printf("RSMultiIns::layout: \n");
    for (int sp = 0; sp < _w; sp++) {
        for (int i = 0; i < _n; i++) {
            printf("%d ", _layout[sp][i]);
        }
        printf("\n");
    }

}

void RSMultiIns::SetSymbols(vector<int> symbols) {
    if (symbols.size() != _num_symbols) {
        printf("invalid symbols\n");
        return;
    }

    _symbols = symbols;

    printf("RSMultiIns::symbol: \n");
    for (int i = 0; i < _num_symbols; i++) {
        printf("%d ", _symbols[i]);
    }
    printf("\n");
}