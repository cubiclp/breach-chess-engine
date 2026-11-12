#include "tt.h"
#include "types.h"
#include "search.h"
#include "board.h"

static tt hash_table[hash_size];

void clear_hash_table(void)
{
    memset(hash_table, 0, sizeof(hash_table));
}

inline int read_hash_entry(int alpha, int beta, int depth)
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
inline int read_hash_move(void)
{
    tt *hash_entry = &hash_table[hash_key & hash_mask];

    if (hash_entry->hash_upper == (U32)(hash_key >> 32))
    {
        return hash_entry->move;
    }

    return 0;
}

inline void write_hash_entry(int score, int depth, int hash_flag, int best_move)
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