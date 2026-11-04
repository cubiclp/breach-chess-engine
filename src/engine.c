#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <math.h>

#ifdef WIN64
#include <windows.h>
#else
#include <sys/time.h>
#endif
#include <unistd.h> // ok?

typedef uint64_t U64;
typedef uint32_t U32;

// FEN dedug positions
#define empty_board "8/8/8/8/8/8/8/8 w - - "
#define start_position "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1 "
#define tricky_position "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1 "
#define killer_position "rnbqkb1r/pp1p1pPp/8/2p1pP2/1P1P4/3P3P/P1P1P3/RNBQKBNR w KQkq e6 0 1"
#define cmk_position "r2q1rk1/ppp2ppp/2n1bn2/2b1p3/3pP3/3P1NPP/PPP1NPB1/R1BQ1RK1 b - - 0 9 "
#define repetitions "2r3k1/R7/8/1R6/8/8/P4KPP/8 w - - 0 40 "

// enum board squares
enum {
    a8, b8, c8, d8, e8, f8, g8, h8,
    a7, b7, c7, d7, e7, f7, g7, h7,
    a6, b6, c6, d6, e6, f6, g6, h6,
    a5, b5, c5, d5, e5, f5, g5, h5,
    a4, b4, c4, d4, e4, f4, g4, h4,
    a3, b3, c3, d3, e3, f3, g3, h3,
    a2, b2, c2, d2, e2, f2, g2, h2,
    a1, b1, c1, d1, e1, f1, g1, h1, no_sq
};

enum {
    white, black, both
};

enum {
    rook, bishop
};

enum {
    castle_none = 0, castle_wk = 1, castle_wq = 2, castle_bk = 4, castle_bq = 8
};

enum {
    piece_P, piece_N, piece_B, piece_R, piece_Q, piece_K, piece_p, piece_n, piece_b, piece_r, piece_q, piece_k
};

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

// "almost" unique position identifier aka hash key or position key
U64 hash_key;

U64 repetition_table[1000];

int repetition_index;

// ply = halfmove, full move = every side made 1 ply
int ply;

// -------- Time control vars ---------- //


// exit from engine flag
int quit = 0;

// UCI "movestogo" command moves counter
int movestogo = 30;

// UCI "movetime" command time counter
int movetime = -1;

// UCI "time" command holder (ms)
int time = -1;

// UCI "inc" command's time increment holder
int inc = 0;

// UCI "starttime" command time holder
U64 starttime = 0;

// UCI "stoptime" command time holder
U64 stoptime = 0;

// variable to flag time control availability
int timeset = 0;

// variable to flag when the time is up
int stopped = 0;

// ----- Functions forked from VICE ------ //

U64 get_time_ms()
{
#ifdef _WIN32
    static LARGE_INTEGER freq;
    LARGE_INTEGER now;
    if (!freq.QuadPart) QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&now);
    return (U64)(now.QuadPart * 1000 / freq.QuadPart);
#else
    struct timeval time_value;
    gettimeofday(&time_value, NULL);
    return time_value.tv_sec * 1000 + time_value.tv_usec / 1000;
#endif
}

/*

  Function to "listen" to GUI's input during search.
  It's waiting for the user input from STDIN.
  OS dependent.

  First Richard Allbert aka BluefeverSoftware grabbed it from somewhere...
  And then Code Monkey King has grabbed it from VICE)

*/

int input_waiting()
{
    #ifndef _WIN32
        fd_set readfds;
        struct timeval tv;
        FD_ZERO (&readfds);
        FD_SET (fileno(stdin), &readfds);
        tv.tv_sec=0; tv.tv_usec=0;
        select(16, &readfds, 0, 0, &tv);

        return (FD_ISSET(fileno(stdin), &readfds));
    #else
        static int init = 0, pipe;
        static HANDLE inh;
        DWORD dw;

        if (!init)
        {
            init = 1;
            inh = GetStdHandle(STD_INPUT_HANDLE);
            pipe = !GetConsoleMode(inh, &dw);
            if (!pipe)
            {
                SetConsoleMode(inh, dw & ~(ENABLE_MOUSE_INPUT|ENABLE_WINDOW_INPUT));
                FlushConsoleInputBuffer(inh);
            }
        }

        if (pipe)
        {
           if (!PeekNamedPipe(inh, NULL, 0, NULL, &dw, NULL)) return 1;
           return dw;
        }

        else
        {
           GetNumberOfConsoleInputEvents(inh, &dw);
           return dw <= 1 ? 0 : dw;
        }

    #endif
}

