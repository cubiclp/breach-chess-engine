#include "attacks.h"

/*
 8 0 1 1 1 1 1 1 1
 7 0 1 1 1 1 1 1 1
 6 0 1 1 1 1 1 1 1
 5 0 1 1 1 1 1 1 1
 4 0 1 1 1 1 1 1 1
 3 0 1 1 1 1 1 1 1
 2 0 1 1 1 1 1 1 1
 1 0 1 1 1 1 1 1 1
   a b c d e f g h
 */
const U64 not_a_file = 18374403900871474942ULL;

/*
 8 1 1 1 1 1 1 1 0
 7 1 1 1 1 1 1 1 0
 6 1 1 1 1 1 1 1 0
 5 1 1 1 1 1 1 1 0
 4 1 1 1 1 1 1 1 0
 3 1 1 1 1 1 1 1 0
 2 1 1 1 1 1 1 1 0
 1 1 1 1 1 1 1 1 0
   a b c d e f g h
*/
const U64 not_h_file = 9187201950435737471ULL;
const U64 not_hg_file = 4557430888798830399ULL;
const U64 not_ab_file = 18229723555195321596ULL;

const int bishop_relevant_bits[64] = {
    6, 5, 5, 5, 5, 5, 5, 6,
    5, 5, 5, 5, 5, 5, 5, 5,
    5, 5, 7, 7, 7, 7, 5, 5,
    5, 5, 7, 9, 9, 7, 5, 5,
    5, 5, 7, 9, 9, 7, 5, 5,
    5, 5, 7, 7, 7, 7, 5, 5,
    5, 5, 5, 5, 5, 5, 5, 5,
    6, 5, 5, 5, 5, 5, 5, 6
};

const int rook_relevant_bits[64] = {
    12, 11, 11, 11, 11, 11, 11, 12,
    11, 10, 10, 10, 10, 10, 10, 11,
    11, 10, 10, 10, 10, 10, 10, 11,
    11, 10, 10, 10, 10, 10, 10, 11,
    11, 10, 10, 10, 10, 10, 10, 11,
    11, 10, 10, 10, 10, 10, 10, 11,
    11, 10, 10, 10, 10, 10, 10, 11,
    12, 11, 11, 11, 11, 11, 11, 12
};

U64 rook_magic_numbers[64] = {
    0x8a80104000800020ULL,
    0x140002000100040ULL,
    0x2801880a0017001ULL,
    0x100081001000420ULL,
    0x200020010080420ULL,
    0x3001c0002010008ULL,
    0x8480008002000100ULL,
    0x2080088004402900ULL,
    0x800098204000ULL,
    0x2024401000200040ULL,
    0x100802000801000ULL,
    0x120800800801000ULL,
    0x208808088000400ULL,
    0x2802200800400ULL,
    0x2200800100020080ULL,
    0x801000060821100ULL,
    0x80044006422000ULL,
    0x100808020004000ULL,
    0x12108a0010204200ULL,
    0x140848010000802ULL,
    0x481828014002800ULL,
    0x8094004002004100ULL,
    0x4010040010010802ULL,
    0x20008806104ULL,
    0x100400080208000ULL,
    0x2040002120081000ULL,
    0x21200680100081ULL,
    0x20100080080080ULL,
    0x2000a00200410ULL,
    0x20080800400ULL,
    0x80088400100102ULL,
    0x80004600042881ULL,
    0x4040008040800020ULL,
    0x440003000200801ULL,
    0x4200011004500ULL,
    0x188020010100100ULL,
    0x14800401802800ULL,
    0x2080040080800200ULL,
    0x124080204001001ULL,
    0x200046502000484ULL,
    0x480400080088020ULL,
    0x1000422010034000ULL,
    0x30200100110040ULL,
    0x100021010009ULL,
    0x2002080100110004ULL,
    0x202008004008002ULL,
    0x20020004010100ULL,
    0x2048440040820001ULL,
    0x101002200408200ULL,
    0x40802000401080ULL,
    0x4008142004410100ULL,
    0x2060820c0120200ULL,
    0x1001004080100ULL,
    0x20c020080040080ULL,
    0x2935610830022400ULL,
    0x44440041009200ULL,
    0x280001040802101ULL,
    0x2100190040002085ULL,
    0x80c0084100102001ULL,
    0x4024081001000421ULL,
    0x20030a0244872ULL,
    0x12001008414402ULL,
    0x2006104900a0804ULL,
    0x1004081002402ULL
};

