#ifndef EVAL_H
#define EVAL_H

#include "types.h"
#include "board.h"
#include "attacks.h"
#include "weights.h"

void init_eval();

// static evaluation from the point of view of the side to move
int evaluate(void);

#endif