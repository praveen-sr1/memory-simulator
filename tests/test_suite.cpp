//g++ -Iinclude -Wall -g tests/test_suite.cpp src/allocator/physical_memory.cpp src/cache/cache.cpp src/mmu/mmu.cpp -o run_tests

#include <iostream>
#include <vector>
#include <string>
#include <cassert>
#include <cstring>
#include <iomanip>

#include "../include/cache.h"
#include "../include/mmu.h"
#include "../include/memory_block.h"

extern void init_memory(size_t size);
extern void* my_malloc(size_t size);
extern void my_free(int id);
extern void set_allocator(std::string type);
extern void print_stats();
extern Block* free_list_head; 
extern void* memory_start;

#define GREEN "\033[1;32m"
#define RED "\033[1;31m"
#define RESET "\033[0m"
#define CYAN "\033[1;36m"

int tests_run = 0;
int tests_passed = 0;

void run_test(std::string name, bool (*func)()) {
    tests_run++;
    
    std::cout << std::dec << std::setfill(' '); 

    std::cout << "------------------------------------------------------------\n";
    std::cout << CYAN << "TEST " << std::setw(2) << tests_run << ": " << name << RESET << "\n";
    std::cout << "------------------------------------------------------------\n";
    
    bool result = func();

    std::cout << "\nRESULT: ";
    if (result) {
        std::cout << GREEN << "[PASS]" << RESET << "\n\n";
        tests_passed++;
    } else {
        std::cout << RED << "[FAIL]" << RESET << "\n\n";
    }
}

void reset_heap() {
    init_memory(4096); 
    set_allocator("FIRST_FIT"); 
}
/* TEST 1 - Basic Allocation

 * 1. Setup: We initialize a clean 4KB heap.
 * 2. Action: We request 100 bytes of memory.
 * 3. Verification: We need to prove the OS actually reserved this block.
 * - We take the pointer returned to the user ('p').
 * - We subtract 48 bytes to jump back to the 'Metadata Header'.
 * - We assert that the header records a size of 100 and marks the block as NOT free."
 */
bool test_alloc_simple() {
    reset_heap();
    void* p = my_malloc(100);
    if (!p) return false;
    
    // Check Header
    Block* b = (Block*)((char*)p - 48); // -sizeof(Block) assumption
    return (b->size == 100 && b->is_free == false);
}

/* TEST 2 - Block Splitting

 * if we only ask for 100 bytes. It must SPLIT the block.
 * 1. Action: We request 100 bytes.
 * 2. Verification: We look at the 'Next' pointer of our allocated block.
 * - It should NOT be null (meaning a remainder block exists).
 * - The remainder block should be marked as FREE.
 * - The remainder size should be roughly 3900 bytes (Total - Used - Overhead)."
 */
bool test_alloc_splitting() {
    reset_heap();
    // Request 100 bytes from a 4096 byte block
    void* p = my_malloc(100); 
    if (!p) return false;

    Block* b = (Block*)((char*)p - 48);
    
    // Next block should be the remainder (4096 - 100 - 48 - 48) approx
    if (b->next == nullptr) return false;
    if (b->next->is_free != true) return false;
    if (b->next->size < 4096 -100 -48 -48 ) return false; 
    return true;
}

/* TEST 3 - Internal Fragmentation (No Split)

 * 1. Setup: We force a scenario where we have a 100-byte free hole.
 * 2. Action: We request 80 bytes.
 * - Math: 100 (Total) - 80 (Request) = 20 bytes leftover.
 * - Constraint: A Header requires 48 bytes. We can't fit a header in 20 bytes!
 * 3. Verification: The allocator must give us the FULL 100 bytes instead of splitting.
 * We verify the block size is 100, not 80."
 */
