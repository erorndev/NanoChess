#include "attacks.h"
#include "book.h"
#include "evaluate.h"
#include "search.h"
#include "tt.h"
#include "uci.h"
#include "zobrist.h"

int main(int argc, char **argv) {
    init_zobrist();
    init_attacks();
    init_eval();
    init_book();
    init_tt(32);
    init_search();

    uci_loop(argc, argv);

    free_tt();
    return 0;
}
