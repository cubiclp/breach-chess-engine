#include "movegen.h"

// for uci purpose
void print_move(int move)
{
    int promoted = get_move_promoted(move);
    printf("%s%s", square_to_coordinates[get_move_source(move)],
                   square_to_coordinates[get_move_target(move)]);
    if (promoted) printf("%c", promoted_pieces[promoted]);
}

// for debug purposes
void print_move_list(moves *move_list)
{
    if (move_list->count == 0)
    {
        printf("\n No moves in the move list");
        return;
    }
    printf("\n     move    piece  capture  double  enpassant  castling\n");
    for (int move_count = 0; move_count < move_list->count; move_count++)
    {
        int move = move_list->moves[move_count];
        printf("     %s%s%c   %c      %d        %d       %d          %d\n",
            square_to_coordinates[get_move_source(move)],
            square_to_coordinates[get_move_target(move)],
            get_move_promoted(move) ? promoted_pieces[get_move_promoted(move)] : ' ',
            ascii_pieces[get_move_piece(move)],
            get_move_capture(move) ? 1 : 0,
            get_move_double_push(move) ? 1 : 0,
            get_move_enpassant(move) ? 1 : 0,
            get_move_castling(move) ? 1 : 0);
    }
    printf("Total number of moves: %d\n\n", move_list->count);
}

