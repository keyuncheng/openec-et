/**
 * @file ETHHXOR.cc
 * @author Keyun Cheng (kycheng@cse.cuhk.edu.hk)
 * @brief 
 * @version 0.1
 * @date 2022-05-10
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#include "ETHHXOR.hh"

ETHHXOR::ETHHXOR(int n, int k, int w, int opt, vector<string> param)
{
    _n = n;
    _k = k;
    _w = w;
    _opt = opt;
    _m = _n - _k;
    _base_w = 2; // for HHXOR, base code sub-packetization = 2

    if (param.size() != 1) {
        printf("error: invalid number of arguments\n");
        return;
    }

    _num_instances = atoi(param[0].c_str()); // number of base code instances

    if (_num_instances * _base_w != _w) {
        printf("error: invalid _w (for base code HHXOR, sub-packetization must be 2)\n");
        return;
    }

    int symbol_id = 0;

    // 1. initialize layouts, uncoupled layouts and additional symbols

    // 1.1 set layout (size: w * k)
    for (int i = 0; i < _w; i++) {
        _layout.push_back(vector<int>());
    }

    for (int i = 0; i < _n; i++) {
        for (int j = 0; j < _w; j++) {
            _layout[j].push_back(symbol_id++);
        }
    }

    // 1.2 set uncoupled layout (size: w * k)
    for (int i = 0; i < _w; i++) {
        _uncoupled_layout.push_back(vector<int>());
    }

    // NOTE: only change code layout, data layout doesn't need to be changed

    // data layout: copy from layout
    for (int i = 0; i < _k; i++) {
        for (int j = 0; j < _w; j++) {
            _uncoupled_layout[j].push_back(_layout[j][i]);
        }
    }
    
    // code layout: add new symbols
    for (int i = 0; i < _n; i++) {
        for (int j = 0; j < _w; j++) {
            _uncoupled_layout[j].push_back(symbol_id++);
        }
    }
    
    // 2. initialize parity groups for elastic transformation
    int num_parity_groups = _m / _num_instances;

    // 2.1 parity group
    for (int i = 0, grp_id = _k; i < num_parity_groups; i++) {
        vector<int> group;
        int group_size = (i < num_parity_groups - 1) ? _num_instances : (_m - _num_instances * i);
        for (int j = 0; j < group_size; j++) {
            group.push_back(grp_id++);
        }
        _parity_et_groups.push_back(group);
    }

    printf("parity_et_groups:\n");
    for (auto &group : _parity_et_groups) {
        printf("group: ");
        for (auto node_id : group) {
            printf("%d ", node_id);
        }
        printf("\n");
    }

    // 3. init et units

    // 3.1 parity et units
    for (int i = 0; i < _parity_et_groups.size(); i++) {
        vector<int> &group = _parity_et_groups[i];
        printf("parity group id: %d\n", i);
        int group_size = group.size();

        vector<vector<int>> uncoupled_layout;
        vector<vector<int>> layout;

        for (int j = 0; j < _w; j++) {
            vector<int> ul;
            vector<int> l;
            for (auto node_id : group) {
                ul.push_back(_uncoupled_layout[j][node_id]);
                l.push_back(_layout[j][node_id]);                
            }

            uncoupled_layout.push_back(ul);
            layout.push_back(l);
        }
        
        ETUnit *et_unit = new ETUnit(_num_instances, group_size, _base_w, uncoupled_layout, layout);
        _parity_et_units.push_back(et_unit);
    }
    
    // 4. update uncoupled layout, and inverse permutated layout from et_unit

    // 4.1 update uncoupled layout (some symbols on diagonals have been replaced by et unit)
    vector<vector<int>> updated_uncoupled_layout;
    for (int i = 0; i < _w; i++) {
        updated_uncoupled_layout.push_back(vector<int>());
    }
    
    // uncoupled data layouts: same as before
    for (int i = 0; i < _w; i++) {
        for (int j = 0; j < _k; j++) {
            updated_uncoupled_layout[i].push_back(_uncoupled_layout[i][j]);
        }
    }

    // uncoupled code layouts: update
    for (auto et_unit_ptr : _parity_et_units) {
        ETUnit &et_unit = *et_unit_ptr;
        vector<vector<int>> uc_layout = et_unit.GetUCLayout();
        for (int i = 0; i < _w; i++) {
            vector<int> &row = uc_layout[i];
            updated_uncoupled_layout[i].insert(updated_uncoupled_layout[i].end(), row.begin(), row.end());
        }
    }

    _uncoupled_layout = updated_uncoupled_layout;

    // 4.2 get inverse permutated uncoupled layout
    for (int i = 0; i < _w; i++) {
        _inv_perm_uc_layout.push_back(vector<int>());
    }

    // uncoupled data layouts: same as before
    for (int i = 0; i < _w; i++) {
        for (int j = 0; j < _k; j++) {
            _inv_perm_uc_layout[i].push_back(_uncoupled_layout[i][j]);
        }
    }

    // uncoupled code layouts: update
    for (auto et_unit_ptr : _parity_et_units) {
        ETUnit &et_unit = *et_unit_ptr;
        vector<vector<int>> inv_perm_uc_layout = et_unit.GetInvPermUCLayout();
        for (int i = 0; i < _w; i++) {
            vector<int> &row = inv_perm_uc_layout[i];
            _inv_perm_uc_layout[i].insert(_inv_perm_uc_layout[i].end(), row.begin(), row.end());
        }
    }

    printf("layout: \n");
    for (int ins_id = 0; ins_id < _w; ins_id++) {
        for (int node_id = 0; node_id < _n; node_id++) {
            printf("%d ", _layout[ins_id][node_id]);
        }
        printf("\n");
    }

    printf("uncoupled_layout: \n");
    for (int ins_id = 0; ins_id < _w; ins_id++) {
        for (int node_id = 0; node_id < _n; node_id++) {
            printf("%d ", _uncoupled_layout[ins_id][node_id]);
        }
        printf("\n");
    }

    printf("inv_perm_uc_layout: \n");
    for (int ins_id = 0; ins_id < _w; ins_id++) {
        for (int node_id = 0; node_id < _n; node_id++) {
            printf("%d ", _inv_perm_uc_layout[ins_id][node_id]);
        }
        printf("\n");
    }

    // 5. init base HHXOR code with inverse permutated uncoupled layout
    for (int i = 0; i < _num_instances; i++) {
        vector<string> param_ins = param;
        param_ins.push_back(to_string(i)); // add the instance_id as param
        // HHXOR takes two additional params: 1. num_instances, instance_id
        HHXOR *instance = new HHXOR(_n, _k, _base_w, opt, param_ins);
        
        vector<vector<int>> layout_instance;
        vector<int> symbols_instance;

        // 5.1 inverse permutated uncoupled layout
        for (int j = 0; j < _base_w; j++) {
            layout_instance.push_back(_inv_perm_uc_layout[i * _base_w + j]);
        }
        // 5.2 for ETHHXOR, there are additional symbols required for decoding
        
        int num_additional_symbols = instance->GetNumSymbols() - _base_w * _n; // TBD

        for (int j = 0; j < _base_w; j++) {
            symbols_instance.insert(symbols_instance.end(),
                _uncoupled_layout[i * _base_w + j].begin(), _uncoupled_layout[i * _base_w + j].end());
        }
        
        for (int j = 0; j < num_additional_symbols; j++) {
            symbols_instance.push_back(symbol_id++);
        }

        instance->SetLayout(layout_instance);
        instance->SetSymbols(symbols_instance); // all symbols belongs to the instance

        _instances.push_back(instance);
    }

    for (auto &instance : _instances) {
        vector<vector<int>> layout_instance = instance->GetLayout();
        printf("instance layout: \n");
        for (int i = 0; i < _base_w; i++) {
            for (int j = 0; j < _n; j++) {
                printf("%d ", layout_instance[i][j]);
            }
            printf("\n");
        }
    }

    printf("ETHHXOR::ETHHXOR finished initialization\n");
}

ETHHXOR::~ETHHXOR()
{
    for (auto ins_ptr : _instances) {
        free(ins_ptr);
    }

    for (auto et_unit_ptr : _parity_et_units) {
        free(et_unit_ptr);
    }
}

ECDAG* ETHHXOR::Encode() {
    ECDAG* ecdag = new ECDAG();

    // 2. base code encode for all instances
    printf("step 2: base code encode for all instances\n");
    for (auto ins_ptr : _instances) {
        printf("base code encoding\n");
        HHXOR &instance = *ins_ptr;
        instance.Encode(ecdag);
    }

    // 3. (for parity nodes) do rotation and pairwise coupling
    for (auto et_unit_ptr : _parity_et_units) {
        printf("step 3: coupling for parity et_units\n");

        ETUnit &et_unit = *et_unit_ptr;
        et_unit.Coupling(ecdag);
    }

    return ecdag;
}

ECDAG* ETHHXOR::Decode(vector<int> from, vector<int> to) {
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

ECDAG *ETHHXOR::DecodeSingle(vector<int> from, vector<int> to) {

    int failed_node = to[0] / _w; // failed node id

    ECDAG* ecdag = new ECDAG();

    if (failed_node < _k) {
        // data node failure

        // for each instance
        for (int i = 0; i < _num_instances; i++) {
            HHXOR* instance = _instances[i];

            vector<int> to_instance;
            for (auto symbol : to) {
                for (int w = 0; w < _base_w; w++) {
                    if (_layout[i * _base_w + w][failed_node] == symbol) {
                        to_instance.push_back(symbol);
                    }
                } 
            }
            
            printf("to_instance:\n");            
            for (auto symbol : to_instance) {
                printf("%d ", symbol);
            }
            printf("\n");

            // 1. get required uncoupled parity symbols for base code repair
            printf("step 1: get required uncoupled parity symbols for base code repair\n");
            vector<int> required_uc_parity_symbols = instance->GetRequiredParitySymbolsSingle(failed_node);

            printf("required_uc_parity_symbols:\n");            
            for (auto symbol : required_uc_parity_symbols) {
                printf("%d ", symbol);
            }
            printf("\n");

            // 2. decouple required uncoupled parity symbols for base code repair
            printf("step 2: decouple required uncoupled parity symbols for base code repair\n");
            for (auto et_unit : _parity_et_units) {
                vector<int> to_et_unit;
                vector<vector<int>> uc_layout_et_unit = et_unit->GetUCLayout();

                for (auto symbol : required_uc_parity_symbols) {
                    for (int i = 0; i < uc_layout_et_unit.size(); i++) {
                        for (int j = 0; j < uc_layout_et_unit[0].size(); j++) {
                            if (symbol == uc_layout_et_unit[i][j]) {
                                to_et_unit.push_back(symbol);
                            }
                        }
                    }
                }
                et_unit->Decoupling(to_et_unit, ecdag);
            }

            // 3. call base code repair for data node
            printf("step 3. call base code repair for data node\n");
            instance->DecodeSingle(from, to_instance, ecdag);
        }

    } else {
        // parity node failure

        int failed_group_idx = -1; // failed data group idx
        int failed_in_group_idx = -1; // the node index in failed group
        vector<int> *failed_group; // failed group
        ETUnit *failed_et_unit_ptr; // failed et unit

        // 1. identify the failed group
        printf("step 1: identify the failed group\n");
        for (int i = 0; i < _parity_et_groups.size(); i++) {
            vector<int> &group = _parity_et_groups[i];
            auto it = find(group.begin(), group.end(), failed_node);
            if (it != group.end()) {
                failed_group_idx = i;
                failed_in_group_idx = it - group.begin();
                failed_group = &_parity_et_groups[i];
                failed_et_unit_ptr = _parity_et_units[i];
                break;
            }
        }

        printf("parity_group failure, failed_node: %d, failed_group_idx: %d, failed_in_group_idx: %d\n",
            failed_node, failed_group_idx, failed_in_group_idx);

        // 2. identify the sub-stripe index required for base code repair
        printf("step 2: identify the instance for base code repair\n");
        int failed_ins_id = -1;

        map<int, vector<int>> f_group_sp_map = failed_et_unit_ptr->GetRequiedUCSymbolsForNode(failed_in_group_idx);
        printf("f_group_sp_map:\n");
        for (auto row : f_group_sp_map) {
            printf("sp: %d, symbols: ", row.first);
            for (auto symbol : row.second) {
                printf("%d ", symbol);
            }
            printf("\n");
        }

        for (auto row : f_group_sp_map) {
            // number of required uc symbols equals to sp - 1
            if (row.second.size() == (_num_instances - 1) * _base_w) {
                failed_ins_id = row.first;
                break;
            }
        }

        printf("failed_ins_id: %d\n", failed_ins_id);

        // 3. get required uncoupled symbols for base code repair

        /**
         * @brief required_uc_symbols_fg: required uncoupled symbols in failed group
         * required_uc_symbols_data: required uncoupled symbols in data
         * 
         * symbols in required_uc_symbols_data will be directly retrieved
         * symbols in required_uc_symbols_fg will be repaired by base code repair
         */

        printf("step 3: get required uncoupled symbols for base code repair\n");
        vector<int> required_uc_symbols_data, required_uc_symbols_fg;

        // collect k * base_w data symbols 
        for (int w = 0; w < _base_w; w++) {
            for (int node_id = 0; node_id < _k; node_id++) {
                required_uc_symbols_data.push_back(_inv_perm_uc_layout[failed_ins_id * _base_w + w][node_id]);
            }
        }

        // for failed group, get all required symbols in inv_uc_layout
        for (int w = 0; w < _base_w; w++) {
            for (auto node_id : *failed_group) {
                int symbol = _inv_perm_uc_layout[failed_ins_id * _base_w + w][node_id];
                required_uc_symbols_fg.push_back(symbol);
            }
        }

        printf("required_uc_symbols_data: ");
        for (auto symbol : required_uc_symbols_data) {
            printf("%d ", symbol);
        }
        printf("\n");

        printf("required_uc_symbols_fg: ");
        for (auto symbol : required_uc_symbols_fg) {
            printf("%d ", symbol);
        }
        printf("\n");

        // 5. base code repair for required_uc_symbols_fg
        printf("step 5: repair uc_symbols in alive groups by base code decode\n");
        vector<int> bc_from = required_uc_symbols_data;
        vector<int> bc_to = required_uc_symbols_fg;

        printf("base code (instance %d) from: ", failed_ins_id);
        for (auto symbol : bc_from) {
            printf("%d ", symbol);
        }
        printf("\n");

        printf("base code (instance %d) to: ", failed_ins_id);
        for (auto symbol : bc_to) {
            printf("%d ", symbol);
        }
        printf("\n");

        _instances[failed_ins_id]->Decode(bc_from, bc_to, ecdag);

        // 6. repair failed packets by pairwise coupling
        printf("step 6: repair layout symbols in failed groups by biased coupling\n");
        failed_et_unit_ptr->BiasedCoupling(to, failed_ins_id, ecdag);
    }

    
    return ecdag;
}

ECDAG *ETHHXOR::DecodeMultiple(vector<int> from, vector<int> to) {

    ECDAG* ecdag = new ECDAG();

    // NOT implemented
    printf("ETHHXOR::DecodeMultiple Not implemented\n");

    return ecdag;
}

void ETHHXOR::Place(vector<vector<int>>& group) {
    return;
}

vector<int> ETHHXOR::GetNodeSymbols(int nodeid) {
    vector<int> symbols;
    for (int i = 0; i < _layout.size(); i++) {
        symbols.push_back(_layout[i][nodeid]);
    }

    return symbols;
}


vector<vector<int>> ETHHXOR::GetLayout() {
    return _layout;
}