// read GUI/user input
void read_input()
{
    char input[256] = "", *endc;

    if (input_waiting())
    {
        int bytes = read(fileno(stdin), input, 255);

        // stdin closed (GUI died): shut down
        if (bytes <= 0) { quit = 1; stopped = 1; return; }

        input[bytes] = 0;
        endc = strchr(input, '\n');
        if (endc) *endc = 0;

        if (!strncmp(input, "quit", 4))
        {
            quit = 1; stopped = 1;
        }
        else if (!strncmp(input, "stop", 4))
        {
            stopped = 1;
        }
        else if (!strncmp(input, "isready", 7))
        {
            printf("readyok\n");
        }
    }
}

int current_search_depth;

// a bridge function to interact between search and GUI input
static void communicate() {
	// if time is up break here
    if(timeset == 1 && current_search_depth > 1 && get_time_ms() > stoptime) {
		// tell engine to stop calculating
		stopped = 1;
	}

    // read GUI input
	read_input();
}

// ----- Random number generation --------- //

U32 random_state = 1804289383;

U32 get_random_number_U32()
{
    U32 number = random_state;
    number ^= number << 13;
    number ^= number >> 17;
    number ^= number << 5;
    random_state = number;
    return number;
}

U64 get_random_number_U64()
{
    U64 n1, n2, n3, n4;
    n1 = (U64) (get_random_number_U32()) & 0xFFFF;
    n2 = (U64) (get_random_number_U32()) & 0xFFFF;
    n3 = (U64) (get_random_number_U32()) & 0xFFFF;
    n4 = (U64) (get_random_number_U32()) & 0xFFFF;
    return n1 | (n2 << 16) | (n3 << 32) | (n4 << 48);
}

U64 generate_magic_number_candidate()
{
    return get_random_number_U64() & get_random_number_U64() & get_random_number_U64();
}


// --- Bit manipulations --- //

#define set_bit(bitboard, square) ((bitboard) |= (1ULL << (square)))
#define get_bit(bitboard, square) ((bitboard) & (1ULL << (square)))
#define pop_bit(bitboard, square) ((bitboard) &= ~(1ULL << (square)))

// replaced theese with builtin functions

static inline int count_bits(U64 bitboard)
{
    return __builtin_popcountll(bitboard);
}

static inline int get_ls1b_index(U64 bitboard)
{
    if (bitboard == 0)
    {
        return -1;
    }
    return __builtin_ctzll(bitboard);
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
void init_random_keys()
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

// generale "almost" unique position identifier aka hash key
U64 generate_hash_keys()
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
    printf("   Bitboard: %llud\n", bitboard);
}

// #define PRINT_BOARD_USE_SYMBOLS

void print_board()
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
    printf("   Hash key: %llx\n\n", hash_key);
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
                int piece = char_pieces[*fen];
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


// ---- Attacks ----- //

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

void init_leaper_attacks()
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

