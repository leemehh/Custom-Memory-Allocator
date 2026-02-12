// memory_allocator_fixed.c
// Custom fixed-size memory pool allocator
// COMPILE: gcc -o allocator memory_allocator_fixed.c
// RUN: ./allocator

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

// ============================================================================
// CONFIGURATION
// ============================================================================

#define POOL_SIZE (1024 * 64)     // 64KB
#define ALIGNMENT 8
#define MAGIC_COOKIE 0xDEADBEEF
#define VISUALIZER_WIDTH 60

// ============================================================================
// BLOCK HEADER STRUCTURE
// ============================================================================

typedef struct block_header {
    uint32_t magic;
    size_t size;
    uint8_t free;
    struct block_header* next;
    struct block_header* prev;
    uint32_t checksum;
} block_header_t;

#define HEADER_SIZE ((sizeof(block_header_t) + ALIGNMENT - 1) & ~(ALIGNMENT - 1))

// ============================================================================
// MEMORY POOL - UNION FOR ALIGNMENT
// ============================================================================

union aligned_pool {
    uint8_t bytes[POOL_SIZE];
    uint64_t aligner;
};

static union aligned_pool memory_pool_union;
static uint8_t* memory_pool = memory_pool_union.bytes;
static block_header_t* free_list_head = NULL;
static size_t total_allocated = 0;
static size_t total_free = POOL_SIZE;
static int allocation_count = 0;
static int fragmentation_score = 0;

// ============================================================================
// FUNCTION PROTOTYPES (DECLARATIONS)
// ============================================================================
// Put these at the top so the compiler knows about all functions

uint32_t calculate_checksum(block_header_t* header);
int verify_header(block_header_t* header);
void init_allocator(void);
void* my_malloc(size_t size);
void my_free(void* ptr);
void calculate_fragmentation(void);
void print_memory_map(void);
void print_statistics(void);
void demo_basic(void);

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

uint32_t calculate_checksum(block_header_t* header) {
    uint32_t sum = 0;
    uint8_t* bytes = (uint8_t*)header;
    for (size_t i = 0; i < sizeof(block_header_t) - sizeof(uint32_t); i++) {
        sum += bytes[i];
    }
    return sum;
}

int verify_header(block_header_t* header) {
    if (header->magic != MAGIC_COOKIE) {
        printf("[ERROR] CORRUPTION: Invalid magic cookie at %p\n", (void*)header);
        return 0;
    }
    if (header->checksum != calculate_checksum(header)) {
        printf("[ERROR] CORRUPTION: Invalid checksum at %p\n", (void*)header);
        return 0;
    }
    return 1;
}

// ============================================================================
// INITIALIZATION
// ============================================================================

void init_allocator(void) {
    printf("\n");
    printf("============================================================\n");
    printf("     CUSTOM MEMORY ALLOCATOR - INITIALIZING                \n");
    printf("============================================================\n\n");
    
    block_header_t* first_block = (block_header_t*)memory_pool;
    first_block->magic = MAGIC_COOKIE;
    first_block->size = POOL_SIZE - HEADER_SIZE;
    first_block->free = 1;
    first_block->next = NULL;
    first_block->prev = NULL;
    first_block->checksum = calculate_checksum(first_block);
    
    free_list_head = first_block;
    total_free = first_block->size;
    
    printf("Memory Pool: %zu KB (%zu bytes)\n", POOL_SIZE / 1024, POOL_SIZE);
    printf("Block Header: %zu bytes\n", HEADER_SIZE);
    printf("Alignment: %d bytes\n", ALIGNMENT);
    printf("Allocator initialized successfully.\n\n");
}

// ============================================================================
// CORE ALLOCATION FUNCTION
// ============================================================================

