#include "eval.h"

// -------------Evaluation -------- //

// material scrore

/*
    ♙ =   100   = ♙
    ♘ =   300   = ♙ * 3
    ♗ =   350   = ♙ * 3 + ♙ * 0.5
    ♖ =   500   = ♙ * 5
    ♕ =   1000  = ♙ * 10
    ♔ =   10000 = ♙ * 100

*/


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

// TODO: optimize and improve later
int evaluate(void)
{
    int mg[2] = {0, 0};
    int eg[2] = {0, 0};
    int us = side;
    int them = us ^ 1;

    eval_material_and_placement(&mg[us], &eg[us], us);
    eval_material_and_placement(&mg[them], &eg[them], them);

    // tapered eval
    int mg_eval = mg[us] - mg[them];
    int eg_eval = eg[us] - eg[them];
    int mg_phase = get_game_phase();
    if (mg_phase > 24) mg_phase = 24;
    int eg_phase = 24 - mg_phase;
    return (mg_eval * mg_phase + eg_eval * eg_phase) / 24;
}