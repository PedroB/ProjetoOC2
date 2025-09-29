#include "tlb.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "clock.h"
#include "constants.h"
#include "log.h"
#include "memory.h"
#include "page_table.h"

typedef struct {
  bool valid;
  bool dirty;
  uint64_t last_access;
  va_t virtual_page_number;
  pa_dram_t physical_page_number;
} tlb_entry_t;

tlb_entry_t tlb_l1[TLB_L1_SIZE];
tlb_entry_t tlb_l2[TLB_L2_SIZE];

uint64_t tlb_l1_hits = 0;
uint64_t tlb_l1_misses = 0;
uint64_t tlb_l1_invalidations = 0;

uint64_t tlb_l2_hits = 0;
uint64_t tlb_l2_misses = 0;
uint64_t tlb_l2_invalidations = 0;

uint64_t get_total_tlb_l1_hits() { return tlb_l1_hits; }
uint64_t get_total_tlb_l1_misses() { return tlb_l1_misses; }
uint64_t get_total_tlb_l1_invalidations() { return tlb_l1_invalidations; }

uint64_t get_total_tlb_l2_hits() { return tlb_l2_hits; }
uint64_t get_total_tlb_l2_misses() { return tlb_l2_misses; }
uint64_t get_total_tlb_l2_invalidations() { return tlb_l2_invalidations; }

void tlb_init() {
  memset(tlb_l1, 0, sizeof(tlb_l1));
  memset(tlb_l2, 0, sizeof(tlb_l2));
  tlb_l1_hits = 0;
  tlb_l1_misses = 0;
  tlb_l1_invalidations = 0;
  tlb_l2_hits = 0;
  tlb_l2_misses = 0;
  tlb_l2_invalidations = 0;
}

void tlb_invalidate(va_t virtual_page_number) {
  (void)(virtual_page_number);  // Suppress unused variable warning. You can
                                // delete this when implementing the actual
                                // function.
  // TODO: implement TLB entry invalidation.
}

pa_dram_t tlb_translate(va_t virtual_address, op_t op) {
  virtual_address &= VIRTUAL_ADDRESS_MASK;
  const va_t vpn = vpn_of(virtual_address);
  const uint64_t off = offset_of(virtual_address);
  // Check L1 TLB
  int i1 = tlb_find(tlb_l1, TLB_L1_SIZE, vpn);
  increment_time(TLB_L1_LATENCY_NS);  // custo do acesso ao L1 (hit ou miss)
  if (i1 >= 0) {
    // L1 hit
    tlb_l1_hits++;
    tlb_l1[i1].last_access = get_time();
    if (op == OP_WRITE) tlb_l1[i1].dirty = true;
    return make_pa(tlb_l1[i1].physical_page_number, off);
  }
  // L1 miss
  tlb_l1_misses++;
  // Check L2 TLB
  int i2 = tlb_find(tlb_l2, TLB_L2_SIZE, vpn);
  increment_time(TLB_L2_LATENCY_NS);  // custo do acesso ao L2 (hit ou miss)
  if (i2 >= 0) {
    // L2 hit
    tlb_l2_hits++;    
    tlb_l2[i2].last_access = get_time();
    if (op == OP_WRITE) tlb_l2[i2].dirty = true;
    // Promote to L1
    tlb_entry_t entry = tlb_l2[i2];
    tlb_insert(tlb_l1, TLB_L1_SIZE, entry);
    return make_pa(entry.physical_page_number, off);
  }
  // L2 miss
  tlb_l2_misses++;
  return page_table_translate(virtual_address, op);
}