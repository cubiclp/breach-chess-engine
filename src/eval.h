#ifndef EVAL_H
#define EVAL_H

#include "types.h"
#include "board.h"
#include "attacks.h"
#include "weights.h"

void eval_set_pawn_cache(int on);

void init_eval();

// static evaluation from the point of view of the side to move
int evaluate(void);

#endif