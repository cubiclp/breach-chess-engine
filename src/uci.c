#include "uci.h"
#include "board.h"
#include "movegen.h"
#include "search.h"

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

// -------- Time control vars ---------- //

int quit = 0;
int movestogo = 30;
int movetime = -1;
int time_left = -1;
int inc = 0;
U64 starttime = 0;
U64 stoptime = 0;
int timeset = 0;
int stopped = 0;

/*

  Function to "listen" to GUI's input during search.
  It's waiting for the user input from STDIN.
  OS dependent.

  First Richard Allbert aka BluefeverSoftware grabbed it from somewhere...
  And then Code Monkey King has grabbed it from VICE)

*/

static int input_waiting(void)
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
static void read_input(void)
{
    char input[256] = "", *endc;

    if (input_waiting())
    {
#ifdef _WIN32
        int bytes = _read(_fileno(stdin), input, 255);
#else
        int bytes = read(fileno(stdin), input, 255);
#endif

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

// a bridge function to interact between search and GUI input
void communicate(void)
{
    // if time is up break here
    if (timeset == 1 && current_search_depth > 1 && get_time_ms() > stoptime) {
        // tell engine to stop calculating
        stopped = 1;
    }

    // read GUI input
    read_input();
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
    time_left = -1;
    inc = 0;
    movetime = -1;
    movestogo = 30;
    timeset = 0;

    if ((argument = strstr(command, "binc")) && side == black) inc = atoi(argument + 5);
    if ((argument = strstr(command, "winc")) && side == white) inc = atoi(argument + 5);
    if ((argument = strstr(command, "wtime")) && side == white) time_left = atoi(argument + 6);
    if ((argument = strstr(command, "btime")) && side == black) time_left = atoi(argument + 6);
    if ((argument = strstr(command, "movestogo"))) movestogo = atoi(argument + 10);
    if ((argument = strstr(command, "movetime"))) movetime = atoi(argument + 9);
    if ((argument = strstr(command, "depth"))) depth = atoi(argument + 6);

    if (movetime != -1)
    {
        time_left = movetime;
        movestogo = 1;
    }

    if (time_left != -1)
    {
        timeset = 1;
        int overhead = 20;                           // ms for GUI/pipe latency
        int budget = (time_left - overhead) / movestogo + inc * 3 / 4;
        if (budget > time_left - overhead) budget = time_left - overhead;
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

void uci_loop(void)
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