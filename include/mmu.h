#ifndef MMU_H
#define MMU_H

#include <map>
#include <vector>
#include <cstdint>
#include <iostream>

// CONFIGURATION
const size_t PAGE_SIZE = 64;      // Small page size for easy testing
const int NUM_FRAMES = 4;         // RAM can only hold 4 pages at once (Forces swapping)

struct PageTableEntry {
    bool valid;         // Is this page currently in RAM ?
    int frame_number;   // If valid, which physical frame is it in?
    uint32_t last_used; // Timestamp for LRU
};

struct FrameOwner {
    int pid;        // Which process owns this frame?
    uint32_t vpn;   // Which virtual page is it?
    bool occupied;  // Is this frame in use?
};

class MMU {
public:
    int page_faults = 0;
    int page_hits = 0;
    
    MMU();
    
    // Core function: Converts Virtual Address -> Physical Address
    uint32_t translate(int pid, uint32_t virtual_addr);
    
    void print_stats();

private:
    uint32_t time_counter;
    
    // The Page Table: Maps Virtual Page Number (VPN) -> Entry
    std::map<uint32_t, PageTableEntry> page_table;
    
    // Physical Memory Tracking: Which VPN owns which Frame? (Target for eviction)
    // index = frame_number, value = vpn
    std::map<int, std::map<uint32_t, PageTableEntry>> process_page_tables;

    FrameOwner frame_map[NUM_FRAMES];

    void handle_page_fault(int pid, uint32_t vpn);
};

#endif
