#ifndef TYPES_H
#define TYPES_H

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <math.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/time.h>
#endif

typedef uint64_t U64;
typedef uint32_t U32;

// FEN debug positions
#define empty_board "8/8/8/8/8/8/8/8 w - - "
#define start_position "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1 "
#define tricky_position "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1 "
#define killer_position "rnbqkb1r/pp1p1pPp/8/2p1pP2/1P1P4/3P3P/P1P1P3/RNBQKBNR w KQkq e6 0 1"
#define cmk_position "r2q1rk1/ppp2ppp/2n1bn2/2b1p3/3pP3/3P1NPP/PPP1NPB1/R1BQ1RK1 b - - 0 9 "
#define repetitions "2r3k1/R7/8/1R6/8/8/P4KPP/8 w - - 0 40 "

// enum board squares
enum {
    a8, b8, c8, d8, e8, f8, g8, h8,
    a7, b7, c7, d7, e7, f7, g7, h7,
    a6, b6, c6, d6, e6, f6, g6, h6,
    a5, b5, c5, d5, e5, f5, g5, h5,
    a4, b4, c4, d4, e4, f4, g4, h4,
    a3, b3, c3, d3, e3, f3, g3, h3,
    a2, b2, c2, d2, e2, f2, g2, h2,
    a1, b1, c1, d1, e1, f1, g1, h1, no_sq
};

enum {
    white, black, both
};

enum {
    rook, bishop
};

enum {
    castle_none = 0, castle_wk = 1, castle_wq = 2, castle_bk = 4, castle_bq = 8
};

enum {
    piece_P, piece_N, piece_B, piece_R, piece_Q, piece_K, piece_p, piece_n, piece_b, piece_r, piece_q, piece_k
};

// --- Bit manipulations --- //

#define set_bit(bitboard, square) ((bitboard) |= (1ULL << (square)))
#define get_bit(bitboard, square) ((bitboard) & (1ULL << (square)))
#define pop_bit(bitboard, square) ((bitboard) &= ~(1ULL << (square)))

static inline int count_bits(U64 bitboard)
{
    return __builtin_popcountll(bitboard);
}

static inline int get_ls1b_index(U64 bitboard)
{
    if (bitboard == 0)
    {
        return -1;
    }
    return __builtin_ctzll(bitboard);
}

/*
    binary move bits                                hexidecmimal constants

0000 0000 0000 0000 0011 1111   source square       0x3f
0000 0000 0000 1111 1100 0000   target square       0xfc0
0000 0000 1111 0000 0000 0000   piece               0xf000
0000 1111 0000 0000 0000 0000   promoted piece      0xf0000
0001 0000 0000 0000 0000 0000   capture flag        0x100000
0010 0000 0000 0000 0000 0000   double push flag    0x200000
0100 0000 0000 0000 0000 0000   enpassant flag      0x400000
1000 0000 0000 0000 0000 0000   castling flag       0x800000
 */

#define encode_move(source, target, piece, promoted, capture, double_push, enpassant, castling) \
    ((source) |             \
    ((target) << 6) |         \
    ((piece) << 12) |         \
    ((promoted) << 16) |      \
    ((capture) << 20) |       \
    ((double_push) << 21) |   \
    ((enpassant) << 22) |     \
    ((castling) << 23))

#define get_move_source(move) ((move) & 0x3f)
#define get_move_target(move) (((move) & 0xfc0) >> 6)
#define get_move_piece(move) (((move) & 0xf000) >> 12)
#define get_move_promoted(move) (((move) & 0xf0000) >> 16)

#define get_move_capture(move) ((move) & 0x100000)
#define get_move_double_push(move) ((move) & 0x200000)
#define get_move_enpassant(move) ((move) & 0x400000)
#define get_move_castling(move) ((move) & 0x800000)

typedef struct {
    int moves[256];
    int count;
} moves;

static inline void add_move(moves *moves_list, int move)
{
    moves_list->moves[moves_list->count] = move;
    moves_list->count++;
}

// --- Time --- //

static inline U64 get_time_ms(void)
{
#ifdef _WIN32
    static LARGE_INTEGER freq;
    LARGE_INTEGER now;
    if (!freq.QuadPart) QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&now);
    return (U64)(now.QuadPart * 1000 / freq.QuadPart);
#else
    struct timeval time_value;
    gettimeofday(&time_value, NULL);
    return (U64)time_value.tv_sec * 1000 + time_value.tv_usec / 1000;
#endif
}

#endif