void* my_malloc(size_t size) {
    if (size == 0) return NULL;
    
    size_t aligned_size = (size + ALIGNMENT - 1) & ~(ALIGNMENT - 1);
    
    printf("[ALLOC] Requesting %zu bytes (aligned: %zu bytes)...\n", size, aligned_size);
    
    block_header_t* current = free_list_head;
    block_header_t* best_fit = NULL;
    
    while (current != NULL) {
        if (!verify_header(current)) {
            return NULL;
        }
        
        if (current->free && current->size >= aligned_size) {
            best_fit = current;
            break;
        }
        current = current->next;
    }
    
    if (best_fit == NULL) {
        printf("[FAIL] Allocation FAILED: No suitable block found\n");
        return NULL;
    }
    
    printf("  Found free block at %p (%zu bytes free)\n", 
           (void*)best_fit, best_fit->size);
    
    size_t remaining_size = best_fit->size - aligned_size;
    
    if (remaining_size >= HEADER_SIZE + ALIGNMENT) {
        block_header_t* new_block = (block_header_t*)((uint8_t*)best_fit + HEADER_SIZE + aligned_size);
        new_block->magic = MAGIC_COOKIE;
        new_block->size = remaining_size - HEADER_SIZE;
        new_block->free = 1;
        new_block->next = best_fit->next;
        new_block->prev = best_fit;
        new_block->checksum = calculate_checksum(new_block);
        
        if (best_fit->next != NULL) {
            best_fit->next->prev = new_block;
        }
        best_fit->next = new_block;
        
        printf("  Splitting block: allocated %zu, new free block at %p (%zu bytes)\n",
               aligned_size, (void*)new_block, new_block->size);
    }
    
    best_fit->free = 0;
    best_fit->size = aligned_size;
    best_fit->checksum = calculate_checksum(best_fit);
    
    total_allocated += aligned_size;
    total_free -= aligned_size;
    allocation_count++;
    
    printf("[OK] Allocated %zu bytes at %p\n", aligned_size, 
           (void*)((uint8_t*)best_fit + HEADER_SIZE));
    
    return (void*)((uint8_t*)best_fit + HEADER_SIZE);
}

// ============================================================================
// CORE FREE FUNCTION
// ============================================================================

void my_free(void* ptr) {
    if (ptr == NULL) {
        printf("[WARN] Attempted to free NULL pointer\n");
        return;
    }
    
    block_header_t* header = (block_header_t*)((uint8_t*)ptr - HEADER_SIZE);
    
    printf("[FREE] Freeing block at %p (header at %p)...\n", ptr, (void*)header);
    
    if (!verify_header(header)) {
        printf("[ERROR] Attempted to free corrupted block!\n");
        return;
    }
    
    if (header->free) {
        printf("[WARN] Double free detected!\n");
        return;
    }
    
    header->free = 1;
    header->checksum = calculate_checksum(header);
    
    total_allocated -= header->size;
    total_free += header->size;
    allocation_count--;
    
    printf("  Freed %zu bytes\n", header->size);
    
    // Coalesce with next block
    if (header->next != NULL && header->next->free) {
        block_header_t* next = header->next;
        
        if (verify_header(next)) {
            printf("  Coalescing with next block at %p\n", (void*)next);
            
            header->size += HEADER_SIZE + next->size;
            header->next = next->next;
            
            if (next->next != NULL) {
                next->next->prev = header;
            }
            
            header->checksum = calculate_checksum(header);
        }
    }
    
    // Coalesce with previous block
    if (header->prev != NULL && header->prev->free) {
        block_header_t* prev = header->prev;
        
        if (verify_header(prev)) {
            printf("  Coalescing with previous block at %p\n", (void*)prev);
            
            prev->size += HEADER_SIZE + header->size;
            prev->next = header->next;
            
            if (header->next != NULL) {
                header->next->prev = prev;
            }
            
            prev->checksum = calculate_checksum(prev);
            header = prev;
        }
    }
    
    printf("[OK] Block freed and coalesced successfully\n");
}

// ============================================================================
// FRAGMENTATION ANALYSIS
// ============================================================================

void calculate_fragmentation(void) {
    block_header_t* current = free_list_head;
    int free_block_count = 0;
    size_t largest_free_block = 0;
    size_t total_free_space = 0;
    
    while (current != NULL) {
        if (verify_header(current) && current->free) {
            free_block_count++;
            total_free_space += current->size;
            if (current->size > largest_free_block) {
                largest_free_block = current->size;
            }
        }
        current = current->next;
    }
    
    if (free_block_count > 1 && total_free_space > 0) {
        fragmentation_score = 100 - (largest_free_block * 100 / total_free_space);
    } else {
        fragmentation_score = 0;
    }
}

