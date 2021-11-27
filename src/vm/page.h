#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct supp_page_table {
    struct hash page_table;
    // supplementary features(?)
};

struct page {
    uint32_t address;
    bool read_only;
    struct hash_elem elem;
};