void init_magic_numbers()
{
    for (int square = 0; square < 64; square++)
    {
        printf(" 0x%llxULL\n", find_magic_number(square, rook_relevant_bits[square], rook));
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

static inline U64 get_bishop_attacks(int square, U64 occupancy)
{
    occupancy &= bishop_masks[square];
    occupancy *= bishop_magic_numbers[square];
    occupancy >>= 64 - bishop_relevant_bits[square];
    return bishop_attacks[square][occupancy];
}

static inline U64 get_rook_attacks(int square, U64 occupancy)
{
    occupancy &= rook_masks[square];
    occupancy *= rook_magic_numbers[square];
    occupancy >>= 64 - rook_relevant_bits[square];
    return rook_attacks[square][occupancy];
}

static inline U64 get_queen_attacks(int square, U64 occupancy)
{
    U64 queen_attacks = 0ULL;
    U64 bishop_occupancies = occupancy;
    U64 rook_occupancies = occupancy;

    bishop_occupancies &= bishop_masks[square];
    bishop_occupancies *= bishop_magic_numbers[square];
    bishop_occupancies >>= 64 - bishop_relevant_bits[square];
    queen_attacks = bishop_attacks[square][bishop_occupancies];

    rook_occupancies &= rook_masks[square];
    rook_occupancies *= rook_magic_numbers[square];
    rook_occupancies >>= 64 - rook_relevant_bits[square];
    queen_attacks |= rook_attacks[square][rook_occupancies];

    return queen_attacks;
}


// ---------- Move generator ----------- //


static inline bool is_square_attacked(int square, int side)
{
    // for leaper peaces
    if (side == white && pawn_attacks[black][square] & bitboards[piece_P]) return true;
    if (side == black && pawn_attacks[white][square] & bitboards[piece_p]) return true;

    if (knight_attacks[square] & ((side == white) ? bitboards[piece_N] : bitboards[piece_n])) return true;

    if (king_attacks[square] & ((side == white) ? bitboards[piece_K] : bitboards[piece_k])) return true;

    //for sliders
    if (get_bishop_attacks(square, occupancies[both]) & ((side == white) ? bitboards[piece_B] : bitboards[piece_b]))
    {
        return true;
    }

    if (get_rook_attacks(square, occupancies[both]) & ((side == white) ? bitboards[piece_R] : bitboards[piece_r]))
    {
        return true;
    }

    if (get_queen_attacks(square, occupancies[both]) & ((side == white) ? bitboards[piece_Q] : bitboards[piece_q]))
    {
        return true;
    }

    return false;
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

/*
    binary move bits                                hexidecmimal constants

0000 0000 0000 0000 0011 1111   source square       0x3f
0000 0000 0000 1111 1100 0000   target square       0xfc0
0000 0000 1111 0000 0000 0000   piece               0xf000
0000 1111 0000 0000 0000 0000   promoted piece      0xf0000
0001 0000 0000 0000 0000 0000   capture flag        0x100000
0010 0000 0000 0000 0000 0000   double push flag    0x200000
0100 0000 0000 0000 0000 0000   enpassant flag      0x400000
1000 0000 0000 0000 0000 0000   castling flag       0x800000
 */

#define encode_move(source, target, piece, promoted, capture, double_push, enpassant, castling) \
    ((source) |             \
    ((target) << 6) |         \
    ((piece) << 12) |         \
    ((promoted) << 16) |      \
    ((capture) << 20) |       \
    ((double_push) << 21) |   \
    ((enpassant) << 22) |     \
    ((castling) << 23))

#define get_move_source(move) ((move) & 0x3f)
#define get_move_target(move) (((move) & 0xfc0) >> 6)
#define get_move_piece(move) (((move) & 0xf000) >> 12)
#define get_move_promoted(move) (((move) & 0xf0000) >> 16)

#define get_move_capture(move) ((move) & 0x100000)
#define get_move_double_push(move) ((move) & 0x200000)
#define get_move_enpassant(move) ((move) & 0x400000)
#define get_move_castling(move) ((move) & 0x800000)


typedef struct {
    int moves[256];
    int count;
} moves;

static inline void add_move(moves *moves_list, int move)
{
    moves_list->moves[moves_list->count] = move;
    moves_list->count++;
}

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

#define copy_board()                                                        \
    U64 bitboards_copy[12], occupancies_copy[3];                            \
    int side_copy, enpassant_copy, castle_copy;                             \
    memcpy(bitboards_copy, bitboards, 96);                                  \
    memcpy(occupancies_copy, occupancies, 24);                              \
    side_copy = side, enpassant_copy = enpassant, castle_copy = castle;     \
    U64 hash_key_copy = hash_key;

#define restore_board()                                                     \
    memcpy(bitboards, bitboards_copy, 96);                                  \
    memcpy(occupancies, occupancies_copy, 24);                              \
    side = side_copy, enpassant = enpassant_copy, castle = castle_copy;     \
    hash_key = hash_key_copy;

enum { all_moves, only_captures };


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
const int castling_rights[64] = {
    7, 15, 15, 15,  3, 15, 15, 11,
   15, 15, 15, 15, 15, 15, 15, 15,
   15, 15, 15, 15, 15, 15, 15, 15,
   15, 15, 15, 15, 15, 15, 15, 15,
   15, 15, 15, 15, 15, 15, 15, 15,
   15, 15, 15, 15, 15, 15, 15, 15,
   15, 15, 15, 15, 15, 15, 15, 15,
   13, 15, 15, 15, 12, 15, 15, 14
};


static inline int make_move(int move)
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

static inline void generate_moves(moves *move_list)
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
static inline void generate_captures(moves *move_list)
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

U64 nodes;

static inline void perft_driver(int depth)
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

        // debug

        /*
        U64 hash_from_scratch = generate_hash_keys();
        if (hash_key != hash_from_scratch)
        {
            printf(" Take back\n");
            printf("move: ");
            print_move(move_list->moves[move_count]);
            print_board();
            printf("hash key should be: %llx\n", hash_from_scratch);
            getchar();
        }
         */

        // debug end
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
                               old_nodes);
    }

    printf("\n    Depth: %d\n", depth);
    printf("    Nodes: %llu\n", nodes);
    printf("    Time:  %llu ms\n\n", get_time_ms() - start_time);
}

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

