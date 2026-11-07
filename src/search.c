#include "search.h"
#include "attacks.h"
#include "movegen.h"
#include "eval.h"
#include "uci.h"

// ------------- Search ------------ //

#define infinity 50000
#define mate_value 49000
#define mate_score 48000

int current_search_depth;

// most valuable victim & less valuable attacker

/*

    (Victims) Pawn Knight Bishop   Rook  Queen   King
  (Attackers)
        Pawn   105    205    305    405    505    605
      Knight   104    204    304    404    504    604
      Bishop   103    203    303    403    503    603
        Rook   102    202    302    402    502    602
       Queen   101    201    301    401    501    601
        King   100    200    300    400    500    600

*/

// MVV LVA [attacker][victim]
static int mvv_lva[12][12] = {
    105, 205, 305, 405, 505, 605,  105, 205, 305, 405, 505, 605,
   104, 204, 304, 404, 504, 604,  104, 204, 304, 404, 504, 604,
   103, 203, 303, 403, 503, 603,  103, 203, 303, 403, 503, 603,
   102, 202, 302, 402, 502, 602,  102, 202, 302, 402, 502, 602,
   101, 201, 301, 401, 501, 601,  101, 201, 301, 401, 501, 601,
   100, 200, 300, 400, 500, 600,  100, 200, 300, 400, 500, 600,

   105, 205, 305, 405, 505, 605,  105, 205, 305, 405, 505, 605,
   104, 204, 304, 404, 504, 604,  104, 204, 304, 404, 504, 604,
   103, 203, 303, 403, 503, 603,  103, 203, 303, 403, 503, 603,
   102, 202, 302, 402, 502, 602,  102, 202, 302, 402, 502, 602,
   101, 201, 301, 401, 501, 601,  101, 201, 301, 401, 501, 601,
   100, 200, 300, 400, 500, 600,  100, 200, 300, 400, 500, 600
};

#define max_ply 64

// killer moves [id][ply]
static int killer_moves[2][max_ply];

// history moves [piece][board square]
static int history_moves[12][64];

/*
      ================================
            Triangular PV table
      --------------------------------
        PV line: e2e4 e7e5 g1f3 b8c6
      ================================

           0    1    2    3    4    5

      0    m1   m2   m3   m4   m5   m6

      1    0    m2   m3   m4   m5   m6

      2    0    0    m3   m4   m5   m6

      3    0    0    0    m4   m5   m6

      4    0    0    0    0    m5   m6

      5    0    0    0    0    0    m6
*/

static int pv_length[max_ply];

static int pv_table[max_ply][max_ply];

static int follow_pv, score_pv;

// --------- Transposition table ---------- //

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

static tt hash_table[hash_size];

void clear_hash_table(void)
{
    memset(hash_table, 0, sizeof(hash_table));
}

static inline int read_hash_entry(int alpha, int beta, int depth)
{
    tt *hash_entry = &hash_table[hash_key & hash_mask];

    if (hash_entry->hash_upper == (U32)(hash_key >> 32))
    {
        if (hash_entry->depth >= depth)
        {
            int score = hash_entry->score;

            // retrieve score independent from the actual path
            // from root node (position) to current node (position)
            if (score < -mate_score) score += ply;
            if (score > mate_score) score -= ply;

            // match PV
            if (hash_entry->flag == hash_flag_exact)
            {
                return score;
            }
            // match fail low
            if ((hash_entry->flag == hash_flag_alpha) && (score <= alpha))
            {
                return alpha;
            }
            // match fail high
            if ((hash_entry->flag == hash_flag_beta) && (score >= beta))
            {
                return beta;
            }
        }
    }

    // TODO: bug in tutorial code? uncomment this
    return no_hash_entry;
}

// read only the stored best move of the current position (0 if none).
// Independent of depth and bounds: even a shallow entry's move is a good guess
// for move ordering. A hash collision can only yield a move that is not in the
// generated move list, in which case it is simply never matched.
static inline int read_hash_move(void)
{
    tt *hash_entry = &hash_table[hash_key & hash_mask];

    if (hash_entry->hash_upper == (U32)(hash_key >> 32))
    {
        return hash_entry->move;
    }

    return 0;
}

