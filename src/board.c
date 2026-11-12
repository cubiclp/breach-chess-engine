#include "board.h"
#include "attacks.h"

const char *square_to_coordinates[] = {
    "a8", "b8", "c8", "d8", "e8", "f8", "g8", "h8",
    "a7", "b7", "c7", "d7", "e7", "f7", "g7", "h7",
    "a6", "b6", "c6", "d6", "e6", "f6", "g6", "h6",
    "a5", "b5", "c5", "d5", "e5", "f5", "g5", "h5",
    "a4", "b4", "c4", "d4", "e4", "f4", "g4", "h4",
    "a3", "b3", "c3", "d3", "e3", "f3", "g3", "h3",
    "a2", "b2", "c2", "d2", "e2", "f2", "g2", "h2",
    "a1", "b1", "c1", "d1", "e1", "f1", "g1", "h1"
};

const char ascii_pieces[12] = "PNBRQKpnbrqk";

char *unicode_pieces[12] = {"♙", "♘", "♗", "♖", "♕", "♔", "♟︎", "♞", "♝", "♜", "♛", "♚"};

int char_pieces[] = {
    ['P'] = piece_P,
    ['N'] = piece_N,
    ['B'] = piece_B,
    ['R'] = piece_R,
    ['Q'] = piece_Q,
    ['K'] = piece_K,
    ['p'] = piece_p,
    ['n'] = piece_n,
    ['b'] = piece_b,
    ['r'] = piece_r,
    ['q'] = piece_q,
    ['k'] = piece_k
};

char promoted_pieces[] = {
    [piece_Q] = 'q',
    [piece_R] = 'r',
    [piece_B] = 'b',
    [piece_N] = 'n',
    [piece_q] = 'q',
    [piece_r] = 'r',
    [piece_b] = 'b',
    [piece_n] = 'n'
};

U64 bitboards[12];
U64 occupancies[3];
int side;
int enpassant = no_sq;
int castle;

U64 hash_key;

U64 repetition_table[1000];
int repetition_index;

int ply;

U64 nodes;

/*
                           castling   move     in      in
                              right update     binary  decimal

 king & rooks didn't move:     1111 & 1111  =  1111    15

        white king  moved:     1111 & 1100  =  1100    12
  white king's rook moved:     1111 & 1110  =  1110    14
 white queen's rook moved:     1111 & 1101  =  1101    13

         black king moved:     1111 & 0011  =  1011    3
  black king's rook moved:     1111 & 1011  =  1011    11
 black queen's rook moved:     1111 & 0111  =  0111    7

*/

// castling rights update constants
static const int castling_rights[64] = {
    7, 15, 15, 15,  3, 15, 15, 11,
   15, 15, 15, 15, 15, 15, 15, 15,
   15, 15, 15, 15, 15, 15, 15, 15,
   15, 15, 15, 15, 15, 15, 15, 15,
   15, 15, 15, 15, 15, 15, 15, 15,
   15, 15, 15, 15, 15, 15, 15, 15,
   15, 15, 15, 15, 15, 15, 15, 15,
   13, 15, 15, 15, 12, 15, 15, 14
};

