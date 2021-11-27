#include "page.h"

static struct supp_page_table *supp_page_table;

void init_pages()
{
  hash_init(&supp_page_table->page_table, hash_func, hash_less, NULL);
}

uint32_t hash_func(const struct hash_elem *e, void *aux UNUSED)
{
  struct page *page = hash_entry(e, struct page, elem);
  return page->address;
}

bool hash_less(const struct hash_elem *a, const struct hash_elem *b, void *aux)
{
  struct page *page_a = hash_entry(a, struct page, elem);
  struct page *page_b = hash_entry(b, struct page, elem);
  return page_a->address < page_b->address;
}