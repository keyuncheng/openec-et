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


ETUnit::ETUnit(int r, int c, int base_w, bool is_parity, vector<vector<int>> uncoupled_layout, vector<vector<int>> layout, int e)
{
    if (uncoupled_layout.size() != r * base_w || layout.size() != r * base_w || r > c) {
        printf("error: invalid arguments\n");
        return;
    }

    _r = r;
    _c = c;
    _base_w = base_w;
    _e = e;
    _is_parity = is_parity;
    _uncoupled_layout = uncoupled_layout;
    _layout = layout;

    int mtxr = _r * _c;

    printf("r: %d, c: %d, base_w: %d, e: %d, eta: %d\n", _r, _c, _base_w, _e, _eta);

    // rotation matrix
    _rotation_matrix = (int *) malloc(mtxr * mtxr * sizeof(int));
    memset(_rotation_matrix, 0, mtxr * mtxr * sizeof(int));

    gen_rotation_matrix(r, c, _rotation_matrix);

    // pairwise coupling matrix and inverse
    _pc_matrix = (int *) malloc(mtxr * mtxr * sizeof(int));
    memset(_pc_matrix, 0, mtxr * mtxr * sizeof(int));

    _inv_pc_matrix = (int *) malloc(mtxr * mtxr * sizeof(int));
    memset(_inv_pc_matrix, 0, mtxr * mtxr * sizeof(int));


    if (_r < _c) {
        int mtxr_inner = _r * _r;
        int *pc_matrix_square = (int *) calloc(mtxr_inner * mtxr_inner, sizeof(int));
        int *pc_mtx_upper = (int *) calloc(mtxr * mtxr, sizeof(int));
        int *pc_mtx_lower = (int *) calloc(mtxr * mtxr, sizeof(int));

        for (int i = mtxr_inner; i < mtxr; i++) {
            pc_mtx_upper[i * mtxr + i] = 1;
        }

        for (int i = 0; i < (_c - _r) * _r; i++) {
            pc_mtx_lower[i * mtxr + i] = 1;
        }
        
        gen_square_pc_matrix(_r, pc_matrix_square);

        for (int i = 0; i < mtxr_inner; i++) {
            memcpy(&pc_mtx_upper[i * mtxr], &pc_matrix_square[i * mtxr_inner], mtxr_inner * sizeof(int));
        }

        for (int i = 0; i < mtxr_inner; i++) {
            memcpy(&pc_mtx_lower[((_c - _r) * _r + i) * mtxr + (_c - _r) * _r], &pc_matrix_square[i * mtxr_inner], mtxr_inner * sizeof(int));
        }

        printf("ETUnitSquare::pc_mtx_upper:\n");
        for (int i = 0; i < mtxr; i++) {
            for (int j = 0; j < mtxr; j++) {
                printf("%d ", pc_mtx_upper[i * mtxr + j]);
            }
            printf("\n");
        }
        printf("\n");

        printf("ETUnitSquare::pc_mtx_lower:\n");
        for (int i = 0; i < mtxr; i++) {
            for (int j = 0; j < mtxr; j++) {
                printf("%d ", pc_mtx_lower[i * mtxr + j]);
            }
            printf("\n");
        }
        printf("\n");

        int *pc_matrix = jerasure_matrix_multiply(pc_mtx_lower, pc_mtx_upper, mtxr, mtxr, mtxr, mtxr, 8);
        memcpy(_pc_matrix, pc_matrix, mtxr * mtxr * sizeof(int));

        free(pc_matrix_square);
        free(pc_mtx_upper);
        free(pc_mtx_lower);
        free(pc_matrix);
    } else {
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
    for (int ins_id = 0; ins_id < r; ins_id++) {
        for (int w = 0; w < _base_w; w++) {
            for (int node_id = 0; node_id < _c; node_id++) {
                printf("%d ", _layout[ins_id * _base_w + w][node_id]);
            }
            printf("\n");
        }
    }

    printf("ETUnit::uncoupled layout (before):\n");
    for (int ins_id = 0; ins_id < r; ins_id++) {
        for (int w = 0; w < _base_w; w++) {
            for (int node_id = 0; node_id < _c; node_id++) {
                printf("%d ", _uncoupled_layout[ins_id * _base_w + w][node_id]);
            }
            printf("\n");
        }
    }

    // for rows in pc_matrix with only one non-zero coef
    // replace the uncoupled layout item with layout item
    for (int ins_id = 0; ins_id < r; ins_id++) {
        for (int node_id = 0; node_id < _c; node_id++) {
            int packet_id = ins_id * _c + node_id;
            vector<int> non_zero_coefs;
            int *pc_matrix_row = &_pc_matrix[packet_id * mtxr];
            for (int i = 0; i < mtxr; i++) {
                if (pc_matrix_row[i] != 0) {
                    non_zero_coefs.push_back(pc_matrix_row[i]);
                }
            }

            if (non_zero_coefs.size() <= 1) {
                printf("packet_id: %d\n", packet_id);
                for (int w = 0; w < _base_w; w++) {
                    // reset uncoupled layout to layout
                    _uncoupled_layout[ins_id * _base_w + w][node_id] = _layout[ins_id * _base_w + w][node_id];
                }
            }
            
        }
        
    }

    // permutate uncoupled layout
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

    printf("ETUnit::gen_square_pc_matrix:\n");
    for (int i = 0; i < mtxr; i++) {
        for (int j = 0; j < mtxr; j++) {
            printf("%d ", pc_matrix[i * mtxr + j]);
        }
        printf("\n");
    }
    printf("\n");
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

void ETUnit::Encode(ECDAG *ecdag) {
    if (ecdag == NULL) {
        printf("error: invalid input ecdag\n");
        return;
    }

    int mtxr = _r * _c;

    if (_is_parity == false) {

        // for data nodes, compute (inverse permutated) uncoupled layout
        for (int ins_id = 0; ins_id < _r; ins_id++) {
            for (int node_id = 0; node_id < _c; node_id++) {
                for (int w = 0; w < _base_w; w++) {
                    int uc_id = _uncoupled_layout[ins_id * _base_w + w][node_id];
                    int packet_idx = ins_id * _c + node_id;
                    vector<int> layout_ids;
                    vector<int> coefs;
                    int *coefs_row = &_inv_pc_matrix[packet_idx * mtxr];

                    for (int i = 0; i < _c; i++) {
                        for (int j = 0; j < _r; j++) {
                            int coef = coefs_row[i * _r + j];
                            if (coef != 0) {
                                coefs.push_back(coef);
                                layout_ids.push_back(_layout[j * _base_w + w][i]);
                            }
                        }
                    }
                    
                    // encode
                    if (layout_ids.size() > 1) {
                        ecdag->Join(uc_id, layout_ids, coefs);

                        // BindX and BindY
                        ecdag->BindY(uc_id, layout_ids[0]); // bind to symbol 0
                    }
                }
            }
        }
    } else {
        int count = 0;
        // for parity, compute code layout with uncoupled layout
        for (int ins_id = 0; ins_id < _r; ins_id++) {
            for (int node_id = 0; node_id < _c; node_id++) {
                for (int w = 0; w < _base_w; w++) {
                    int coupled_id = _layout[ins_id * _base_w + w][node_id];
                    int packet_idx = ins_id * _c + node_id;
                    vector<int> uncoupled_ids;
                    vector<int> coefs;
                    int *coefs_row = &_pc_matrix[packet_idx * mtxr];

                    for (int i = 0; i < _c; i++) {
                        for (int j = 0; j < _r; j++) {
                            int coef = coefs_row[i * _r + j];
                            if (coef != 0) {
                                coefs.push_back(coef);
                                uncoupled_ids.push_back(_uncoupled_layout[j * _base_w + w][i]);
                            }
                        }
                    }
                    
                    if (uncoupled_ids.size() > 1) {
                        ecdag->Join(coupled_id, uncoupled_ids, coefs);
                    
                        // BindY
                        ecdag->BindY(coupled_id, uncoupled_ids[0]); // bind to symbol 0
                    }
                }
            }
        }

    }

}

void ETUnit::Decode(ECDAG *ecdag) {
    return;
}

vector<vector<int>> ETUnit::GetInvPermUCLayout() {
    return _inv_perm_uc_layout;
}