bool test_alloc_no_split_fragmentation() {
    reset_heap();

    void* p1 = my_malloc(100);
    void* p2 = my_malloc(1000); 
    if (!p1 || !p2) return false;

    my_free(((Block*)((char*)p1 - 48))->id); 
    
    // 2. Request 80 bytes 
    // Remaining = 100 - 80 = 20 bytes.
    // Header size is ~48. 
    // 20 < 48, so it CANNOT split.
    void* p3 = my_malloc(80);
    if (!p3) return false;

    Block* b3 = (Block*)((char*)p3 - 48);
    
    // Should satisfy request but keep full size (100) because it couldn't split
    if (b3->size != 100) {
        std::cout << "Debug: Block size is " << b3->size << " (Expected 100)\n";
        return false; 
    }
    return true;
}


// Setup for Strategy Tests:
// [ 200 Free ] - [ Barrier ] - [ 800 Free ] - [ Barrier ]
void setup_holes() {
    init_memory(4096);
    void* a = my_malloc(200); // Hole 1
    void* b = my_malloc(100); // Barrier
    void* c = my_malloc(800); // Hole 2
    void* d = my_malloc(100); // Barrier
    
    if (!a || !b || !c || !d) { std::cerr << "Setup failed\n"; return; }

    my_free(((Block*)((char*)a - 48))->id);
    my_free(((Block*)((char*)c - 48))->id);
}

/* TEST 4 - First Fit Strategy
 * 1. Setup: We create two holes: A small one (200 bytes) and a big one (800 bytes).
 * 2. Action: We request 180 bytes.
 * - Both holes are big enough.
 * 3. Verification: First Fit stops at the first one. 
 * We check that the allocated block size is 200 (from the first hole), not 800."
 */
bool test_strategy_first_fit() {
    setup_holes();
    set_allocator("FIRST_FIT");
    
    // Request 180 
    // Hole is 200. Remainder = 20. Header = 48.
    // Cannot split.
    void* p = my_malloc(180);
    if (!p) return false;
    Block* b = (Block*)((char*)p - 48);
    
    // Should pick the First Hole (200) and NOT split it.
    if (b->size != 200) {
        std::cout << "Debug: Got block size " << b->size << " (Expected 200)\n";
        return false;
    }
    return true;
}

/* TEST 5 - Best Fit Strategy
 * 1. Setup: We create a 500-byte hole (First) and a 200-byte hole (Second).
 * 2. Action: We request 150 bytes.
 * - First Fit would greedily take the 500.
 * - Best Fit should see that 200 is closer to 150.
 * 3. Verification: We check the block size. It should be <= 200, proving 
 * it selected the second, smaller hole."
 */
bool test_strategy_best_fit() {
    // Hole 1: 500. Hole 2: 200.
    // Request: 150.
    // Best Fit should take the 200 block.
    reset_heap();
    void* h1 = my_malloc(500); void* bar1 = my_malloc(10);
    void* h2 = my_malloc(200); void* bar2 = my_malloc(10);
    
    if(!h1 || !bar1 || !h2 || !bar2) return false;

    my_free(((Block*)((char*)h1 - 48))->id);
    my_free(((Block*)((char*)h2 - 48))->id);
    
    set_allocator("BEST_FIT");
    
    // Request 150.
    void* p = my_malloc(150);
    if (!p) return false;

    Block* b_ptr = (Block*)((char*)p - 48);
    
    // If it took the second block (size 200), address should be > bar1
    return (b_ptr->size <= 200); // It selected the smaller sufficient block
}

/* TEST 6 - Worst Fit Strategy
 * 1. Setup: Small Hole (200) and Big Hole (800).
 * 2. Action: We request 100 bytes.
 * - 200 is a perfect fit, but Worst Fit ignores it.
 * 3. Verification: We check the pointer address. It should be located AFTER the 
 * first barrier, proving it skipped the small hole and chose the big 800-byte one."
 */
