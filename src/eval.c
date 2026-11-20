#include "eval.h"

#include <assert.h>

#define FILE_A_BB 0x0101010101010101ULL

U64 file_masks[64];

static U64 mob_area[2];
static U64 passed_masks[2][64];
static U64 adjacent_files[8];

static int eval_king_sq[2];
static U64 enemy_zone[2]; // enemy_zone[c] = zone around the king that color c attacks
static int atk_units[2];
static int atk_count[2];
U64 king_zone_masks[2][64];   // zone around a king of the given color
static U64 shield_near[2][64];  // 3 squares directly in front of the king
static U64 shield_far[2][64];   // 3 squares two ranks in front

void init_eval()
{
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

    // init king safety
    for (int sq = 0; sq < 64; sq++)
    {
        int f = sq & 7;
        int r = sq >> 3;

        // zone: king moves + own square, extended one rank toward the enemy
        U64 z = king_attacks[sq] | (1ULL << sq);
        king_zone_masks[white][sq] = z | (z >> 8);   // white attacks toward rank 8 = lower indices
        king_zone_masks[black][sq] = z | (z << 8);

        // shield: clamp the file so the 3-wide window always fits on the board
        int cf = f < 1 ? 1 : (f > 6 ? 6 : f);
        shield_near[white][sq] = shield_far[white][sq] = 0ULL;
        shield_near[black][sq] = shield_far[black][sq] = 0ULL;

        for (int ff = cf - 1; ff <= cf + 1; ff++)
        {
            if (r >= 1) shield_near[white][sq] |= 1ULL << ((r - 1) * 8 + ff);
            if (r >= 2) shield_far[white][sq]  |= 1ULL << ((r - 2) * 8 + ff);
            if (r <= 6) shield_near[black][sq] |= 1ULL << ((r + 1) * 8 + ff);
            if (r <= 5) shield_far[black][sq]  |= 1ULL << ((r + 2) * 8 + ff);
        }
    }
}