U64 bishop_magic_numbers[64] = {
    0x40040844404084ULL,
    0x2004208a004208ULL,
    0x10190041080202ULL,
    0x108060845042010ULL,
    0x581104180800210ULL,
    0x2112080446200010ULL,
    0x1080820820060210ULL,
    0x3c0808410220200ULL,
    0x4050404440404ULL,
    0x21001420088ULL,
    0x24d0080801082102ULL,
    0x1020a0a020400ULL,
    0x40308200402ULL,
    0x4011002100800ULL,
    0x401484104104005ULL,
    0x801010402020200ULL,
    0x400210c3880100ULL,
    0x404022024108200ULL,
    0x810018200204102ULL,
    0x4002801a02003ULL,
    0x85040820080400ULL,
    0x810102c808880400ULL,
    0xe900410884800ULL,
    0x8002020480840102ULL,
    0x220200865090201ULL,
    0x2010100a02021202ULL,
    0x152048408022401ULL,
    0x20080002081110ULL,
    0x4001001021004000ULL,
    0x800040400a011002ULL,
    0xe4004081011002ULL,
    0x1c004001012080ULL,
    0x8004200962a00220ULL,
    0x8422100208500202ULL,
    0x2000402200300c08ULL,
    0x8646020080080080ULL,
    0x80020a0200100808ULL,
    0x2010004880111000ULL,
    0x623000a080011400ULL,
    0x42008c0340209202ULL,
    0x209188240001000ULL,
    0x400408a884001800ULL,
    0x110400a6080400ULL,
    0x1840060a44020800ULL,
    0x90080104000041ULL,
    0x201011000808101ULL,
    0x1a2208080504f080ULL,
    0x8012020600211212ULL,
    0x500861011240000ULL,
    0x180806108200800ULL,
    0x4000020e01040044ULL,
    0x300000261044000aULL,
    0x802241102020002ULL,
    0x20906061210001ULL,
    0x5a84841004010310ULL,
    0x4010801011c04ULL,
    0xa010109502200ULL,
    0x4a02012000ULL,
    0x500201010098b028ULL,
    0x8040002811040900ULL,
    0x28000010020204ULL,
    0x6000020202d0240ULL,
    0x8918844842082200ULL,
    0x4010011029020020ULL
};

//[side][square]
U64 pawn_attacks[2][64];
U64 knight_attacks[64];
U64 king_attacks[64];

U64 bishop_masks[64];
U64 rook_masks[64];

U64 bishop_attacks[64][512];
U64 rook_attacks[64][4096];

U64 mask_pawn_attacks(int side, int square)
{
    U64 attacks = 0ULL;
    U64 bitboard = 0ULL;
    set_bit(bitboard, square);
    if (side == white)
    {
        if ((bitboard >> 7) & not_a_file) attacks |= (bitboard >> 7);
        if ((bitboard >> 9) & not_h_file) attacks |= (bitboard >> 9);
    } else
    {
        if ((bitboard << 7) & not_h_file) attacks |= (bitboard << 7);
        if ((bitboard << 9) & not_a_file) attacks |= (bitboard << 9);
    }
    return attacks;
}

U64 mask_knight_attacks(int square)
{
    U64 attacks = 0ULL;
    U64 bitboard = 0ULL;
    set_bit(bitboard, square);

    if ((bitboard >> 17) & not_h_file) attacks |= (bitboard >> 17);
    if ((bitboard >> 15) & not_a_file) attacks |= (bitboard >> 15);
    if ((bitboard >> 10) & not_hg_file) attacks |= (bitboard >> 10);
    if ((bitboard >> 6) & not_ab_file) attacks |= (bitboard >> 6);

    if ((bitboard << 17) & not_a_file) attacks |= (bitboard << 17);
    if ((bitboard << 15) & not_h_file) attacks |= (bitboard << 15);
    if ((bitboard << 10) & not_ab_file) attacks |= (bitboard << 10);
    if ((bitboard << 6) & not_hg_file) attacks |= (bitboard << 6);

    return attacks;
}