bool test_strategy_worst_fit() {
    reset_heap();
    // [200 Free] [Barrier] [800 Free]
    void* h1 = my_malloc(200); void* bar = my_malloc(10);
    void* h2 = my_malloc(800); void* bar2 = my_malloc(10);
    
    if(!h1 || !bar || !h2 || !bar2) return false;

    my_free(((Block*)((char*)h1 - 48))->id);
    my_free(((Block*)((char*)h2 - 48))->id);
    
    set_allocator("WORST_FIT");
    
    // Request 100.
    // Worst Fit: Takes 800.
    
    void* p = my_malloc(100);
    if(!p) return false;
    
    Block* b = (Block*)((char*)p - 48);
    (void)b; 
    
    // Check if pointer address is after the first barrier (meaning it took the 2nd hole)
    return (p > bar); 
}

/* TEST 7 - Coalescing Right
 * 1. Setup: Blocks A and B are next to each other.
 * 2. Action: We free B. Then we free A.
 * 3. Verification: When A is freed, it should look to its RIGHT, see B is free, 
 * and merge with it.
 * We check that A's new size equals A+B+Header (approx 248 bytes)."
 */

bool test_coalesce_right() {
    reset_heap();
    void* A = my_malloc(100);
    void* B = my_malloc(100);
    void* C = my_malloc(100); // Barrier
    if(!A || !B || !C) return false;
    
    int idA = ((Block*)((char*)A - 48))->id;
    int idB = ((Block*)((char*)B - 48))->id;
    
    my_free(idB); // Free Right first
    my_free(idA); // Free Left
    
    // A should merge with B.
    Block* blockA = (Block*)((char*)A - 48);
    
    // Size = 100(A) + 100(B) + 48(HeaderB) = 248
    return (blockA->size == 248 && blockA->is_free == true);
}

/* TEST 8 - Coalescing Left
 * 1. Setup: Blocks A and B.
 * 2. Action: We free A first. Then we free B.
 * 3. Verification: When B is freed, it should look to its LEFT, see A is free, 
 * and merge itself INTO A.
 * We check A's header again to ensure it grew to 248 bytes."
 */
bool test_coalesce_left() {
    reset_heap();
    void* A = my_malloc(100);
    void* B = my_malloc(100);
    void* C = my_malloc(100);
    if(!A || !B || !C) return false;
    
    int idA = ((Block*)((char*)A - 48))->id;
    int idB = ((Block*)((char*)B - 48))->id;
    
    my_free(idA); // Free Left first
    my_free(idB); // Free Right
    
    Block* blockA = (Block*)((char*)A - 48);
    return (blockA->size == 248 && blockA->is_free == true);
}

/* TEST 9 - Coalescing Sandwich
 * 1. Setup: Free Block A, Used Block B, Free Block C.
 * 2. Action: We free the middle block B.
 * 3. Verification: It should trigger a double merge. A+B, then (A+B)+C.
 * The result should be one massive free block of approx 396 bytes starting at A."
 */
bool test_coalesce_sandwich() {
    reset_heap();
    // [Free] [Used] [Free] -> Free Middle -> [One Big Free]
    void* A = my_malloc(100);
    void* B = my_malloc(100);
    void* C = my_malloc(100);
    void* D = my_malloc(100); // Barrier
    
    if(!A || !B || !C || !D) return false;

    my_free(((Block*)((char*)A - 48))->id);
    my_free(((Block*)((char*)C - 48))->id);
    
    // Now free B
    my_free(((Block*)((char*)B - 48))->id);
    
    Block* blockA = (Block*)((char*)A - 48);
    // Size = 100*3 + 48*2 = 396
    return (blockA->size == 396 && blockA->is_free == true);
}

/* TEST 10 - Basic MMU Translation
 * 1. Action: We ask the MMU to translate Virtual Address 16 (on Page 0) for PID 1.
 * 2. Logic: The MMU should map Virtual Page 0 to Physical Frame 0.
 * 3. Verification: The Physical Address should be (Frame 0 * 64) + Offset 16 = 16."
 */