int mg_value[6] = { 82, 337, 365, 477, 1025,  0};
int eg_value[6] = { 94, 281, 297, 512,  936,  0};

int mg_pawn_table[64] = {
      0,   0,   0,   0,   0,   0,  0,   0,
     98, 134,  61,  95,  68, 126, 34, -11,
     -6,   7,  26,  31,  65,  56, 25, -20,
    -14,  13,   6,  21,  23,  12, 17, -23,
    -27,  -2,  -5,  12,  17,   6, 10, -25,
    -26,  -4,  -4, -10,   3,   3, 33, -12,
    -35,  -1, -20, -23, -15,  24, 38, -22,
      0,   0,   0,   0,   0,   0,  0,   0,
};

int eg_pawn_table[64] = {
      0,   0,   0,   0,   0,   0,   0,   0,
    178, 173, 158, 134, 147, 132, 165, 187,
     94, 100,  85,  67,  56,  53,  82,  84,
     32,  24,  13,   5,  -2,   4,  17,  17,
     13,   9,  -3,  -7,  -7,  -8,   3,  -1,
      4,   7,  -6,   1,   0,  -5,  -1,  -8,
     13,   8,   8,  10,  13,   0,   2,  -7,
      0,   0,   0,   0,   0,   0,   0,   0,
};

int mg_knight_table[64] = {
    -167, -89, -34, -49,  61, -97, -15, -107,
     -73, -41,  72,  36,  23,  62,   7,  -17,
     -47,  60,  37,  65,  84, 129,  73,   44,
      -9,  17,  19,  53,  37,  69,  18,   22,
     -13,   4,  16,  13,  28,  19,  21,   -8,
     -23,  -9,  12,  10,  19,  17,  25,  -16,
     -29, -53, -12,  -3,  -1,  18, -14,  -19,
    -105, -21, -58, -33, -17, -28, -19,  -23,
};

int eg_knight_table[64] = {
    -58, -38, -13, -28, -31, -27, -63, -99,
    -25,  -8, -25,  -2,  -9, -25, -24, -52,
    -24, -20,  10,   9,  -1,  -9, -19, -41,
    -17,   3,  22,  22,  22,  11,   8, -18,
    -18,  -6,  16,  25,  16,  17,   4, -18,
    -23,  -3,  -1,  15,  10,  -3, -20, -22,
    -42, -20, -10,  -5,  -2, -20, -23, -44,
    -29, -51, -23, -15, -22, -18, -50, -64,
};