void generate_moves(moves *move_list)
{
    move_list->count = 0;
    int source_square;
    int target_square;
    U64 bitboard;
    U64 attacks;

    for (int piece = piece_P; piece <= piece_k; piece++)
    {
        bitboard = bitboards[piece];

        if (side == white)
        {
            if (piece == piece_P)
            {
                while (bitboard != 0)
                {
                    source_square = get_ls1b_index(bitboard);
                    target_square = source_square - 8;
                    // quiet pawn moves
                    if (!(target_square < a8) && !get_bit(occupancies[both], target_square))
                    {
                        // pawn promotion
                        if (source_square >= a7 && source_square <= h7)
                        {
                            add_move(move_list, encode_move(source_square, target_square, piece, piece_Q, 0, 0, 0, 0));
                            add_move(move_list, encode_move(source_square, target_square, piece, piece_R, 0, 0, 0, 0));
                            add_move(move_list, encode_move(source_square, target_square, piece, piece_B, 0, 0, 0, 0));
                            add_move(move_list, encode_move(source_square, target_square, piece, piece_N, 0, 0, 0, 0));
                        }
                        else
                        {
                            // single push
                            add_move(move_list, encode_move(source_square, target_square, piece, 0, 0, 0, 0, 0));
                            // double push
                            if ((source_square >= a2 && source_square <= h2) && !get_bit(occupancies[both], target_square - 8))
                            {
                                add_move(move_list, encode_move(source_square, target_square - 8, piece, 0, 0, 1, 0, 0));
                            }
                        }
                    }
                    attacks = pawn_attacks[side][source_square] & occupancies[black];
                    while (attacks)
                    {
                        target_square = get_ls1b_index(attacks);
                        if (source_square >= a7 && source_square <= h7)
                        {
                            // pawn capture
                            add_move(move_list, encode_move(source_square, target_square, piece, piece_Q, 1, 0, 0, 0));
                            add_move(move_list, encode_move(source_square, target_square, piece, piece_R, 1, 0, 0, 0));
                            add_move(move_list, encode_move(source_square, target_square, piece, piece_B, 1, 0, 0, 0));
                            add_move(move_list, encode_move(source_square, target_square, piece, piece_N, 1, 0, 0, 0));
                        }
                        else
                        {
                            add_move(move_list, encode_move(source_square, target_square, piece, 0, 1, 0, 0, 0));
                        }
                        pop_bit(attacks, target_square);
                    }
                    if (enpassant != no_sq)
                    {
                        U64 enpassant_attacks = pawn_attacks[side][source_square] & (1ULL << enpassant);
                        if (enpassant_attacks != 0)
                        {
                            int target_enpassant = get_ls1b_index(enpassant_attacks);
                            add_move(move_list, encode_move(source_square, target_enpassant, piece, 0, 1, 0, 1, 0));
                        }
                    }
                    pop_bit(bitboard, source_square);
                }
            }

            // castling (TODO: potential bug in attack checking here!)
            if (piece == piece_K)
            {
                if ((castle & castle_wk) != 0 && !get_bit(occupancies[both], f1)
                    && !get_bit(occupancies[both], g1) && !is_square_attacked(e1, black)
                    && !is_square_attacked(f1, black))
                {
                    add_move(move_list, encode_move(e1, g1, piece, 0, 0, 0, 0, 1));
                }
                if ((castle & castle_wq) != 0 && !get_bit(occupancies[both], d1)
                    && !get_bit(occupancies[both], c1) && !get_bit(occupancies[both], b1)
                    && !is_square_attacked(e1, black) && !is_square_attacked(d1, black))
                {
                    add_move(move_list, encode_move(e1, c1, piece, 0, 0, 0, 0, 1));
                }
            }
        }
        else
        {
            if (piece == piece_p)
            {
                while (bitboard != 0)
                {
                    source_square = get_ls1b_index(bitboard);
                    target_square = source_square + 8;
                    // quiet pawn moves
                    if (!(target_square > h1) && !get_bit(occupancies[both], target_square))
                    {
                        // pawn promotion
                        if (source_square >= a2 && source_square <= h2)
                        {
                            add_move(move_list, encode_move(source_square, target_square, piece, piece_q, 0, 0, 0, 0));
                            add_move(move_list, encode_move(source_square, target_square, piece, piece_r, 0, 0, 0, 0));
                            add_move(move_list, encode_move(source_square, target_square, piece, piece_b, 0, 0, 0, 0));
                            add_move(move_list, encode_move(source_square, target_square, piece, piece_n, 0, 0, 0, 0));
                        }
                        else
                        {
                            add_move(move_list, encode_move(source_square, target_square, piece, 0, 0, 0, 0, 0));
                            if ((source_square >= a7 && source_square <= h7) && !get_bit(occupancies[both], target_square + 8))
                            {
                                add_move(move_list, encode_move(source_square, target_square + 8, piece, 0, 0, 1, 0, 0));
                            }
                        }
                    }
                    attacks = pawn_attacks[side][source_square] & occupancies[white];
                    while (attacks)
                    {
                        target_square = get_ls1b_index(attacks);
                        if (source_square >= a2 && source_square <= h2)
                        {
                            add_move(move_list, encode_move(source_square, target_square, piece, piece_q, 1, 0, 0, 0));
                            add_move(move_list, encode_move(source_square, target_square, piece, piece_r, 1, 0, 0, 0));
                            add_move(move_list, encode_move(source_square, target_square, piece, piece_b, 1, 0, 0, 0));
                            add_move(move_list, encode_move(source_square, target_square, piece, piece_n, 1, 0, 0, 0));
                        }
                        else
                        {
                            add_move(move_list, encode_move(source_square, target_square, piece, 0, 1, 0, 0, 0));
                        }
                        pop_bit(attacks, target_square);
                    }
                    if (enpassant != no_sq)
                    {
                        U64 enpassant_attacks = pawn_attacks[side][source_square] & (1ULL << enpassant);
                        if (enpassant_attacks != 0)
                        {
                            int target_enpassant = get_ls1b_index(enpassant_attacks);
                            add_move(move_list, encode_move(source_square, target_enpassant, piece, 0, 1, 0, 1, 0));
                        }
                    }
                    pop_bit(bitboard, source_square);
                }

            }

            // castling (TODO: potential bug in attack checking here!)
            if (piece == piece_k)
            {
                if ((castle & castle_bk) != 0 && !get_bit(occupancies[both], f8)
                    && !get_bit(occupancies[both], g8) && !is_square_attacked(e8, white)
                    && !is_square_attacked(f8, white))
                {
                    add_move(move_list, encode_move(e8, g8, piece, 0, 0, 0, 0, 1));
                }
                if ((castle & castle_bq) != 0 && !get_bit(occupancies[both], d8)
                    && !get_bit(occupancies[both], c8) && !get_bit(occupancies[both], b8)
                    && !is_square_attacked(e8, white) && !is_square_attacked(d8, white))
                {
                    add_move(move_list, encode_move(e8, c8, piece, 0, 0, 0, 0, 1));
                }
            }
        }

        // generate knight moves
        if ((side == white && piece == piece_N) || (side == black && piece == piece_n))
        {
            while (bitboard)
            {
                source_square = get_ls1b_index(bitboard);
                attacks = knight_attacks[source_square] & ((side == white) ? ~occupancies[white] : ~occupancies[black]);
                while (attacks)
                {
                    target_square = get_ls1b_index(attacks);
                    // quiet move
                    if (!get_bit(((side == white) ? occupancies[black] : occupancies[white]), target_square))
                    {
                        add_move(move_list, encode_move(source_square, target_square, piece, 0, 0, 0, 0, 0));
                    }
                    // capture move
                    else
                    {
                        add_move(move_list, encode_move(source_square, target_square, piece, 0, 1, 0, 0, 0));
                    }
                    pop_bit(attacks, target_square);
                }
                pop_bit(bitboard, source_square);
            }
        }

        // generate bishop moves
        if ((side == white && piece == piece_B) || (side == black && piece == piece_b))
        {
            while (bitboard)
            {
                source_square = get_ls1b_index(bitboard);
                attacks = get_bishop_attacks(source_square, occupancies[both]) & ((side == white) ? ~occupancies[white] : ~occupancies[black]);
                while (attacks)
                {
                    target_square = get_ls1b_index(attacks);
                    // quiet move
                    if (!get_bit(((side == white) ? occupancies[black] : occupancies[white]), target_square))
                    {
                        add_move(move_list, encode_move(source_square, target_square, piece, 0, 0, 0, 0, 0));
                    }
                    // capture move
                    else
                    {
                        add_move(move_list, encode_move(source_square, target_square, piece, 0, 1, 0, 0, 0));
                    }
                    pop_bit(attacks, target_square);
                }
                pop_bit(bitboard, source_square);
            }
        }

        // generate rook moves
        if ((side == white && piece == piece_R) || (side == black && piece == piece_r))
        {
            while (bitboard)
            {
                source_square = get_ls1b_index(bitboard);
                attacks = get_rook_attacks(source_square, occupancies[both]) & ((side == white) ? ~occupancies[white] : ~occupancies[black]);
                while (attacks)
                {
                    target_square = get_ls1b_index(attacks);
                    // quiet move
                    if (!get_bit(((side == white) ? occupancies[black] : occupancies[white]), target_square))
                    {
                        add_move(move_list, encode_move(source_square, target_square, piece, 0, 0, 0, 0, 0));
                    }
                    // capture move
                    else
                    {
                        add_move(move_list, encode_move(source_square, target_square, piece, 0, 1, 0, 0, 0));
                    }
                    pop_bit(attacks, target_square);
                }
                pop_bit(bitboard, source_square);
            }
        }

        // generate queen moves
        if ((side == white && piece == piece_Q) || (side == black && piece == piece_q))
        {
            while (bitboard)
            {
                source_square = get_ls1b_index(bitboard);
                attacks = get_queen_attacks(source_square, occupancies[both]) & ((side == white) ? ~occupancies[white] : ~occupancies[black]);
                while (attacks)
                {
                    target_square = get_ls1b_index(attacks);
                    // quiet move
                    if (!get_bit(((side == white) ? occupancies[black] : occupancies[white]), target_square))
                    {
                        add_move(move_list, encode_move(source_square, target_square, piece, 0, 0, 0, 0, 0));
                    }
                    // capture move
                    else
                    {
                        add_move(move_list, encode_move(source_square, target_square, piece, 0, 1, 0, 0, 0));
                    }
                    pop_bit(attacks, target_square);
                }
                pop_bit(bitboard, source_square);
            }
        }

        // generate king moves
        if ((side == white && piece == piece_K) || (side == black && piece == piece_k))
        {
            while (bitboard)
            {
                source_square = get_ls1b_index(bitboard);
                attacks = king_attacks[source_square] & ((side == white) ? ~occupancies[white] : ~occupancies[black]);
                while (attacks)
                {
                    target_square = get_ls1b_index(attacks);
                    // quiet move
                    if (!get_bit(((side == white) ? occupancies[black] : occupancies[white]), target_square))
                    {
                        add_move(move_list, encode_move(source_square, target_square, piece, 0, 0, 0, 0, 0));
                    }
                    // capture move
                    else
                    {
                        add_move(move_list, encode_move(source_square, target_square, piece, 0, 1, 0, 0, 0));
                    }
                    pop_bit(attacks, target_square);
                }
                pop_bit(bitboard, source_square);
            }
        }
    }
}