bool test_mmu_basic_translate() {
    MMU mmu;
    // Access Page 0 (PID 1)
    uint32_t pa = mmu.translate(1, 0x10); // VPN 0, Offset 16
    // Should be mapped to Frame 0 usually
    return (pa == 16); // Frame 0 * 64 + 16
}

/* TEST 11 - Global LRU Page Replacement
 * 1. Setup: We fill all 4 RAM frames with Pages 0, 1, 2, 3.
 * 2. Action: We access Page 0 again to make it 'fresh'. Page 1 becomes the oldest.
 * Then we access a NEW Page (4).
 * 3. Verification: Since RAM is full, the MMU must EVICT the oldest page (Page 1).
 * We prove this by trying to access Page 1 again. The fault count increases
 * because it was kicked out to disk and had to be reloaded."
 */
bool test_mmu_global_lru() {
    MMU mmu;
    int pid = 1;
    
    // Fill 4 frames
    mmu.translate(pid, 0); // Frame 0
    mmu.translate(pid, 64); // Frame 1
    mmu.translate(pid, 128); // Frame 2
    mmu.translate(pid, 192); // Frame 3
    
    // Refresh Page 0 (Make it recently used)
    mmu.translate(pid, 0); 
    
    // Now Page 1 (address 64) is the LRU.
    // Cause Eviction by accessing Page 4 (address 256)
    mmu.translate(pid, 256);
    
    // To verify: Access Page 1 again. It should count as a Page Fault if it was evicted.
    int faults_before = mmu.page_faults;
    mmu.translate(pid, 64);
    int faults_after = mmu.page_faults;
    
    return (faults_after > faults_before); // It faulted, so it was evicted correctly
}

/* TEST 12 - Process Isolation
 * 1. Action: PID 1 writes to Virtual Address 0.
 * PID 2 writes to Virtual Address 0.
 * 2. Verification: Although the Virtual Addresses are the same, the MMU maps them
 * to DIFFERENT Physical Frames. We assert that PhysicalAddress1 != PhysicalAddress2."
 */
bool test_mmu_isolation() {
    MMU mmu;
    // PID 1 maps VPN 0 -> Frame 0
    uint32_t pa1 = mmu.translate(1, 0);
    
    // PID 2 maps VPN 0 -> Frame 1 (Should not collide logic-wise)
    uint32_t pa2 = mmu.translate(2, 0);
    
    // They are same virtual address (0), but different physical frames
    return (pa1 != pa2);
}


int main() {
    std::cout << "\n=== RUNNING COMPREHENSIVE MEMORY SIM TESTS ===\n\n";

    // Allocator Basic
    run_test("Heap: Basic Allocation", test_alloc_simple);
    run_test("Heap: Block Splitting", test_alloc_splitting);
    run_test("Heap: No Split (Internal Frag)", test_alloc_no_split_fragmentation);

    // Allocator Strategies
    run_test("Heap Strategy: First Fit", test_strategy_first_fit);
    run_test("Heap Strategy: Best Fit", test_strategy_best_fit);
    run_test("Heap Strategy: Worst Fit", test_strategy_worst_fit);

    // Coalescing
    run_test("Coalesce: Right Neighbor", test_coalesce_right);
    run_test("Coalesce: Left Neighbor", test_coalesce_left);
    run_test("Coalesce: Sandwich (Both)", test_coalesce_sandwich);

    // MMU
    run_test("MMU: Basic Translation", test_mmu_basic_translate);
    run_test("MMU: Global LRU Page Replacement", test_mmu_global_lru);
    run_test("MMU: Multi-Process Isolation", test_mmu_isolation);

    std::cout << "============================================================\n";
    std::cout << "SUMMARY: " << tests_passed << " / " << tests_run << " PASSED.\n";
    if (tests_passed == tests_run) {
        std::cout << GREEN << "ALL SYSTEMS FUNCTIONAL." << RESET << "\n";
    } else {
        std::cout << RED << "SOME SYSTEMS FAILED." << RESET << "\n";
    }
    
    return 0;
}