static inline void write_hash_entry(int score, int depth, int hash_flag, int best_move)
{
    tt *hash_entry = &hash_table[hash_key & hash_mask];

    // store score independent from actual path from root position to current position
    // from root node (position) to current node (position)
    if (score < -mate_score) score -= ply;
    if (score > mate_score) score += ply;

    U32 hash_upper = (U32)(hash_key >> 32);

    // no move found at this node (fail low): keep the old move if the entry
    // belongs to the same position, so we don't throw away a useful ordering hint
    if (best_move == 0 && hash_entry->hash_upper == hash_upper)
    {
        best_move = hash_entry->move;
    }

    // write hash entry data
    hash_entry->hash_upper = hash_upper;
    hash_entry->score = score;
    hash_entry->move = best_move;
    hash_entry->flag = (uint8_t) hash_flag;
    hash_entry->depth = (uint8_t) depth;
}

static inline void enable_pv_scoring(moves *move_list)
{
    follow_pv = 0;

    for (int count = 0; count < move_list->count; count++)
    {
        if (pv_table[0][ply] == move_list->moves[count])
        {
            score_pv = 1;
            follow_pv = 1;
        }
    }
}

// ---------- Static exchange evaluation ---------- //

// simple material values for SEE (indexed by piece type: P N B R Q K)
// the king is huge so that a king capture into a defended square always looks losing
static const int see_values[6] = { 100, 300, 300, 500, 900, 20000 };

// all pieces (both colors) attacking `square`, given an occupancy
static inline U64 attackers_to_square(int square, U64 occ)
{
    U64 diagonal_sliders = bitboards[piece_B] | bitboards[piece_b] | bitboards[piece_Q] | bitboards[piece_q];
    U64 straight_sliders = bitboards[piece_R] | bitboards[piece_r] | bitboards[piece_Q] | bitboards[piece_q];

    return (pawn_attacks[black][square] & bitboards[piece_P])    // white pawns attacking square
         | (pawn_attacks[white][square] & bitboards[piece_p])    // black pawns attacking square
         | (knight_attacks[square] & (bitboards[piece_N] | bitboards[piece_n]))
         | (king_attacks[square]   & (bitboards[piece_K] | bitboards[piece_k]))
         | (get_bishop_attacks(square, occ) & diagonal_sliders)
         | (get_rook_attacks(square, occ)   & straight_sliders);
}

