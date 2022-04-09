#include "ETRSConv.hh"

ETRSConv::ETRSConv(int n, int k, int w, int opt, vector<string> param) {
    _n = n;
    _k = k;
    _w = w;
    _opt = opt;
    _m = _n - _k;


    if (param.size() < 1) {
        printf("error: invalid number of arguments\n");
        return;
    }

    if (_k < _w) {
        printf("error: invalid arguments\n");
        return;
    }

    int num_instances = atoi(param[0].c_str());

    if (num_instances != _w) {
        printf("error: invalid _w (for base code, _w must be 1)\n");
        return;
    }

    int symbol_id = 0;

    // set layout
    for (int i = 0; i < num_instances; i++) {
        _layout.push_back(vector<int>());
    }

    for (int i = 0; i < _n; i++) {
        for (int j = 0; j < num_instances; j++) {
            _layout[j].push_back(symbol_id++);
        }
    }

    // set uncoupled layout
    for (int i = 0; i < num_instances; i++) {
        _uncoupled_layout.push_back(vector<int>());
    }

    for (int i = 0; i < _n; i++) {
        for (int j = 0; j < num_instances; j++) {
            _uncoupled_layout[j].push_back(symbol_id++);
        }
    }

    // et groups
    int num_data_groups = _k / _w;
    int num_parity_groups = _m / _w;

    // data
    for (int i = 0, grp_id = 0; i < num_data_groups; i++) {
        vector<int> group;
        int group_size = (i < num_data_groups - 1) ? _w : (_k - _w * i);
        for (int j = 0; j < group_size; j++) {
            group.push_back(grp_id++);
        }
        _data_et_groups.push_back(group);
    }

    // parity
    for (int i = 0, grp_id = _k; i < num_parity_groups; i++) {
        vector<int> group;
        int group_size = (i < num_parity_groups - 1) ? _w : (_m - _w * i);
        for (int j = 0; j < group_size; j++) {
            group.push_back(grp_id++);
        }
        _parity_et_groups.push_back(group);
    }

    printf("data_et_groups:\n");
    for (auto &group : _data_et_groups) {
        for (auto node_id : group) {
            printf("%d ", node_id);
        }
        printf("\n");
    }

    printf("parity_et_groups:\n");
    for (auto &group : _parity_et_groups) {
        for (auto node_id : group) {
            printf("%d ", node_id);
        }
        printf("\n");
    }

    // data et units
    for (int i = 0; i < _data_et_groups.size(); i++) {
        vector<int> &group = _data_et_groups[i];
        printf("group id: %d\n", i);
        int group_size = group.size();

        vector<vector<int>> uncoupled_layout;
        vector<vector<int>> layout;

        for (int j = 0; j < num_instances; j++) {
            vector<int> ul;
            vector<int> l;
            for (auto node_id : group) {
                ul.push_back(_uncoupled_layout[j][node_id]);
                l.push_back(_layout[j][node_id]);                
            }

            uncoupled_layout.push_back(ul);
            layout.push_back(l);
        }
        
        ETUnit *et_unit = new ETUnit(_w, group_size, 1, false, uncoupled_layout, layout);
        _data_et_units.push_back(et_unit);
    }

    // parity et units
    for (int i = 0; i < _parity_et_groups.size(); i++) {
        vector<int> &group = _parity_et_groups[i];
        printf("group id: %d\n", i);
        int group_size = group.size();

        vector<vector<int>> uncoupled_layout;
        vector<vector<int>> layout;

        for (int j = 0; j < num_instances; j++) {
            vector<int> ul;
            vector<int> l;
            for (auto node_id : group) {
                ul.push_back(_uncoupled_layout[j][node_id]);
                l.push_back(_layout[j][node_id]);                
            }

            uncoupled_layout.push_back(ul);
            layout.push_back(l);
        }
        
        ETUnit *et_unit = new ETUnit(_w, group_size, 1, true, uncoupled_layout, layout);
        _parity_et_units.push_back(et_unit);
    }
    
    // get inverse permutated uncoupled layout
    for (int i = 0; i < num_instances; i++) {
        _inv_perm_uc_layout.push_back(vector<int>());
    }

    for (auto et_unit_ptr : _data_et_units) {
        ETUnit &et_unit = *et_unit_ptr;
        vector<vector<int>> inv_perm_uc_layout = et_unit.GetInvPermUCLayout();
        for (int i = 0; i < num_instances; i++) {
            vector<int> &row = inv_perm_uc_layout[i];
            _inv_perm_uc_layout[i].insert(_inv_perm_uc_layout[i].end(), row.begin(), row.end());
        }
    }

    for (auto et_unit_ptr : _parity_et_units) {
        ETUnit &et_unit = *et_unit_ptr;
        vector<vector<int>> inv_perm_uc_layout = et_unit.GetInvPermUCLayout();
        for (int i = 0; i < num_instances; i++) {
            vector<int> &row = inv_perm_uc_layout[i];
            _inv_perm_uc_layout[i].insert(_inv_perm_uc_layout[i].end(), row.begin(), row.end());
        }
    }

    printf("layout: \n");
    for (int sp = 0; sp < _w; sp++) {
        for (int i = 0; i < _n; i++) {
            printf("%d ", _layout[sp][i]);
        }
        printf("\n");
    }

    printf("uncoupled_layout: \n");
    for (int sp = 0; sp < _w; sp++) {
        for (int i = 0; i < _n; i++) {
            printf("%d ", _uncoupled_layout[sp][i]);
        }
        printf("\n");
    }

    printf("inv_perm_uc_layout: \n");
    for (int sp = 0; sp < _w; sp++) {
        for (int i = 0; i < _n; i++) {
            printf("%d ", _inv_perm_uc_layout[sp][i]);
        }
        printf("\n");
    }

    // init RS code with inverse permutated uncoupled layout
    for (int i = 0; i < num_instances; i++) {
        vector<string> param_ins = param;
        param_ins.push_back(to_string(i));
        RSConv *instance = new RSConv(_n, _k, 1, opt, param_ins);
        
        vector<vector<int>> layout_instance;
        vector<int> symbols_instance;

        layout_instance.push_back(_inv_perm_uc_layout[i]);
        // for ETRS, there are no additional symbols required for decoding
        symbols_instance = _uncoupled_layout[i];

        instance->SetLayout(layout_instance);
        instance->SetSymbols(symbols_instance);

        _instances.push_back(instance);
    }


    for (auto &instance : _instances) {
        vector<vector<int>> layout_instance = instance->GetLayout();
        printf("instance layout: \n");
        for (int i = 0; i < _n; i++) {
            printf("%d ", layout_instance[0][i]);
        }
        printf("\n");
    }
}

