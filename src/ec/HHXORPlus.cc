/**
 * @file HHXORPlus.cc
 * @author Keyun Cheng (kycheng@cse.cuhk.edu.hk)
 * @brief 
 * @version 0.1
 * @date 2022-03-22
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#include "HHXORPlus.hh"

HHXORPlus::HHXORPlus(int n, int k, int w, int opt, vector<string> param) {
    
    _n = n;
    _k = k;
    _w = w;
    _opt = opt;

    if (_w != 2) {
        printf("error: w != 2\n");
        return;
    }

    if (_n - _k < 3) {
        printf("error: n - k should >= 3\n");
        return;
    }

    // divide data into groups
    int num_groups = n - k - 1;

    if (_k < num_groups * 2) {
        printf("error: k should >= (n - k - 1) * 2\n");
        return;
    }

    int avg_group_size = _k / num_groups;
    // Note: remove the last element out of the last group!
    int last_group_size = _k - avg_group_size * (num_groups - 1) - 1;

    for (int i = 0, data_idx = 0; i < num_groups; i++) {
        int parity_idx = i + 1;
        int group_size = (i < num_groups - 1) ? avg_group_size : last_group_size;
        for (int did = 0; did < group_size; did++) {
            _pid_group_map[parity_idx].push_back(data_idx);
            data_idx++;
        }

        printf("group[%d]: ", parity_idx);
        for (int did = 0; did < group_size; did++) {
            printf("%d ", _pid_group_map[parity_idx].at(did));
        }
        printf("\n");
    }

    // get layouts
    _layout = GetLayout();

    for (int sp = 0; sp < _w; sp++) {
        vector<int> &layout = _layout[sp];
        vector<int> data_layout;
        vector<int> code_layout;

        for (int i = 0; i < _n; i++) {
            if (i < _k) {
                data_layout.push_back(layout[i]);
            } else {
                code_layout.push_back(layout[i]);
            }
        }
        
        _data_layout.push_back(data_layout);
        _code_layout.push_back(code_layout);
    }

    _uncoupled_code_layout = _code_layout;
    // handle sp[0]
    int vs_id = _n * _w; // virtual symbol id
    _uncoupled_code_layout[0][couple_parity_id] = vs_id++;
    for (auto const &pid_group : _pid_group_map) {
        int parity_idx = pid_group.first;
        _pid_group_code_map[parity_idx] = vs_id++;
    }

    // handle sp[1]
    for (auto const &pid_group : _pid_group_map) {
        int parity_idx = pid_group.first;
        _uncoupled_code_layout[1][parity_idx] = vs_id++;
    }

    num_symbols = vs_id + 1; // total number of symbols

    printf("num_symbols: %d\n", num_symbols);

    printf("layout:\n");
    for (int sp = 0; sp < _w; sp++) {
        for (int i = 0; i < _n; i++) {
            printf("%d ", _layout[sp][i]);
        }
        printf("\n");
    }
    printf("\n");

    printf("data layout:\n");
    for (int sp = 0; sp < _w; sp++) {
        for (int i = 0; i < _k; i++) {
            printf("%d ", _data_layout[sp][i]);
        }
        printf("\n");
    }
    printf("\n");

    printf("code layout:\n");
    for (int sp = 0; sp < _w; sp++) {
        for (int i = 0; i < _n - _k; i++) {
            printf("%d ", _code_layout[sp][i]);
        }
        printf("\n");
    }
    printf("\n");

    printf("uncoupled code layout:\n");
    for (int sp = 0; sp < _w; sp++) {
        for (int i = 0; i < _n - _k; i++) {
            printf("%d ", _uncoupled_code_layout[sp][i]);
        }
        printf("\n");
    }
    printf("\n");

    printf("group code layout:\n");
    for (auto const &pid_group_code : _pid_group_code_map) {
        printf("pid: %d, code: %d\n", pid_group_code.first, pid_group_code.second);
    }
    printf("\n");

    // generate encoding matrix
    _rs_encode_matrix = (int *)malloc(_n * _k * sizeof(int));
    // generate_vandermonde_matrix(_rs_encode_matrix, _n, _k, 8);
    generate_cauchy_matrix(_rs_encode_matrix, _n, _k, 8);

    /**
     * @brief swap the first and second row of encode matrix
     * Originally, the first row are all 1, move to the second row
     */
    int *tmp_row = (int *)malloc(_k * sizeof(int));
    memcpy(tmp_row, &_rs_encode_matrix[_k * _k], _k * sizeof(int));
    memcpy(&_rs_encode_matrix[_k * _k], &_rs_encode_matrix[(_k + 1) * _k], _k * sizeof(int));
    memcpy(&_rs_encode_matrix[(_k + 1) * _k], tmp_row, _k * sizeof(int));
    free(tmp_row);

    printf("info: Hitchhiker-XORPlus(%d, %d) initialized\n", _n, _k);
}