int mg_bishop_table[64] = {
    -29,   4, -82, -37, -25, -42,   7,  -8,
    -26,  16, -18, -13,  30,  59,  18, -47,
    -16,  37,  43,  40,  35,  50,  37,  -2,
     -4,   5,  19,  50,  37,  37,   7,  -2,
     -6,  13,  13,  26,  34,  12,  10,   4,
      0,  15,  15,  15,  14,  27,  18,  10,
      4,  15,  16,   0,   7,  21,  33,   1,
    -33,  -3, -14, -21, -13, -12, -39, -21,
};

int eg_bishop_table[64] = {
    -14, -21, -11,  -8, -7,  -9, -17, -24,
     -8,  -4,   7, -12, -3, -13,  -4, -14,
      2,  -8,   0,  -1, -2,   6,   0,   4,
     -3,   9,  12,   9, 14,  10,   3,   2,
     -6,   3,  13,  19,  7,  10,  -3,  -9,
    -12,  -3,   8,  10, 13,   3,  -7, -15,
    -14, -18,  -7,  -1,  4,  -9, -15, -27,
    -23,  -9, -23,  -5, -9, -16,  -5, -17,
};

int mg_rook_table[64] = {
     32,  42,  32,  51, 63,  9,  31,  43,
     27,  32,  58,  62, 80, 67,  26,  44,
     -5,  19,  26,  36, 17, 45,  61,  16,
    -24, -11,   7,  26, 24, 35,  -8, -20,
    -36, -26, -12,  -1,  9, -7,   6, -23,
    -45, -25, -16, -17,  3,  0,  -5, -33,
    -44, -16, -20,  -9, -1, 11,  -6, -71,
    -19, -13,   1,  17, 16,  7, -37, -26,
};

int eg_rook_table[64] = {
    13, 10, 18, 15, 12,  12,   8,   5,
    11, 13, 13, 11, -3,   3,   8,   3,
     7,  7,  7,  5,  4,  -3,  -5,  -3,
     4,  3, 13,  1,  2,   1,  -1,   2,
     3,  5,  8,  4, -5,  -6,  -8, -11,
    -4,  0, -5, -1, -7, -12,  -8, -16,
    -6, -6,  0,  2, -9,  -9, -11,  -3,
    -9,  2,  3, -1, -5, -13,   4, -20,
};

int mg_queen_table[64] = {
    -28,   0,  29,  12,  59,  44,  43,  45,
    -24, -39,  -5,   1, -16,  57,  28,  54,
    -13, -17,   7,   8,  29,  56,  47,  57,
    -27, -27, -16, -16,  -1,  17,  -2,   1,
     -9, -26,  -9, -10,  -2,  -4,   3,  -3,
    -14,   2, -11,  -2,  -5,   2,  14,   5,
    -35,  -8,  11,   2,   8,  15,  -3,   1,
     -1, -18,  -9,  10, -15, -25, -31, -50,
};

int eg_queen_table[64] = {
     -9,  22,  22,  27,  27,  19,  10,  20,
    -17,  20,  32,  41,  58,  25,  30,   0,
    -20,   6,   9,  49,  47,  35,  19,   9,
      3,  22,  24,  45,  57,  40,  57,  36,
    -18,  28,  19,  47,  31,  34,  39,  23,
    -16, -27,  15,   6,   9,  17,  10,   5,
    -22, -23, -30, -16, -16, -23, -36, -32,
    -33, -28, -22, -43,  -5, -32, -20, -41,
};