int make_move(int move)
{
    copy_board();

    int source_square = get_move_source(move);
    int target_square = get_move_target(move);
    int piece = get_move_piece(move);
    int promoted_piece = get_move_promoted(move);
    int capture_flag = get_move_capture(move);
    int double_push_flag = get_move_double_push(move);
    int enpassant_flag = get_move_enpassant(move);
    int castling_flag = get_move_castling(move);

    // side that is making the move (do NOT call this "side": it would shadow the global)
    int us = piece <= piece_K ? white : black;
    int them = us ^ 1;

    // move piece
    pop_bit(bitboards[piece], source_square);
    set_bit(bitboards[piece], target_square);
    hash_key ^= piece_keys[piece][source_square];
    hash_key ^= piece_keys[piece][target_square];

    // handle captures
    if (capture_flag)
    {
        int start_piece = (us == white) ? piece_p : piece_P;
        int end_piece   = (us == white) ? piece_k : piece_K;
        for (int bb_piece = start_piece; bb_piece <= end_piece; bb_piece++)
        {
            if (get_bit(bitboards[bb_piece], target_square))
            {
                pop_bit(bitboards[bb_piece], target_square);
                hash_key ^= piece_keys[bb_piece][target_square];
                break;
            }
        }
    }

    // pawn promotions
    if (promoted_piece != 0)
    {
        int pawn = (us == white) ? piece_P : piece_p;
        pop_bit(bitboards[pawn], target_square);
        hash_key ^= piece_keys[pawn][target_square];

        set_bit(bitboards[promoted_piece], target_square);
        hash_key ^= piece_keys[promoted_piece][target_square];
    }

    // enpassant capture
    if (enpassant_flag)
    {
        if (us == white)
        {
            pop_bit(bitboards[piece_p], target_square + 8);
            hash_key ^= piece_keys[piece_p][target_square + 8];
        }
        else
        {
            pop_bit(bitboards[piece_P], target_square - 8);
            hash_key ^= piece_keys[piece_P][target_square - 8];
        }
    }

    // hash enpassant (remove)
    if (enpassant != no_sq)
    {
        hash_key ^= enpassant_keys[enpassant];
    }
    enpassant = no_sq;

    // double pawn push
    if (double_push_flag)
    {
        enpassant = (us == white) ? target_square + 8 : target_square - 8;
        hash_key ^= enpassant_keys[enpassant];
    }

    // castling (rook move)
    if (castling_flag)
    {
        switch (target_square)
        {
            case g1:
                pop_bit(bitboards[piece_R], h1); set_bit(bitboards[piece_R], f1);
                hash_key ^= piece_keys[piece_R][h1]; hash_key ^= piece_keys[piece_R][f1];
                break;
            case c1:
                pop_bit(bitboards[piece_R], a1); set_bit(bitboards[piece_R], d1);
                hash_key ^= piece_keys[piece_R][a1]; hash_key ^= piece_keys[piece_R][d1];
                break;
            case g8:
                pop_bit(bitboards[piece_r], h8); set_bit(bitboards[piece_r], f8);
                hash_key ^= piece_keys[piece_r][h8]; hash_key ^= piece_keys[piece_r][f8];
                break;
            case c8:
                pop_bit(bitboards[piece_r], a8); set_bit(bitboards[piece_r], d8);
                hash_key ^= piece_keys[piece_r][a8]; hash_key ^= piece_keys[piece_r][d8];
                break;
        }
    }

    // update castling rights (+ hash)
    hash_key ^= castle_keys[castle];
    castle &= castling_rights[source_square];
    castle &= castling_rights[target_square];
    hash_key ^= castle_keys[castle];

    // ---- incremental occupancy update ----

    // 1. move the piece
    pop_bit(occupancies[us], source_square);
    set_bit(occupancies[us], target_square);
    pop_bit(occupancies[both], source_square);
    set_bit(occupancies[both], target_square);

    // 2. remove captured piece
    if (capture_flag)
    {
        if (enpassant_flag)
        {
            int ep_square = (us == white) ? (target_square + 8) : (target_square - 8);
            pop_bit(occupancies[them], ep_square);
            pop_bit(occupancies[both], ep_square);
        }
        else
        {
            // target square stays occupied in `both` (the mover is there now)
            pop_bit(occupancies[them], target_square);
        }
    }

    // 3. move the rook when castling
    if (castling_flag)
    {
        switch (target_square)
        {
            case g1:
                pop_bit(occupancies[us], h1); set_bit(occupancies[us], f1);
                pop_bit(occupancies[both], h1); set_bit(occupancies[both], f1);
                break;
            case c1:
                pop_bit(occupancies[us], a1); set_bit(occupancies[us], d1);
                pop_bit(occupancies[both], a1); set_bit(occupancies[both], d1);
                break;
            case g8:
                pop_bit(occupancies[us], h8); set_bit(occupancies[us], f8);
                pop_bit(occupancies[both], h8); set_bit(occupancies[both], f8);
                break;
            case c8:
                pop_bit(occupancies[us], a8); set_bit(occupancies[us], d8);
                pop_bit(occupancies[both], a8); set_bit(occupancies[both], d8);
                break;
        }
    }

    // change the GLOBAL side to move + hash it
    side ^= 1;
    hash_key ^= side_key;

    // legality: is the king of the side that just moved attacked by the new side to move?
    int king_square = get_ls1b_index(bitboards[(us == white) ? piece_K : piece_k]);
    if (is_square_attacked(king_square, side))
    {
        restore_board();   // now restores the global side too
        return 0;
    }

    return 1;
}