U64 mask_king_attacks(int square)
{
    U64 attacks = 0ULL;
    U64 bitboard = 0ULL;
    set_bit(bitboard, square);

    if (bitboard >> 8) attacks |= (bitboard >> 8);
    if ((bitboard >> 9) & not_h_file) attacks |= (bitboard >> 9);
    if ((bitboard >> 7) & not_a_file) attacks |= (bitboard >> 7);
    if ((bitboard >> 1) & not_h_file) attacks |= (bitboard >> 1);

    if (bitboard << 8) attacks |= (bitboard << 8);
    if ((bitboard << 9) & not_a_file) attacks |= (bitboard << 9);
    if ((bitboard << 7) & not_h_file) attacks |= (bitboard << 7);
    if ((bitboard << 1) & not_a_file) attacks |= (bitboard << 1);

    return attacks;
}

U64 mask_bishop_attacks(int square)
{
    U64 attacks = 0ULL;

    int rank, file;
    int target_rank = square / 8;
    int target_file = square % 8;

    for (rank = target_rank + 1, file = target_file + 1; rank <= 6 && file <= 6; file++, rank++)
    {
        attacks |= (1ULL << (rank * 8 + file));
    }
    for (rank = target_rank - 1, file = target_file + 1; rank >= 1 && file <= 6; file++, rank--)
    {
        attacks |= (1ULL << (rank * 8 + file));
    }
    for (rank = target_rank + 1, file = target_file - 1; rank <= 6 && file >= 1; file--, rank++)
    {
        attacks |= (1ULL << (rank * 8 + file));
    }
    for (rank = target_rank - 1, file = target_file - 1; rank >= 1 && file >= 1; file--, rank--)
    {
        attacks |= (1ULL << (rank * 8 + file));
    }

    return attacks;
}

U64 mask_rook_attacks(int square)
{
    U64 attacks = 0ULL;

    int rank, file;
    int target_rank = square / 8;
    int target_file = square % 8;

    for (rank = target_rank + 1; rank <= 6; rank++)
    {
        attacks |= (1ULL << (rank * 8 + target_file));
    }
    for (rank = target_rank - 1; rank >= 1; rank--)
    {
        attacks |= (1ULL << (rank * 8 + target_file));
    }
    for (file = target_file + 1; file <= 6; file++)
    {
        attacks |= (1ULL << (target_rank * 8 + file));
    }
    for (file = target_file - 1; file >= 1; file--)
    {
        attacks |= (1ULL << (target_rank * 8 + file));
    }

    return attacks;
}

U64 bishop_attacks_irt(int square, U64 block)
{
    U64 attacks = 0ULL;

    int rank, file;
    int target_rank = square / 8;
    int target_file = square % 8;

    for (rank = target_rank + 1, file = target_file + 1; rank <= 7 && file <= 7; file++, rank++)
    {
        attacks |= (1ULL << (rank * 8 + file));
        if ((1ULL << (rank * 8 + file)) & block)
        {
            break;
        }
    }
    for (rank = target_rank - 1, file = target_file + 1; rank >= 0 && file <= 7; file++, rank--)
    {
        attacks |= (1ULL << (rank * 8 + file));
        if ((1ULL << (rank * 8 + file)) & block)
        {
            break;
        }
    }
    for (rank = target_rank + 1, file = target_file - 1; rank <= 7 && file >= 0; file--, rank++)
    {
        attacks |= (1ULL << (rank * 8 + file));
        if ((1ULL << (rank * 8 + file)) & block)
        {
            break;
        }
    }
    for (rank = target_rank - 1, file = target_file - 1; rank >= 0 && file >= 0; file--, rank--)
    {
        attacks |= (1ULL << (rank * 8 + file));
        if ((1ULL << (rank * 8 + file)) & block)
        {
            break;
        }
    }

    return attacks;
}

U64 rook_attacks_irt(int square, U64 block)
{
    U64 attacks = 0ULL;

    int rank, file;
    int target_rank = square / 8;
    int target_file = square % 8;

    for (rank = target_rank + 1; rank <= 7; rank++)
    {
        attacks |= (1ULL << (rank * 8 + target_file));
        if ((1ULL << (rank * 8 + target_file)) & block)
        {
            break;
        }
    }
    for (rank = target_rank - 1; rank >= 0; rank--)
    {
        attacks |= (1ULL << (rank * 8 + target_file));
        if ((1ULL << (rank * 8 + target_file)) & block)
        {
            break;
        }
    }
    for (file = target_file + 1; file <= 7; file++)
    {
        attacks |= (1ULL << (target_rank * 8 + file));
        if ((1ULL << (target_rank * 8 + file)) & block)
        {
            break;
        }
    }
    for (file = target_file - 1; file >= 0; file--)
    {
        attacks |= (1ULL << (target_rank * 8 + file));
        if ((1ULL << (target_rank * 8 + file)) & block)
        {
            break;
        }
    }

    return attacks;
}

