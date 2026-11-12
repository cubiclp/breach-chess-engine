#ifndef TT_H
#define TT_H

#include "types.h"

// hash table size (0x800000 entries * 16 bytes = 128MB)
#define hash_size 0x800000

#define no_hash_entry 100000

#define hash_flag_exact 0
#define hash_flag_alpha 1
#define hash_flag_beta 2

#define hash_mask (hash_size - 1)

// transposition table entry
//   hash_upper: upper 32 bits of the position key (lower bits are the table index)
//   score:      score relative to the node (mate scores are made path independent)
//   move:       best move found at this node (0 = none), used for move ordering
//   depth/flag: search depth and score type (exact / alpha / beta)
typedef struct {
    U32 hash_upper;
    int score;
    int move;
    uint8_t depth;
    uint8_t flag;
} tt;

_Static_assert(sizeof(tt) == 16, "tt entry must be 16 bytes");

void clear_hash_table(void);
int read_hash_entry(int alpha, int beta, int depth);
int read_hash_move(void);
void write_hash_entry(int score, int depth, int hash_flag, int best_move);

#endif