// generate only capture moves (for quiescence search)
void generate_captures(moves *move_list)
{
    move_list->count = 0;
    int source_square;
    int target_square;
    U64 bitboard;
    U64 attacks;

    // enemy pieces = the only legal capture targets
    U64 enemy = (side == white) ? occupancies[black] : occupancies[white];

    for (int piece = piece_P; piece <= piece_k; piece++)
    {
        bitboard = bitboards[piece];

        if (side == white)
        {
            if (piece == piece_P)
            {
                while (bitboard != 0)
                {
                    source_square = get_ls1b_index(bitboard);

                    // pawn captures
                    attacks = pawn_attacks[white][source_square] & occupancies[black];
                    while (attacks)
                    {
                        target_square = get_ls1b_index(attacks);
                        if (source_square >= a7 && source_square <= h7)
                        {
                            add_move(move_list, encode_move(source_square, target_square, piece, piece_Q, 1, 0, 0, 0));
                            add_move(move_list, encode_move(source_square, target_square, piece, piece_R, 1, 0, 0, 0));
                            add_move(move_list, encode_move(source_square, target_square, piece, piece_B, 1, 0, 0, 0));
                            add_move(move_list, encode_move(source_square, target_square, piece, piece_N, 1, 0, 0, 0));
                        }
                        else
                        {
                            add_move(move_list, encode_move(source_square, target_square, piece, 0, 1, 0, 0, 0));
                        }
                        pop_bit(attacks, target_square);
                    }

                    // en passant
                    if (enpassant != no_sq)
                    {
                        U64 enpassant_attacks = pawn_attacks[white][source_square] & (1ULL << enpassant);
                        if (enpassant_attacks != 0)
                        {
                            int target_enpassant = get_ls1b_index(enpassant_attacks);
                            add_move(move_list, encode_move(source_square, target_enpassant, piece, 0, 1, 0, 1, 0));
                        }
                    }

                    pop_bit(bitboard, source_square);
                }
            }

            // castling: never a capture -> skipped entirely
        }
        else
        {
            if (piece == piece_p)
            {
                while (bitboard != 0)
                {
                    source_square = get_ls1b_index(bitboard);

                    // pawn captures
                    attacks = pawn_attacks[black][source_square] & occupancies[white];
                    while (attacks)
                    {
                        target_square = get_ls1b_index(attacks);
                        if (source_square >= a2 && source_square <= h2)
                        {
                            add_move(move_list, encode_move(source_square, target_square, piece, piece_q, 1, 0, 0, 0));
                            add_move(move_list, encode_move(source_square, target_square, piece, piece_r, 1, 0, 0, 0));
                            add_move(move_list, encode_move(source_square, target_square, piece, piece_b, 1, 0, 0, 0));
                            add_move(move_list, encode_move(source_square, target_square, piece, piece_n, 1, 0, 0, 0));
                        }
                        else
                        {
                            add_move(move_list, encode_move(source_square, target_square, piece, 0, 1, 0, 0, 0));
                        }
                        pop_bit(attacks, target_square);
                    }

                    // en passant
                    if (enpassant != no_sq)
                    {
                        U64 enpassant_attacks = pawn_attacks[black][source_square] & (1ULL << enpassant);
                        if (enpassant_attacks != 0)
                        {
                            int target_enpassant = get_ls1b_index(enpassant_attacks);
                            add_move(move_list, encode_move(source_square, target_enpassant, piece, 0, 1, 0, 1, 0));
                        }
                    }

                    pop_bit(bitboard, source_square);
                }
            }
        }

        // knight captures
        if ((side == white && piece == piece_N) || (side == black && piece == piece_n))
        {
            while (bitboard)
            {
                source_square = get_ls1b_index(bitboard);
                attacks = knight_attacks[source_square] & enemy;
                while (attacks)
                {
                    target_square = get_ls1b_index(attacks);
                    add_move(move_list, encode_move(source_square, target_square, piece, 0, 1, 0, 0, 0));
                    pop_bit(attacks, target_square);
                }
                pop_bit(bitboard, source_square);
            }
        }

        // bishop captures
        if ((side == white && piece == piece_B) || (side == black && piece == piece_b))
        {
            while (bitboard)
            {
                source_square = get_ls1b_index(bitboard);
                attacks = get_bishop_attacks(source_square, occupancies[both]) & enemy;
                while (attacks)
                {
                    target_square = get_ls1b_index(attacks);
                    add_move(move_list, encode_move(source_square, target_square, piece, 0, 1, 0, 0, 0));
                    pop_bit(attacks, target_square);
                }
                pop_bit(bitboard, source_square);
            }
        }

        // rook captures
        if ((side == white && piece == piece_R) || (side == black && piece == piece_r))
        {
            while (bitboard)
            {
                source_square = get_ls1b_index(bitboard);
                attacks = get_rook_attacks(source_square, occupancies[both]) & enemy;
                while (attacks)
                {
                    target_square = get_ls1b_index(attacks);
                    add_move(move_list, encode_move(source_square, target_square, piece, 0, 1, 0, 0, 0));
                    pop_bit(attacks, target_square);
                }
                pop_bit(bitboard, source_square);
            }
        }

        // queen captures
        if ((side == white && piece == piece_Q) || (side == black && piece == piece_q))
        {
            while (bitboard)
            {
                source_square = get_ls1b_index(bitboard);
                attacks = get_queen_attacks(source_square, occupancies[both]) & enemy;
                while (attacks)
                {
                    target_square = get_ls1b_index(attacks);
                    add_move(move_list, encode_move(source_square, target_square, piece, 0, 1, 0, 0, 0));
                    pop_bit(attacks, target_square);
                }
                pop_bit(bitboard, source_square);
            }
        }

        // king captures
        if ((side == white && piece == piece_K) || (side == black && piece == piece_k))
        {
            while (bitboard)
            {
                source_square = get_ls1b_index(bitboard);
                attacks = king_attacks[source_square] & enemy;
                while (attacks)
                {
                    target_square = get_ls1b_index(attacks);
                    add_move(move_list, encode_move(source_square, target_square, piece, 0, 1, 0, 0, 0));
                    pop_bit(attacks, target_square);
                }
                pop_bit(bitboard, source_square);
            }
        }
    }
}

