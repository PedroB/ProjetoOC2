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

// Helper function to find a TLB entry by virtual page number
static int tlb_l1_find_entry(va_t virtual_page_number) {
  for (int i = 0; i < TLB_L1_SIZE; i++) {
    if (tlb_l1[i].valid && tlb_l1[i].virtual_page_number == virtual_page_number) {
      return i;
    }
  }
  return -1; // Not found
}

static int tlb_l2_find_entry(va_t virtual_page_number) {
  for (int i = 0; i < TLB_L2_SIZE; i++) {
    if (tlb_l2[i].valid && tlb_l2[i].virtual_page_number == virtual_page_number) {
      return i;
    }
  }
  return -1; // Not found
}

// Helper function to find LRU entry for replacement
static int tlb_l1_find_lru_entry() {
  int lru_index = 0;
  uint64_t oldest_time = tlb_l1[0].last_access;
  
  for (int i = 1; i < TLB_L1_SIZE; i++) {
    if (!tlb_l1[i].valid) {
      return i; // Found an invalid (empty) entry
    }
    if (tlb_l1[i].last_access < oldest_time) {
      oldest_time = tlb_l1[i].last_access;
      lru_index = i;
    }
  }
  return lru_index;
}

void tlb_invalidate(va_t virtual_page_number) {
  // Add L1 TLB latency for invalidation operation
  increment_time(TLB_L1_LATENCY_NS);
  
  // Invalidate entry in L1 TLB
  int index = tlb_l1_find_entry(virtual_page_number);
  if (index != -1) {
    tlb_l1[index].valid = false;
    tlb_l1_invalidations++;
  }
  // Invalidate entry in L2 TLB
  index = tlb_l2_find_entry(virtual_page_number);
  if (index != -1) {
    tlb_l2[index].valid = false;
    tlb_l2_invalidations++;
  }
  
  // TODO: Also invalidate in L2 TLB when implemented
}

pa_dram_t tlb_translate(va_t virtual_address, op_t op) {
  // Extract virtual page number and offset
  virtual_address &= VIRTUAL_ADDRESS_MASK;
  va_t virtual_page_number = (virtual_address >> PAGE_SIZE_BITS) & PAGE_INDEX_MASK;
  va_t virtual_page_offset = virtual_address & PAGE_OFFSET_MASK;
  
  // Add L1 TLB latency first
  increment_time(TLB_L1_LATENCY_NS);
  
  // Check L1 TLB first
  int l1_index = tlb_l1_find_entry(virtual_page_number);
  
  if (l1_index != -1) {
    // L1 TLB hit
    tlb_l1_hits++;
    tlb_l1[l1_index].last_access = get_time();
    
    // Update dirty bit if this is a write operation
    if (op == OP_WRITE) {
      tlb_l1[l1_index].dirty = true;
    }
    
    // Return physical address
    return (tlb_l1[l1_index].physical_page_number << PAGE_SIZE_BITS) | virtual_page_offset;
  }
  
  // L1 TLB miss - need to check page table
  tlb_l1_misses++;
  
  // Get translation from page table (this handles page faults if needed)
  pa_dram_t physical_address = page_table_translate(virtual_address, op);
  pa_dram_t physical_page_number = physical_address >> PAGE_SIZE_BITS;
  
  // Find entry to replace in L1 TLB
  int replace_index = tlb_l1_find_lru_entry();
  
  // Write back dirty entry if needed
  if (tlb_l1[replace_index].valid && tlb_l1[replace_index].dirty) {
    write_back_tlb_entry(tlb_l1[replace_index].physical_page_number << PAGE_SIZE_BITS);
  }
  
  // Update L1 TLB entry
  tlb_l1[replace_index].valid = true;
  tlb_l1[replace_index].virtual_page_number = virtual_page_number;
  tlb_l1[replace_index].physical_page_number = physical_page_number;
  tlb_l1[replace_index].dirty = (op == OP_WRITE);
  tlb_l1[replace_index].last_access = get_time();
  
  return physical_address;
}
