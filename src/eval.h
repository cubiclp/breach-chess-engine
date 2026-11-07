#ifndef EVAL_H
#define EVAL_H

#include "types.h"
#include "board.h"
#include "attacks.h"

// builds the combined material + piece-square tables, call once at startup
void init_tables(void);

// static evaluation from the point of view of the side to move
int evaluate(void);

#endif