// ============================================================================
// MEMORY VISUALIZATION
// ============================================================================

void print_memory_map(void) {
    printf("\n");
    printf("+----------------------------------------------------------+\n");
    printf("|                 MEMORY MAP VISUALIZATION                 |\n");
    printf("+----------------------------------------------------------+\n");
    
    block_header_t* current = (block_header_t*)memory_pool;
    int block_num = 0;
    
    while (1) {
        if (!verify_header(current)) {
            printf("| WARNING: CORRUPTED BLOCK DETECTED!                       |\n");
            break;
        }
        
        int visual_size = (current->size * VISUALIZER_WIDTH) / POOL_SIZE;
        if (visual_size < 1) visual_size = 1;
        
        char bar[VISUALIZER_WIDTH + 1];
        memset(bar, ' ', VISUALIZER_WIDTH);
        
        if (current->free) {
            memset(bar, '.', visual_size);
        } else {
            memset(bar, '#', visual_size);
        }
        bar[VISUALIZER_WIDTH] = '\0';
        
        printf("| #%02d %s |\n", block_num, bar);
        printf("|     Size: %7zu bytes | %s | %p-%p |\n", 
               current->size,
               current->free ? "FREE      " : "ALLOCATED ",
               (void*)current,
               (void*)((uint8_t*)current + HEADER_SIZE + current->size));
        
        if (current->next == NULL) break;
        current = current->next;
        block_num++;
        
        if (block_num > 100) break;
    }
    
    printf("+----------------------------------------------------------+\n");
}

// ============================================================================
// STATISTICS
// ============================================================================

void print_statistics(void) {
    calculate_fragmentation();
    
    printf("\n");
    printf("+----------------------------------------------------------+\n");
    printf("|                 ALLOCATOR STATISTICS                    |\n");
    printf("+----------------------------------------------------------+\n");
    printf("| Total Pool Size: %28zu bytes               |\n", POOL_SIZE);
    printf("| Total Allocated: %28zu bytes               |\n", total_allocated);
    printf("| Total Free:      %28zu bytes               |\n", total_free);
    printf("| Active Allocations: %25d                    |\n", allocation_count);
    printf("| Fragmentation Score: %23d%%                   |\n", fragmentation_score);
    printf("| Header Size:     %28zu bytes               |\n", HEADER_SIZE);
    printf("| Alignment:       %28d bytes               |\n", ALIGNMENT);
    printf("+----------------------------------------------------------+\n");
}

// ============================================================================
// DEMO FUNCTIONS
// ============================================================================

void demo_basic(void) {
    printf("\n");
    printf("============================================================\n");
    printf("              BASIC ALLOCATION DEMO                        \n");
    printf("============================================================\n");
    
    void* p1 = my_malloc(128);
    void* p2 = my_malloc(256);
    void* p3 = my_malloc(64);
    
    print_memory_map();
    print_statistics();
    
    my_free(p2);
    print_memory_map();
    
    my_free(p1);
    my_free(p3);
    print_memory_map();
    print_statistics();
}

// ============================================================================
// MAIN FUNCTION
// ============================================================================

int main(void) {
    printf("\n");
    printf("============================================================\n");
    printf("                                                           \n");
    printf("         CUSTOM MEMORY ALLOCATOR - v1.0                   \n");
    printf("                                                           \n");
    printf("   Fixed-size pool . First-fit . Block splitting          \n");
    printf("   Fragmentation tracking . Coalescing . Visualization    \n");
    printf("                                                           \n");
    printf("============================================================\n");
    
    init_allocator();
    demo_basic();
    
    printf("\n");
    printf("============================================================\n");
    printf("              ALLOCATOR SHUTDOWN                           \n");
    printf("============================================================\n");
    print_statistics();
    printf("\nMemory allocator terminated.\n\n");
    
    return 0;
}