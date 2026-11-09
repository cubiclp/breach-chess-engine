#include "types.h"
#include "board.h"
#include "attacks.h"
#include "movegen.h"
#include "eval.h"
#include "search.h"
#include "uci.h"


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
    U64 time = get_time_ms();
    parse_fen(tricky_position);
    search_position(10);
    U64 end = get_time_ms();
    printf("%llu ms", end - time);
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