HHXORPlus::~HHXORPlus() {
    free(_rs_encode_matrix);
}

void HHXORPlus::generate_vandermonde_matrix(int* matrix, int rows, int cols, int w) {
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

void HHXORPlus::generate_cauchy_matrix(int* matrix, int rows, int cols, int w) {
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

ECDAG* HHXORPlus::Encode() {
    ECDAG* ecdag = new ECDAG();

    // calculate uncoupled parity
    for (int sp = 0; sp < _w; sp++) {
        vector<int> &data_layout = _data_layout[sp];
        vector<int> &uncoupled_code_layout = _uncoupled_code_layout[sp];
        
        for (int i = 0; i < _n - _k; i++) {
            vector<int> coefs(&_rs_encode_matrix[(i + _k) * _k], _rs_encode_matrix + (i + _k + 1) * _k);
            ecdag->Join(uncoupled_code_layout[i], data_layout, coefs);
        }

        // BindX and BindY (spw)
        int vidx = ecdag->BindX(uncoupled_code_layout);
        ecdag->BindY(vidx, data_layout[0]);
    }

    // calculate group code
    int num_groups = _pid_group_map.size();
    for (auto const &pid_group : _pid_group_map) {
        int parity_idx = pid_group.first;
        const vector<int> &group = pid_group.second;
        int pidx = _pid_group_code_map[parity_idx];
        int group_size = group.size();

        vector<int> cidx_group;
        for (auto node_id : group) {
            cidx_group.push_back(_layout[0][node_id]);
        }
        
        vector<int> coefs_group(group_size);
        for (int i = 0; i < group_size; i++) {
            coefs_group[i] = _rs_encode_matrix[(_k + couple_parity_id) * _k + group[i]];
        }

        ecdag->Join(pidx, cidx_group, coefs_group);

        // BindY (sp0)
        ecdag->BindY(pidx, cidx_group[0]);
    }

    // couple the only parity in sp[0]
    ecdag->Join(_code_layout[0][couple_parity_id],
        {_uncoupled_code_layout[0][couple_parity_id],
            _uncoupled_code_layout[1][couple_parity_id],
            _pid_group_code_map[couple_parity_id]},
        {1, 1, 1});

    // BindY (sp0)
    ecdag->BindY(_code_layout[0][couple_parity_id], _uncoupled_code_layout[0][couple_parity_id]);

    // couple the parities in sp[1]
    for (auto const &pid_group : _pid_group_map) {
        int parity_idx = pid_group.first;
        ecdag->Join(_code_layout[1][parity_idx],
        {_uncoupled_code_layout[1][parity_idx],
            _pid_group_code_map[parity_idx]},
        {1, 1});

        // BindY (sp1)
        ecdag->BindY(_code_layout[1][parity_idx], _uncoupled_code_layout[1][parity_idx]);
    }

    return ecdag;
}

ECDAG* HHXORPlus::Decode(vector<int> from, vector<int> to) {

    // num_lost_symbols * w lost symbols
    if (from.size() % _w != 0 || to.size() % _w != 0) {
        printf("error: invalid number of symbols\n");
        return NULL;
    }

    int num_failed_nodes = to.size() / _w;

    if (num_failed_nodes == 1) {
        return DecodeSingle(from, to);
    } else {
        return DecodeMultiple(from, to);
    }
}

void HHXORPlus::Place(vector<vector<int>>& group) {
    return;
}

ECDAG* HHXORPlus::DecodeSingle(vector<int> from, vector<int> to) {

    int failed_node = to[0] / _w; // failed node id
    if (failed_node >= _k) { // parity node failure
        return DecodeMultiple(from, to); // resort to conventional repair
    }

    ECDAG* ecdag = new ECDAG();

    // create node_syms_map: <node_id, <symbol_0, symbol_1, ...>>
    map<int, vector<int>> node_syms_map; // ordered by node id
    for (auto symbol : from) {
        node_syms_map[symbol / _w].push_back(symbol);
    }

    vector<int> avail_data_node; // available data node
    vector<int> avail_parity_node; // available parity node
    for (auto node_syms : node_syms_map) {
        int node_id = node_syms.first;
        if (node_id < _k) {
            avail_data_node.push_back(node_id);
        } else {
            avail_parity_node.push_back(node_id);
        }
    }

    // data node failure
    int *recover_matrix = (int *) malloc(_k * _k * sizeof(int));
    int *recover_matrix_inv = (int *) malloc(_k * _k * sizeof(int));
    int *select_vector = (int *) malloc(_k * sizeof(int));

    for (int i = 0; i < avail_data_node.size(); i++) {
        memcpy(&recover_matrix[i * _k], &_rs_encode_matrix[avail_data_node[i] * _k], _k * sizeof(int));
    }

    // copy the first parity function
    memcpy(&recover_matrix[(_k - 1) * _k], &_rs_encode_matrix[_k * _k], _k * sizeof(int));

    int mtx_invertible = jerasure_invert_matrix(recover_matrix, recover_matrix_inv, _k, 8);

    if (mtx_invertible == -1) {
        printf("error: recover_matrix not invertible!\n");
    }

    // get decode_vector for data symbol in sp[1]
    memcpy(select_vector, &recover_matrix_inv[failed_node * _k], _k * sizeof(int));

    vector<int> cidx_sp1;
    for (auto node_id : avail_data_node) {
        cidx_sp1.push_back(_layout[1][node_id]);
    }
    cidx_sp1.push_back(_layout[1][_k]); // append the first parity

    // recover data symbol in sp[1]
    vector<int> coefs_data_sp1(select_vector, select_vector + _k);
    ecdag->Join(_data_layout[1][failed_node], cidx_sp1, coefs_data_sp1);

    if (failed_node < _k - 1) { // first k - 1 node    
        // locate the group
        for (auto const &pid_group : _pid_group_map) {
            const vector<int> &group = pid_group.second;
            if (std::find(group.begin(), group.end(), failed_node) == group.end()) {
                continue;
            }
            int group_size = group.size();
            int parity_idx = pid_group.first;

            // get decode_vector for uncoupled parity symbol in sp[1]
            memcpy(select_vector, &_rs_encode_matrix[(_k + parity_idx) * _k], _k * sizeof(int));

            int *decode_parity_vector = jerasure_matrix_multiply(
                select_vector, recover_matrix_inv, 1, _k, _k, _k, 8);

            // recover uncoupled parity symbol in sp[1]
            vector<int> coefs_parity_sp1(decode_parity_vector, decode_parity_vector + _k);
            ecdag->Join(_uncoupled_code_layout[1][parity_idx], cidx_sp1, coefs_parity_sp1);

            // BindX and BindY (sp1)
            int vidx = ecdag->BindX({_data_layout[1][failed_node], _uncoupled_code_layout[1][parity_idx]});
            ecdag->BindY(vidx, cidx_sp1[0]);
            ecdag->BindY(_data_layout[1][failed_node], vidx);
            ecdag->BindY(_uncoupled_code_layout[1][parity_idx], vidx);

            // XOR the coupled parity to get group code
            ecdag->Join(_pid_group_code_map[parity_idx],
                {_uncoupled_code_layout[1][parity_idx], _code_layout[1][parity_idx]},
                {1, 1});

            // BindY
            ecdag->BindY(_pid_group_code_map[parity_idx], _uncoupled_code_layout[1][parity_idx]);

            // recover data symbol in sp[0] by solving f(couple_parity_id)
            int *recover_matrix_group = (int *) malloc(group_size * group_size * sizeof(int));
            int *recover_matrix_group_inv = (int *) malloc(group_size * group_size * sizeof(int));
            int *select_vector_group = (int *) malloc(group_size * sizeof(int));
            
            int in_group_idx = -1;
            memset(recover_matrix_group, 0, group_size * group_size * sizeof(int));
            for (int i = 0, row_idx = 0; i < group_size; i++) {
                if (group[i] != failed_node) {
                    recover_matrix_group[row_idx * group_size + i] = 1;
                    row_idx++;
                } else {
                    in_group_idx = i;
                }
            }
            for (int i = 0; i < group_size; i++) {
                recover_matrix_group[(group_size - 1) * group_size + i] = _rs_encode_matrix[(_k + couple_parity_id) * _k + group[i]];
            }

            mtx_invertible = jerasure_invert_matrix(recover_matrix_group, recover_matrix_group_inv, group_size, 8);

            if (mtx_invertible == -1) {
                printf("error: recover_matrix not invertible!\n");
            }

            memcpy(select_vector_group, &recover_matrix_group_inv[in_group_idx * group_size], group_size * sizeof(int));

            vector<int> cidx_group;
            for (auto node_id : group) {
                if (node_id != failed_node) {
                    cidx_group.push_back(_layout[0][node_id]);
                }
            }
            cidx_group.push_back(_pid_group_code_map[parity_idx]);
            vector<int> coefs_group(select_vector_group, select_vector_group + group_size);
            
            ecdag->Join(_data_layout[0][failed_node], cidx_group, coefs_group);

            // BindY
            ecdag->BindY(_data_layout[0][failed_node], cidx_group[group_size - 1]);
            
            free(decode_parity_vector);
            free(recover_matrix_group);
            free(recover_matrix_group_inv);
            free(select_vector_group);
        }

    } else { // special handling for the last data node

        // BindX
        vector<int> symbols_bindx;
        symbols_bindx.push_back(_data_layout[1][failed_node]);

        for (auto const &pid_group : _pid_group_map) {
            const vector<int> &group = pid_group.second;
            int group_size = group.size();
            int parity_idx = pid_group.first;

            // get decode_vector for uncoupled parity symbol in sp[1]
            memcpy(select_vector, &_rs_encode_matrix[(_k + parity_idx) * _k], _k * sizeof(int));
            int *decode_parity_vector = jerasure_matrix_multiply(
                select_vector, recover_matrix_inv, 1, _k, _k, _k, 8);

            // recover uncoupled parity symbol in sp[1]
            vector<int> coefs_parity_sp1(decode_parity_vector, decode_parity_vector + _k);
            ecdag->Join(_uncoupled_code_layout[1][parity_idx], cidx_sp1, coefs_parity_sp1);
            
            symbols_bindx.push_back(_uncoupled_code_layout[1][parity_idx]);
        }

        // BindX and BindY (sp1)
        int vidx = ecdag->BindX(symbols_bindx);
        ecdag->BindY(vidx, cidx_sp1[0]);

        for (auto symbol : symbols_bindx) {
            ecdag->BindY(symbol, vidx);
        }
        
        // XOR couples
        vector<int> cidx_couples;
        vector<int> coefs_couples;
        for (auto const &pid_group_code : _pid_group_code_map) {
            int parity_idx = pid_group_code.first;
            int uncoupled_code = _uncoupled_code_layout[1][parity_idx];
            cidx_couples.push_back(uncoupled_code);
            if (parity_idx != couple_parity_id) {
                cidx_couples.push_back(_code_layout[1][parity_idx]);
            } else {
                cidx_couples.push_back(_code_layout[0][parity_idx]);
            }
            coefs_couples.push_back(1);
            coefs_couples.push_back(1);                
        }

        int pidx = _uncoupled_code_layout[1][_n - _k - 1] + 1; // add a new virtual symbol
        ecdag->Join(pidx, cidx_couples, coefs_couples);

        // BindY
        ecdag->BindY(pidx, cidx_couples[0]);

        int coef = galois_single_divide(
            1, _rs_encode_matrix[(_k + couple_parity_id) * _k + failed_node], 8);

        ecdag->Join(_data_layout[0][failed_node], {pidx}, {coef});

        // BindY
        ecdag->BindY(_data_layout[0][failed_node], pidx);
    }

    free(recover_matrix);
    free(recover_matrix_inv);
    free(select_vector);

    return ecdag;
}

ECDAG* HHXORPlus::DecodeMultiple(vector<int> from, vector<int> to) {
    ECDAG* ecdag = new ECDAG();

    // create avail_node_syms_map: <node_id, <symbol_0, symbol_1, ...>>
    map<int, vector<int>> avail_node_syms_map; // ordered by node id
    for (auto symbol : from) {
        avail_node_syms_map[symbol / _w].push_back(symbol);
    }

    // create lost_node_syms_map: <node_id, <symbol_0, symbol_1, ...>>
    map<int, vector<int>> lost_node_syms_map; // ordered by node id
    for (auto symbol : to) {
        lost_node_syms_map[symbol / _w].push_back(symbol);
    }
    int num_lost_nodes = lost_node_syms_map.size();

    // available node
    vector<int> avail_data_node; // available data node
    vector<int> avail_parity_node; // available parity node
    int num_avail_data = 0;
    int num_avail_parity = 0;

    // append all data
    for (auto avail_node_syms : avail_node_syms_map) {
        int node_id = avail_node_syms.first;
        if (node_id < _k) {
            avail_data_node.push_back(node_id);
            num_avail_data++;
        }
    }

    // append parities until (data + parity = k)
    for (auto avail_node_syms : avail_node_syms_map) {
        int node_id = avail_node_syms.first;
        if (node_id >= _k && num_avail_data + num_avail_parity < _k) {
            avail_parity_node.push_back(node_id);
            num_avail_parity++;
        }
    }

    // make sure we use k nodes only
    assert(num_avail_data + num_avail_parity == _k);

    // lost node
    vector<int> lost_data_node; // lost data node
    vector<int> lost_parity_node; // lost parity node
    for (auto lost_node_syms : lost_node_syms_map) {
        int node_id = lost_node_syms.first;
        if (node_id < _k) {
            lost_data_node.push_back(node_id);
        } else {
            lost_parity_node.push_back(node_id);
        }
    }

    for (auto node_id : avail_data_node) {
        printf("avail_data: %d\n", node_id);
    }

    for (auto node_id : avail_parity_node) {
        printf("avail_parity: %d\n", node_id);
    }

    for (auto node_id : lost_data_node) {
        printf("lost_data: %d\n", node_id);
    }

    for (auto node_id : lost_parity_node) {
        printf("lost_parity: %d\n", node_id);
    }

    // special handling for coupled item
    if (std::find(avail_parity_node.begin(), avail_parity_node.end(), couple_parity_id + _k) != avail_parity_node.end()) {
        ecdag->Join(_uncoupled_code_layout[0][couple_parity_id],
            {_code_layout[0][couple_parity_id], _code_layout[1][couple_parity_id]},
            {1, 1});

        // BindY
        ecdag->BindY(_uncoupled_code_layout[0][couple_parity_id], _code_layout[0][couple_parity_id]);
    }

    // recover all symbols in sp[0]
    int *recover_matrix = (int *) malloc(_k * _k * sizeof(int));
    int *recover_matrix_inv = (int *) malloc(_k * _k * sizeof(int));
    int *select_vector = (int *) malloc(_k * sizeof(int));

    // copy the first parity function
    for (int i = 0; i < avail_data_node.size(); i++) {
        memcpy(&recover_matrix[i * _k], &_rs_encode_matrix[avail_data_node[i] * _k], _k * sizeof(int));
    }

    for (int i = 0; i < avail_parity_node.size(); i++) {
        memcpy(&recover_matrix[(num_avail_data + i) * _k], &_rs_encode_matrix[avail_parity_node[i] * _k], _k * sizeof(int));
    }

    int mtx_invertible = jerasure_invert_matrix(recover_matrix, recover_matrix_inv, _k, 8);

    if (mtx_invertible == -1) {
        printf("error: recover_matrix not invertible!\n");
    }

    // cidx sp[0]
    vector<int> cidx_sp0;
    for (auto node_id : avail_data_node) {
        cidx_sp0.push_back(_layout[0][node_id]);
    }
    
    for (auto node_id : avail_parity_node) {
        cidx_sp0.push_back(_uncoupled_code_layout[0][node_id - _k]);
    }

    // BindX (sp0)
    vector<int> bindx_sp0;

    for (auto lost_node_syms : lost_node_syms_map) {
        // get decode_vector in sp[0]
        int failed_node = lost_node_syms.first;
        memcpy(select_vector, &_rs_encode_matrix[failed_node * _k], _k * sizeof(int));

        int *decode_parity_vector = jerasure_matrix_multiply(
            select_vector, recover_matrix_inv, 1, _k, _k, _k, 8);
        
        vector<int> coefs_parity_sp0(decode_parity_vector, decode_parity_vector + _k);

        if (failed_node < _k) {
            ecdag->Join(_data_layout[0][failed_node], cidx_sp0, coefs_parity_sp0);

            // BindX
            bindx_sp0.push_back(_data_layout[0][failed_node]);
        } else { // for parity, calculate uncoupled code and group code
            int parity_idx = failed_node - _k;

            // calculate uncoupled code
            ecdag->Join(_uncoupled_code_layout[0][parity_idx], cidx_sp0, coefs_parity_sp0);

            // BindX
            bindx_sp0.push_back(_uncoupled_code_layout[0][parity_idx]);

            if (_pid_group_map.find(parity_idx) != _pid_group_map.end()) {
                // calculate group code
                const vector<int> &group = _pid_group_map[parity_idx];
                int pidx = _pid_group_code_map[parity_idx];
                int group_size = group.size();

                vector<int> cidx_group;
                for (auto node_id : group) {
                    cidx_group.push_back(_layout[0][node_id]);
                }
                
                vector<int> coefs_group(group_size);
                for (int i = 0; i < group_size; i++) {
                    coefs_group[i] = _rs_encode_matrix[(_k + couple_parity_id) * _k + group[i]];
                }

                ecdag->Join(pidx, cidx_group, coefs_group);

                // BindY
                ecdag->BindY(pidx, cidx_group[0]);
            }
        }

        free(decode_parity_vector);
    }

    // recover uncoupled items for sp[1]
    for (int i = 0; i < num_avail_parity; i++) {
        int parity_idx = avail_parity_node[i] - _k;

        if (_pid_group_map.find(parity_idx) != _pid_group_map.end()) {
            // calculate group code
            const vector<int> &group = _pid_group_map[parity_idx];
            int group_size = group.size();
            int pidx = _pid_group_code_map[parity_idx];
            
            vector<int> cidx_group;
            for (auto node_id : group) {
                cidx_group.push_back(_layout[0][node_id]);
            }

            vector<int> coefs_group(group_size);
            for (int i = 0; i < group_size; i++) {
                coefs_group[i] = _rs_encode_matrix[(_k + couple_parity_id) * _k + group[i]];
            }
            ecdag->Join(pidx, cidx_group, coefs_group);

            // BindY (sp0)
            ecdag->BindY(pidx, cidx_sp0[0]);

            ecdag->Join(_uncoupled_code_layout[1][parity_idx],
                {_code_layout[1][parity_idx], pidx},
                {1, 1});

            // BindY (sp0)
            ecdag->BindY(_uncoupled_code_layout[1][parity_idx], pidx);
        }
    }

    // BindX and BindY (sp0)
    if (bindx_sp0.size() > 1) {
        int vidx = ecdag->BindX(bindx_sp0);
        ecdag->BindY(vidx, cidx_sp0[0]);

        for (auto symbol : bindx_sp0) {
            ecdag->BindY(symbol, vidx);
        }
    } else {
        ecdag->BindY(bindx_sp0[0], cidx_sp0[0]);
    }

    // cidx sp[1]
    vector<int> cidx_sp1;
    for (auto node_id : avail_data_node) {
        cidx_sp1.push_back(_data_layout[1][node_id]);
    }
    for (auto node_id : avail_parity_node) {
        cidx_sp1.push_back(_uncoupled_code_layout[1][node_id - _k]);
    }

    // BindX (sp1)
    vector<int> bindx_sp1;

    for (auto lost_node_syms : lost_node_syms_map) {
        // get decode_vector in sp[1]
        int failed_node = lost_node_syms.first;
        memcpy(select_vector, &_rs_encode_matrix[failed_node * _k], _k * sizeof(int));
        int *decode_parity_vector = jerasure_matrix_multiply(
            select_vector, recover_matrix_inv, 1, _k, _k, _k, 8);
        
        vector<int> coefs_parity_sp1(decode_parity_vector, decode_parity_vector + _k);

        if (failed_node < _k) {
            ecdag->Join(_data_layout[1][failed_node], cidx_sp1, coefs_parity_sp1);

            // BindX (sp1)
            bindx_sp1.push_back(_data_layout[1][failed_node]);
        } else { // for parity, calculate uncoupled code
            ecdag->Join(_uncoupled_code_layout[1][failed_node - _k], cidx_sp1, coefs_parity_sp1);

            // BindX (sp1)
            bindx_sp1.push_back(_uncoupled_code_layout[1][failed_node - _k]);
        }

        free(decode_parity_vector);
    }

    // BindX and BindY (sp1)
    if (bindx_sp1.size() > 1) {
        int vidx = ecdag->BindX(bindx_sp1);
        ecdag->BindY(vidx, cidx_sp1[0]);

        for (auto symbol : bindx_sp1) {
            ecdag->BindY(symbol, vidx);
        }
    } else {
        ecdag->BindY(bindx_sp1[0], cidx_sp1[0]);
    }

    // re-couple the only parity in sp[0]
    if (std::find(lost_parity_node.begin(), lost_parity_node.end(), couple_parity_id + _k) != lost_parity_node.end()) {
        ecdag->Join(_code_layout[0][couple_parity_id],
            {_uncoupled_code_layout[0][couple_parity_id],
                _uncoupled_code_layout[1][couple_parity_id],
                _pid_group_code_map[couple_parity_id]},
            {1, 1, 1});

        // BindY (sp0)
        ecdag->BindY(_code_layout[0][couple_parity_id], _uncoupled_code_layout[0][couple_parity_id]);
    }

    // couple the parities in sp[1]
    for (int i = 0; i < lost_parity_node.size(); i++) {
        int parity_idx = lost_parity_node[i] - _k;

        if (_pid_group_map.find(parity_idx) != _pid_group_map.end()) {
            ecdag->Join(_code_layout[1][parity_idx],
            {_uncoupled_code_layout[1][parity_idx],
                _pid_group_code_map[parity_idx]},
            {1, 1});
            
            // BindY (sp1)
            ecdag->BindY(_code_layout[1][parity_idx], _uncoupled_code_layout[1][parity_idx]);
        }

    }

    return ecdag;
}

vector<int> HHXORPlus::GetNodeSymbols(int nodeid) {
    vector<int> symbols;
    for (int i = 0; i < _w; i++) {
        symbols.push_back(_layout[i][nodeid]);
    }

    return symbols;
}

vector<vector<int>> HHXORPlus::GetLayout() {
    vector<vector<int>> layout(_w);

    for (int sp = 0; sp < _w; sp++) {
        for (int i = 0; i < _n; i++) {
            layout[sp].push_back(_w * i + sp);
        }
    }

    return layout;
}