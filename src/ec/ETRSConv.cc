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
        printf("error: k should be larger than w\n");
        return;
    }

    int num_instances = atoi(param[0].c_str());

    if (num_instances != _w) {
        printf("error: invalid _w (for base code RS, sub-packetization must be 1)\n");
        return;
    }

    int symbol_id = 0;

    // 1. initialize layouts, uncoupled layouts and additional symbols

    // 1.1 set layout (size: w * k)
    for (int i = 0; i < num_instances; i++) {
        _layout.push_back(vector<int>());
    }

    for (int i = 0; i < _n; i++) {
        for (int j = 0; j < num_instances; j++) {
            _layout[j].push_back(symbol_id++);
        }
    }

    // 1.2 set uncoupled layout (size: w * k)
    for (int i = 0; i < num_instances; i++) {
        _uncoupled_layout.push_back(vector<int>());
    }

    for (int i = 0; i < _n; i++) {
        for (int j = 0; j < num_instances; j++) {
            _uncoupled_layout[j].push_back(symbol_id++);
        }
    }

    // 2. initialize data and parity groups for elastic transformation

    int num_data_groups = _k / _w;
    int num_parity_groups = _m / _w;

    // 2.1 data group
    for (int i = 0, grp_id = 0; i < num_data_groups; i++) {
        vector<int> group;
        int group_size = (i < num_data_groups - 1) ? _w : (_k - _w * i);
        for (int j = 0; j < group_size; j++) {
            group.push_back(grp_id++);
        }
        _data_et_groups.push_back(group);
    }

    // 2.2 parity group
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
        printf("group: ");
        for (auto node_id : group) {
            printf("%d ", node_id);
        }
        printf("\n");
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

    // 3.1 data et units
    for (int i = 0; i < _data_et_groups.size(); i++) {
        vector<int> &group = _data_et_groups[i];
        printf("data group id: %d\n", i);
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
        
        ETUnit *et_unit = new ETUnit(_w, group_size, 1, uncoupled_layout, layout);
        _data_et_units.push_back(et_unit);
    }

    // 3.2 parity et units
    for (int i = 0; i < _parity_et_groups.size(); i++) {
        vector<int> &group = _parity_et_groups[i];
        printf("parity group id: %d\n", i);
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
        
        ETUnit *et_unit = new ETUnit(_w, group_size, 1, uncoupled_layout, layout);
        _parity_et_units.push_back(et_unit);
    }
    
    // 4. update uncoupled layout, and inverse permutated layout from et_unit

    // 4.1 update uncoupled layout (some symbols on diagonals have been replaced by et unit)
    vector<vector<int>> updated_uncoupled_layout;
    for (int i = 0; i < num_instances; i++) {
        updated_uncoupled_layout.push_back(vector<int>());
    }

    for (auto et_unit_ptr : _data_et_units) {
        ETUnit &et_unit = *et_unit_ptr;
        vector<vector<int>> uc_layout = et_unit.GetUCLayout();
        for (int i = 0; i < num_instances; i++) {
            vector<int> &row = uc_layout[i];
            updated_uncoupled_layout[i].insert(updated_uncoupled_layout[i].end(), row.begin(), row.end());
        }
    }

    for (auto et_unit_ptr : _parity_et_units) {
        ETUnit &et_unit = *et_unit_ptr;
        vector<vector<int>> uc_layout = et_unit.GetUCLayout();
        for (int i = 0; i < num_instances; i++) {
            vector<int> &row = uc_layout[i];
            updated_uncoupled_layout[i].insert(updated_uncoupled_layout[i].end(), row.begin(), row.end());
        }
    }

    _uncoupled_layout = updated_uncoupled_layout;

    // 4.2 get inverse permutated uncoupled layout
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
    for (int node_id = 0; node_id < _n; node_id++) {
        for (int ins_id = 0; ins_id < _w; ins_id++) {
            printf("%d ", _layout[ins_id][node_id]);
        }
        printf("\n");
    }

    printf("uncoupled_layout: \n");
    for (int node_id = 0; node_id < _n; node_id++) {
        for (int ins_id = 0; ins_id < _w; ins_id++) {
            printf("%d ", _uncoupled_layout[ins_id][node_id]);
        }
        printf("\n");
    }

    printf("inv_perm_uc_layout: \n");
    for (int node_id = 0; node_id < _n; node_id++) {
        for (int ins_id = 0; ins_id < _w; ins_id++) {
            printf("%d ", _inv_perm_uc_layout[ins_id][node_id]);
        }
        printf("\n");
    }

    // 5. init base RS code with inverse permutated uncoupled layout
    for (int i = 0; i < num_instances; i++) {
        vector<string> param_ins = param;
        param_ins.push_back(to_string(i)); // add the instance_id as param
        // RSConv takes two additional params: 1. num_instances, instance_id
        RSConv *instance = new RSConv(_n, _k, 1, opt, param_ins);
        
        vector<vector<int>> layout_instance;
        vector<int> symbols_instance;

        // 5.1 inverse permutated uncoupled layout
        layout_instance.push_back(_inv_perm_uc_layout[i]);
        // 5.2 for ETRS, there are no additional symbols required for decoding

        symbols_instance = _uncoupled_layout[i];

        instance->SetLayout(layout_instance);
        instance->SetSymbols(symbols_instance); // all symbols belongs to the instance

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

    printf("ETRSConv::ETRSConv finished initialization\n");
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

    // 1. (for data nodes) do pairwise decoupling and inverse rotation
    printf("step 1: decoupling for data et_units\n");
    for (auto et_unit_ptr : _data_et_units) {
        printf("decoupling\n");

        ETUnit &et_unit = *et_unit_ptr;
        et_unit.Decoupling(ecdag);
    }

    // 2. base code encode for all instances
    printf("step 2: base code encode for all instances\n");
    for (auto ins_ptr : _instances) {
        printf("base code encoding\n");
        RSConv &instance = *ins_ptr;
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

    int failed_node = to[0] / _w; // failed node id

    ECDAG* ecdag = new ECDAG();

    // 1. identify the failed group
    printf("step 1: identify the failed group\n");
    bool is_parity_group;
    int failed_group_idx = -1; // failed data group idx
    int failed_in_group_idx = -1; // the node index in failed group
    vector<int> *failed_group; // failed group
    ETUnit *failed_et_unit_ptr; // failed et unit

    if (failed_node < _k) {
        is_parity_group = false;
        for (int i = 0; i < _data_et_groups.size(); i++) {
            vector<int> &group = _data_et_groups[i];
            auto it = find(group.begin(), group.end(), failed_node);
            if (it != group.end()) {
                failed_group_idx = i;
                failed_in_group_idx = it - group.begin();
                failed_group = &_data_et_groups[i];
                failed_et_unit_ptr = _data_et_units[i];
                break;
            }
        }
    } else {
        is_parity_group = true;
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
    }

    printf("is_parity_group: %d, failed_node: %d, failed_group_idx: %d, failed_in_group_idx: %d\n",
        is_parity_group, failed_node, failed_group_idx, failed_in_group_idx);

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
        if (row.second.size() == _w - 1) {
            failed_ins_id = row.first;
            break;
        }
    }

    printf("failed_ins_id: %d\n", failed_ins_id);

    // 3. get required uncoupled symbols for base code repair

    /**
     * @brief required_uc_symbols_fg: required uncoupled symbols in failed group
     * required_uc_symbols_ag_data(parity): required uncoupled symbols in alive groups (data and parity)
     * 
     * symbols in required_uc_symbols_ag_* will be repaired by decoupling
     * symbols in required_uc_symbols_fg will be repaired by base code repair
     */

    printf("step 3: get required uncoupled symbols for base code repair\n");
    vector<int> required_uc_symbols_fg;
    map<int, vector<int>> required_uc_symbols_ag_data, required_uc_symbols_ag_parity;
    int num_required_uc_symbols_ag = 0;

    // For failed_ins_id, collect k - failed_group.size() data and , parity inv_uc_symbols
    for (int i = 0; i < _data_et_groups.size(); i++) {
        if (is_parity_group == false && i == failed_group_idx) { // for data node failure, skip failed group
            continue;
        }

        vector<int> &group = _data_et_groups[i];
        for (auto node_id : group) {
            required_uc_symbols_ag_data[i].push_back(_inv_perm_uc_layout[failed_ins_id][node_id]);
            num_required_uc_symbols_ag++;
        }
    }

    for (int i = 0; i < _parity_et_groups.size(); i++) {
        vector<int> &group = _parity_et_groups[i];
        for (auto node_id : group) {
            if (num_required_uc_symbols_ag < _k) {
                required_uc_symbols_ag_parity[i].push_back(_inv_perm_uc_layout[failed_ins_id][node_id]);            
                num_required_uc_symbols_ag++;
            } else {
                break;
            }
        }
    }

    // if the failed group size is larger than n - k, try to retrieve symbols from the failed group
    if (num_required_uc_symbols_ag < _k) {
        printf("get unrelated uc symbols\n");
        vector<int> unrelated_uc_symbols = failed_et_unit_ptr->GetUnrelatedUCSymbols(failed_in_group_idx, failed_ins_id);

        for (auto symbol : unrelated_uc_symbols) {
            if (num_required_uc_symbols_ag < _k) {
                required_uc_symbols_ag_data[failed_group_idx].push_back(symbol);
                num_required_uc_symbols_ag++;
            }
        }
    }

    // for failed group, get all required symbols in inv_uc_layout
    for (auto node_id : *failed_group) {
        int symbol = _inv_perm_uc_layout[failed_ins_id][node_id];
        if (is_parity_group == false) {
            if (find(required_uc_symbols_ag_data[failed_group_idx].begin(),
            required_uc_symbols_ag_data[failed_group_idx].end(),
            symbol) == required_uc_symbols_ag_data[failed_group_idx].end()) {
                required_uc_symbols_fg.push_back(symbol);
            }
        } else {
            if (find(required_uc_symbols_ag_parity[failed_group_idx].begin(),
            required_uc_symbols_ag_parity[failed_group_idx].end(),
            symbol) == required_uc_symbols_ag_parity[failed_group_idx].end()) {
                required_uc_symbols_fg.push_back(symbol);
            }
        }
    }

    printf("required_uc_symbols_fg: ");
    for (auto symbol : required_uc_symbols_fg) {
        printf("%d ", symbol);
    }
    printf("\n");

    printf("required_uc_symbols_ag_data:\n");
    for (auto item : required_uc_symbols_ag_data) {
        printf("group: %d, symbols: ", item.first);
        for (auto symbol : item.second) {
            printf("%d ", symbol);
        }
        printf("\n");
        
    }

    printf("required_uc_symbols_ag_parity:\n");
    for (auto item : required_uc_symbols_ag_parity) {
        printf("group: %d, symbols: ", item.first);
        for (auto symbol : item.second) {
            printf("%d ", symbol);
        }
        printf("\n");
    }

    // 4. repair required_uc_symbols_ag_data and required_uc_symbols_ag_parity by decoupling
    printf("step 4: repair required_uc_symbols in alive groups by decoupling\n");

    for (auto item : required_uc_symbols_ag_data) {
        int group_id = item.first;
        vector<int> required_uc_symbols = item.second;
        printf("decoupling for data group %d\n", group_id);

        ETUnit &et_unit = *_data_et_units[group_id];
        et_unit.Decoupling(required_uc_symbols, ecdag);
    }

    for (auto item : required_uc_symbols_ag_parity) {
        int group_id = item.first;
        vector<int> required_uc_symbols = item.second;
        printf("decoupling for parity group %d\n", group_id);

        ETUnit &et_unit = *_parity_et_units[group_id];
        et_unit.Decoupling(required_uc_symbols, ecdag);
    }

    // 5. base code repair for required_uc_symbols_fg
    printf("step 5: repair uc_symbols in alive groups by base code decode\n");
    vector<int> bc_from;
    vector<int> bc_to = required_uc_symbols_fg;
    for (auto item : required_uc_symbols_ag_data) {
        vector<int> required_uc_symbols = item.second;
        bc_from.insert(bc_from.end(), required_uc_symbols.begin(), required_uc_symbols.end());
    }
    for (auto item : required_uc_symbols_ag_parity) {
        vector<int> required_uc_symbols = item.second;
        bc_from.insert(bc_from.end(), required_uc_symbols.begin(), required_uc_symbols.end());
    }

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
    
    return ecdag;
}

ECDAG *ETRSConv::DecodeMultiple(vector<int> from, vector<int> to) {

    ECDAG* ecdag = new ECDAG();

    // NOT implemented
    printf("ETRSConv::DecodeMultiple Not implemented\n");

    return ecdag;
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