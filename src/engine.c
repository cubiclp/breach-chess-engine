#include "types.h"
#include "board.h"
#include "attacks.h"
#include "movegen.h"
#include "eval.h"
#include "search.h"
#include "uci.h"
#include "weights.h"


void init_all()
{
    init_leaper_attacks();
    init_sliders_attacks(bishop);
    init_sliders_attacks(rook);
    init_random_keys();
    init_search_tables();
    init_weights();
    // clear hash table
    clear_hash_table();
}

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

void debug()
{
    parse_fen(tricky_position);
    U64 total = 0;
    for (int i = 0; i < 100; i++)
    {
        clear_hash_table();
        U64 time = get_time_ms();
        search_position(10);
        U64 end = get_time_ms();
        printf("%llu ms\n", end - time);
        total += end - time;
    }
    printf("%llu ms on average\n", total / 100);
}

//#define BREACH_DEBUG

int main()
{
    init_all();

#ifdef BREACH_DEBUG
    debug();
#else
    uci_loop();
#endif

    return 0;
}