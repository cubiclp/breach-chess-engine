#include "search.h"
#include "attacks.h"
#include "movegen.h"
#include "eval.h"
#include "uci.h"
#include "board.h"
#include "tt.h"
#include "staticanalysis.h"

#define max_history 7000

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

/*  =======================
         Move ordering
    =======================

    1. PV move                      20000
    2. Transposition table move     15000
    3. Good captures in MVV/LVA     10000+
    4. 1st killer move              9000
    5. 2nd killer move              8000
    6. History moves                +-7000
    7. Unsorted moves               0
    8. Bad captures                 -10000
*/

static inline int score_move(int move, int tt_move, int use_see)
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
        if (use_see)
        {
            int mvv = mvv_lva[get_move_piece(move)][target_piece];
            int attacker = get_move_piece(move) % 6;
            int victim   = target_piece % 6;
            int good     = see_values[victim] >= see_values[attacker] || see_ge(move, 0);
            return good ? mvv + 10000 : mvv - 10000;
        } else
        {
            return mvv_lva[get_move_piece(move)][target_piece] + 10000;
        }
    }
    // score quiet move
    else
    {
        // quiet promotions: queen first, underpromotions last
        int promoted = get_move_promoted(move);
        if (promoted)
        {
            if (promoted == piece_Q || promoted == piece_q) return 9500;
            return -8000;   // below any history value (history stays within ±7000)
        }
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
static inline void sort_moves(moves *move_list, int tt_move, int use_see)
{
    int move_scores[move_list->count];
    for (int count = 0; count < move_list->count; count++)
    {
        move_scores[count] = score_move(move_list->moves[count], tt_move, use_see);
    }
    // sort the moves
    shell_sort(move_list, move_scores);
}

void print_move_scores(moves *move_list, int use_see)
{
    printf("  Move scores: \n\n");
    for (int count = 0; count < move_list->count; count++)
    {
        printf("  move: ");
        print_move(move_list->moves[count]);
        printf(" score: %d\n", score_move(move_list->moves[count], 0, use_see));
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

static inline void update_history(int piece, int target, int bonus)
{
    int *h = &history_moves[piece][target];
    *h += bonus - *h * abs(bonus) / max_history;
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
    sort_moves(move_list, 0, 0);

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
static const int full_depth_moves = 3;
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

    //  too deep ply, array overflow, likely this is never reached (64 plys deep)
    if (ply >= max_ply - 1)
    {
        return evaluate();
    }

    int in_check = is_square_attacked((side == white)
        ? get_ls1b_index(bitboards[piece_K])
        : get_ls1b_index(bitboards[piece_k]), side ^ 1);

    if (in_check) depth++;
    if (depth == 0) return quiescene(alpha, beta);

    nodes++;

    int static_eval = in_check ? 0 : evaluate();

    // reverse futility pruning: static eval is so far above beta that a shallow search won't change that
    if (!pv_node && !in_check && depth <= 6 && abs(beta) < mate_score && static_eval - 100 * depth >= beta)
    {
        return beta;
    }

    int legal_moves = 0;

    // null move pruning
    if (!pv_node && depth >= 3 && in_check == 0 && ply && static_eval >= beta
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

    sort_moves(move_list, tt_move, 1);

    int moves_searched = 0;

    int quiets[256];
    int quiet_count = 0;

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
                    // killer moves (don't store the same move in both slots)
                    if (killer_moves[0][ply] != move_list->moves[count])
                    {
                        killer_moves[1][ply] = killer_moves[0][ply];
                        killer_moves[0][ply] = move_list->moves[count];
                    }

                    // history bonus for the cutoff move, malus for the quiets tried before it
                    int bonus = depth * depth;
                    if (bonus > 1600) bonus = 1600;
                    update_history(get_move_piece(move_list->moves[count]), get_move_target(move_list->moves[count]), bonus);
                    for (int i = 0; i < quiet_count; i++)
                    {
                        update_history(get_move_piece(quiets[i]), get_move_target(quiets[i]), -bonus);
                    }
                }

                // move/node fails high
                return beta;
            }
        }

        if (get_move_capture(move_list->moves[count]) == 0 && get_move_promoted(move_list->moves[count]) == 0)
        {
            quiets[quiet_count++] = move_list->moves[count];
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