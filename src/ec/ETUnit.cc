/**
 * @file ETUnit.cc
 * @author Keyun Cheng (kycheng@cse.cuhk.edu.hk)
 * @brief Elastic Transformation Unit
 * @version 0.1
 * @date 2022-04-08
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#include "ETUnit.hh"


ETUnit::ETUnit(int r, int c, int base_w, vector<vector<int>> uncoupled_layout, vector<vector<int>> layout, int e)
{
    // input argument check
    if (uncoupled_layout.size() != r * base_w || layout.size() != r * base_w || r > c) {
        printf("error: invalid arguments r: %d, c: %d, base_w: %d, layout: %lu\n", _r, _c, _base_w, layout.size());
        return;
    }

    _r = r;
    _c = c;
    _base_w = base_w;
    _e = e;
    _uncoupled_layout = uncoupled_layout;
    _layout = layout;

    int mtxr = _r * _c;

    printf("ETUnit: r: %d, c: %d, base_w: %d, e: %d, eta: %d\n", _r, _c, _base_w, _e, _eta);

    // // 1. generate rotation matrix
    // _rotation_matrix = (int *) malloc(mtxr * mtxr * sizeof(int));
    // memset(_rotation_matrix, 0, mtxr * mtxr * sizeof(int));
    // gen_rotation_matrix(r, c, _rotation_matrix);

    // 2. generate pairwise coupling matrix and decoupling matrix (inv_pc_matrix)
    _pc_matrix = (int *) malloc(mtxr * mtxr * sizeof(int));
    memset(_pc_matrix, 0, mtxr * mtxr * sizeof(int));

    _inv_pc_matrix = (int *) malloc(mtxr * mtxr * sizeof(int));
    memset(_inv_pc_matrix, 0, mtxr * mtxr * sizeof(int));

    // nnon-square et unit
    if (_r < _c) {
        int mtxr_inner = _r * _r;
        int *pc_matrix_square = (int *) calloc(mtxr_inner * mtxr_inner, sizeof(int));
        int *pc_mtx_upper = (int *) calloc(mtxr * mtxr, sizeof(int));
        int *pc_mtx_lower = (int *) calloc(mtxr * mtxr, sizeof(int));

        // lower diagonal
        for (int i = mtxr_inner; i < mtxr; i++) {
            pc_mtx_upper[i * mtxr + i] = 1;
        }

        // upper diagonal
        for (int i = 0; i < (_c - _r) * _r; i++) {
            pc_mtx_lower[i * mtxr + i] = 1;
        }
        
        // generate the square pc matrix and copy to pc_mtx_lower and upper
        gen_square_pc_matrix(_r, pc_matrix_square);

        for (int i = 0; i < mtxr_inner; i++) {
            memcpy(&pc_mtx_upper[i * mtxr], &pc_matrix_square[i * mtxr_inner], mtxr_inner * sizeof(int));
        }

        for (int i = 0; i < mtxr_inner; i++) {
            memcpy(&pc_mtx_lower[((_c - _r) * _r + i) * mtxr + (_c - _r) * _r], &pc_matrix_square[i * mtxr_inner], mtxr_inner * sizeof(int));
        }

        // printf("ETUnitSquare::pc_mtx_upper:\n");
        // for (int i = 0; i < mtxr; i++) {
        //     for (int j = 0; j < mtxr; j++) {
        //         printf("%d ", pc_mtx_upper[i * mtxr + j]);
        //     }
        //     printf("\n");
        // }
        // printf("\n");

        // printf("ETUnitSquare::pc_mtx_lower:\n");
        // for (int i = 0; i < mtxr; i++) {
        //     for (int j = 0; j < mtxr; j++) {
        //         printf("%d ", pc_mtx_lower[i * mtxr + j]);
        //     }
        //     printf("\n");
        // }
        // printf("\n");

        // multiply the matrix to get pc_matrix
        int *pc_matrix = jerasure_matrix_multiply(pc_mtx_lower, pc_mtx_upper, mtxr, mtxr, mtxr, mtxr, 8);
        memcpy(_pc_matrix, pc_matrix, mtxr * mtxr * sizeof(int));

        free(pc_matrix_square);
        free(pc_mtx_upper);
        free(pc_mtx_lower);
        free(pc_matrix);
    } else {
        // directly generate square pc matrix
        gen_square_pc_matrix(_r, _pc_matrix);
    }
    
    int *tmp_pc_matrix = (int *) malloc(mtxr * mtxr * sizeof(int));
    memcpy(tmp_pc_matrix, _pc_matrix, mtxr * mtxr * sizeof(int));
    int mtx_invertible = jerasure_invert_matrix(tmp_pc_matrix, _inv_pc_matrix, mtxr, 8);
    
    if (mtx_invertible == -1) {
        printf("error: recover_matrix not invertible!\n");
    }

    printf("ETUnitSquare::pc_matrix:\n");
    for (int i = 0; i < mtxr; i++) {
        for (int j = 0; j < mtxr; j++) {
            printf("%d ", _pc_matrix[i * mtxr + j]);
        }
        printf("\n");
    }
    printf("\n");

    printf("ETUnitSquare::inv_pc_matrix:\n");
    for (int i = 0; i < mtxr; i++) {
        for (int j = 0; j < mtxr; j++) {
            printf("%d ", _inv_pc_matrix[i * mtxr + j]);
        }
        printf("\n");
    }
    printf("\n");

    printf("ETUnit::layout:\n");
    for (int ins_id = 0; ins_id < _r; ins_id++) {
        for (int w = 0; w < _base_w; w++) {
            for (int node_id = 0; node_id < _c; node_id++) {
                printf("%d ", _layout[ins_id * _base_w + w][node_id]);   
            }
            printf("\n");
        }
    }

    printf("ETUnit::uncoupled layout (before):\n");
    for (int ins_id = 0; ins_id < _r; ins_id++) {
        for (int w = 0; w < _base_w; w++) {
            for (int node_id = 0; node_id < _c; node_id++) {
                printf("%d ", _uncoupled_layout[ins_id * _base_w + w][node_id]);
            }
            printf("\n");
        }
    }

    // 3. for rows in pc_matrix with only one non-zero coef
    // replace the uncoupled layout item with layout item
    for (int node_id = 0; node_id < _c; node_id++) {
        for (int ins_id = 0; ins_id < r; ins_id++) {
            int packet_idx = node_id * _r + ins_id;
            vector<int> non_zero_coefs;
            int *pc_matrix_row = &_pc_matrix[packet_idx * mtxr];
            for (int i = 0; i < mtxr; i++) {
                if (pc_matrix_row[i] != 0) {
                    non_zero_coefs.push_back(pc_matrix_row[i]);
                }
            }

            if (non_zero_coefs.size() <= 1) {
                for (int w = 0; w < _base_w; w++) {
                    // reset uncoupled layout to layout
                    _uncoupled_layout[ins_id * _base_w + w][node_id] = _layout[ins_id * _base_w + w][node_id];
                }
            }
            
        }
        
    }

    // 4. permutate uncoupled layout
    for (int ins_id = 0; ins_id < r; ins_id++) {
        for (int w = 0; w < _base_w; w++) {
            vector<int> ulrow = _uncoupled_layout[ins_id * _base_w + w];
            vector<int> ulrow_inv = _uncoupled_layout[ins_id * _base_w + w];
            std::rotate(ulrow.begin(), ulrow.begin() + ins_id, ulrow.end());
            std::rotate(ulrow_inv.rbegin(), ulrow_inv.rbegin() + ins_id, ulrow_inv.rend());

            _perm_uc_layout.push_back(ulrow);
            _inv_perm_uc_layout.push_back(ulrow_inv);
        }
    }

    printf("ETUnit::uncoupled layout (updated):\n");
    for (int ins_id = 0; ins_id < r; ins_id++) {
        for (int w = 0; w < _base_w; w++) {
            for (int node_id = 0; node_id < _c; node_id++) {
                printf("%d ", _uncoupled_layout[ins_id * _base_w + w][node_id]);
            }
            printf("\n");
        }
    }

    printf("ETUnit::permutated layout:\n");
    for (int ins_id = 0; ins_id < r; ins_id++) {
        for (int w = 0; w < _base_w; w++) {
            for (int node_id = 0; node_id < _c; node_id++) {
                printf("%d ", _perm_uc_layout[ins_id * _base_w + w][node_id]);
            }
            printf("\n");
        }
    }

    printf("ETUnit::inverse permutated layout:\n");
    for (int ins_id = 0; ins_id < r; ins_id++) {
        for (int w = 0; w < _base_w; w++) {
            for (int node_id = 0; node_id < _c; node_id++) {
                printf("%d ", _inv_perm_uc_layout[ins_id * _base_w + w][node_id]);
            }
            printf("\n");
        }
    }

    free(tmp_pc_matrix);

    printf("ETUnit::ETUnit finished initialization\n");
}

ETUnit::~ETUnit()
{
    free(_rotation_matrix);
    free(_pc_matrix);
    free(_pc_matrix);
}

void ETUnit::gen_square_pc_matrix(int r, int *pc_matrix) {
    if (pc_matrix == NULL) {
        printf("error: invalid argument\n");
        return;
    }

    int mtxr = _r * _r;

    memset(pc_matrix, 0, mtxr * mtxr * sizeof(int));

    // set pairwise coupling matrix
    for (int i = 0; i < _r; i++) {
        for (int j = 0; j < _r; j++) {
            int pkt_idx = i * _r + j;
            if (i == j) {
                pc_matrix[pkt_idx * mtxr + i * _r + i] = 1; // symmetric row
            } else {
                pc_matrix[pkt_idx * mtxr + j * _r + i] = _eta; // eta
                if (i < j) {
                    pc_matrix[pkt_idx * mtxr + i * _r + j] = 1; // theta
                } else {
                    pc_matrix[pkt_idx * mtxr + i * _r + j] = _e; // theta
                }
            }
        }
    }

    // printf("ETUnit::gen_square_pc_matrix:\n");
    // for (int i = 0; i < mtxr; i++) {
    //     for (int j = 0; j < mtxr; j++) {
    //         printf("%d ", pc_matrix[i * mtxr + j]);
    //     }
    //     printf("\n");
    // }
    // printf("\n");
}

void ETUnit::gen_rotation_matrix(int r, int c, int *rotation_matrix) {
    if (rotation_matrix == NULL) {
        printf("error: invalid argument\n");
        return;
    }

    int mtxr = r * c;

    memset(rotation_matrix, 0, mtxr * mtxr * sizeof(int));

    for (int i = 0; i < r; i++) {
        for (int j = 0; j < c; j++) {
            int packet_id = i * c + j;
            int offset = i;
            if ((packet_id + offset) >= (i + 1) * c && i > 0) {
                offset -= c;
            }
            rotation_matrix[packet_id * mtxr + packet_id + offset] = 1;
        }
    }

    printf("rotation_matrix:\n");
    for (int i = 0; i < mtxr; i++) {
        for (int j = 0; j < mtxr; j++) {
            printf("%d ", rotation_matrix[i * mtxr + j]);
        }
        printf("\n");
    }
}

void ETUnit::Coupling(ECDAG *ecdag, set<int> *sources, int max_src_id) {
    if (ecdag == NULL) {
        printf("error: invalid input ecdag\n");
        return;
    }

    int mtxr = _r * _c;

    int count = 0;
    // for parity, compute code layout with uncoupled layout
    for (int node_id = 0; node_id < _c; node_id++) {
        for (int ins_id = 0; ins_id < _r; ins_id++) {
            for (int w = 0; w < _base_w; w++) {
                int coupled_id = _layout[ins_id * _base_w + w][node_id];
                int packet_idx = node_id * _r + ins_id;
                vector<int> uncoupled_ids;
                vector<int> coefs;
                int *coefs_row = &_pc_matrix[packet_idx * mtxr];

                for (int i = 0; i < _c; i++) {
                    for (int j = 0; j < _r; j++) {
                        int coef = coefs_row[i * _r + j];
                        if (coef != 0) {
                            int symbol = _uncoupled_layout[j * _base_w + w][i];
                            coefs.push_back(coef);
                            uncoupled_ids.push_back(symbol);
                            if (sources && symbol <= max_src_id) { sources->insert(symbol); }
                        }
                    }
                }
                
                if (uncoupled_ids.size() > 1) {
                    ecdag->Join(coupled_id, uncoupled_ids, coefs);
                
                    // BindY
                    ecdag->BindY(coupled_id, uncoupled_ids[0]); // bind to the first symbol in uncoupled_ids
                }
            }
        }
    }

}

void ETUnit::Decoupling(ECDAG *ecdag, set<int> *sources, int max_src_id) {
    if (ecdag == NULL) {
        printf("error: invalid input ecdag\n");
        return;
    }

    int mtxr = _r * _c;

    // for data nodes, compute (inverse permutated) uncoupled layout
    for (int node_id = 0; node_id < _c; node_id++) {
        for (int ins_id = 0; ins_id < _r; ins_id++) {
            for (int w = 0; w < _base_w; w++) {
                int uc_id = _uncoupled_layout[ins_id * _base_w + w][node_id];
                int packet_idx = node_id * _r + ins_id;
                vector<int> layout_ids;
                vector<int> coefs;
                int *coefs_row = &_inv_pc_matrix[packet_idx * mtxr];

                for (int i = 0; i < _c; i++) {
                    for (int j = 0; j < _r; j++) {
                        int coef = coefs_row[i * _r + j];
                        if (coef != 0) {
                            int symbol = _layout[j * _base_w + w][i];
                            coefs.push_back(coef);
                            layout_ids.push_back(symbol);
                            if (sources && symbol <= max_src_id) { sources->insert(symbol); }
                        }
                    }
                }
                
                // encode
                if (layout_ids.size() > 1) {
                    ecdag->Join(uc_id, layout_ids, coefs);

                    // BindX and BindY
                    ecdag->BindY(uc_id, layout_ids[0]); // bind to the first symbol in layout_ids
                }
            }
        }
    }
}


void ETUnit::BiasedCoupling(vector<int> to, int alive_ins_id, ECDAG *ecdag, set<int> *sources, int max_src_id) {
    printf("ETUnit::BiasedCoupling to: ");
    for (auto pkt_id : to) {
        printf("%d ", pkt_id);
    }
    printf("\n");

    int mtxr = _r * _c;

    for (int i = 0; i < to.size(); i++) {
        // locate the symbol in layout
        int failed_symbol = to[i];
        printf("ETUnit::Decoupling repairing %d\n", failed_symbol);
        int failed_ins_id = -1;
        int failed_node_id = -1;
        int failed_w = -1;
        int failed_pkt_idx = -1;
        for (int node_id = 0; node_id < _c && failed_pkt_idx == -1; node_id++) {
            for (int ins_id = 0; ins_id < _r && failed_pkt_idx == -1; ins_id++) {
                for (int w = 0; w < _base_w && failed_pkt_idx == -1; w++) {
                    if (_layout[ins_id * _base_w + w][node_id] == failed_symbol) {
                        failed_ins_id = ins_id;
                        failed_node_id = node_id;
                        failed_w = w;
                        failed_pkt_idx = node_id * _r + ins_id;
                        break;
                    }
                }
            }
        }

        printf("(failed_ins_id: %d, failed_node_id: %d, failed_w: %d, failed_pkt_idx: %d), %d\n",
            failed_ins_id, failed_node_id, failed_w, failed_pkt_idx, failed_symbol);
        

        // biased coupling
        // use all the layout symbols in the same sp and an additional uncoupled symbol to repair

        // 1. locate ALL the required layout symbols in ins_id
        vector<int> packet_idxs; // packet indices

        for (int pkt_idx = 0; pkt_idx < mtxr; pkt_idx++) {
            int *coefs_row = &_pc_matrix[pkt_idx * mtxr];
            if (coefs_row[failed_pkt_idx] != 0) {
                packet_idxs.push_back(pkt_idx);
            }
        }

        // for some symbols, the coef maybe 0 on failed_pkt_idx
        // we need to check the coef rows for all related symbols
        vector<int> tmp_packet_idxs = packet_idxs;

        while (true) {
            vector<int> tmp_packet_idxs = packet_idxs;

            for (int pkt_idx : tmp_packet_idxs) {
                if (pkt_idx == failed_pkt_idx) {
                    continue;
                }
                int *coefs_row = &_pc_matrix[pkt_idx * mtxr];
                for (int i = 0; i < mtxr; i++) {
                    if (coefs_row[i] != 0) {
                        if (find(packet_idxs.begin(), packet_idxs.end(), i) == packet_idxs.end()) {
                            packet_idxs.push_back(i);
                        }
                    }
                }
            }
            
            // no new related packets added
            if (packet_idxs.size() == tmp_packet_idxs.size()) {
                break;
            }
        }

        sort(packet_idxs.begin(), packet_idxs.end());

        int num_elements = packet_idxs.size();

        vector<int> coefs; // nonzero packet coefs in pc_matrix
        int *coefs_row = &_pc_matrix[failed_pkt_idx * mtxr];

        for (int i = 0; i < num_elements; i++) {
            coefs.push_back(coefs_row[packet_idxs[i]]);
        }

        printf("packet_idx: %d\n", failed_pkt_idx);

        printf("coefs: ");
        for (auto coef : coefs) {
            printf("%d ", coef);
        
        }
        printf("\n");
        
        printf("packet_idxs: ");
        for (auto pkt : packet_idxs) {
            printf("%d ", pkt);
        
        }
        printf("\n");
        
        // skip the packet that doesn't needs coupling
        if (coefs.size() == 1) {
            continue;
        }

        // construct the repair matrix
        int *repair_matrix = (int *) calloc(num_elements * num_elements, sizeof(int));
        int *inv_repair_matrix = (int *) calloc(num_elements * num_elements, sizeof(int));
        int *coefs_row_nonzero = (int *) calloc(num_elements, sizeof(int));

        // the first row is for the uncoupled packet in failed_sp, the corresponding index = 1
        int alive_uc_pkt_idx = -1;
        int alive_uc_pkt_idx_in_couple = -1;
        for (int i = 0; i < num_elements; i++) {
            if (packet_idxs[i] % _r == alive_ins_id) {
                alive_uc_pkt_idx = packet_idxs[i];
                alive_uc_pkt_idx_in_couple = i;
                break;
            }
        }

        int alive_uc_pkt_idx_ins_id = alive_uc_pkt_idx % _r;
        int alive_uc_pkt_idx_node_id = alive_uc_pkt_idx / _r;

        printf("alive_uc_pkt_idx: %d, " \
            "node_id: %d, ins_id: %d, " \
            "alive_uc_pkt_idx_in_couple: %d\n",
            alive_uc_pkt_idx, 
            alive_uc_pkt_idx_ins_id, alive_uc_pkt_idx_node_id, 
            alive_uc_pkt_idx_in_couple);

        repair_matrix[0 * num_elements + alive_uc_pkt_idx_in_couple] = 1;

        // the second to num_elements - 1 rows are layout rows
        for (int i = 0, mtx_row = 0; i < num_elements; i++) {
            if (packet_idxs[i] == failed_pkt_idx) {
                continue;
            }
            int *coefs_row = &_pc_matrix[packet_idxs[i] * mtxr];
            for (int cnt = 0; cnt < num_elements; cnt++) {
                repair_matrix[(1 + mtx_row) * num_elements + cnt] = coefs_row[packet_idxs[cnt]];
            }
            mtx_row++;
        }

        // printf("repair_matrix:\n");
        // for (int i = 0; i < num_elements; i++) {
        //     for (int j = 0; j < num_elements; j++) {
        //         printf("%d ", repair_matrix[i * num_elements + j]);
        //     }
        //     printf("\n");
        // }

        int mtx_invertible = jerasure_invert_matrix(repair_matrix, inv_repair_matrix, num_elements, 8);
    
        if (mtx_invertible == -1) {
            printf("error: recover_matrix not invertible!\n");
        }

        // printf("inv_repair_matrix:\n");
        // for (int i = 0; i < num_elements; i++) {
        //     for (int j = 0; j < num_elements; j++) {
        //         printf("%d ", inv_repair_matrix[i * num_elements + j]);
        //     }
        //     printf("\n");
        // }

        // copy the items in coefs_row into coefs_row_nonzero
        for (int i = 0; i < num_elements; i++) {
            coefs_row_nonzero[i] = coefs[i];
        }

        // printf("coefs_row_nonzero:\n");
        // for (int i = 0; i < num_elements; i++) {
        //     printf("%d ", coefs_row_nonzero[i]);
        // }
        // printf("\n");

        int* coef_decode_row = jerasure_matrix_multiply(
            coefs_row_nonzero, inv_repair_matrix, 1, num_elements, num_elements, num_elements, 8);

        // printf("coef_decode_row:\n");
        // for (int i = 0; i < num_elements; i++) {
        //     printf("%d ", coef_decode_row[i]);
        // }
        // printf("\n");
        
        vector<int> required_symbols;

        // add the additional uncoupled symbol
        int symbol = _uncoupled_layout[alive_uc_pkt_idx_ins_id * _base_w + failed_w][alive_uc_pkt_idx_node_id];
        required_symbols.push_back(symbol);
        if (sources && symbol <= max_src_id) { sources->insert(symbol); }

        // add all alive layout symbols for decoupling
        for (int i = 0; i < num_elements; i++) {
            int ins_id = packet_idxs[i] % _r;
            int node_id = packet_idxs[i] / _r;

            int l_symbol = _layout[ins_id * _base_w + failed_w][node_id];

            if (l_symbol == failed_symbol) {
                continue;
            }
            required_symbols.push_back(l_symbol);
            if (sources && l_symbol <= max_src_id) { sources->insert(l_symbol); }
        }

        // printf("required_symbols:\n");
        // for (int i = 0; i < num_elements; i++) {
        //     printf("%d ", required_symbols[i]);
        // }
        // printf("\n");
        
        vector<int> decode_row(coef_decode_row, coef_decode_row + num_elements);
        
        ecdag->Join(failed_symbol, required_symbols, decode_row);

        // BindX and BindY
        ecdag->BindY(failed_symbol, required_symbols[0]); // bind to symbol 0

        free(coef_decode_row);
        free(coefs_row_nonzero);
        free(inv_repair_matrix);
        free(repair_matrix);

    }
}

void ETUnit::Decoupling(vector<int> to, ECDAG *ecdag, set<int> *sources, int max_src_id) {
    printf("ETUnit::Decoupling to:");
    for (auto pkt_id : to) {
        printf("%d ", pkt_id);
    }
    printf("\n");

    int mtxr = _r * _c;

    for (int i = 0; i < to.size(); i++) {
        // locate the symbol in uncoupled layout
        int failed_symbol = to[i];
        printf("ETUnit::Decoupling repairing %d\n", failed_symbol);
        int failed_ins_id = -1;
        int failed_node_id = -1;
        int failed_w = -1;
        int failed_pkt_idx = -1;
        for (int node_id = 0; node_id < _c && failed_pkt_idx == -1; node_id++) {
            for (int ins_id = 0; ins_id < _r && failed_pkt_idx == -1; ins_id++) {
                for (int w = 0; w < _base_w && failed_pkt_idx == -1; w++) {
                    if (_uncoupled_layout[ins_id * _base_w + w][node_id] == failed_symbol) {
                        failed_ins_id = ins_id;
                        failed_node_id = node_id;
                        failed_w = w;
                        failed_pkt_idx = node_id * _r + ins_id;
                        break;
                    }
                }
            }
        }

        printf("(failed_ins_id: %d, failed_node_id: %d, failed_w: %d, failed_pkt_idx: %d), %d\n",
            failed_ins_id, failed_node_id, failed_w, failed_pkt_idx, failed_symbol);
        
        vector<int> layout_symbols;
        vector<int> coefs;
        int *coefs_row = &_inv_pc_matrix[failed_pkt_idx * mtxr];
        
        for (int node_id = 0; node_id < _c; node_id++) {
            for (int ins_id = 0; ins_id < _r; ins_id++) {
                int coef = coefs_row[node_id * _r + ins_id];
                if (coef != 0) {
                    int symbol = _layout[ins_id * _base_w + failed_w][node_id];
                    coefs.push_back(coef);
                    layout_symbols.push_back(symbol);
                    if (sources && symbol <= max_src_id) { sources->insert(symbol); }
                }
            }
        }
        
        // encode
        if (layout_symbols.size() > 1) {
            ecdag->Join(failed_symbol, layout_symbols, coefs);

            // BindX and BindY
            ecdag->BindY(failed_symbol, layout_symbols[0]); // bind to layout symbol 0
        }
    }
}

vector<vector<int>> ETUnit::GetUCLayout() {
    return _uncoupled_layout;
}

vector<vector<int>> ETUnit::GetInvPermUCLayout() {
    return _inv_perm_uc_layout;
}

map<int, vector<int>> ETUnit::GetRequiredUCSymbolsForSP(int sp) {
    map<int, vector<int>> required_sp_map; // node -> uc symbol

    if (sp < 0 || sp >= _r) {
        printf("error: invalid sp: %d\n", sp);
        return required_sp_map;
    }

    int mtxr = _r * _c;

    for (int packet_idx = 0; packet_idx < mtxr; packet_idx++) {
        int *pc_matrix_row = &_pc_matrix[packet_idx * mtxr];
        bool is_row_required = false;

        for (int i = 0; i < _c; i++) {
            for (int j = 0; j < _r; j++) {
                if (pc_matrix_row[i * _r + j] != 0 && j == sp) {
                    is_row_required = true;
                }
            }
        }

        // all packets in this row is required
        if (is_row_required == true) {
            for (int i = 0; i < _c; i++) {
                for (int j = 0; j < _r; j++) {
                    if (pc_matrix_row[i * _r + j] != 0) {
                        int required_uc_packet = _uncoupled_layout[j][i];
                        if (find(required_sp_map[j].begin(), required_sp_map[j].end(), required_uc_packet)
                            == required_sp_map[j].end()) {
                            required_sp_map[j].push_back(required_uc_packet);
                        }
                    }
                }
            }
        }
    }

    return required_sp_map;
}

map<int, vector<int>> ETUnit::GetRequiedUCSymbolsForNode(int failed_grp_idx) {
    map<int, vector<int>> required_sp_map; // sp -> uc symbol

    if (failed_grp_idx < 0 || failed_grp_idx >= _c) {
        printf("error: invalid failed_grp_idx: %d\n", failed_grp_idx);
        return required_sp_map;
    }

    int mtxr = _r * _c;

    // check all symbols in the failed node, and get required uncoupled symbols for repairing each symbol
    for (int sp = 0; sp < _r; sp++) {
        int packet_idx = failed_grp_idx * _r + sp;
        int *pc_matrix_row = &_pc_matrix[packet_idx * mtxr];
        for (int node_id = 0; node_id < _c; node_id++) {
            for (int ins_id = 0; ins_id < _r; ins_id++) {
                if (pc_matrix_row[node_id * _r + ins_id] != 0 && node_id != failed_grp_idx) {
                    for (int w = 0; w < _base_w; w++) {
                        int required_uc_packet = _uncoupled_layout[ins_id * _base_w + w][node_id];
                        required_sp_map[ins_id].push_back(required_uc_packet);
                    }
                }
            }
        }
    }

    return required_sp_map;
}

vector<int> ETUnit::GetUnrelatedUCSymbols(int failed_grp_idx, int failed_ins_id) {
    vector<int> unrelated_symbols;

    if (failed_grp_idx < 0 || failed_grp_idx >= _c) {
        printf("error: invalid failed_grp_idx: %d\n", failed_grp_idx);
        return unrelated_symbols;
    }

    if (failed_ins_id < 0 || failed_ins_id >= _r) {
        printf("error: invalid failed_ins_id: %d\n", failed_ins_id);
        return unrelated_symbols;
    }

    int mtxr = _r * _c;

    for (int node_id = 0; node_id < _c; node_id++) {
        if (node_id == failed_grp_idx) {
            continue;
        }
        
        int packet_idx = node_id * _r + failed_ins_id;

        int *pc_matrix_row = &_inv_pc_matrix[packet_idx * mtxr];
        vector<int> related_pkt_idxs;

        int num_non_zero_coefs = 0;
        bool is_row_valid = true;

        for (int i = 0; i < _c; i++) {
            for (int j = 0; j < _r; j++) {
                if (pc_matrix_row[i * _r + j] != 0) {
                    if (i == failed_grp_idx) {
                        is_row_valid = false;
                    } else {
                        num_non_zero_coefs += 1;
                        related_pkt_idxs.push_back(i * _r + j);
                    }
                }
            }
        }

        // if (num_non_zero_coefs <= 1 || is_row_valid == false) {
        //     continue;
        // }

        for (auto pkt_idx : related_pkt_idxs) {
            int *pc_matrix_related_row = &_inv_pc_matrix[pkt_idx * mtxr];
            
            for (int i = 0; i < _c; i++) {
                for (int j = 0; j < _r; j++) {
                    if (pc_matrix_related_row[i * _r + j] != 0 && i == failed_grp_idx) {
                        is_row_valid = false;
                    }
                }
            }
        }

        if (is_row_valid == true) {
            for (int w = 0; w < _base_w; w++) {
                unrelated_symbols.push_back(_uncoupled_layout[failed_ins_id * _base_w + w][node_id]);
            }
        }
    }

    return unrelated_symbols;
}

vector<int> ETUnit::GetRelatedUCSymbolsForUCSymbol(int uc_symbol_id) {
    int mtxr = _r * _c;
    // locate the symbol in uncoupled layout
    int uc_ins_id = -1;
    int uc_node_id = -1;
    int uc_w = -1;
    int uc_pkt_idx = -1;
    for (int node_id = 0; node_id < _c && uc_pkt_idx == -1; node_id++) {
        for (int ins_id = 0; ins_id < _r && uc_pkt_idx == -1; ins_id++) {
            for (int w = 0; w < _base_w && uc_pkt_idx == -1; w++) {
                if (_uncoupled_layout[ins_id * _base_w + w][node_id] == uc_symbol_id) {
                    uc_ins_id = ins_id;
                    uc_node_id = node_id;
                    uc_w = w;
                    uc_pkt_idx = node_id * _r + ins_id;
                    break;
                }
            }
        }
    }

    // printf("(uc_ins_id: %d, uc_node_id: %d, uc_w: %d, uc_pkt_idx: %d), %d\n",
    //     uc_ins_id, uc_node_id, uc_w, uc_pkt_idx, uc_symbol_id);

    vector<int> related_uc_symbols;

    int *coefs_row = &_inv_pc_matrix[uc_pkt_idx * mtxr];
    
    for (int node_id = 0; node_id < _c; node_id++) {
        for (int ins_id = 0; ins_id < _r; ins_id++) {
            int coef = coefs_row[node_id * _r + ins_id];
            if (coef != 0) {
                for (int w = 0; w < _base_w; w++) {
                    related_uc_symbols.push_back(_uncoupled_layout[ins_id * _base_w + w][node_id]);
                }
            }
        }
    }

    return related_uc_symbols;
}

vector<int> ETUnit::GetRelatedUCSymbolsForUCSymbol(int uc_symbol_id, int ins_id) {
    int mtxr = _r * _c;
    // locate the symbol in uncoupled layout
    int uc_ins_id = -1;
    int uc_node_id = -1;
    int uc_w = -1;
    int uc_pkt_idx = -1;
    for (int node_id = 0; node_id < _c && uc_pkt_idx == -1; node_id++) {
        for (int ins_id = 0; ins_id < _r && uc_pkt_idx == -1; ins_id++) {
            for (int w = 0; w < _base_w && uc_pkt_idx == -1; w++) {
                if (_uncoupled_layout[ins_id * _base_w + w][node_id] == uc_symbol_id) {
                    uc_ins_id = ins_id;
                    uc_node_id = node_id;
                    uc_w = w;
                    uc_pkt_idx = node_id * _r + ins_id;
                    break;
                }
            }
        }
    }

    // printf("(uc_ins_id: %d, uc_node_id: %d, uc_w: %d, uc_pkt_idx: %d), %d\n",
    //     uc_ins_id, uc_node_id, uc_w, uc_pkt_idx, uc_symbol_id);

    vector<int> related_uc_symbols;

    int *coefs_row = &_inv_pc_matrix[uc_pkt_idx * mtxr];
    
    for (int node_id = 0; node_id < _c; node_id++) {
        int coef = coefs_row[node_id * _r + ins_id];
        if (coef != 0) {
            for (int w = 0; w < _base_w; w++) {
                related_uc_symbols.push_back(_uncoupled_layout[ins_id * _base_w + w][node_id]);
            }
        }
    }

    return related_uc_symbols;
}
