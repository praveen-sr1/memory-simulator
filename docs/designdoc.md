                    Memory Management Simulator 
                        Design Document
        
    1. System Overview & Architecture
        1.1 Dual-Mode Operation
        The system is controlled by a master switch in main.cpp (enum SimMode), creating two distinct memory lifecycles:

        1.Continuous Mode (Physical Heap):

            Direct Access: Calls my_malloc / my_free.

            Addressing: The CPU accesses memory using Physical Addresses (offsets from memory_start).

            Simulation: Focuses on heap fragmentation and allocator efficiency.

        2.Discontinuous Mode (Virtual Memory):

            Translation Layer: CPU requests are intercepted by the MMU.

            Addressing: The CPU generates Virtual Addresses. The MMU translates these to Physical Addresses using a Page Table.

            Simulation: Focuses on Paging, Page Faults, and Frame Eviction.

        Note: In both modes, the final Physical Address is passed to the Cache Hierarchy to simulate hardware access latencies.

        1.2 Data Flow Diagram


    ```mermaid
    graph TD
    User[User Command] --> Mode{Mode Check}
    
    %% Mode 1: Heap
    Mode -- Malloc/Free --> Heap[Physical Allocator]
    Heap -- Returns Ptr --> CPU_Phys[CPU (Physical Addr)]
    
    %% Mode 2: Virtual
    Mode -- Read/Write --> CPU_Virt[CPU (Virtual Addr)]
    CPU_Virt --> MMU[MMU Translate]
    MMU -- Hit --> PA[Physical Addr]
    MMU -- Fault --> Swap[Evict & Swap]
    Swap --> PA
    
    %% Convergence
    CPU_Phys --> L1[L1 Cache]
    PA --> L1
    L1 -- Miss --> L2[L2 Cache]
    L2 -- Miss --> RAM[Physical RAM Access]
    ```

    2. Subsystem Deep-Dive

        2.1 The Physical Allocator (Heap)
            
            Structure: Explicit Free List. The allocator maintains a doubly-linked list of Block headers embedded directly in the free memory.

            Initialization: init_memory creates one massive free block covering the entire arena (minus the first header).

            Allocation Logic (malloc):

                Search: Traverses the list using the selected strategy (FIRST_FIT is default).

                Splitting Threshold: A block is only split if the remaining space is large enough to hold a new Header + at least 1 byte of data.

                Formula: if (block->size >= requested_size + sizeof(Block) + 1)

            Deallocation Logic (free):

                Coalescing: When a block is freed, it checks prev and next. If they are is_free == true, it merges them to form a larger contiguous block, updating pointers to bypass the consumed headers.

        2.2 The Cache Hierarchy

            L1 Cache: 1KB size, FIFO Replacement.

                Implementation: Uses inserted_time timestamp. This timestamp is NOT updated on a cache hit (strictly First-In, First-Out).

            L2 Cache: 4KB size, LRU Replacement.

                Implementation: Uses last_used_time timestamp. This timestamp IS updated on every cache hit to keep the line "fresh."

            Inclusivity: The system implies non-inclusive/independent caches, but main.cpp chains them manually (l1->next_level = l2).

        2.3 The MMU (Virtual Memory)
        
            Multi-Process Support: Unlike simple simulators, your mmu.cpp uses a nested map structure: process_page_tables[pid][vpn]. This allows switching contexts (current_pid) without losing page mappings.

            Global Allocation: The Physical RAM is treated as a global pool of 4 Frames (NUM_FRAMES = 4).

            Eviction Policy (Global LRU):

                When a Page Fault occurs and memory is full, the MMU scans all frames (belonging to any PID).

                It looks up the last_used time in the owner's page table.

                The frame with the oldest timestamp (globally) is evicted, regardless of which process owns it.


    3. Low-Level Data Structures & Overhead Analysis

        3.1 struct Block (Exact Size Analysis)
             On a standard 64-bit architecture (which size_t implies), structure alignment padding significantly increases the size of your header beyond the raw sum of variables.

        C++ code :- 

            struct Block {
                int id;             // 4 bytes
                                    // [4 BYTES PADDING] - To align 'size_t' to 8-byte boundary
                size_t size;        // 8 bytes
                size_t data_size;   // 8 bytes
                bool is_free;       // 1 byte
                                    // [7 BYTES PADDING] - To align 'Block*' to 8-byte boundary
                Block* next;        // 8 bytes
                Block* prev;        // 8 bytes
            };

            Raw Size: 37 bytes.

            Actual Aligned Size: 48 bytes.

            Impact: Every malloc call consumes request + 48 bytes of physical memory.
        
        3.2 struct CacheLine

        C++ code :- 
            struct CacheLine {
                bool valid;          // 1 byte
                bool dirty;          // 1 byte
                                    // [2 BYTES PADDING]
                uint32_t tag;        // 4 bytes
                uint32_t last_used;  // 4 bytes
                uint32_t inserted;   // 4 bytes
            };
            
            Total Size: 16 bytes.

        3.3 struct PageTableEntry

        C++ code:- 
            struct PageTableEntry {
                bool valid;          // 1 byte
                                    // [3 BYTES PADDING]
                int frame_number;    // 4 bytes
                uint32_t last_used;  // 4 bytes
            };

            Total Size: 12 bytes (or 16 depending on compiler strictness).
    
    4. Algorithmic Logic (Step-by-Step)
        
        4.1 malloc Request Flow
            
            Input: User calls malloc(size).

            Alignment: size is often aligned to 8 bytes (implementation dependent, but best practice).

            Traversal: The Allocator iterates through free_list.

            Fit Check: if (block->is_free && block->size >= size).

            Split Decision:

                Case A (Exact Fit / Small Remainder): If remaining space < 49 bytes (Header + 1), mark entire block as USED. Internal fragmentation increases.

                Case B (Split): If remaining space is large, create a new Block header at (char*)block + 48 + size.

                Update new_block->size = old_size - size - 48.

                Update block->size = size.

                Insert new_block into linked list.

            Return: Pointer to (char*)block + 48.

    
         4.2 Page Fault Handling Flow

            Translation: CPU requests Virtual Address VA.

            Calculation: VPN = VA / 64, Offset = VA % 64.

            Lookup: check process_page_tables[pid][vpn].

            Fault Detected: entry.valid == false.

            Victim Selection:

                Iterate frame_map[0..3].

                Retrieve pid and vpn for each frame.

                Check last_used time for that page.

                Identify Frame F with smallest last_used.

            Eviction:

                Locate Victim Page Table Entry (using Victim PID/VPN).

                Set victim_entry.valid = false, victim_entry.frame = -1.

            Loading:

                Update frame_map[F] with New PID/VPN.

                Update New Page Table Entry: valid = true, frame = F, last_used = current_time.

            Retry: The translation is retried, now resulting in a Hit.

    
    5. CLI Command Reference

        The system interacts with the user via a command-line interface implemented in main.cpp. The commands are context-sensitive based on the active Simulation Mode.

        5.1 System-Wide Commands

            Available in all modes.

            Command	        Arguments	        Description
            
            mode	        1 or 2	            Switches the simulation kernel.
                                                1: Continuous (Heap Allocator).
                                                2: Discontinuous (Virtual Memory/MMU).

            read	        <virtual_addr>	    Simulates a CPU LOAD instruction. Triggers MMU translation (if Mode 2) and Cache Hierarchy  access.
            
            write	        <virtual_addr>	    Simulates a CPU STORE instruction. Marks CacheLine as dirty. Triggers MMU translation (if Mode 2).
            
            stats	        (none)	            Dumps current metrics:
                                                - Heap: Fragmentation, usage %.
                                                - MMU: Page hits/faults.
                                                - Cache: L1/L2 hits, misses, and hit rates.
        
        5.2 Continuous Mode Commands (Heap Manager)

            Active when current_mode == MODE_CONTINUOUS.  

            Command	        Arguments	        Description

            malloc	        <bytes>	            Requests a physical memory block.
            
            Returns: Virtual Address (Offset) or "Allocation Failed".

            free	        <block_id>	        Frees the block associated with the unique id. Triggers Coalescing (merging with adjacent free blocks).

            dump	        (none)	            Visualizes the physical memory map, showing the state (FREE/USED) of every block in the linked list.

        5.3 Discontinuous Mode Commands (Virtual Memory)
            
            Active when current_mode == MODE_DISCONTINUOUS.
            
            Command         Arguments           Description
            
            switch          <pid>                  Performs a Context Switch. 
                                                   Changes the current_pid register. 
                                                   Future memory accesses will use this PID's Page Table for translation.


    6. Functional Deep-Dive & Design Rationale

        This section explains the why behind the specific implementation choices found in the source code.

        6.1 Memory Management Strategies
        
        Why three allocation strategies?The allocator supports configurable strategies to demonstrate standard OS trade-offs:       
        
        First Fit (FIRST_FIT):
            
            Logic: Takes the first block that fits.
            
            Pro: Lowest search time ($O(1)$ best case).
            
            Con: tends to accumulate small "splinters" of free space at the beginning of memory (External Fragmentation).
            
        Best Fit (BEST_FIT):
            
            Logic: Scans the entire list to find the smallest block that fits the request.
            
            Pro: Minimizes wasted space in the specific block being split.
            
            Con: Slowest performance ($O(N)$ always) and creates tiny, unusable gaps.
            
        Worst Fit (WORST_FIT):
        
            Logic: picks the largest available block.
            
            Rationale: After splitting, the remaining chunk is likely still large enough to be useful for another allocation.
            
        
        6.2 The "Thrashing" Simulation
        
            Design Choice: NUM_FRAMES = 4 vs PAGE_SIZE = 64 [mmu.h]The system is intentionally constrained to hold only 4 physical pages (frames) in RAM simultaneously.
                
                The Effect: This forces the system into a high-pressure state very quickly.
                
                The Result: Users can easily observe "Thrashing"—where the MMU spends more time evicting and loading pages (Swapping) than actually executing CPU instructions. This is critical for testing the robustness of the Global LRU eviction algorithm.
                
        6.3 Cache Write Policies
        
            The simulator implements specific policies to maintain data consistency:
            
                Write-Allocate:
                    On a Write Miss, the system does not just write to RAM. It first loads the block from RAM into the L1/L2 cache, and then modifies it.
                    
                    Benefit: Subsequent writes to this variable will be fast L1 hits.
                    
                Write-Back (via dirty bit):
                    
                    When data is modified in the cache, it is not immediately written to RAM. It is marked dirty = true.
                    
                    The expensive write to Main Memory only happens when that specific line is evicted to make room for new data.
                    
        6.4 The Coalescing Algorithm (Anti-Fragmentation)
            
            File: physical_memory.cppWithout coalescing, a heap would eventually crumble into unusable small blocks. The free() function performs a specific check:
            
                Look Left: Is current->prev free? If yes, merge them.
                    
                Look Right: Is current->next free? If yes, merge them.
                
                Critical Detail: The merging math involves ptr arithmetic. When merging Block A and Block B, the new size is A.size + B.size + sizeof(Block Header). The header of Block B is "consumed" and becomes part of the raw payload space of the new merged block.
                
        6.5 Global vs. Local Page Replacement
            
            File: mmu.cppThe MMU implements Global Replacement.
                
                Scenario: Process A is active (switch 1), but RAM is full of pages owned by Process B.
                
                Behavior: The MMU will evict a page from Process B to make room for Process A if Process B's page has an older last_used timestamp.
                
                Implication: Processes compete for resources. One "noisy" process can degrade the performance of others by stealing their frames.