static inline void add_king_attack(int color, int type, U64 attacks)
{
    U64 hit = attacks & enemy_zone[color];
    if (hit)
    {
        atk_units[color] += mg_weights.king_attack_weight[type] * count_bits(hit);
        atk_count[color]++;
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
    U64 att;

    bb = bitboards[base + piece_N];
    while (bb)
    {
        int sq = get_ls1b_index(bb);
        bb &= bb - 1;
        att = knight_attacks[sq];
        add_king_attack(color, 0, att);
        int m = count_bits(att & area);
        mg += (m - mg_weights.mobility_base[0]) * mg_weights.mobility[0];
        eg += (m - eg_weights.mobility_base[0]) * eg_weights.mobility[0];
    }

    bb = bitboards[base + piece_B];
    while (bb)
    {
        int sq = get_ls1b_index(bb);
        bb &= bb - 1;
        att = get_bishop_attacks(sq, occ);
        add_king_attack(color, 1, att);
        int m = count_bits(att & area);
        mg += (m - mg_weights.mobility_base[1]) * mg_weights.mobility[1];
        eg += (m - eg_weights.mobility_base[1]) * eg_weights.mobility[1];
    }

    bb = bitboards[base + piece_R];
    while (bb)
    {
        int sq = get_ls1b_index(bb);
        bb &= bb - 1;
        att = get_rook_attacks(sq, occ);
        add_king_attack(color, 2, att);
        int m = count_bits(att & area);
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

static inline void eval_passed_pawns(int *mg_total, int *eg_total, int color, U64 *passed_out)
{
    int base = P_FOR_SIDE(color);
    U64 enemy_pawns = bitboards[P_FOR_SIDE(color ^ 1) + piece_P];

    U64 passed = 0ULL;
    U64 bb = bitboards[base + piece_P];
    while (bb)
    {
        int sq = get_ls1b_index(bb);

        // passed: no enemy pawn on its file or adjacent files in front of it
        if (!(passed_masks[color][sq] & enemy_pawns))
        {
            passed |= 1ULL << sq;

            int r = relative_rank(color, sq);
            *mg_total += mg_weights.passed_pawns[r];
            *eg_total += eg_weights.passed_pawns[r];
        }

        pop_bit(bb, sq);
    }

    *passed_out = passed;
}

typedef struct {
    U64 wp, bp;      // key: exact pawn bitboards, so no collisions possible
    int mg, eg;      // pawn-structure + passed score, white minus black
    U64 att[2];      // pawn attacks per color, reused for the mobility area
    U64 passed[2];   // passed pawns per color
} PawnEntry;

#define PAWN_TABLE_BITS 14
#define PAWN_TABLE_SIZE (1 << PAWN_TABLE_BITS) // 16k entries is plenty
static PawnEntry pawn_table[PAWN_TABLE_SIZE];

static inline PawnEntry *probe_pawns(void)
{
    U64 wp = bitboards[piece_P], bp = bitboards[piece_p];

    U64 h = (wp * 0x9E3779B97F4A7C15ULL) ^ (bp * 0xC2B2AE3D27D4EB4FULL);
    PawnEntry *e = &pawn_table[h >> (64 - PAWN_TABLE_BITS)];

    if (e->wp == wp && e->bp == bp)
    {
        return e; // hit
    }

    int mgw = 0, egw = 0, mgb = 0, egb = 0;
    U64 pw = 0ULL, pb = 0ULL;
    eval_pawn_structure(&mgw, &egw, white);
    eval_passed_pawns(&mgw, &egw, white, &pw);
    eval_pawn_structure(&mgb, &egb, black);
    eval_passed_pawns(&mgb, &egb, black, &pb);

    e->wp = wp;  e->bp = bp;
    e->mg = mgw - mgb;
    e->eg = egw - egb;
    e->passed[white] = pw;
    e->passed[black] = pb;
    e->att[white] = ((wp >> 7) & not_a_file) | ((wp >> 9) & not_h_file);
    e->att[black] = ((bp << 7) & not_h_file) | ((bp << 9) & not_a_file);
    return e;
}

static inline void eval_king_safety(int *mg_total, int *eg_total, int color)
{
    int enemy = color ^ 1;
    int ksq = eval_king_sq[color];
    U64 own_pawns = bitboards[P_FOR_SIDE(color) + piece_P];
    U64 enemy_pawns = bitboards[P_FOR_SIDE(enemy) + piece_P];

    // deal with the queen since we dont do that in mobility
    U64 q = bitboards[Q_FOR_SIDE(enemy)];
    int has_queen = q != 0;
    while (q)
    {
        int sq = get_ls1b_index(q);
        q &= q - 1;
        add_king_attack(enemy, 3, get_queen_attacks(sq, occupancies[both]));
    }

    // king zone attack checking
    if (atk_count[enemy] >= 2)
    {
        int units = atk_units[enemy];
        if (!has_queen)
        {
            units >>= 1;
        }
        int danger = units * units / mg_weights.king_danger_div;
        if (danger > mg_weights.king_danger_max)
        {
            danger = mg_weights.king_danger_max;
        }
        *mg_total -= danger;
    }

    // pawn shield
    *mg_total += mg_weights.shield_near * count_bits(own_pawns & shield_near[color][ksq]);
    *mg_total += mg_weights.shield_far * count_bits(own_pawns & shield_far[color][ksq]);

    // open and semi open files next to king
    int file = ksq & 7;
    int f = file > 0 ? file - 1 : 0;
    int max = file < 7 ? file + 1 : 7;
    for (int ff = f; ff <= max; ff++)
    {
        U64 file = FILE_A_BB << ff;
        if (!(own_pawns & file))
        {
            *mg_total -= enemy_pawns & file ? mg_weights.king_semi_open : mg_weights.king_open;
        }
    }
}

static inline int king_dist(int a, int b)
{
    int df = abs((a & 7) - (b & 7));
    int dr = abs((a >> 3) - (b >> 3));
    return df > dr ? df : dr;
}

static inline void eval_passed_pieces(int *mg_total, int *eg_total, int color, U64 passed)
{
    int enemy = color ^ 1;

    while (passed)
    {
        int sq = get_ls1b_index(passed);
        passed &= passed - 1;

        int rf   = relative_rank(color, sq) - 1;          // 0..5
        int stop = (color == white) ? sq - 8 : sq + 8;    // square in front of the pawn

        // enemy piece sitting on the stop square
        if (occupancies[enemy] & (1ULL << stop))
        {
            *mg_total -= rf * mg_weights.passed_blocked;
            *eg_total -= rf * eg_weights.passed_blocked;
        }

        // enemy king far away is good, own king close is good (endgame only)
        *eg_total += rf * (king_dist(eval_king_sq[enemy], stop) * eg_weights.passed_king_enemy
                         - king_dist(eval_king_sq[color], stop) * eg_weights.passed_king_own);
    }
}

static inline void eval_prepare()
{
    int wk = get_ls1b_index(bitboards[piece_K]);
    int bk = get_ls1b_index(bitboards[piece_k]);
    eval_king_sq[white] = wk;
    eval_king_sq[black] = bk;
    enemy_zone[white] = king_zone_masks[black][bk];
    enemy_zone[black] = king_zone_masks[white][wk];

    atk_units[white] = atk_units[black] = 0;
    atk_count[white] = atk_count[black] = 0;
}

// TODO: optimize and improve later
int evaluate(void)
{
    int mg[2] = {0, 0};
    int eg[2] = {0, 0};
    int us = side;
    int them = us ^ 1;

    PawnEntry *pe = probe_pawns();
    mob_area[white] = ~(bitboards[piece_P] | pe->att[black]);
    mob_area[black] = ~(bitboards[piece_p] | pe->att[white]);

    eval_prepare();

    eval_material_and_placement(&mg[us], &eg[us], us);
    eval_material_and_placement(&mg[them], &eg[them], them);

    eval_mobility(&mg[us], &eg[us], us);
    eval_mobility(&mg[them], &eg[them], them);

    eval_bishop_pair(&mg[us], &eg[us], us);
    eval_bishop_pair(&mg[them], &eg[them], them);

    mg[us] += (us == white) ? pe->mg : -pe->mg;   // entry is white-relative
    eg[us] += (us == white) ? pe->eg : -pe->eg;

    eval_king_safety(&mg[us], &eg[us], us);
    eval_king_safety(&mg[them], &eg[them], them);

    eval_passed_pieces(&mg[us], &eg[us], us, pe->passed[us]);
    eval_passed_pieces(&mg[them], &eg[them], them, pe->passed[them]);

    // tapered eval
    int mg_eval = mg[us] - mg[them];
    int eg_eval = eg[us] - eg[them];
    int mg_phase = get_game_phase();
    if (mg_phase > 24) mg_phase = 24;
    int eg_phase = 24 - mg_phase;
    return (mg_eval * mg_phase + eg_eval * eg_phase) / 24;
}