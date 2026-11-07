#ifndef MOVEGEN_H
#define MOVEGEN_H

#include "types.h"
#include "board.h"
#include "attacks.h"

enum { all_moves, only_captures };

// for uci purpose
void print_move(int move);

// for debug purposes
void print_move_list(moves *move_list);

// make a move on the board, returns 0 (and restores the board) if it leaves the king in check
int make_move(int move);

void generate_moves(moves *move_list);

// generate only capture moves (for quiescence search)
void generate_captures(moves *move_list);

// perft
void perft_driver(int depth);
void perft_test(int depth);

#endif