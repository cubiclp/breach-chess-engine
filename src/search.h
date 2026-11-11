#ifndef SEARCH_H
#define SEARCH_H

#include "types.h"
#include "board.h"

// depth of the iteration currently being searched (used by communicate() for time checks)
extern int current_search_depth;

// builds the late move reduction table, call once at startup
void init_search_tables(void);

void clear_hash_table(void);

void print_move_scores(moves *move_list, int use_see);

void search_position(int depth);

#endif