#include "../../include/mmu.h"
#include <iomanip>

MMU::MMU() {
    time_counter = 0;
    for (int i = 0; i < NUM_FRAMES; i++) {
        frame_map[i].occupied = false;
        frame_map[i].pid = -1;
    }
    std::cout << "MMU Initialized. Multi-Process Support Enabled.\n";
}

uint32_t MMU::translate(int pid, uint32_t virtual_addr) {
    time_counter++;
    
    uint32_t vpn = virtual_addr / PAGE_SIZE;
    uint32_t offset = virtual_addr % PAGE_SIZE;

    // 1. Select the Page Table for this PID
    // Note: process_page_tables[pid] automatically creates a new table if pid is new
    std::map<uint32_t, PageTableEntry>& current_table = process_page_tables[pid];

    // 2. Check for Hit
    if (current_table[vpn].valid) {
        page_hits++;
        current_table[vpn].last_used = time_counter; // Update LRU
        
        int pfn = current_table[vpn].frame_number;
        return (pfn * PAGE_SIZE) + offset;
    } 
    else {
        // 3. Page Fault
        page_faults++;
        std::cout << "[MMU] PAGE FAULT (PID " << pid << ", VPN " << vpn << ")! ";
        
        handle_page_fault(pid, vpn);
        
        // Retry
        int pfn = current_table[vpn].frame_number;
        return (pfn * PAGE_SIZE) + offset;
    }
}

void MMU::handle_page_fault(int pid, uint32_t new_vpn) {
    // A. Find a Free Frame
    int target_frame = -1;
    for (int i = 0; i < NUM_FRAMES; i++) {
        if (!frame_map[i].occupied) {
            target_frame = i;
            break;
        }
    }

    // B. If Full, Evict LRU (Global LRU)
    if (target_frame == -1) {
        uint32_t min_time = -1;
        int victim_frame = -1;

        // Scan all frames to find the oldest one
        for (int i = 0; i < NUM_FRAMES; i++) {
            int owner_pid = frame_map[i].pid;
            uint32_t owner_vpn = frame_map[i].vpn;
            
            // Look up the last_used time in the OWNER'S page table
            uint32_t last_used = process_page_tables[owner_pid][owner_vpn].last_used;
            
            if (last_used < min_time) {
                min_time = last_used;
                victim_frame = i;
            }
        }
        
        target_frame = victim_frame;
        
        // Invalidate the Victim
        int victim_pid = frame_map[target_frame].pid;
        uint32_t victim_vpn = frame_map[target_frame].vpn;
        
        std::cout << "Evicting PID " << victim_pid << "'s Page " << victim_vpn 
                  << " (Frame " << target_frame << ")... ";
                  
        process_page_tables[victim_pid][victim_vpn].valid = false;
        process_page_tables[victim_pid][victim_vpn].frame_number = -1;
    }

    // C. Load New Page
    std::cout << "Loading PID " << pid << "'s Page " << new_vpn 
              << " into Frame " << target_frame << ".\n";
    
    // Update Page Table
    process_page_tables[pid][new_vpn].valid = true;
    process_page_tables[pid][new_vpn].frame_number = target_frame;
    process_page_tables[pid][new_vpn].last_used = time_counter;
    
    // Update Frame Map
    frame_map[target_frame].occupied = true;
    frame_map[target_frame].pid = pid;
    frame_map[target_frame].vpn = new_vpn;
}

void MMU::print_stats() {
    std::cout << "--- MMU Stats ---\n";
    std::cout << "Active Processes: " << process_page_tables.size() << "\n";
    std::cout << "Page Faults: " << page_faults << "\n";
    std::cout << "Page Hits:   " << page_hits << "\n";
}