void init_leaper_attacks(void)
{
    for (int square = 0; square < 64; square++)
    {
        pawn_attacks[white][square] = mask_pawn_attacks(white, square);
        pawn_attacks[black][square] = mask_pawn_attacks(black, square);
        knight_attacks[square] = mask_knight_attacks(square);
        king_attacks[square] = mask_king_attacks(square);
    }
}

U64 set_occupancy(int index, int bits_in_attack_mask, U64 attack_mask)
{
    U64 occupancy = 0ULL;
    for (int count = 0; count < bits_in_attack_mask; count++)
    {
        int square = get_ls1b_index(attack_mask);
        pop_bit(attack_mask, square);
        if (index & (1 << count))
        {
            occupancy |= (1ULL << square);
        }
    }
    return occupancy;
}

// ------ Magics ------- //

U64 find_magic_number(int square, int relevant_bits, int bishop)
{
    U64 occupancies[4096];
    U64 attacks[4096];
    U64 used_attacks[4096];
    U64 attack_mask = bishop ? mask_bishop_attacks(square) : mask_rook_attacks(square);
    int occupancy_indicies = 1 << relevant_bits;
    for (int index = 0; index < occupancy_indicies; index++)
    {
        occupancies[index] = set_occupancy(index, relevant_bits, attack_mask);
        attacks[index] = bishop
                             ? bishop_attacks_irt(square, occupancies[index])
                             : rook_attacks_irt(square, occupancies[index]);
    }
    for (int random_count = 0; random_count < 100000000; random_count++)
    {
        U64 magic_number = generate_magic_number_candidate();
        if (count_bits((attack_mask * magic_number) & 0xFF00000000000000) < 6)
        {
            continue;
        }
        memset(used_attacks, 0ULL, sizeof(used_attacks));
        int index, fail;
        for (index = 0, fail = 0; fail == 0 && index < occupancy_indicies; index++)
        {
            U64 magic_index = (occupancies[index] * magic_number) >> (64U - relevant_bits);
            if (used_attacks[magic_index] == 0ULL)
            {
                used_attacks[index] = attacks[index];
            } else if (used_attacks[magic_index] != attacks[index])
            {
                fail = 1;
            }
        }
        if (fail == 0)
        {
            return magic_number;
        }
    }
    printf("Magic number not found");
    return 0ULL;
}

void init_magic_numbers(void)
{
    for (int square = 0; square < 64; square++)
    {
        printf(" 0x%llxULL\n", (unsigned long long) find_magic_number(square, rook_relevant_bits[square], rook));
    }
}

void init_sliders_attacks(int bishop)
{
    for (int square = 0; square < 64; square++)
    {
        bishop_masks[square] = mask_bishop_attacks(square);
        rook_masks[square] = mask_rook_attacks(square);
        U64 attack_mask = bishop ? bishop_masks[square] : rook_masks[square];
        int relevant_bit_count = count_bits(attack_mask);
        int occupancy_indicies = (1 << relevant_bit_count);
        for (int index = 0; index < occupancy_indicies; index++)
        {
            if (bishop == 1)
            {
                U64 occupancy = set_occupancy(index, relevant_bit_count, attack_mask);
                int magic_index = (occupancy * bishop_magic_numbers[square]) >> (64 - bishop_relevant_bits[square]);
                bishop_attacks[square][magic_index] = bishop_attacks_irt(square, occupancy);
            } else
            {
                U64 occupancy = set_occupancy(index, relevant_bit_count, attack_mask);
                int magic_index = (occupancy * rook_magic_numbers[square]) >> (64 - rook_relevant_bits[square]);
                rook_attacks[square][magic_index] = rook_attacks_irt(square, occupancy);
            }
        }
    }
}

void print_attacked_squares(int side)
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
            printf("%d ", is_square_attacked(square, side) ? 1 : 0);
        }
        printf("\n");
    }
    printf("   a b c d e f g h\n\n");
}