ETRSConv::~ETRSConv() {
    for (auto ins_ptr : _instances) {
        free(ins_ptr);
    }

    for (auto et_unit_ptr : _data_et_units) {
        free(et_unit_ptr);
    }

    for (auto et_unit_ptr : _parity_et_units) {
        free(et_unit_ptr);
    }
}

ECDAG* ETRSConv::Encode() {
    ECDAG* ecdag = new ECDAG();

    // // 1. Brute forcely do all computation on the first node
    // for (int i = 0; i < _w; i++) {
    //     for (int j = 0; j < _n; j++) {
    //         int vidx = _layout[i][j];
    //         if (j == 0) {
    //             continue;
    //         }
    //         ecdag->BindY(vidx, 0);
    //     }
    // }

    // printf("hello2\n");

    // 2. (for data nodes) do inverse pairwise coupling and inverse rotation
    for (auto et_unit_ptr : _data_et_units) {
        printf("inverse pairwise coupling\n");

        ETUnit &et_unit = *et_unit_ptr;
        et_unit.Encode(ecdag);
    }

    // 3. (for parity nodes) encode for data nodes
    for (auto ins_ptr : _instances) {
        printf("base code encoding\n");
        RSConv &instance = *ins_ptr;
        instance.Encode(ecdag);
    }

    // 4. (for parity nodes) do rotation and pairwise coupling
    for (auto et_unit_ptr : _parity_et_units) {
        printf("pairwise coupling\n");

        ETUnit &et_unit = *et_unit_ptr;
        et_unit.Encode(ecdag);
    }

    return ecdag;
}

ECDAG* ETRSConv::Decode(vector<int> from, vector<int> to) {
    ECDAG* ecdag = new ECDAG();

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
    
    return ecdag;
}

ECDAG *ETRSConv::DecodeSingle(vector<int> from, vector<int> to) {
    return NULL;

    // TODO: verify the correctness!!!!
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

            // XOR the coupled parity to get group code
            ecdag->Join(_pid_group_code_map[parity_idx],
                {_uncoupled_code_layout[1][parity_idx], _code_layout[1][parity_idx]},
                {1, 1});

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

            // XOR the coupled parity to get group code
            ecdag->Join(_pid_group_code_map[parity_idx],
                {_uncoupled_code_layout[1][parity_idx], _code_layout[1][parity_idx]},
                {1, 1});
            
            symbols_bindx.push_back(_uncoupled_code_layout[1][parity_idx]);
        }

        // BindX and BindY (sp1)
        int vidx = ecdag->BindX(symbols_bindx);
        ecdag->BindY(vidx, cidx_sp1[0]);
        
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

        int coef = galois_single_divide(
            1, _rs_encode_matrix[(_k + couple_parity_id) * _k + failed_node], 8);

        ecdag->Join(_data_layout[0][failed_node], {pidx}, {coef});
    }

    free(recover_matrix);
    free(recover_matrix_inv);
    free(select_vector);
}

ECDAG *ETRSConv::DecodeMultiple(vector<int> from, vector<int> to) {
    return NULL;
}

void ETRSConv::Place(vector<vector<int>>& group) {
}

vector<int> ETRSConv::GetNodeSymbols(int nodeid) {
    vector<int> symbols;
    for (int i = 0; i < _layout.size(); i++) {
        symbols.push_back(_layout[i][nodeid]);
    }

    return symbols;
}

vector<vector<int>> ETRSConv::GetLayout() {
    return _layout;
}