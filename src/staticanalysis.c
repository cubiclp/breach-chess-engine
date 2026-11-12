#include "staticanalysis.h"
#include "types.h"
#include "board.h"
#include "attacks.h"

// simple material values for SEE (indexed by piece type: P N B R Q K)
// the king is huge so that a king capture into a defended square always looks losing
const int see_values[6] = { 100, 300, 300, 500, 900, 20000 };

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
inline int see(int move)
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
inline int see_ge(int move, int threshold)
{
    return see(move) >= threshold;
}
