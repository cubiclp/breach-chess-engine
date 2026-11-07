#ifndef ATTACKS_H
#define ATTACKS_H

#include "types.h"
#include "board.h"

// file masks
extern const U64 not_a_file;
extern const U64 not_h_file;
extern const U64 not_hg_file;
extern const U64 not_ab_file;

extern const int bishop_relevant_bits[64];
extern const int rook_relevant_bits[64];

extern U64 rook_magic_numbers[64];
extern U64 bishop_magic_numbers[64];

//[side][square]
extern U64 pawn_attacks[2][64];
extern U64 knight_attacks[64];
extern U64 king_attacks[64];

extern U64 bishop_masks[64];
extern U64 rook_masks[64];

extern U64 bishop_attacks[64][512];
extern U64 rook_attacks[64][4096];

U64 mask_pawn_attacks(int side, int square);
U64 mask_knight_attacks(int square);
U64 mask_king_attacks(int square);
U64 mask_bishop_attacks(int square);
U64 mask_rook_attacks(int square);
U64 bishop_attacks_irt(int square, U64 block);
U64 rook_attacks_irt(int square, U64 block);

void init_leaper_attacks(void);
U64 set_occupancy(int index, int bits_in_attack_mask, U64 attack_mask);
U64 find_magic_number(int square, int relevant_bits, int bishop);
void init_magic_numbers(void);
void init_sliders_attacks(int bishop);

static inline U64 get_bishop_attacks(int square, U64 occupancy)
{
    occupancy &= bishop_masks[square];
    occupancy *= bishop_magic_numbers[square];
    occupancy >>= 64 - bishop_relevant_bits[square];
    return bishop_attacks[square][occupancy];
}

static inline U64 get_rook_attacks(int square, U64 occupancy)
{
    occupancy &= rook_masks[square];
    occupancy *= rook_magic_numbers[square];
    occupancy >>= 64 - rook_relevant_bits[square];
    return rook_attacks[square][occupancy];
}

static inline U64 get_queen_attacks(int square, U64 occupancy)
{
    U64 queen_attacks = 0ULL;
    U64 bishop_occupancies = occupancy;
    U64 rook_occupancies = occupancy;

    bishop_occupancies &= bishop_masks[square];
    bishop_occupancies *= bishop_magic_numbers[square];
    bishop_occupancies >>= 64 - bishop_relevant_bits[square];
    queen_attacks = bishop_attacks[square][bishop_occupancies];

    rook_occupancies &= rook_masks[square];
    rook_occupancies *= rook_magic_numbers[square];
    rook_occupancies >>= 64 - rook_relevant_bits[square];
    queen_attacks |= rook_attacks[square][rook_occupancies];

    return queen_attacks;
}

static inline bool is_square_attacked(int square, int side)
{
    // for leaper peaces
    if (side == white && pawn_attacks[black][square] & bitboards[piece_P]) return true;
    if (side == black && pawn_attacks[white][square] & bitboards[piece_p]) return true;

    if (knight_attacks[square] & ((side == white) ? bitboards[piece_N] : bitboards[piece_n])) return true;

    if (king_attacks[square] & ((side == white) ? bitboards[piece_K] : bitboards[piece_k])) return true;

    //for sliders
    if (get_bishop_attacks(square, occupancies[both]) & ((side == white) ? bitboards[piece_B] : bitboards[piece_b]))
    {
        return true;
    }

    if (get_rook_attacks(square, occupancies[both]) & ((side == white) ? bitboards[piece_R] : bitboards[piece_r]))
    {
        return true;
    }

    if (get_queen_attacks(square, occupancies[both]) & ((side == white) ? bitboards[piece_Q] : bitboards[piece_q]))
    {
        return true;
    }

    return false;
}

void print_attacked_squares(int side);

#endif