// static exchange evaluation: net material result (centipawns, from the mover's view)
// of making `move` and then both sides optimally recapturing on the target square.
// Does not modify any global state.
static inline int see(int move)
{
    int from = get_move_source(move);
    int to = get_move_target(move);
    int piece = get_move_piece(move);
    int promoted = get_move_promoted(move);

    int us = (piece <= piece_K) ? white : black;
    int them = us ^ 1;

    U64 occ = occupancies[both];

    // ---- value of what the move captures ----
    int victim_value = 0;

    if (get_move_enpassant(move))
    {
        victim_value = see_values[0];
        // the captured pawn is not on the target square: remove it from the occupancy
        pop_bit(occ, (us == white) ? to + 8 : to - 8);
    }
    else if (get_bit(occupancies[them], to))
    {
        for (int p = 0; p < 6; p++)
        {
            if (get_bit(bitboards[p + them * 6], to))
            {
                victim_value = see_values[p];
                break;
            }
        }
    }

    int gain[33];
    int d = 0;
    gain[0] = victim_value;

    // value of the piece that now stands on the target square (what gets captured next)
    int piece_on_square = see_values[piece % 6];

    if (promoted)
    {
        gain[0] += see_values[promoted % 6] - see_values[0];
        piece_on_square = see_values[promoted % 6];
    }

    // take the mover off the board so sliders behind it are revealed
    pop_bit(occ, from);

    U64 diagonal_sliders = bitboards[piece_B] | bitboards[piece_b] | bitboards[piece_Q] | bitboards[piece_q];
    U64 straight_sliders = bitboards[piece_R] | bitboards[piece_r] | bitboards[piece_Q] | bitboards[piece_q];

    U64 attackers = attackers_to_square(to, occ) & occ;

    int stm = them;   // side to recapture next

    while (1)
    {
        // least valuable attacker of the side to move
        U64 stm_attackers = attackers & occupancies[stm];
        if (!stm_attackers)
        {
            break;
        }

        int attacker_type = 0;
        U64 attacker_set = 0ULL;
        for (; attacker_type < 6; attacker_type++)
        {
            attacker_set = stm_attackers & bitboards[attacker_type + stm * 6];
            if (attacker_set)
            {
                break;
            }
        }

        // speculative score if stm captures here: it wins the piece on the square,
        // minus whatever the other side had gained so far
        d++;
        gain[d] = piece_on_square - gain[d - 1];
        piece_on_square = see_values[attacker_type];

        // remove the attacker from the board
        pop_bit(occ, get_ls1b_index(attacker_set));

        // reveal sliders that were standing behind it (x-rays)
        if (attacker_type == piece_P || attacker_type == piece_B || attacker_type == piece_Q)
        {
            attackers |= get_bishop_attacks(to, occ) & diagonal_sliders;
        }
        if (attacker_type == piece_R || attacker_type == piece_Q)
        {
            attackers |= get_rook_attacks(to, occ) & straight_sliders;
        }
        attackers &= occ;

        stm ^= 1;
    }

    // back-propagate: at every step the side to move may stand pat instead of recapturing
    while (d > 0)
    {
        int stand_pat = -gain[d - 1];
        gain[d - 1] = -((stand_pat > gain[d]) ? stand_pat : gain[d]);
        d--;
    }

    return gain[0];
}

// convenience wrapper: is the exchange worth at least `threshold`?
static inline int see_ge(int move, int threshold)
{
    return see(move) >= threshold;
}

/*  =======================
         Move ordering
    =======================

    1. PV move
    2. Transposition table move
    3. Captures in MVV/LVA
    4. 1st killer move
    5. 2nd killer move
    6. History moves
    7. Unsorted moves
*/

static inline int score_move(int move, int tt_move)
{
    if (score_pv)
    {
        if (pv_table[0][ply] == move)
        {
            score_pv = 0;
            return 20000;
        }
    }

    if (tt_move && move == tt_move)
    {
        return 15000;
    }

    // score capture move
    if (get_move_capture(move))
    {
        int target_piece = piece_P;
        int start_piece, end_piece;
        if (side == white)
        {
            start_piece = piece_p;
            end_piece = piece_k;
        }
        else
        {
            start_piece = piece_P;
            end_piece = piece_K;
        }
        for (int bb_piece = start_piece; bb_piece <= end_piece; bb_piece++)
        {
            if (get_bit(bitboards[bb_piece], get_move_target(move)))
            {
                target_piece = bb_piece;
                break;
            }
        }
        return mvv_lva[get_move_piece(move)][target_piece] + 10000;
    }
    // score quiet move
    else
    {
        // score 1st killer move
        if (killer_moves[0][ply] == move)
        {
            return 9000;
        }
        else if (killer_moves[1][ply] == move)
        {
            return 8000;
        }
        // history moves
        else
        {
            return history_moves[get_move_piece(move)][get_move_target(move)];
        }
    }

    return 0;
}

static inline void shell_sort(moves *move_list, int move_scores[])
{
    int count = move_list->count;

    // Start with a large gap, then reduce it step by step
    for (int gap = count / 2; gap > 0; gap /= 2) {

        // Perform a "gapped" insertion sort for this gap size
        for (int i = gap; i < count; i++) {

            // Current element to be placed correctly
            int temp_move = move_list->moves[i];
            int temp_score = move_scores[i];
            int j = i;

            // Shift smaller elements right so temp lands after all larger scores (descending)
            while (j >= gap && move_scores[j - gap] < temp_score) {
                move_list->moves[j] = move_list->moves[j - gap];
                move_scores[j] = move_scores[j - gap];
                j -= gap;
            }

            // Place temp in its correct location
            move_list->moves[j] = temp_move;
            move_scores[j] = temp_score;
        }
    }
}