// ------ Perft ------ //

void perft_driver(int depth)
{
    if (depth == 0)
    {
        nodes++;
        return;
    }
    moves move_list[1];
    generate_moves(move_list);
    for (int move_count = 0; move_count < move_list->count; move_count++)
    {
        copy_board();
        if (!make_move(move_list->moves[move_count]))
        {
            continue;
        }
        perft_driver(depth - 1);
        restore_board();
    }
}

void perft_test(int depth)
{
    nodes = 0;
    printf("\n    Performance test \n");
    moves move_list[1];
    generate_moves(move_list);

    U64 start_time = get_time_ms();

    for (int move_count = 0; move_count < move_list->count; move_count++)
    {
        copy_board();
        int move = move_list->moves[move_count];
        if (!make_move(move))
        {
            continue;
        }
        U64 commulative_nodes = nodes;
        perft_driver(depth - 1);
        U64 old_nodes = nodes - commulative_nodes;
        restore_board();
        printf("     move: ");
        int promoted = get_move_promoted(move);
        printf("%s%s%c   nodes: %llu\n", square_to_coordinates[get_move_source(move)],
                               square_to_coordinates[get_move_target(move)],
                               promoted ? promoted_pieces[promoted] : ' ',
                               (unsigned long long) old_nodes);
    }

    printf("\n    Depth: %d\n", depth);
    printf("    Nodes: %llu\n", (unsigned long long) nodes);
    printf("    Time:  %llu ms\n\n", (unsigned long long) (get_time_ms() - start_time));
}