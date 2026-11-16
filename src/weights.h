#ifndef WEIGHTS_H
#define WEIGHTS_H

#include "types.h"

typedef struct weights {
    I16 material[6];
    I16 pst_pawn[64];
    I16 pst_knight[64];
    I16 pst_bishop[64];
    I16 pst_rook[64];
    I16 pst_queen[64];
    I16 pst_king[64];
    I16 mobility_base[3];
    I16 mobility[3];
    I16 bishop_pair;
    I16 doubled_pawns;
    I16 isolated_pawns;
    I16 passed_pawns[8];
    I16 king_attack_weight[4];
    I16 king_danger_div;
    I16 king_danger_max;
    I16 shield_near;
    I16 shield_far;
    I16 king_semi_open;
    I16 king_open;
    I16 passed_blocked;
    I16 passed_king_enemy;
    I16 passed_king_own;
} weights;

extern weights mg_weights;
extern weights eg_weights;

void init_weights();
void deinit_weights();
void parse_weights(char *raw);
void parse_weights_from_file(char *file);
void write_weights(char *file);

#endif