// TODO: optimize this later, probably optimize the sorting part
// tt_move: best move from the transposition table for this position (0 = none)
static inline void sort_moves(moves *move_list, int tt_move)
{
    int move_scores[move_list->count];
    for (int count = 0; count < move_list->count; count++)
    {
        move_scores[count] = score_move(move_list->moves[count], tt_move);
    }
    // sort the moves
    shell_sort(move_list, move_scores);
}

void print_move_scores(moves *move_list)
{
    printf("  Move scores: \n\n");
    for (int count = 0; count < move_list->count; count++)
    {
        printf("  move: ");
        print_move(move_list->moves[count]);
        printf(" score: %d\n", score_move(move_list->moves[count], 0));
    }
}

static inline int is_repetition(void)
{
    for (int index = 0; index < repetition_index; index++)
    {
        if (repetition_table[index] == hash_key)
        {
            return 1;
        }
    }
    return 0;
}

static inline int quiescene(int alpha, int beta)
{
    if ((nodes & 2047) == 0)
    {
        communicate();
    }

    nodes++;

    //  too deep ply, array overflow, likely this is never reached (64 plys deep)
    if (ply > max_ply - 1)
    {
        return evaluate();
    }

    int evaluation = evaluate();

    // fail hard cutoff
    if (evaluation >= beta)
    {
        return beta;
    }

    if (evaluation > alpha)
    {
        alpha = evaluation;
    }

    moves move_list[1];
    generate_captures(move_list);

    // quiescence does not probe the transposition table, so no hash move here
    sort_moves(move_list, 0);

    for (int count = 0; count < move_list->count; count++)
    {
        if (see(move_list->moves[count]) < 0)
        {
            continue;
        }

        copy_board();
        ply++;

        // increment repetition index & store hash key
        repetition_index++;
        repetition_table[repetition_index] = hash_key;

        if (make_move(move_list->moves[count]) == 0)
        {
            // illegal move
            ply--;

            // decrement repetition index
            repetition_index--;

            continue;
        }

        int score = -quiescene(-beta, -alpha);

        ply--;

        // decrement repetition index
        repetition_index--;

        restore_board();

        if (stopped == 1)
        {
            return 0;
        }

        // found a better move
        if (score > alpha)
        {
            // PV (principal variation) node/move
            alpha = score;

            //fail hard beta cutoff
            if (score >= beta)
            {
                // move/node fails high
                return beta;
            }
        }
    }

    // node/move fails low
    return alpha;
}

// maybe decrease this to 3 or 2
static const int full_depth_moves = 4;
static const int reduction_limit = 3;

static int lmr_table[64][64];

void init_search_tables(void)
{
    for (int d = 1; d < 64; d++)
    {
        for (int m = 1; m < 64; m++)
        {
            lmr_table[d][m] = (int)(0.75 + log(d) * log(m) / 2.25);
        }
    }
}