int mg_king_table[64] = {
    -65,  23,  16, -15, -56, -34,   2,  13,
     29,  -1, -20,  -7,  -8,  -4, -38, -29,
     -9,  24,   2, -16, -20,   6,  22, -22,
    -17, -20, -12, -27, -30, -25, -14, -36,
    -49,  -1, -27, -39, -46, -44, -33, -51,
    -14, -14, -22, -46, -44, -30, -15, -27,
      1,   7,  -8, -64, -43, -16,   9,   8,
    -15,  36,  12, -54,   8, -28,  24,  14,
};

int eg_king_table[64] = {
    -74, -35, -18, -18, -11,  15,   4, -17,
    -12,  17,  14,  17,  17,  38,  23,  11,
     10,  17,  23,  15,  20,  45,  44,  13,
     -8,  22,  24,  27,  26,  33,  26,   3,
    -18,  -4,  21,  24,  27,  23,   9, -11,
    -19,  -3,  11,  21,  23,  16,   7,  -9,
    -27, -11,   4,  13,  14,   4,  -5, -17,
    -53, -34, -21, -11, -28, -14, -24, -43
};

int* mg_pesto_table[6] =
{
    mg_pawn_table,
    mg_knight_table,
    mg_bishop_table,
    mg_rook_table,
    mg_queen_table,
    mg_king_table
};

int* eg_pesto_table[6] =
{
    eg_pawn_table,
    eg_knight_table,
    eg_bishop_table,
    eg_rook_table,
    eg_queen_table,
    eg_king_table
};

int gamephaseInc[12] = {0, 1, 1, 2, 4, 0,   0, 1, 1, 2, 4, 0};   // P N B R Q K | p n b r q k
int mg_table[12][64];
int eg_table[12][64];

// mirror positional score tables for opposite side
const int mirror_score[128] =
{
    a1, b1, c1, d1, e1, f1, g1, h1,
    a2, b2, c2, d2, e2, f2, g2, h2,
    a3, b3, c3, d3, e3, f3, g3, h3,
    a4, b4, c4, d4, e4, f4, g4, h4,
    a5, b5, c5, d5, e5, f5, g5, h5,
    a6, b6, c6, d6, e6, f6, g6, h6,
    a7, b7, c7, d7, e7, f7, g7, h7,
    a8, b8, c8, d8, e8, f8, g8, h8
};

void init_tables()
{
    int p, sq;
    for (p = piece_P; p <= piece_K; p++) {
        for (sq = 0; sq < 64; sq++) {
            mg_table[p][sq] = mg_value[p] + mg_pesto_table[p][sq];
            eg_table[p][sq] = eg_value[p] + eg_pesto_table[p][sq];
            mg_table[p + 6][sq] = mg_value[p] + mg_pesto_table[p][mirror_score[sq]];
            eg_table[p + 6][sq] = eg_value[p] + eg_pesto_table[p][mirror_score[sq]];
        }
    }
}


// TODO: optimize and improve later
static inline int evaluate()
{
    int mg[2];
    int eg[2];
    int gamePhase = 0;

    mg[white] = 0;
    mg[black] = 0;
    eg[white] = 0;
    eg[black] = 0;

    for (int piece = piece_P; piece <= piece_k; piece++)
    {
        U64 bitboard_copy = bitboards[piece];
        while (bitboard_copy)
        {
            int piece_color = (piece <= piece_K) ? white : black;
            int sq = get_ls1b_index(bitboard_copy);
            mg[piece_color] += mg_table[piece][sq];
            eg[piece_color] += eg_table[piece][sq];
            gamePhase += gamephaseInc[piece];
            pop_bit(bitboard_copy, sq);
        }
    }

    /* tapered eval */
    int mgScore = mg[side] - mg[side ^ 1];
    int egScore = eg[side] - eg[side ^ 1];
    int mgPhase = gamePhase;
    if (mgPhase > 24) mgPhase = 24; /* in case of early promotion */
    int egPhase = 24 - mgPhase;
    return (mgScore * mgPhase + egScore * egPhase) / 24;
}

// ------------- Search ------------ //

#define infinity 50000
#define mate_value 49000
#define mate_score 48000

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
int killer_moves[2][max_ply];

// history moves [piece][board square]
int history_moves[12][64];


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

