#ifndef BOARD_H
#define BOARD_H

#include "types.h"

// lookup data (defined in board.c)
extern const char *square_to_coordinates[];
extern const char ascii_pieces[12];
extern char *unicode_pieces[12];
extern int char_pieces[];
extern char promoted_pieces[];

// position state
extern U64 bitboards[12];
extern U64 occupancies[3];
extern int side;
extern int enpassant;
extern int castle;

// "almost" unique position identifier aka hash key or position key
extern U64 hash_key;

extern U64 repetition_table[1000];
extern int repetition_index;

// ply = halfmove, full move = every side made 1 ply
extern int ply;

// node counter shared by perft and search
extern U64 nodes;

// make a move on the board, returns 0 (and restores the board) if it leaves the king in check
int make_move(int move);

// random numbers
extern U32 random_state;
U32 get_random_number_U32(void);
U64 get_random_number_U64(void);
U64 generate_magic_number_candidate(void);

// zobrist keys
extern U64 piece_keys[12][64];
extern U64 enpassant_keys[64];
extern U64 castle_keys[16];
extern U64 side_key;

void init_random_keys(void);
U64 generate_hash_keys(void);

// input / output
void print_bitboard(U64 bitboard);
void print_board(void);
void parse_fen(char *fen);

#define copy_board()                                                        \
    U64 bitboards_copy[12], occupancies_copy[3];                            \
    int side_copy, enpassant_copy, castle_copy;                             \
    memcpy(bitboards_copy, bitboards, 96);                                  \
    memcpy(occupancies_copy, occupancies, 24);                              \
    side_copy = side, enpassant_copy = enpassant, castle_copy = castle;     \
    U64 hash_key_copy = hash_key;

#define restore_board()                                                     \
    memcpy(bitboards, bitboards_copy, 96);                                  \
    memcpy(occupancies, occupancies_copy, 24);                              \
    side = side_copy, enpassant = enpassant_copy, castle = castle_copy;     \
    hash_key = hash_key_copy;

#endif