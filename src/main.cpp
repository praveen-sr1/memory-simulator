#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <iomanip>

#include "../include/memory_block.h" 
#include "../include/cache.h"       
#include "../include/mmu.h"          

extern void init_memory(size_t size);
extern void* my_malloc(size_t size);
extern void my_free(int id);
extern void dump_memory();
extern void print_stats();
extern void set_allocator(std::string type);
extern void* memory_start; 

int current_pid = 1;


Cache* l1_cache = nullptr;
Cache* l2_cache = nullptr;
MMU* mmu = nullptr;

enum SimMode { MODE_CONTINUOUS, MODE_DISCONTINUOUS };
SimMode current_mode;

uint32_t ptr_to_va(void* ptr) {
    if (ptr == nullptr) return 0xFFFFFFFF; 
    return (uint32_t)((char*)ptr - (char*)memory_start);
}

void init_cache_system() {
    if (l2_cache) delete l2_cache;
    if (l1_cache) delete l1_cache;
    
    // L2: 4KB, 64B block, 4-way, LRU
    l2_cache = new Cache(2, 4096, 64, 4, nullptr, "LRU");
    // L1: 1KB, 64B block, 2-way, FIFO
    l1_cache = new Cache(1, 1024, 64, 2, l2_cache, "FIFO");
}

void select_mode() {
    std::cout << "\n=========================================\n";
    std::cout << "      MEMORY SIMULATOR CONFIGURATION      \n";
    std::cout << "=========================================\n";
    std::cout << "Select Operation Mode:\n";
    std::cout << "  1. Continuous Allocation (Heap Management)\n";
    std::cout << "  2. Discontinuous Allocation (Virtual Memory / Paging)\n";
    std::cout << "> ";
    
    int choice;
    std::cin >> choice;
    
    if (choice == 1) {
        current_mode = MODE_CONTINUOUS;
        
        // --- SUB-MENU FOR CONTINUOUS MODE ---
        std::cout << "\n--- Continuous Mode Setup ---\n";
        std::cout << "Select Allocation Strategy:\n";
        std::cout << "  1. First Fit\n";
        std::cout << "  2. Best Fit\n";
        std::cout << "  3. Worst Fit\n";
        std::cout << "> ";
        
        int strat;
        std::cin >> strat;
        std::cin.ignore(); // Clear newline

        init_memory(1024);   // Initialize Heap
        init_cache_system(); // Initialize Cache

        if (strat == 1) set_allocator("first");
        else if (strat == 2) set_allocator("best");
        else if (strat == 3) set_allocator("worst");
        else {
            std::cout << "Invalid choice. Defaulting to First Fit.\n";
            set_allocator("first");
        }
        std::cout << ">> Mode 1 Ready: Continuous Allocator + Cache\n";
    } 
    else {
        current_mode = MODE_DISCONTINUOUS;
        std::cin.ignore(); // Clear newline
        
        if (mmu) delete mmu;
        mmu = new MMU();     // Initialize MMU
        init_cache_system(); // Initialize Cache
        
        std::cout << "\n--- Discontinuous Mode Setup ---\n";
        std::cout << ">> Mode 2 Ready: Virtual Memory (Paging) + Cache\n";
    }
}

void cpu_access(uint32_t addr, bool is_write) {
    uint32_t final_phys_addr;

    if (current_mode == MODE_DISCONTINUOUS) {
        // Mode 2: Translate VA -> PA using MMU
        std::cout << "\n[CPU (PID " << current_pid << ")] Requesting (VM) " 
                  << (is_write ? "WRITE" : "READ") << " at VA 0x" << std::hex << addr << std::dec << "\n";
        
        final_phys_addr = mmu->translate(current_pid, addr);
    } 
    else {
        // Mode 1: Direct Access (Flat Memory)
        std::cout << "\n[CPU] Requesting (Phys) " 
                  << (is_write ? "WRITE" : "READ") << " at PA 0x" << std::hex << addr << std::dec << "\n";
        
        final_phys_addr = addr;
    }

   
    if (l1_cache) l1_cache->access(final_phys_addr, is_write);
}

void print_help() {
    std::cout << "\nAvailable Commands:\n";
    std::cout << "  mode               : Restart and switch configuration\n";
    std::cout << "  read <addr>        : CPU Read (Triggers Cache)\n";
    std::cout << "  write <addr>       : CPU Write (Triggers Cache)\n";
    
    if (current_mode == MODE_CONTINUOUS) {
        std::cout << "  malloc <bytes>     : Allocate block\n";
        std::cout << "  free <id>          : Free block\n";
        std::cout << "  dump               : Show heap layout\n";
    } else {
        std::cout << "  switch <pid>       : Context switch process\n";
    }
    
    std::cout << "  stats              : Show system stats\n";
    std::cout << "  exit               : Quit\n";
}

int main() {
    select_mode();
    print_help();

    std::string line, command;

    while (true) {
        std::cout << "\n> ";
        if (!std::getline(std::cin, line)) break;
        std::stringstream ss(line);
        ss >> command;

        if (command == "exit") break;
        else if (command == "help") print_help();
        else if (command == "mode") {
            select_mode();
            print_help();
        }
        
        // --- SHARED COMMANDS ---
        else if (command == "read") {
            uint32_t addr; ss >> std::hex >> addr;
            cpu_access(addr, false);
        }
        else if (command == "write") {
            uint32_t addr; ss >> std::hex >> addr;
            cpu_access(addr, true);
        }
        else if (command == "stats") {
            if (current_mode == MODE_CONTINUOUS) {
                std::cout << "=== CONTINUOUS MEMORY STATS ===\n";
                print_stats(); 
            }
            if (current_mode == MODE_DISCONTINUOUS && mmu) {
                mmu->print_stats();
            }
            if (l1_cache) {
                l1_cache->print_stats();
                if (l2_cache) l2_cache->print_stats();
            }
        }

        // --- MODE 1: CONTINUOUS ---
        else if (current_mode == MODE_CONTINUOUS) {
            if (command == "malloc") {
                size_t size; ss >> size;
                void* ptr = my_malloc(size);
                if (ptr) {
                    uint32_t va = ptr_to_va(ptr);
                    std::cout << ">> [OS] Block allocated at Address: 0x" << std::hex << va << std::dec << "\n";
                }
            }
            else if (command == "free") {
                int id; ss >> id;
                my_free(id);
            }
            else if (command == "dump") dump_memory();
        }
        
        // --- MODE 2: DISCONTINUOUS ---
        else if (current_mode == MODE_DISCONTINUOUS) {
             if (command == "switch") {
                int pid; ss >> std::dec >> pid;
                current_pid = pid;
                std::cout << ">> Context Switch: Now running Process " << current_pid << "\n";
            }
        }
    }
    return 0;
}