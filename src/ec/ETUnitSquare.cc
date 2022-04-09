/**
 * @file ETUnitSquare.cc
 * @author Keyun Cheng (kycheng@cse.cuhk.edu.hk)
 * @brief 
 * @version 0.1
 * @date 2022-04-01
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#include "ETUnitSquare.hh"


ETUnitSquare::ETUnitSquare(int r, int base_w, vector<vector<int>> uncoupled_layout, vector<vector<int>> layout, int e)
{
    _r = r;
    _base_w = base_w;

    if (uncoupled_layout.size() != base_w * r || layout.size() != base_w * r) {
        printf("error: invalid arguments\n");
        return;
    }
    
    if ((uncoupled_layout[0].size() != r) || (layout[0].size() != r)) {
        printf("error: invalid arguments\n");
        return;
    }

    _uncoupled_layout = uncoupled_layout;
    _layout = layout;
    _e = e;

    int mtxr = _r * _r;
    _encode_matrix = (int *) malloc(mtxr * mtxr * sizeof(int));
    memset(_encode_matrix, 0, mtxr * mtxr * sizeof(int));


    printf("ETUnitSquare::uncoupled layout:\n");
    for (int ins_id = 0; ins_id < r; ins_id++) {
        for (int w = 0; w < _base_w; w++) {
            for (int node_id = 0; node_id < r; node_id++) {
                printf("%d ", _uncoupled_layout[ins_id * _base_w + w][node_id]);
            }
            printf("\n");
        }
    }

    printf("ETUnitSquare::layout:\n");
    for (int ins_id = 0; ins_id < r; ins_id++) {
        for (int w = 0; w < _base_w; w++) {
            for (int node_id = 0; node_id < r; node_id++) {
                printf("%d ", _layout[ins_id * _base_w + w][node_id]);
            }
            printf("\n");
        }
    }

    // set pairwise coupling matrix
    for (int i = 0; i < _r; i++) {
        for (int j = 0; j < _r; j++) {
            int pkt_idx = i * _r + j;
            if (i == j) {
                _encode_matrix[pkt_idx * mtxr + i * _r + i] = 1; // symmetric row
            } else {
                _encode_matrix[pkt_idx * mtxr + j * _r + i] = _eta; // eta
                if (i < j) {
                    _encode_matrix[pkt_idx * mtxr + i * _r + j] = 1; // theta
                } else {
                    _encode_matrix[pkt_idx * mtxr + i * _r + j] = _e; // theta
                }
            }
        }
    }

    printf("ETUnitSquare::encode_matrix:\n");
    for (int i = 0; i < mtxr; i++) {
        for (int j = 0; j < mtxr; j++) {
            printf("%d ", _encode_matrix[i * mtxr + j]);
        }
        printf("\n");
    }
    printf("\n");
}

ETUnitSquare::~ETUnitSquare()
{
    free(_encode_matrix);
}

void ETUnitSquare::Encode(ECDAG *ecdag) {
    if (ecdag == NULL) {
        printf("error: invalid input ecdag\n");
        return;
    }

    int mtxr = _r * _r;

    for (int i = 0; i < _r; i++) {
        for (int j = 0; j < _r; j++) {
            if (i == j) {
                // perform no coupling
                for (int w = 0; w < _base_w; w++) {
                    int uncoupled_id = _uncoupled_layout[i * _base_w + w][j];
                    int coupled_id = _layout[i * _base_w + w][j];
                    ecdag->Join(coupled_id, {uncoupled_id}, {1});
                }
            } else {
                for (int w = 0; w < _base_w; w++) {
                    int pkt_id = i * _r + j;
                    int pkt_id_pair = j * _r + i;

                    int uncoupled = _uncoupled_layout[i * _base_w + w][j];
                    int uncoupled_pair = _uncoupled_layout[j * _base_w + w][i];
                    
                    int coupled_id = _layout[i * _base_w + w][j];

                    int coef_a;
                    int coef_b;

                    if (pkt_id < pkt_id_pair) {
                        coef_a = _encode_matrix[pkt_id * mtxr + pkt_id];
                        coef_b = _encode_matrix[pkt_id * mtxr + pkt_id_pair];
                        ecdag->Join(coupled_id, {uncoupled, uncoupled_pair}, {coef_a, coef_b});
                    } else {
                        coef_a = _encode_matrix[pkt_id * mtxr + pkt_id_pair];
                        coef_b = _encode_matrix[pkt_id * mtxr + pkt_id];
                        ecdag->Join(coupled_id, {uncoupled_pair, uncoupled}, {coef_a, coef_b});
                    }
                }
            }
        }
    }
}

void ETUnitSquare::Decode(ECDAG *ecdag) {
    return;
}