// negamax alpha beta search
static inline int negamax(int alpha, int beta, int depth)
{
    int score;

    int hash_flag = hash_flag_alpha;

    // best move found at this node, stored in the transposition table
    int best_move = 0;

    if (ply && is_repetition())
    {
        return 0;
    }

    // a hack by Pedro Castro to figure out whether the current is PV node or not
    int pv_node = beta - alpha > 1;

    // read hash entry if not rook ply and not PV node
    if (ply && (score = read_hash_entry(alpha, beta, depth)) != no_hash_entry && pv_node == 0)
    {
        return score;
    }

    if ((nodes & 2047) == 0)
    {
        communicate();
    }

    pv_length[ply] = ply;

    if (depth == 0)
    {
        return quiescene(alpha, beta);
    }

    //  too deep ply, array overflow, likely this is never reached (64 plys deep)
    if (ply > max_ply - 1)
    {
        return evaluate();
    }

    nodes++;

    int in_check = is_square_attacked((side == white)
        ? get_ls1b_index(bitboards[piece_K])
        : get_ls1b_index(bitboards[piece_k]), side ^ 1);

    // increate depth when king is in check to avoid checkmate
    if (in_check) depth++;

    int static_eval = in_check ? 0 : evaluate();

    // reverse futility pruning: static eval is so far above beta that a shallow search won't change that
    if (!pv_node && !in_check && depth <= 6 && abs(beta) < mate_score && static_eval - 100 * depth >= beta)
    {
        return beta;
    }

    int legal_moves = 0;

    // null move pruning
    if (depth >= 3 && in_check == 0 && ply && static_eval >= beta
        && (occupancies[side] & ~(bitboards[side * 6 + piece_P] | bitboards[side * 6 + piece_K])))
    {
        copy_board();

        ply++;

        // increment repetition index & store hash key
        repetition_index++;
        repetition_table[repetition_index] = hash_key;

        if (enpassant != no_sq)
        {
            hash_key ^= enpassant_keys[enpassant];
        }

        enpassant = no_sq;

        side ^= 1;

        hash_key ^= side_key;

        // depth - 1 - R, R grows with depth
        score = -negamax(-beta, -beta + 1, depth - 1 - (2 + depth / 6));

        ply--;

        // decrement repetition index
        repetition_index--;

        restore_board();

        if (stopped == 1)
        {
            return 0;
        }

        // fail hard beta cutoff
        if (score >= beta)
        {
            // node/move fails high
            return beta;
        }
    }

    // best move stored for this position (hash_key is restored after the null move above).
    // read it regardless of depth/bounds: it is only used to order the moves.
    int tt_move = read_hash_move();

    // internal iterative reduction
    if (depth >= 4 && tt_move == 0)
    {
        depth--;
    }

    moves move_list[1];
    generate_moves(move_list);

    if (follow_pv)
    {
        enable_pv_scoring(move_list);
    }

    sort_moves(move_list, tt_move);

    int moves_searched = 0;

    for (int count = 0; count < move_list->count; count++)
    {
        copy_board();
        ply++;

        // increment repetition index & store hash key
        repetition_index++;
        repetition_table[repetition_index] = hash_key;

        if (make_move(move_list->moves[count]) == 0)
        {
            // illegal move
            ply--;

            // decrement repetition index
            repetition_index--;

            continue;
        }

        legal_moves++;

        // full depth search
        if (moves_searched == 0)
        {
            score = -negamax(-beta, -alpha, depth - 1);
        }
        // LMR (late move reduction)
        else
        {
            // condition to consider LMR
            if(moves_searched >= full_depth_moves &&
                depth >= reduction_limit &&
                in_check == 0 &&
                get_move_capture(move_list->moves[count]) == 0 &&
                get_move_promoted(move_list->moves[count]) == 0)
            {
                // search current move with reduced depth:
                int R = lmr_table[depth < 63 ? depth : 63][moves_searched < 63 ? moves_searched : 63];
                if (pv_node) R--;
                if (R < 1) R = 1;
                if (R > depth - 2) R = depth - 2;
                score = -negamax(-alpha - 1, -alpha, depth - 1 - R);
            }
            // hack to ensure that full-depth search is done
            else
            {
                score = alpha + 1;
            }

            // PVS (principle variation search)
            if(score > alpha)
            {
                // re-search at full depth but with narrowed score bandwith
                score = -negamax(-alpha - 1, -alpha, depth-1);

                // if LMR fails re-search at full depth and full score bandwith
                if((score > alpha) && (score < beta))
                    score = -negamax(-beta, -alpha, depth-1);
            }
        }


        ply--;

        // decrement repetition index
        repetition_index--;

        restore_board();

        if (stopped == 1)
        {
            return 0;
        }

        moves_searched++;

        // found a better move
        if (score > alpha)
        {
            hash_flag = hash_flag_exact;

            // remember the best move so far for the transposition table
            best_move = move_list->moves[count];

            if (get_move_capture(move_list->moves[count]) == 0)
            {
                // store history moves
                history_moves[get_move_piece(move_list->moves[count])][get_move_target(move_list->moves[count])] += depth;
            }

            // PV (principal variation) node/move
            alpha = score;

            pv_table[ply][ply] = move_list->moves[count];

            for (int next_ply = ply + 1; next_ply < pv_length[ply + 1]; next_ply++)
            {
                pv_table[ply][next_ply] = pv_table[ply + 1][next_ply];
            }

            pv_length[ply] = pv_length[ply + 1];

            //fail hard beta cutoff
            if (score >= beta)
            {
                write_hash_entry(beta, depth, hash_flag_beta, best_move);

                // only for quiet moves
                if (get_move_capture(move_list->moves[count]) == 0)
                {
                    // story killer moves
                    killer_moves[1][ply] = killer_moves[0][ply];
                    killer_moves[0][ply] = move_list->moves[count];
                }

                // move/node fails high
                return beta;
            }
        }
    }

    // we have no legel moves left
    if (legal_moves == 0)
    {
        // we are in checkmate
        if (in_check)
        {
            return -mate_value + ply;
        }
        // we are in stalemate
        else
        {
            return 0;
        }
    }

    write_hash_entry(alpha, depth, hash_flag, best_move);

    // node/move fails low
    return alpha;
}

