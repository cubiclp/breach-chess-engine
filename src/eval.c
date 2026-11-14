#include "eval.h"

#define FILE_A_BB 0x0101010101010101ULL

U64 file_masks[64];

static U64 mob_area[2];
static U64 passed_masks[2][64];
static U64 adjacent_files[8];

void init_eval()
{
    // init mob area
    U64 wp = bitboards[piece_P];
    U64 bp = bitboards[piece_p];

    // a8 = 0, so white pawns move toward lower indices
    U64 white_att = ((wp >> 7) & not_a_file) | ((wp >> 9) & not_h_file);
    U64 black_att = ((bp << 7) & not_h_file) | ((bp << 9) & not_a_file);

    mob_area[white] = ~(wp | black_att);
    mob_area[black] = ~(bp | white_att);

    // init file masks
    for (int sq = 0; sq < 64; sq++)
    {
        file_masks[sq] = FILE_A_BB << (sq & 7);
    }

    // init adjacent files
    for (int f = 0; f < 8; f++)
    {
        adjacent_files[f] = 0ULL;
        if (f > 0)
        {
            adjacent_files[f] |= FILE_A_BB << (f - 1);
        }
        if (f < 7)
        {
            adjacent_files[f] |= FILE_A_BB << (f + 1);
        }
    }

    // init passed pawn masks
    for (int sq = 0; sq < 64; sq++)
    {
        int f = sq % 8;
        int r = sq / 8;   // r = 0 is the 8th rank
        passed_masks[white][sq] = 0ULL;
        passed_masks[black][sq] = 0ULL;

        for (int rr = 0; rr < 8; rr++)
        {
            for (int ff = f - 1; ff <= f + 1; ff++)
            {
                if (ff < 0 || ff > 7) continue;
                if (rr < r)
                {
                    // white moves toward rank 8
                    passed_masks[white][sq] |= 1ULL << (rr * 8 + ff);
                }
                if (rr > r)
                {
                    // black moves toward rank 1
                    passed_masks[black][sq] |= 1ULL << (rr * 8 + ff);
                }
            }
        }
    }
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

// doubled and isolated pawns
static inline void eval_pawn_structure(int *mg_total, int *eg_total, int color)
{
    U64 own_pawns = bitboards[P_FOR_SIDE(color) + piece_P];

    for (int f = 0; f < 8; f++)
    {
        int n = count_bits(own_pawns & file_masks[f]);
        if (n == 0) continue;

        // doubled: penalize each pawn beyond the first on the file
        if (n > 1)
        {
            *mg_total -= mg_weights.doubled_pawns * (n - 1);
            *eg_total -= eg_weights.doubled_pawns * (n - 1);
        }

        // isolated: no friendly pawns on adjacent files
        if (!(own_pawns & adjacent_files[f]))
        {
            *mg_total -= mg_weights.isolated_pawns * n;
            *eg_total -= eg_weights.isolated_pawns * n;
        }
    }
}

// 0 = own back rank, 7 = promotion rank
static inline int relative_rank(int color, int sq)
{
    return color == white ? 7 - (sq >> 3) : (sq >> 3);
}

static inline void eval_passed_pawns(int *mg_total, int *eg_total, int color)
{
    int base = P_FOR_SIDE(color);
    U64 enemy_pawns = bitboards[P_FOR_SIDE(color ^ 1) + piece_P];

    U64 bb = bitboards[base + piece_P];
    while (bb)
    {
        int sq = get_ls1b_index(bb);

        // passed: no enemy pawn on its file or adjacent files in front of it
        if (!(passed_masks[color][sq] & enemy_pawns))
        {
            int r = relative_rank(color, sq);
            *mg_total += mg_weights.passed_pawns[r];
            *eg_total += eg_weights.passed_pawns[r];
        }

        pop_bit(bb, sq);
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

    eval_pawn_structure(&mg[us], &eg[us], us);
    eval_pawn_structure(&mg[them], &eg[them], them);

    eval_passed_pawns(&mg[us], &eg[us], us);
    eval_passed_pawns(&mg[them], &eg[them], them);

    // tapered eval
    int mg_eval = mg[us] - mg[them];
    int eg_eval = eg[us] - eg[them];
    int mg_phase = get_game_phase();
    if (mg_phase > 24) mg_phase = 24;
    int eg_phase = 24 - mg_phase;
    return (mg_eval * mg_phase + eg_eval * eg_phase) / 24;
}