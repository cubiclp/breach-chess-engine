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
} weights;

extern weights mg_weights;
extern weights eg_weights;

void init_weights();
void deinit_weights();
void parse_weights(char *raw);
void parse_weights_from_file(char *file);
void write_weights(char *file);

#endif