void search_position(int depth)
{
    starttime = get_time_ms();
    int score = 0;
    nodes = 0;
    stopped = 0;
    follow_pv = 0;
    score_pv = 0;

    memset(killer_moves, 0, sizeof(killer_moves));
    memset(history_moves, 0, sizeof(history_moves));
    memset(pv_table, 0, sizeof(pv_table));
    memset(pv_length, 0, sizeof(pv_length));

    int alpha = -infinity;
    int beta = infinity;

    int best_root_move = 0;

    // iterative deepening
    for (int current_depth = 1; current_depth <= depth; current_depth++)
    {
        current_search_depth = current_depth;

        if (stopped == 1)
        {
            break;
        }

        follow_pv = 1;

        score = negamax(alpha, beta, current_depth);

        if (stopped == 1) break;

        if ((score <= alpha) || (score >= beta))
        {
            alpha = -infinity;
            beta = infinity;
            // DEBUG: claude suggested this
            current_depth--;
            continue;
        }

        best_root_move = pv_table[0][0];

        alpha = score - 50;
        beta = score + 50;

        if (score > -mate_value && score < -mate_score)
            printf("info score mate %d depth %d nodes %llu time %llu pv ", -(score + mate_value) / 2 - 1, current_depth, (unsigned long long) nodes, (unsigned long long) (get_time_ms() - starttime));

        else if (score > mate_score && score < mate_value)
            printf("info score mate %d depth %d nodes %llu time %llu pv ", (mate_value - score) / 2 + 1, current_depth, (unsigned long long) nodes, (unsigned long long) (get_time_ms() - starttime));

        else
            printf("info score cp %d depth %d nodes %llu time %llu pv ", score, current_depth, (unsigned long long) nodes, (unsigned long long) (get_time_ms() - starttime));

        for (int count = 0; count < pv_length[0]; count++)
        {
            print_move(pv_table[0][count]);
            printf(" ");
        }

        printf("\n");
    }

    // never print an empty/garbage move (e.g. "stop" during depth 1)
    // safety in case of a stop during search 1 depth
    // just get the first legal move and play it instead of an illegal move
    if (best_root_move == 0)
    {
        moves ml[1];
        generate_moves(ml);
        for (int i = 0; i < ml->count; i++)
        {
            copy_board();
            int legal = make_move(ml->moves[i]);
            restore_board();
            if (legal)
            {
                best_root_move = ml->moves[i];
                break;
            }
        }
    }

    printf("bestmove ");
    print_move(best_root_move);
    printf("\n");
}