// ----- Random number generation --------- //

U32 random_state = 1804289383;

U32 get_random_number_U32(void)
{
    U32 number = random_state;
    number ^= number << 13;
    number ^= number >> 17;
    number ^= number << 5;
    random_state = number;
    return number;
}

U64 get_random_number_U64(void)
{
    U64 n1, n2, n3, n4;
    n1 = (U64) (get_random_number_U32()) & 0xFFFF;
    n2 = (U64) (get_random_number_U32()) & 0xFFFF;
    n3 = (U64) (get_random_number_U32()) & 0xFFFF;
    n4 = (U64) (get_random_number_U32()) & 0xFFFF;
    return n1 | (n2 << 16) | (n3 << 32) | (n4 << 48);
}

U64 generate_magic_number_candidate(void)
{
    return get_random_number_U64() & get_random_number_U64() & get_random_number_U64();
}

// --------- Zobrist Keys ----------- //

// random piece keys [piece][square]
U64 piece_keys[12][64];

// random enpassant keys
U64 enpassant_keys[64];

// random castling keys
U64 castle_keys[16];

// random side key
U64 side_key;

// initialize random hashing keys
void init_random_keys(void)
{
    random_state = 1804289383;

    for (int piece = piece_P; piece <= piece_k; piece++)
    {
        for (int square = 0; square < 64; square++)
        {
            piece_keys[piece][square] = get_random_number_U64();
        }
    }

    for (int square = 0; square < 64; square++)
    {
        enpassant_keys[square] = get_random_number_U64();
    }

    for (int index = 0; index < 16; index++)
    {
        castle_keys[index] = get_random_number_U64();
    }

    side_key = get_random_number_U64();
}

// generate "almost" unique position identifier aka hash key
U64 generate_hash_keys(void)
{
    U64 final_key = 0ULL;

    U64 bitboard;

    for (int piece = piece_P; piece <= piece_k; piece++)
    {
        bitboard = bitboards[piece];
        while (bitboard)
        {
            int square = get_ls1b_index(bitboard);

            // hash piece
            final_key ^= piece_keys[piece][square];

            pop_bit(bitboard, square);
        }
    }

    if (enpassant != no_sq)
    {
        // hash enpassant
        final_key ^= enpassant_keys[enpassant];
    }

    // hash castle
    final_key ^= castle_keys[castle];

    // hash side only if black to move
    if (side == black)
    {
        final_key ^= side_key;
    }

    return final_key;
}

// --------- Input and Output -------------- //

void print_bitboard(U64 bitboard)
{
    printf("\n");
    for (int rank = 0; rank < 8; rank++)
    {
        for (int file = 0; file < 8; file++)
        {
            int square = rank * 8 + file;
            if (file == 0)
            {
                printf(" %d ", 8 - rank);
            }
            printf("%d ", get_bit(bitboard, square) ? 1 : 0);
        }
        if (rank < 7)
        {
            printf("\n");
        }
    }
    printf("\n   a b c d e f g h\n");
    printf("   Bitboard: %llud\n", (unsigned long long) bitboard);
}

// #define PRINT_BOARD_USE_SYMBOLS

