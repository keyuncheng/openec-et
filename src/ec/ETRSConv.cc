#include "ETRSConv.hh"

#include <iomanip> // setprecision
#include <set>
#include <queue>

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

    //if (_k < _w) {
    //    printf("error: k should be larger than w\n");
    //    return;
    //}

    //int num_instances = atoi(param[0].c_str());

    //if (num_instances != _w) {
    //    printf("error: invalid _w (for base code RS, sub-packetization must be 1)\n");
    //    return;
    //}

    int symbol_id = 0;
    multiset<int, greater<int>> prime_factors; // number of instances in each group (descending order)

    // 0. find the sub-packetization of groups

    // 0.1 factorize the final sub-packetization into prime factors
    int target_w = _w;
    while (target_w > 1) {
        int i = 2;
        for (; i <= _m; i++) {
            if (target_w % i == 0) {
                prime_factors.insert(i);
                target_w /= i;
                break;
            }
        }
        // cannot be factorized anymore
        if (i == _m + 1) { break; }
    }

    //cout << "prime factors: ";
    //for (auto f : prime_factors) { cout << f << " "; }
    //cout << endl;

    int base_w = 1;

    int num_prime_factors = prime_factors.size();
    auto prime_factors_it = prime_factors.begin();

    int num_instances = *prime_factors_it;
    int num_data_groups = _k / num_instances;
    int num_parity_groups = _m / num_instances;
    int num_groups = num_data_groups + num_parity_groups;

    multiset<int, greater<int>> prime_factors_backup;

    // 0.2 merge the number of instances at intermidate levels to fit into the number available groups
    while (prime_factors.size() > num_groups || prime_factors_it != prime_factors.end()) {
        auto start = prime_factors_it;
        int base = *prime_factors_it;

        if (++prime_factors_it == prime_factors.end()) { break; }

        int factor = *prime_factors_it;
        int combined = base * factor;

        // combinating condition:
        // 1. combined value is at most (n-k) (max supported number of instances), and
        //    a. there is insufficient groups for all factors, or
        //    b. the combined value is a new higher supported number of instances (combining to a higher number results in better 'overlap and reuse' for repair saving)
        if (
                combined <= _m && 
                (prime_factors.size() > num_groups || prime_factors.count(combined) == 0)
        ) {

            // if the total number of groups reduces to fewer than the number of prime factros after combination, back up the current 'best' solution
            int current_base_num_instances = *prime_factors.begin();
            int updated_base_num_instances = std::max(current_base_num_instances, combined);
            num_data_groups = _k / updated_base_num_instances;
            num_parity_groups = _m / updated_base_num_instances;
            if (num_data_groups + num_parity_groups < prime_factors.size() - 1) {
                prime_factors_backup = prime_factors;
            }

            // remove the two components and insert a combined number
            ++prime_factors_it;
            prime_factors.erase(start, prime_factors_it);
            prime_factors.insert(combined);

            // reset merge target criteria
            prime_factors_it = prime_factors.begin();
            num_instances = *prime_factors_it;
            num_data_groups = _k / num_instances;
            num_parity_groups = _m / num_instances;
            num_groups = num_data_groups + num_parity_groups;

            cout << "Combined " << base << " and " << factor << " into " << combined << "; updated num groups = " << num_groups << endl;
        }
    }

    // 0.3 calculate the number of instances and sub-packetization per group 
    // 0.3.1 data group, fix the base as RS sub-packetization = 1
    int num_factors_left = prime_factors.size() - 1;
    auto num_instances_it = prime_factors.begin();
    num_instances = *num_instances_it;
    num_data_groups = _k / num_instances;
    num_parity_groups = _m / num_instances;
    num_groups = num_data_groups + num_parity_groups;

    base_w = 1;
    for (int i = 0; i < num_data_groups; i++) {
        int num_inst = *num_instances_it;
        int potential_new_base_w = base_w * num_inst * num_inst;
        if (i == 0) { // RS base sub-packetization for the first data group
            base_w = 1;
        } else if (base_w == _data_base_w.at(i-1)) { // avoid reusing the base sub-packetization
            if (potential_new_base_w < _w) {
            //if (potential_new_base_w <= _w) {
                if (_w % potential_new_base_w == 0) { 
                    base_w *= num_inst;
                //} else { // this shifts the parity repair savings to data with some loss
                //    base_w = _w / num_inst;
                }
            }
        }
        _data_num_instances.push_back(num_inst);
        _data_base_w.push_back(base_w);
        if (num_factors_left > 1 && i < num_data_groups - 1) {
            base_w *= num_inst;
            num_instances_it++;
            num_factors_left--;
        }
    }

    // 0.3.2 parity group, fix the final target to final sub-packetization
    num_instances_it = prime_factors.begin();
    num_factors_left = prime_factors.size();
    base_w = _w;
    for (int i = num_parity_groups - 1; i >= 0; i--) {
        int num_inst = *num_instances_it;
        if (i == num_parity_groups - 1) { base_w /= num_inst; num_factors_left--; }
        _parity_num_instances.push_back(num_inst);
        _parity_base_w.push_back(base_w);
        if (num_factors_left > 0) {
            base_w /= num_inst;
            num_instances_it++;
            num_factors_left--;
        }
    }


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

    for (int i = 0; i < _n; i++) {
        for (int j = 0; j < _w; j++) {
            _uncoupled_layout[j].push_back(symbol_id++);
        }
    }

    // 2. initialize data and parity groups for elastic transformation

    num_instances = _data_num_instances.at(0);
    num_data_groups = _k / num_instances;
    num_parity_groups = _m / num_instances;

    // 2.1 data group
    for (int i = 0, grp_id = 0; i < num_data_groups; i++) {
        vector<int> group;
        int group_size = (i < num_data_groups - 1) ? num_instances : (_k - num_instances * i);
        for (int j = 0; j < group_size; j++) {
            group.push_back(grp_id++);
        }
        assert(group_size >= _data_num_instances.at(i));
        _data_et_groups.push_back(group);
    }

    // 2.2 parity group
    for (int i = 0, grp_id = _k; i < num_parity_groups; i++) {
        vector<int> group;
        int group_size = (i < num_parity_groups - 1) ? num_instances : (_m - num_instances * i);
        for (int j = 0; j < group_size; j++) {
            group.push_back(grp_id++);
        }
        assert(group_size >= _parity_num_instances.at(i));
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

    _data_et_units.resize(num_data_groups);

    // 3.1 data et units
    for (int i = 0; i < _data_et_groups.size(); i++) {
        vector<int> &group = _data_et_groups[i];
        printf("data group id: %d\n", i);
        int group_size = group.size();

        int base_w = _data_base_w.at(i);
        int num_instances = _data_num_instances.at(i);
        int target_w = base_w * num_instances;
        int num_et_units = _w / target_w;

        for (int et_unit_idx = 0; et_unit_idx < num_et_units; et_unit_idx++) {
            vector<vector<int>> uncoupled_layout;
            vector<vector<int>> layout;

            for (int j = et_unit_idx * target_w; j < (et_unit_idx + 1) * target_w; j++) {
                vector<int> ul;
                vector<int> l;
                for (auto node_id : group) {
                    ul.push_back(_uncoupled_layout[j][node_id]);
                    l.push_back(_layout[j][node_id]);                
                }

                uncoupled_layout.push_back(ul);
                layout.push_back(l);
            }
            
            ETUnit *et_unit = new ETUnit(num_instances, group_size, base_w, uncoupled_layout, layout);
            _data_et_units[i].push_back(et_unit);
        }
    }

    _parity_et_units.resize(num_parity_groups);

    // 3.2 parity et units
    for (int i = 0; i < _parity_et_groups.size(); i++) {
        vector<int> &group = _parity_et_groups[i];
        printf("parity group id: %d\n", i);
        int group_size = group.size();

        int base_w = _parity_base_w.at(i);
        int num_instances = _parity_num_instances.at(i);
        int target_w = base_w * num_instances;
        int num_et_units = _w / target_w;

        for (int et_idx = 0; et_idx < num_et_units; et_idx++) {
            vector<vector<int>> uncoupled_layout;
            vector<vector<int>> layout;

            for (int j = et_idx * target_w; j < (et_idx + 1) * target_w; j++) {
                vector<int> ul;
                vector<int> l;
                for (auto node_id : group) {
                    ul.push_back(_uncoupled_layout[j][node_id]);
                    l.push_back(_layout[j][node_id]);                
                }

                uncoupled_layout.push_back(ul);
                layout.push_back(l);
            }
            //cout << " base w = " << base_w << " num instances = " << num_instances << " target w = " << target_w << " num et " << num_et_units << endl;

            ETUnit *et_unit = new ETUnit(num_instances, group_size, base_w, uncoupled_layout, layout);
            _parity_et_units[i].push_back(et_unit);
        }
    }
    
    // 4. update uncoupled layout, and inverse permutated layout from et_unit

    // 4.1 update uncoupled layout (some symbols on diagonals have been replaced by et unit)
    vector<vector<int>> updated_uncoupled_layout;
    for (int i = 0; i < _w; i++) {
        updated_uncoupled_layout.push_back(vector<int>());
    }

    // insertion order: across all sub-packetization (horizontal) in a group and then across all groups (vertical)
    for (int group_idx = 0; group_idx < _data_et_groups.size(); group_idx++) { 
        int base_w = _data_base_w[group_idx];
        int num_instances = _data_num_instances[group_idx];
        int target_w = base_w * num_instances;
        vector<ETUnit*> &et_units = _data_et_units[group_idx];
        for (int et_unit_idx = 0; et_unit_idx < et_units.size(); et_unit_idx++) {
            ETUnit &et_unit = *et_units[et_unit_idx];
            vector<vector<int>> uc_layout = et_unit.GetUCLayout();
            for (int i = 0; i < target_w; i++) {
                vector<int> &row = uc_layout[i];
                int global_w_idx = et_unit_idx * target_w + i;
                updated_uncoupled_layout[global_w_idx].insert(updated_uncoupled_layout[global_w_idx].end(), row.begin(), row.end());
            }
        }
    }

    for (int group_idx = 0; group_idx < _parity_et_groups.size(); group_idx++) { 
        int base_w = _parity_base_w[group_idx];
        int num_instances = _parity_num_instances[group_idx];
        int target_w = base_w * num_instances;
        vector<ETUnit*> &et_units = _parity_et_units[group_idx];
        for (int et_unit_idx = 0; et_unit_idx < et_units.size(); et_unit_idx++) {
            ETUnit &et_unit = *et_units[et_unit_idx];
            vector<vector<int>> uc_layout = et_unit.GetUCLayout();
            int w_offset = et_unit_idx * target_w;
            for (int i = 0; i < target_w; i++) {
                vector<int> &row = uc_layout[i];
                int global_w_idx = et_unit_idx * target_w + i;
                updated_uncoupled_layout[global_w_idx].insert(updated_uncoupled_layout[global_w_idx].end(), row.begin(), row.end());
            }
        }
    }

    _uncoupled_layout = updated_uncoupled_layout;

    // 4.2 get inverse permutated uncoupled layout
    for (int i = 0; i < _w; i++) {
        _inv_perm_uc_layout.push_back(vector<int>());
    }

    for (int group_idx = 0; group_idx < _data_et_groups.size(); group_idx++) { 
        int base_w = _data_base_w[group_idx];
        int num_instances = _data_num_instances[group_idx];
        int target_w = base_w * num_instances;

        vector<ETUnit*> &et_units = _data_et_units[group_idx];

        for (int et_unit_idx = 0; et_unit_idx < et_units.size(); et_unit_idx++) {
            ETUnit &et_unit = *et_units[et_unit_idx];
            vector<vector<int>> inv_perm_uc_layout = et_unit.GetInvPermUCLayout();
            int w_offset = et_unit_idx * target_w;
            for (int i = 0; i < target_w; i++) {
                int global_w_idx = et_unit_idx * target_w + i;
                vector<int> &row = inv_perm_uc_layout[i];
                _inv_perm_uc_layout[global_w_idx].insert(_inv_perm_uc_layout[global_w_idx].end(), row.begin(), row.end());
                updated_uncoupled_layout[global_w_idx].insert(updated_uncoupled_layout[global_w_idx].end(), row.begin(), row.end());
            }
        }
    }

    for (int group_idx = 0; group_idx < _parity_et_groups.size(); group_idx++) { 
        int base_w = _parity_base_w[group_idx];
        int num_instances = _parity_num_instances[group_idx];
        int target_w = base_w * num_instances;

        vector<ETUnit*> &et_units = _parity_et_units[group_idx];

        for (int et_unit_idx = 0; et_unit_idx < et_units.size(); et_unit_idx++) {
            ETUnit &et_unit = *et_units[et_unit_idx];
            vector<vector<int>> inv_perm_uc_layout = et_unit.GetInvPermUCLayout();
            int w_offset = et_unit_idx * target_w;
            for (int i = 0; i < target_w; i++) {
                int global_w_idx = et_unit_idx * target_w + i;
                vector<int> &row = inv_perm_uc_layout[i];
                _inv_perm_uc_layout[global_w_idx].insert(_inv_perm_uc_layout[global_w_idx].end(), row.begin(), row.end());
                updated_uncoupled_layout[global_w_idx].insert(updated_uncoupled_layout[global_w_idx].end(), row.begin(), row.end());
            }
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
    for (int i = 0; i < _w; i++) {
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

    cout << "ETRSConv: data groups (base_w x instances) = { ";
    for (int i = 0; i < _data_base_w.size(); i++) {
        if (i > 0) cout << ", ";
        cout << _data_base_w.at(i) << " x " << _data_num_instances.at(i);
    }
    cout << " }, parity groups (base_w x instances) = { " ;
    for (int i = 0; i < _parity_base_w.size(); i++) {
        if (i > 0) cout << ", ";
        cout << _parity_base_w.at(i) << " x " << _parity_num_instances.at(i);
    }
    cout << " }, num_data_groups = " << num_data_groups 
         << " }, num_parity_groups = " << num_parity_groups
         << endl;
}

ETRSConv::~ETRSConv() {
    for (auto ins_ptr : _instances) {
        free(ins_ptr);
    }

    for (auto &et_units : _data_et_units) {
        for (auto et_unit_ptr : et_units) {
            free(et_unit_ptr);
        }
    }

    for (auto &et_units : _parity_et_units) {
        for (auto et_unit_ptr : et_units) {
            free(et_unit_ptr);
        }
    }
}

ECDAG* ETRSConv::Encode() {
    ECDAG* ecdag = new ECDAG();

    // 1. (for data nodes) do pairwise decoupling and inverse rotation
    printf("step 1: decoupling for data et_units\n");
    for (const auto &et_units : _data_et_units) {
        for (auto et_unit_ptr : et_units) {
            printf("decoupling\n");

            ETUnit &et_unit = *et_unit_ptr;
            et_unit.Decoupling(ecdag);
        }
    }

    // 2. base code encode for all instances
    printf("step 2: base code encode for all instances\n");
    for (auto ins_ptr : _instances) {
        printf("base code encoding\n");
        RSConv &instance = *ins_ptr;
        instance.Encode(ecdag);
    }

    // 3. (for parity nodes) do rotation and pairwise coupling
    for (const auto &et_units : _parity_et_units) {
        for (auto et_unit_ptr : et_units) {
            printf("step 3: coupling for parity et_units\n");

            ETUnit &et_unit = *et_unit_ptr;
            et_unit.Coupling(ecdag);
        }
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
    set<int> sources;
    int max_source_id = _n * _w;

    // 1. identify the failed group
    printf("step 1: identify the failed group\n");
    bool is_parity_group;
    int failed_group_idx = -1; // failed data group idx
    int failed_in_group_idx = -1; // the node index in failed group
    vector<int> *failed_group; // failed group
    vector<ETUnit*> *failed_et_unit_ptr; // failed et unit

    if (failed_node < _k) {
        is_parity_group = false;
        for (int i = 0; i < _data_et_groups.size(); i++) {
            vector<int> &group = _data_et_groups[i];
            auto it = find(group.begin(), group.end(), failed_node);
            if (it != group.end()) {
                failed_group_idx = i;
                failed_in_group_idx = it - group.begin();
                failed_group = &_data_et_groups[i];
                failed_et_unit_ptr = &_data_et_units[i];
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
                failed_et_unit_ptr = &_parity_et_units[i];
                break;
            }
        }
    }

    printf("is_parity_group: %d, failed_node: %d, failed_group_idx: %d, failed_in_group_idx: %d\n",
        is_parity_group, failed_node, failed_group_idx, failed_in_group_idx);

    int repair_bdwt = 0;

    int base_w = 0;
    int target_w = 0;
    if (!is_parity_group) {
        base_w = _data_base_w.at(failed_group_idx);
        target_w = base_w * _data_num_instances.at(failed_group_idx);
    } else {
        base_w = _parity_base_w.at(failed_group_idx);
        target_w = base_w * _parity_num_instances.at(failed_group_idx);
    }

    for (int failed_et_unit_idx = 0; failed_et_unit_idx < _w / target_w; failed_et_unit_idx++) {
        // 2. identify the sub-stripe index required for base code repair
        printf("step 2: identify the instance for base code repair\n");
        int failed_ins_id = -1;

        map<int, vector<int>> f_group_sp_map = failed_et_unit_ptr->at(failed_et_unit_idx)->GetRequiedUCSymbolsForNode(failed_in_group_idx);
        printf("f_group_sp_map:\n");
        for (auto row : f_group_sp_map) {
            printf("sp: %d, symbols: ", row.first);
            for (auto symbol : row.second) {
                printf("%d ", symbol);
            }
            printf("\n");
        }

        // number of required uc symbols equals to target sp - base sp
        int expected_num_symbols = target_w - base_w;
        for (auto row : f_group_sp_map) {
            if (row.second.size() == expected_num_symbols) {
                failed_ins_id = row.first;
                break;
            }
        }

        for (int w_idx = 0; w_idx < base_w; w_idx++) {

            // 3. get required uncoupled symbols for base code repair (Keyun revised on Jun 25)

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
            map<int, vector<pair<int, int>>> candidate_uc_symbols_ag_data, candidate_uc_symbols_ag_parity;
            int num_required_uc_symbols_ag = 0;
            int num_candidate_uc_symbols_ag = 0;

            int failed_ins_id_global = failed_et_unit_idx * target_w + failed_ins_id * base_w + w_idx;

            // For failed_ins_id, collect k - failed_group.size() data and , parity inv_uc_symbols
            for (int i = 0; i < _data_et_groups.size(); i++) {
                if (is_parity_group == false && i == failed_group_idx) { // for data node failure, skip failed group
                    continue;
                }

                int data_group_base_w = _data_base_w.at(i);
                int data_group_num_instances = _data_num_instances.at(i);
                int data_group_target_w = data_group_base_w * data_group_num_instances;
                int data_group_et_unit_idx = failed_ins_id_global / data_group_target_w;

                vector<int> &group = _data_et_groups[i];
                for (auto node_id : group) {
                    int uc_symbol = _inv_perm_uc_layout[failed_ins_id_global][node_id];
                    candidate_uc_symbols_ag_data[i].push_back(make_pair(uc_symbol, _data_et_units[i][data_group_et_unit_idx]->GetRelatedUCSymbolsForUCSymbol(uc_symbol).size()));
                    num_candidate_uc_symbols_ag++;
                }
            }
            
            for (int i = 0; i < _parity_et_groups.size(); i++) {
                if (is_parity_group == true && i == failed_group_idx) { // for parity node failure, skip failed group
                    continue;
                }

                int parity_group_base_w = _parity_base_w.at(i);
                int parity_group_num_instances = _parity_num_instances.at(i);
                int parity_group_target_w = parity_group_base_w * parity_group_num_instances;
                int parity_group_et_unit_idx = failed_ins_id_global / parity_group_target_w;

                vector<int> &group = _parity_et_groups[i];
                for (auto node_id : group) {
                    int uc_symbol = _inv_perm_uc_layout[failed_ins_id_global][node_id];
                    candidate_uc_symbols_ag_parity[i].push_back(make_pair(uc_symbol, _parity_et_units[i][parity_group_et_unit_idx]->GetRelatedUCSymbolsForUCSymbol(uc_symbol).size()));
                    num_candidate_uc_symbols_ag++;
                }
            }

            // if the failed group size is larger than n - k, try to retrieve symbols from the failed group
            vector<int> unrelated_uc_symbols = failed_et_unit_ptr->at(failed_et_unit_idx)->GetUnrelatedUCSymbols(failed_in_group_idx, failed_ins_id);

            if (is_parity_group == false) {
                for (auto symbol : unrelated_uc_symbols) {
                    if (symbol % base_w != w_idx) { continue; }
                    candidate_uc_symbols_ag_data[failed_group_idx].push_back(make_pair(symbol, failed_et_unit_ptr->at(failed_et_unit_idx)->GetRelatedUCSymbolsForUCSymbol(symbol).size()));
                    num_candidate_uc_symbols_ag++;
                }
            } else {
                for (auto symbol : unrelated_uc_symbols) {
                    if (symbol % base_w != w_idx) { continue; }
                    candidate_uc_symbols_ag_parity[failed_group_idx].push_back(make_pair(symbol, failed_et_unit_ptr->at(failed_et_unit_idx)->GetRelatedUCSymbolsForUCSymbol(symbol).size()));
                    num_candidate_uc_symbols_ag++;
                }
            }

            // put the candidates for data and parity together and sort by num of required uc symbols
            vector<pair<int, int>> candidate_uc_symbols;
            for (auto item : candidate_uc_symbols_ag_data) {
                for (auto p : item.second) {
                    candidate_uc_symbols.push_back(p);
                }
            }
            for (auto item : candidate_uc_symbols_ag_parity) {
                for (auto p : item.second) {
                    candidate_uc_symbols.push_back(p);
                }
            }

            // sort the candidate symbols by the number of required uc symbols
            sort(candidate_uc_symbols.begin(), candidate_uc_symbols.end(),
                [](const pair<int, int> lhs, const pair<int, int> rhs) {return lhs.second < rhs.second;});

            // printf("candidate_uc_symbols: \n");
            // for (auto item : candidate_uc_symbols) {
            //     printf("pkt_id: %d, num_required_uc_symbols: %d\n", item.first, item.second);
            // }

            // pick the first k candidate_uc_symbols
            for (size_t i = 0; i < candidate_uc_symbols.size(); i++) {
                if (i >= _k) { // stop after k
                    break;
                }
                int uc_symbol = candidate_uc_symbols[i].first;
                for (auto item : candidate_uc_symbols_ag_data) {
                    for (auto p : item.second) {
                        if (uc_symbol == p.first) {
                            required_uc_symbols_ag_data[item.first].push_back(uc_symbol);
                            num_required_uc_symbols_ag++;
                            repair_bdwt += p.second;
                        }
                    }
                }
                for (auto item : candidate_uc_symbols_ag_parity) {
                    for (auto p : item.second) {
                        if (uc_symbol == p.first) {
                            required_uc_symbols_ag_parity[item.first].push_back(uc_symbol);
                            num_required_uc_symbols_ag++;
                            repair_bdwt += p.second;
                        }
                    }
                }
            }

            vector<int> uc_symbols_failed_node;
            //for (int w = 0; w < base_w; w++) {
                int global_w_idx = target_w * failed_et_unit_idx + failed_ins_id * base_w;
                int symbol = _uncoupled_layout[global_w_idx][failed_node];
                vector<int> related_uc_symbols = failed_et_unit_ptr->at(failed_et_unit_idx)->GetRelatedUCSymbolsForUCSymbol(symbol, failed_ins_id);
                for (auto related_uc_symbol : related_uc_symbols) {
                    if (related_uc_symbol % base_w == w_idx) {
                        uc_symbols_failed_node.push_back(related_uc_symbol);
                    }
                }
            //}

            // for failed group, get all required symbols in inv_uc_layout
            for (auto symbol : uc_symbols_failed_node) {
                if (is_parity_group == false) {
                    if (find(required_uc_symbols_ag_data[failed_group_idx].begin(),
                    required_uc_symbols_ag_data[failed_group_idx].end(),
                    symbol) == required_uc_symbols_ag_data[failed_group_idx].end()) {
                        required_uc_symbols_fg.push_back(symbol);
                    }

                    // accumulate repair bandwidth
                    int rb = failed_et_unit_ptr->at(failed_et_unit_idx)->GetRelatedUCSymbolsForUCSymbol(symbol).size();
                    repair_bdwt += rb - 1;
                } else {
                    if (find(required_uc_symbols_ag_parity[failed_group_idx].begin(),
                    required_uc_symbols_ag_parity[failed_group_idx].end(),
                    symbol) == required_uc_symbols_ag_parity[failed_group_idx].end()) {
                        required_uc_symbols_fg.push_back(symbol);
                    }

                    // accumulate repair bandwidth
                    int rb = failed_et_unit_ptr->at(failed_et_unit_idx)->GetRelatedUCSymbolsForUCSymbol(symbol).size();
                    repair_bdwt += rb - 1;
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
                int target_group_target_w = _data_base_w.at(group_id) * _data_num_instances.at(group_id);
                int et_unit_idx = failed_ins_id_global / target_group_target_w;
                vector<int> required_uc_symbols = item.second;
                printf("decoupling for data group %d\n", group_id);

                ETUnit &et_unit = *_data_et_units[group_id][et_unit_idx];
                et_unit.Decoupling(required_uc_symbols, ecdag, &sources, max_source_id);
            }

            for (auto item : required_uc_symbols_ag_parity) {
                int group_id = item.first;
                int target_group_target_w = _parity_base_w.at(group_id) * _parity_num_instances.at(group_id);
                int et_unit_idx = failed_ins_id_global / target_group_target_w;
                vector<int> required_uc_symbols = item.second;
                printf("decoupling for parity group %d\n", group_id);

                ETUnit &et_unit = *_parity_et_units[group_id][et_unit_idx];
                et_unit.Decoupling(required_uc_symbols, ecdag, &sources, max_source_id);
            }

            // 5. base code repair for required_uc_symbols_fg
            printf("step 5: repair uc_symbols in alive groups by base code decode\n");
            vector<int> bc_from;
            vector<int> bc_to = required_uc_symbols_fg;
            for (auto s : f_group_sp_map.at(failed_ins_id)) {
                if (s % base_w == w_idx) { bc_to.push_back(s); }
            }
            for (auto item : required_uc_symbols_ag_data) {
                vector<int> required_uc_symbols = item.second;
                bc_from.insert(bc_from.end(), required_uc_symbols.begin(), required_uc_symbols.end());
            }
            for (auto item : required_uc_symbols_ag_parity) {
                vector<int> required_uc_symbols = item.second;
                bc_from.insert(bc_from.end(), required_uc_symbols.begin(), required_uc_symbols.end());
            }

            printf("base code (instance %d) from: ", failed_ins_id_global);
            for (auto symbol : bc_from) {
                printf("%d ", symbol);
            }
            printf("\n");

            printf("base code (instance %d) to: ", failed_ins_id_global);
            for (auto symbol : bc_to) {
                printf("%d ", symbol);
            }
            printf("\n");

            _instances[failed_ins_id_global]->Decode(bc_from, bc_to, ecdag);
        }

        // 6. repair failed packets by pairwise coupling
        printf("step 6: repair layout symbols in failed groups by biased coupling\n");
        vector<int> unit_to;
        for (int i = failed_et_unit_idx * target_w; i < (failed_et_unit_idx + 1) * target_w; i++) {
            unit_to.push_back(to.at(i));
        }
        failed_et_unit_ptr->at(failed_et_unit_idx)->BiasedCoupling(unit_to, failed_ins_id, ecdag, &sources, max_source_id);
    }

    int num_packet_read = sources.size();
    cout << "Number of packets read = " << num_packet_read 
         << ", Normalized bandwidth = " << fixed << setprecision(3) << num_packet_read * 1.0 / (_k * _w)
         << endl;

    //printf("Normalized repair bandwidth: %d / %d: %f\n", repair_bdwt, _k * _w, repair_bdwt * 1.0 / (_k * _w));
    
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
