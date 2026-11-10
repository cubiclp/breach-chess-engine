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
} weights;

extern weights mg_weights;
extern weights eg_weights;

void init_weights();
void parse_weights(char *raw);
void parse_weights_from_file(char *file);

#endif