int pv_length[max_ply];

int pv_table[max_ply][max_ply];

int follow_pv, score_pv;

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

tt hash_table[hash_size];

void clear_hash_table()
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
static inline int read_hash_move()
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
static inline int sort_moves(moves *move_list, int tt_move)
{
    int move_scores[move_list->count];
    for (int count = 0; count < move_list->count; count++)
    {
        move_scores[count] = score_move(move_list->moves[count], tt_move);
    }
    // sort the moves
    shell_sort(move_list, move_scores);
    /*
    for (int current_move = 0; current_move < move_list->count; current_move++)
    {
        for (int next_move = current_move + 1; next_move < move_list->count; next_move++)
        {
            if (move_scores[current_move] < move_scores[next_move])
            {
                int temp_score = move_scores[current_move];
                move_scores[current_move] = move_scores[next_move];
                move_scores[next_move] = temp_score;

                int temp_move = move_list->moves[current_move];
                move_list->moves[current_move] = move_list->moves[next_move];
                move_list->moves[next_move] = temp_move;
            }
        }
    }
    */
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

static inline int is_repetition()
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
const int full_depth_moves = 4;
const int reduction_limit = 3;

int lmr_table[64][64];

void init_search_tables()
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
            printf("info score mate %d depth %d nodes %llu time %llu pv ", -(score + mate_value) / 2 - 1, current_depth, nodes, get_time_ms() - starttime);

        else if (score > mate_score && score < mate_value)
            printf("info score mate %d depth %d nodes %llu time %llu pv ", (mate_value - score) / 2 + 1, current_depth, nodes, get_time_ms() - starttime);

        else
            printf("info score cp %d depth %d nodes %llu time %llu pv ", score, current_depth, nodes, get_time_ms() - starttime);

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

// ------- UCI integration ----- //

int parse_move(char *move_string)
{
    moves move_list[1];
    generate_moves(move_list);
    int source_square = (move_string[0] - 'a') + (8 - (move_string[1] - '0')) * 8;
    int target_square = (move_string[2] - 'a') + (8 - (move_string[3] - '0')) * 8;
    for (int move_count = 0; move_count < move_list->count; move_count++)
    {
        int move = move_list->moves[move_count];
        if (source_square == get_move_source(move) && target_square == get_move_target(move))
        {
            int promoted_piece = get_move_promoted(move);

            if (promoted_piece)
            {
                if ((promoted_piece == piece_Q || promoted_piece == piece_q) && move_string[4] == 'q')
                {
                    return move;
                }
                if ((promoted_piece == piece_R || promoted_piece == piece_r) && move_string[4] == 'r')
                {
                    return move;
                }
                if ((promoted_piece == piece_B || promoted_piece == piece_b) && move_string[4] == 'b')
                {
                    return move;
                }
                if ((promoted_piece == piece_N || promoted_piece == piece_n) && move_string[4] == 'n')
                {
                    return move;
                }
                continue;
            }
            // legal move
            return move;
        }
    }

    return 0;
}

/*
    Example UCI commands to init position on chess board

    // init start position
    position startpos

    // init start position and make the moves on chess board
    position startpos moves e2e4 e7e5

    // init position from FEN string
    position fen r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1

    // init position from fen string and make moves on chess board
    position fen r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1 moves e2a6 e8g8
*/

void parse_position(char *command)
{
    command += 9;
    char *current_char = command;

    // parse UCI startpos command
    if (strncmp(command, "startpos", 8) == 0)
    {
        parse_fen(start_position);
    }
    // parse UCI fen command
    else
    {
        current_char = strstr(command, "fen");
        if (current_char == NULL)
        {
            parse_fen(start_position);
        }
        else
        {
            current_char += 4;
            parse_fen(current_char);
        }
    }

    current_char = strstr(command, "moves");
    if (current_char != NULL)
    {
        current_char += 6;
        while (*current_char)
        {
            int move = parse_move(current_char);
            if (move == 0)
            {
                break;
            }

            repetition_index++;
            repetition_table[repetition_index] = hash_key;
            make_move(move);

            while (*current_char && *current_char != ' ')
            {
                current_char++;
            }
            current_char++;
        }
    }
}

// uci "go" command
void parse_go(char *command)
{
    int depth = -1;
    char *argument = NULL;

    // reset time control state (these used to persist between "go" commands)
    time = -1;
    inc = 0;
    movetime = -1;
    movestogo = 30;
    timeset = 0;

    if ((argument = strstr(command, "binc")) && side == black) inc = atoi(argument + 5);
    if ((argument = strstr(command, "winc")) && side == white) inc = atoi(argument + 5);
    if ((argument = strstr(command, "wtime")) && side == white) time = atoi(argument + 6);
    if ((argument = strstr(command, "btime")) && side == black) time = atoi(argument + 6);
    if ((argument = strstr(command, "movestogo"))) movestogo = atoi(argument + 10);
    if ((argument = strstr(command, "movetime"))) movetime = atoi(argument + 9);
    if ((argument = strstr(command, "depth"))) depth = atoi(argument + 6);

    if (movetime != -1)
    {
        time = movetime;
        movestogo = 1;
    }

    if (time != -1)
    {
        timeset = 1;
        int overhead = 20;                           // ms for GUI/pipe latency
        int budget = (time - overhead) / movestogo + inc * 3 / 4;
        if (budget > time - overhead) budget = time - overhead;
        if (budget < 1) budget = 1;
        stoptime = get_time_ms() + budget;
    }

    if (depth == -1) depth = 64;

    search_position(depth);
}

/*
 GUI -> isready
 Engine -> readyok
 */

void uci_loop()
{
    setbuf(stdin, NULL);
    setbuf(stdout, NULL);

    char input[2000];

    while (1)
    {
        memset(input, 0, sizeof(input));
        fflush(stdout);

        if (!fgets(input, 2000, stdin))
        {
            continue;
        }
        if (input[0] == '\n')
        {
            continue;
        }
        if (strncmp(input, "isready", 7) == 0)
        {
            printf("readyok\n");
            continue;
        }
        else if (strncmp(input, "position", 8) == 0)
        {
            parse_position(input);
            // TODO: hmmm just leave this now?
            //clear_hash_table();
        }
        else if (strncmp(input, "ucinewgame", 10) == 0)
        {
            parse_fen(start_position);
            clear_hash_table();
        }
        else if (strncmp(input, "go", 2) == 0)
        {
            parse_go(input);
            if (quit) break;
        }
        else if (strncmp(input, "quit", 4) == 0)
        {
            break;
        }
        else if (strncmp(input, "uci", 3) == 0)
        {
            printf("id name Breach\n");
            printf("id author cubiclp\n");
            printf("uciok\n");
        }
    }
}


// ---- Main ----- //

void init_all()
{
    init_leaper_attacks();
    init_sliders_attacks(bishop);
    init_sliders_attacks(rook);
    init_random_keys();
    init_tables();
    init_search_tables();
    // clear hash table
    clear_hash_table();
}

// potential function idea for is endgame
// when (no queens && mionor pieces < 3) or (1 queen && minor pieces < 2)

void bench()
{
    printf("startpos\n");
    parse_fen(start_position);
    perft_test(6);
    clear_hash_table();
    printf("kiwipete\n");
    parse_fen(tricky_position);
    perft_test(5);
}

int main()
{
    init_all();

    int debug = 1;

    if (debug)
    {
        //parse_fen("4k3/Q7/8/4K3/8/8/8/8 w - - ");
        /*
        *printf("start position: \n");
        parse_fen(start_position);
        perft_test(6);

        printf("kiwi pete position: \n");

        parse_fen(tricky_position);
        perft_test(5);
         **/
        parse_fen(start_position);

        U64 start_time = get_time_ms();
        search_position(12);
        U64 end_time = get_time_ms();
        printf("search time: %llu\n", end_time - start_time);
    }
    else
    {
        uci_loop();
    }

    return 0;
}