void print_board(void)
{
    printf("\n");
    for (int rank = 0; rank < 8; rank++)
    {
        for (int file = 0; file < 8; file++)
        {
            int square = rank * 8 + file;
            if (file == 0)
            {
                printf(" %d", 8 - rank);
            }
            int piece = -1;
            for (int bitboard_piece = piece_P; bitboard_piece <= piece_k; bitboard_piece++)
            {
                if (get_bit(bitboards[bitboard_piece], square))
                {
                    piece = bitboard_piece;
                }
            }
#ifdef PRINT_BOARD_USE_SYMBOLS
            printf(" %s", (piece == -1) ? "." : unicode_pieces[piece]);
#else
            printf(" %c", (piece == -1) ? '.' : ascii_pieces[piece]);
#endif
        }
        printf("\n");
    }
    printf("   a b c d e f g h\n\n");

    if (side >= 0)
    {
        printf("   Side:     %s\n", (side == 0) ? "white" : "black");
    } else
    {
        printf("   Side:     not set\n");
    }
    printf("   Enpassant:   %s\n", (enpassant != no_sq) ? square_to_coordinates[enpassant] : "no");
    printf("   Castling:  %c%c%c%c\n\n", (castle & castle_wk) ? 'K' : '-', (castle & castle_wq) ? 'Q' : '-',
           (castle & castle_bk) ? 'k' : '-', (castle & castle_bq) ? 'q' : '-');
    printf("   Hash key: %llx\n\n", (unsigned long long) hash_key);
}

// TODO: put clear_hash_table at the beginning here?
void parse_fen(char *fen)
{
    // reset board position and state variables
    memset(bitboards, 0ULL, sizeof(bitboards));
    memset(occupancies, 0ULL, sizeof(occupancies));
    side = 0;
    enpassant = no_sq;
    castle = castle_none;

    repetition_index = 0;
    memset(repetition_table, 0ULL, sizeof(repetition_table));
    //ply = 0;

    for (int rank = 0; rank < 8; rank++)
    {
        for (int file = 0; file < 8; file++)
        {
            int square = rank * 8 + file;
            // char piece to encoding
            if ((*fen >= 'a' && *fen <= 'z') || (*fen >= 'A' && *fen <= 'Z'))
            {
                int piece = char_pieces[(int) *fen];
                set_bit(bitboards[piece], square);
                fen++;
            }
            // match empty squares
            if (*fen >= '0' && *fen <= '9')
            {
                int offset = *fen - '0';
                // check if any piece is on current square
                int piece = -1;
                for (int bb_piece = piece_P; bb_piece <= piece_k; bb_piece++)
                {
                    if (get_bit(bitboards[bb_piece], square))
                    {
                        piece = bb_piece;
                    }
                }
                // if not then we need to dec file to prevent weird piece shifting
                if (piece == -1)
                {
                    file--;
                }
                file += offset;
                fen++;
            }
            // match rank seperator
            if (*fen == '/')
            {
                fen++;
            }
        }
    }

    // parse side to move
    fen++;
    if (*fen == 'w')
    {
        side = white;
    } else
    {
        side = black;
    }

    // parse castling rights
    fen += 2;
    while (*fen != ' ')
    {
        switch (*fen)
        {
            case 'K': castle |= castle_wk;
                break;
            case 'Q': castle |= castle_wq;
                break;
            case 'k': castle |= castle_bk;
                break;
            case 'q': castle |= castle_bq;
                break;
            case '-': break;
        }
        fen++;
    }

    // parse enpassant square
    fen++;
    if (*fen != '-')
    {
        int file = fen[0] - 'a';
        int rank = 8 - (fen[1] - '0');
        enpassant = rank * 8 + file;
    } else
    {
        enpassant = no_sq;
    }

    // make occupancies
    for (int piece = piece_P; piece <= piece_K; piece++)
    {
        occupancies[white] |= bitboards[piece];
    }
    for (int piece = piece_p; piece <= piece_k; piece++)
    {
        occupancies[black] |= bitboards[piece];
    }
    occupancies[both] |= occupancies[white];
    occupancies[both] |= occupancies[black];

    // init hash key
    hash_key = generate_hash_keys();
}