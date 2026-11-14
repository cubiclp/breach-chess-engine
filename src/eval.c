#include "eval.h"

#define NOT_A_FILE 0xFEFEFEFEFEFEFEFEULL
#define NOT_H_FILE 0x7F7F7F7F7F7F7F7FULL

static U64 mob_area[2];

void init_eval()
{

    // init mob area
    U64 wp = bitboards[piece_P];
    U64 bp = bitboards[piece_p];

    // a8 = 0, so white pawns move toward lower indices
    U64 white_att = ((wp >> 7) & NOT_A_FILE) | ((wp >> 9) & NOT_H_FILE);
    U64 black_att = ((bp << 7) & NOT_H_FILE) | ((bp << 9) & NOT_A_FILE);

    mob_area[white] = ~(wp | black_att);
    mob_area[black] = ~(bp | white_att);
}

static int gamephaseInc[12] = {0, 1, 1, 2, 4, 0,   0, 1, 1, 2, 4, 0};   // P N B R Q K | p n b r q k


static inline int get_game_phase()
{
    int knight = count_bits(bitboards[piece_N] | bitboards[piece_n]);
    int bishop = count_bits(bitboards[piece_B] | bitboards[piece_b]);
    int rook = count_bits(bitboards[piece_R] | bitboards[piece_r]);
    int queen = count_bits(bitboards[piece_Q] | bitboards[piece_q]);

    knight *= gamephaseInc[piece_N];
    bishop *= gamephaseInc[piece_B];
    rook *= gamephaseInc[piece_R];
    queen *= gamephaseInc[piece_Q];

    return knight + bishop + rook + queen;
}

static inline void eval_material_and_placement(int *mg_total, int *eg_total, int color)
{
    int base = P_FOR_SIDE(color);

    int mg_score = 0;
    int eg_score = 0;

    I16 *mg_pst[6] = {
        mg_weights.pst_pawn,
        mg_weights.pst_knight,
        mg_weights.pst_bishop,
        mg_weights.pst_rook,
        mg_weights.pst_queen,
        mg_weights.pst_king
    };

    I16 *eg_pst[6] = {
        eg_weights.pst_pawn,
        eg_weights.pst_knight,
        eg_weights.pst_bishop,
        eg_weights.pst_rook,
        eg_weights.pst_queen,
        eg_weights.pst_king
    };

    for (int piece = piece_P; piece <= piece_K; piece++)
    {
        U64 bb = bitboards[base + piece];

        int mg_placement = 0;
        int eg_placement = 0;
        int count = 0;

        while (bb)
        {
            int sq = get_ls1b_index(bb);
            // mirror the table for black
            int index = color == white ? sq : (sq ^ 56);

            mg_placement += mg_pst[piece][index];
            eg_placement += eg_pst[piece][index];

            count++;

            pop_bit(bb, sq);
        }

        mg_score += count * mg_weights.material[piece] + mg_placement;
        eg_score += count * eg_weights.material[piece] + eg_placement;
    }

    *mg_total += mg_score;
    *eg_total += eg_score;
}

static inline void eval_mobility(int *mg_total, int *eg_total, int color)
{
    int base = P_FOR_SIDE(color);
    U64 area = mob_area[color];
    U64 occ  = occupancies[both];

    int mg = 0;
    int eg = 0;
    U64 bb;

    bb = bitboards[base + piece_N];
    while (bb)
    {
        int sq = get_ls1b_index(bb);
        bb &= bb - 1;
        int m = count_bits(knight_attacks[sq] & area);
        mg += (m - mg_weights.mobility_base[0]) * mg_weights.mobility[0];
        eg += (m - eg_weights.mobility_base[0]) * eg_weights.mobility[0];
    }

    bb = bitboards[base + piece_B];
    while (bb)
    {
        int sq = get_ls1b_index(bb);
        bb &= bb - 1;
        int m = count_bits(get_bishop_attacks(sq, occ) & area);
        mg += (m - mg_weights.mobility_base[1]) * mg_weights.mobility[1];
        eg += (m - eg_weights.mobility_base[1]) * eg_weights.mobility[1];
    }

    bb = bitboards[base + piece_R];
    while (bb)
    {
        int sq = get_ls1b_index(bb);
        bb &= bb - 1;
        int m = count_bits(get_rook_attacks(sq, occ) & area);
        mg += (m - mg_weights.mobility_base[2]) * mg_weights.mobility[2];
        eg += (m - eg_weights.mobility_base[2]) * eg_weights.mobility[2];
    }

    *mg_total += mg;
    *eg_total += eg;
}

static inline void eval_bishop_pair(int *mg_total, int *eg_total, int color)
{
    if (count_bits(bitboards[B_FOR_SIDE(color)]) >= 2)
    {
        *mg_total += mg_weights.bishop_pair;
        *eg_total += eg_weights.bishop_pair;
    }
}

// TODO: optimize and improve later
int evaluate(void)
{
    int mg[2] = {0, 0};
    int eg[2] = {0, 0};
    int us = side;
    int them = us ^ 1;

    eval_material_and_placement(&mg[us], &eg[us], us);
    eval_material_and_placement(&mg[them], &eg[them], them);

    eval_mobility(&mg[us], &eg[us], us);
    eval_mobility(&mg[them], &eg[them], them);

    eval_bishop_pair(&mg[us], &eg[us], us);
    eval_bishop_pair(&mg[them], &eg[them], them);

    // tapered eval
    int mg_eval = mg[us] - mg[them];
    int eg_eval = eg[us] - eg[them];
    int mg_phase = get_game_phase();
    if (mg_phase > 24) mg_phase = 24;
    int eg_phase = 24 - mg_phase;
    return (mg_eval * mg_phase + eg_eval * eg_phase) / 24;
}