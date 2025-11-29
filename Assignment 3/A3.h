#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
// Include your Headers below

#include <stddef.h>
#include <stdint.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

// You are not allowed to use the function malloc and calloc directly.

#ifndef TYPES_ENUM_H
#define TYPES_ENUM_H

enum TYPES
{
    BEST,
    WORST,
    FIRST,
    NEXT,
    BUDDY,
    NONE
};

enum TYPES current_type = NONE;

#endif

// ========================= NEXT FIT =========================

#ifndef TYPES_ENUM_H
#define TYPES_ENUM_H

enum TYPES
{
    BEST,
    WORST,
    FIRST,
    NEXT,
    BUDDY,
    NONE
};

enum TYPES current_type = NONE;

#endif


#define NOTFREE_N 100

typedef struct header_N
{
    size_t size;
    int is_free;
    struct header_N *next;
    struct header_N *prev;
} header_N_t;

static inline size_t align8(size_t sz)
{
    return sz;
}

#define MSS_N sizeof(header_N_t)

static header_N_t *HEAD_N = NULL;
static header_N_t *last_alloc_block_N = NULL;
static size_t page_size_cached_N = 0;

static size_t page_size_or_default_N()
{
    if (page_size_cached_N)
        return page_size_cached_N;
    page_size_cached_N = 4096;
    return page_size_cached_N;
}

static inline header_N_t *header_from_payload_N(void *payload)
{
    if (!payload)
        return NULL;
    return (header_N_t *)((char *)payload - sizeof(header_N_t));
}

static inline void *payload_from_header_N(header_N_t *h)
{
    if (!h)
        return NULL;
    return (void *)((char *)h + sizeof(header_N_t));
}

static header_N_t *append_new_region_N(size_t min_payload)
{
    size_t hdr = sizeof(header_N_t);
    size_t total_needed = align8(min_payload + hdr);
    size_t ps = page_size_or_default_N();
    size_t alloc_size = ((total_needed + ps - 1) / ps) * ps;

    void *region = mmap(NULL, alloc_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (region == MAP_FAILED)
    {
        perror("mmap failed");
        return NULL;
    }

    header_N_t *h = (header_N_t *)region;
    h->size = alloc_size - hdr;
    h->is_free = 1;
    h->next = NULL;
    h->prev = NULL;

    if (!HEAD_N)
    {
        HEAD_N = h;
    }
    else
    {
        header_N_t *cur = HEAD_N;
        while (cur->next)
            cur = cur->next;
        cur->next = h;
        h->prev = cur;
    }

    if (!last_alloc_block_N)
        last_alloc_block_N = h;
    return h;
}

static void split_block_if_needed_N(header_N_t *block, size_t req_size)
{
    if (!block)
        return;
    size_t aligned_req = align8(req_size);
    if (block->size < aligned_req)
        return;
    size_t leftover = block->size - aligned_req - sizeof(header_N_t);
    if (leftover >= MSS_N)
    {
        header_N_t *right = (header_N_t *)((char *)payload_from_header_N(block) + aligned_req);
        right->size = leftover;
        right->is_free = 1;
        right->next = block->next;
        right->prev = block;
        if (block->next)
            block->next->prev = right;
        block->next = right;
        block->size = aligned_req;
    }
}

static header_N_t *find_fit_from_N(header_N_t *start, size_t req_size)
{
    if (!start)
        return NULL;
    size_t aligned_req = align8(req_size);
    header_N_t *cur = start;
    while (cur)
    {
        if (cur->is_free != NOTFREE_N && cur->size >= aligned_req)
        {
            return cur;
        }
        cur = cur->next;
    }
    return NULL;
}

void *malloc_next_fit(size_t size)
{

    current_type = NEXT;

    if (size == 0)
        return NULL;
    size_t req = align8(size);

    if (!HEAD_N)
    {
        if (!append_new_region_N(req))
            return NULL;
    }

    header_N_t *start = last_alloc_block_N ? last_alloc_block_N : HEAD_N;
    header_N_t *found = find_fit_from_N(start, req);

    if (!found)
    {
        if (start != HEAD_N)
            found = find_fit_from_N(HEAD_N, req);
    }

    if (!found)
    {
        header_N_t *newh = append_new_region_N(req);
        if (!newh)
            return NULL;
        found = newh;
        if (found->size < req)
            return NULL;
    }

    split_block_if_needed_N(found, req);

    found->is_free = NOTFREE_N;
    last_alloc_block_N = found;

    return payload_from_header_N(found);
}

static void remove_node_N(header_N_t *node)
{
    if (!node)
        return;
    if (node->prev)
        node->prev->next = node->next;
    else
        HEAD_N = node->next;
    if (node->next)
        node->next->prev = node->prev;
    node->next = node->prev = NULL;
}

static header_N_t *coalesce_N(header_N_t *block)
{
    if (!block)
        return NULL;
    // coalesce_N right
    header_N_t *right = block->next;
    if (right && right->is_free != NOTFREE_N)
    {
        // merge right into block
        block->size += sizeof(header_N_t) + right->size;
        block->next = right->next;
        if (right->next)
            right->next->prev = block;
        right->next = right->prev = NULL; // detach
    }

    // coalesce_N left
    header_N_t *left = block->prev;
    if (left && left->is_free != NOTFREE_N)
    {
        if (block == last_alloc_block_N)
        {
            last_alloc_block_N = left;
        }
        left->size += sizeof(header_N_t) + block->size;
        left->next = block->next;
        if (block->next)
            block->next->prev = left;
        block->next = block->prev = NULL; // detach
        return left;
    }

    return block;
}

void my_free_N(void *ptr)
{
    if (!ptr)
        return; // free(NULL) is a no-op
    header_N_t *h = header_from_payload_N(ptr);

    // sanity: check that 'h' belongs to our list
    header_N_t *cur = HEAD_N;
    int found = 0;
    while (cur)
    {
        if (cur == h)
        {
            found = 1;
            break;
        }
        cur = cur->next;
    }
    if (!found)
    {
        return;
    }

    if (h->is_free != NOTFREE_N)
    {
        return;
    }

    h->is_free = 1;
    header_N_t *merged = coalesce_N(h);
    (void)merged;
}

static void print_list_internal_N(void)
{
    fprintf(stderr, "---- allocator list ----\n");
    header_N_t *cur = HEAD_N;
    int idx = 0;
    while (cur)
    {
        fprintf(stderr, "[%d] hdr=%p payload=%p size=%zu %s\n", idx, (void *)cur, payload_from_header_N(cur), cur->size, cur->is_free != NOTFREE_N ? "FREE" : "USED");
        cur = cur->next;
        idx++;
    }
    fprintf(stderr, "last_alloc_block_N=%p\n", (void *)last_alloc_block_N);
    fprintf(stderr, "------------------------\n");
}

static inline header_N_t *get_last_alloc_block_for_test_N(void) { return last_alloc_block_N; }
static inline header_N_t *get_list_head_for_test_N(void) { return HEAD_N; }
static inline void debug_print_list_N(void) { print_list_internal_N(); }

// ========================= FIRST FIT =========================


#ifndef TYPES_ENUM_H
#define TYPES_ENUM_H

enum TYPES {
    BEST,
    WORST,
    FIRST,
    NEXT,
    BUDDY,
    NONE
};

enum TYPES current_type = NONE;

#endif


#define NOTFREE_FF 100


static inline size_t PASS_FUNCTION_FF(size_t sz)
{
    return sz;
}


typedef struct block_header_FF
{
    size_t size;
    int free;
    struct block_header_FF *next;
} block_header_FF_t;

#define MIN_SPLIT_SIZE_FF sizeof(block_header_FF_t)

static block_header_FF_t *HEAD_FF = NULL;

static size_t page_size_or_default_FF()
{
    return 4096;
    // long ps = sysconf(_SC_PAGESIZE);
    // return (ps > 0) ? (size_t)ps : 4096;
}

static inline void *header_to_payload_FF(block_header_FF_t *h)
{
    return (void *)((char *)h + sizeof(block_header_FF_t));
}

static inline block_header_FF_t *payload_to_header_FF(void *p)
{
    return (block_header_FF_t *)((char *)p - sizeof(block_header_FF_t));
}

static void split_block_FF(block_header_FF_t *b, size_t needed)
{
    if (b->size >= needed + sizeof(block_header_FF_t) + MIN_SPLIT_SIZE_FF)
    {
        char *new_block_addr = (char *)header_to_payload_FF(b) + needed;
        block_header_FF_t *nb = (block_header_FF_t *)new_block_addr;
        nb->size = b->size - needed - sizeof(block_header_FF_t);
        nb->free = 1;
        nb->next = b->next;

        b->size = needed;
        b->next = nb;
    }
}

static block_header_FF_t *find_first_fit(size_t needed, block_header_FF_t **prev_out)
{
    block_header_FF_t *curr = HEAD_FF;
    block_header_FF_t *prev = NULL;
    while (curr)
    {
        if (curr->free != NOTFREE_FF && curr->size >= needed)
        {
            if (prev_out)
                *prev_out = prev;
            return curr;
        }
        prev = curr;
        curr = curr->next;
    }
    if (prev_out)
        *prev_out = prev;
    return NULL;
}

static block_header_FF_t *request_from_os_FF(size_t wanted_payload)
{
    size_t ps = page_size_or_default_FF();
    /* Compute total size: header + requested payload. Round up to pages. */
    size_t total_needed = sizeof(block_header_FF_t) + wanted_payload;
    size_t alloc_size = ((total_needed + ps - 1) / ps) * ps;
    if (alloc_size == 0)
        alloc_size = ps;

    void *m = mmap(NULL, alloc_size, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    if (m == MAP_FAILED)
    {
        printf("ERROR: mmap failed\n");
        return NULL;
    }

    block_header_FF_t *b = (block_header_FF_t *)m;
    b->size = alloc_size - sizeof(block_header_FF_t);
    b->free = 1;
    b->next = NULL;

    if (!HEAD_FF)
    {
        HEAD_FF = b;
    }
    else
    {
        block_header_FF_t *t = HEAD_FF;
        while (t->next)
            t = t->next;
        t->next = b;
    }
    return b;
}

static void try_coalesce_FF(block_header_FF_t *b)
{
    while (b && b->next)
    {
        char *b_end = (char *)header_to_payload_FF(b) + b->size;
        char *next_hdr = (char *)b->next;
        if (b->next->free != NOTFREE_FF && b_end == (char *)next_hdr)
        {
            b->size += sizeof(block_header_FF_t) + b->next->size;
            b->next = b->next->next;
        }
        else
        {
            break;
        }
    }
}

void *malloc_first_fit(size_t size)
{

    current_type = FIRST ;

    if (size == 0)
        return NULL;
    size_t needed = PASS_FUNCTION_FF(size);
    block_header_FF_t *prev = NULL;
    block_header_FF_t *f = find_first_fit(needed, &prev);
    if (!f)
    {
        block_header_FF_t *new_block = request_from_os_FF(needed);
        if (!new_block)
            return NULL;
        f = find_first_fit(needed, &prev);
        if (!f)
        {
            printf("ERROR: mmap returned NULL\n");
            return NULL;
        }
    }
    split_block_FF(f, needed);
    f->free = NOTFREE_FF;
    return header_to_payload_FF(f);
}

void my_free_FF(void *ptr)
{
    if (!ptr)
        return;
    block_header_FF_t *h = payload_to_header_FF(ptr);
    if (h->size == 0)
        return;
    h->free = 1;
    try_coalesce_FF(h);
    block_header_FF_t *curr = HEAD_FF;
    block_header_FF_t *prev = NULL;
    while (curr && curr != h)
    {
        prev = curr;
        curr = curr->next;
    }
    if (prev && prev->free == 1)
    {
        try_coalesce_FF(prev);
    }
}

int offset_of_block_FF(block_header_FF_t *b)
{
    return (int)((char *)b - (char *)HEAD_FF);
}

void print_list_FF()
{
    printf("-------------------------------\n");
    printf("Free list:\n");
    block_header_FF_t *curr = HEAD_FF;
    while (curr)
    {
        printf(" Block at offset %d: size=%zu, free=%d\n",
               offset_of_block_FF(curr), curr->size, curr->free == NOTFREE_FF ? 0 : 1);
        curr = curr->next;
    }
    printf("-------------------------------\n");
}

// ========================= BUDDY FIT =========================

#ifndef TYPES_ENUM_H
#define TYPES_ENUM_H

enum TYPES {
    BEST,
    WORST,
    FIRST,
    NEXT,
    BUDDY,
    NONE
};

enum TYPES current_type = NONE;

#endif


#define NOTFREE_BD 100

/* Align size to 8 bytes */
static inline size_t PASS_BD(size_t sz)
{
    return sz;
}

#define MSS_BD 8 // Minimum segment size (smallest block size)

#define MAX_ORDERS_BD 16

typedef struct block_header_BD
{
    struct block_header_BD *next;
    struct block_header_BD *prev;
    unsigned int order;
    unsigned int is_free;
} block_header_BD_t;

/* Globals for arena
arena_base_BD: start address of the memory region (mmap).
arena_size_BD: size of that region (the allocator uses a single arena here).
max_order_BD: highest order used for this arena (computed at init).
free_lists_BD: static array of free-list heads, one per order
All static variables are zero-initialized before main() starts so free_lists_BD[] is NULL initially.
*/
static void *arena_base_BD = NULL;
static size_t arena_size_BD = 0;
static unsigned int max_order_BD = 0; // highest order index (<= MAX_ORDERS_BD-1)
static size_t header_size_BD = 0;
static block_header_BD_t *free_lists_BD[MAX_ORDERS_BD];

/* Helper: get system page size (default 4096) */
static size_t page_size_or_default_BD()
{
    return 16384;
    // long ps = sysconf(_SC_PAGESIZE);
    // return (ps > 0) ? (size_t)ps : 4096;
}

/* Compute block size (bytes) for an order */
static inline size_t block_size_for_order_BD(unsigned int order)
{
    return (size_t)MSS_BD << order;
}

/* Remove a block from its free list */
static void remove_from_free_list_BD(block_header_BD_t *b)
{
    if (!b)
        return;
    unsigned int o = b->order;
    if (b->prev)
        b->prev->next = b->next;
    if (b->next)
        b->next->prev = b->prev;
    if (free_lists_BD[o] == b)
        free_lists_BD[o] = b->next;
    b->next = b->prev = NULL;
}

/* Insert a block at head of free list for its order */
static void insert_to_free_list_BD(block_header_BD_t *b)
{
    unsigned int o = b->order;
    b->is_free = 1;
    b->prev = NULL;
    b->next = free_lists_BD[o];
    if (free_lists_BD[o])
        free_lists_BD[o]->prev = b;
    free_lists_BD[o] = b;
}

/* Initialize arena and free lists on first use */
static void buddy_init_if_needed()
{
    if (arena_base_BD)
        return;

    size_t ps = page_size_or_default_BD();
    arena_size_BD = ps;

    /* compute max_order_BD so that MSS_BD << max_order_BD == arena_size_BD */
    size_t ratio = arena_size_BD / MSS_BD;
    if ((arena_size_BD % MSS_BD) != 0)
    {
        printf("arena size (%zu) not divisible by MSS_BD (%d)\n",
               arena_size_BD, MSS_BD);
        exit(1);
    }

    unsigned int o = 0;
    while ((1u << o) < ratio)
        o++;
    if (o >= MAX_ORDERS_BD)
    {
        printf("required max_order_BD %u exceeds MAX_ORDERS_BD-1 (%d)\n", o, MAX_ORDERS_BD - 1);
        exit(1);
    }
    max_order_BD = o; /* (MSS_BD << max_order_BD) == arena_size_BD */

    /* mmap the arena (page-aligned) */
    void *m = mmap(NULL, arena_size_BD, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (m == MAP_FAILED)
    {
        printf("mmap failed in buddy_init_if_needed\n");
        exit(1);
    }
    arena_base_BD = m;

    /* compute header size aligned to 8 for safe payload alignment */
    header_size_BD = PASS_BD(sizeof(block_header_BD_t));

    /* one big free block of max_order_BD at arena_base_BD */
    block_header_BD_t *initial = (block_header_BD_t *)arena_base_BD;
    initial->order = max_order_BD;
    initial->is_free = 1;
    initial->next = initial->prev = NULL;
    insert_to_free_list_BD(initial);
}

/* Given a block pointer,
compute byte offset of block header b measured from arena_base_BD
Example: if arena_base_BD == 0x1000 and b == 0x1080, this returns 0x80 (128).
used in finding a block’s buddy
Uses uintptr_t to do pointer to integer safely.
The result is unsigned;
if b is before arena_base_BD the subtraction underflows
    ensure b actually lies inside the arena
    before calling this (the allocator checks this elsewhere).*/
static inline size_t offset_of_block_BD(block_header_BD_t *b)
{
    return (size_t)((uintptr_t)b - (uintptr_t)arena_base_BD);
}

/* Returns the offset of the buddy of the block that starts at offset and has size block_size
B = block_size is a power of two (e.g. 32, 64, 128...)
B has a single 1 bit at position k and zeros elsewhere.
In any 2B region (aligned to 2B),
the two B-sized blocks have offsets that differ exactly by B (the bit at position k)
XORing flips that bit: offset ^ B toggles the kth bit and thus jumps to the other half of that 2B region: the buddy.

Ex:
B = 128. Offsets (in decimal) of the two buddies in a 256-byte region could be 0 and 128.
0 ^ 128 = 128, 128 ^ 128 = 0

For block at offset 384 (384 = 256 + 128), buddy is 384 ^ 128 = 512

Preconditions for correctness:
block_size must be a power of two.
arena_base_BD / arena offsets must align so that within the arena the 2B alignment boundaries are consistent — i.e., the arena must be sized and aligned to powers of two that match the buddy scheme. In this code that holds because arena_size_BD = MSS_BD * 2^max_order_BD and mmap returns a page-aligned region whose size is the page size.

Edge cases:
The computed buddy offset might be outside [0, arena_size_BD) — always check bounds before using it (the allocator does this).
XOR by itself doesn’t check whether the buddy is actually free or same order — it only gives the candidate address.
*/
static inline size_t buddy_offset(size_t offset, size_t block_size)
{
    return offset ^ block_size;
}

/* Given an offset, return block header pointer */
static inline block_header_BD_t *block_from_offset_BD(size_t offset)
{
    return (block_header_BD_t *)((char *)arena_base_BD + offset);
}

/* Find the smallest block order (size class) that can
contain the requested payload plus the allocator header,
taking alignment into account. */
static int order_for_request_BD(size_t req_payload)
{
    size_t total = PASS_BD(req_payload + header_size_BD);
    unsigned int order = 0;
    while (order <= max_order_BD)
    {
        if (block_size_for_order_BD(order) >= total)
            return (int)order;
        order++;
    }
    return -1; /* too large for this arena */
}

/* returns a block header b whose block size equals block_size_for_order_BD(target_order).
If a free block of that exact order already exists, it is removed and returned.
If not, the function finds a larger free block and repeatedly splits it into
two equal halves until the desired order is reached.
Each split produces a “left” half (kept for further splitting/return)
and a “right” half (inserted back into the free-list for that smaller order). */
static block_header_BD_t *alloc_block_of_order_BD(unsigned int target_order)
{
    unsigned int o = target_order;
    while (o <= max_order_BD && free_lists_BD[o] == NULL)
        o++;
    if (o > max_order_BD)
        return NULL; /* no block available */

    // free_lists_BD[o] != NULL;
    block_header_BD_t *b = free_lists_BD[o];
    remove_from_free_list_BD(b);

    while (o > target_order)
    {
        o--;
        size_t half_size = block_size_for_order_BD(o);
        block_header_BD_t *left = b;
        block_header_BD_t *right = (block_header_BD_t *)((char *)left + half_size);

        left->order = o;
        left->is_free = 1;
        left->next = left->prev = NULL;

        right->order = o;
        right->is_free = 1;
        right->next = right->prev = NULL;

        insert_to_free_list_BD(right);
        b = left;
    }

    b->is_free = NOTFREE_BD;
    b->next = b->prev = NULL;
    return b;
}

void *malloc_buddy_alloc(size_t size)
{
    current_type = BUDDY ;
    
    if (size == 0)
        return NULL;
    buddy_init_if_needed();

    int target_order = order_for_request_BD(size);
    if (target_order < 0)
    {
        printf("request size %zu too large for buddy allocator\n", size);
        return NULL;
    }

    block_header_BD_t *b = alloc_block_of_order_BD((unsigned int)target_order);
    if (!b)
    {
        printf("no suitable block available for order %d\n", target_order);
        return NULL;
    }

    void *payload = (void *)((char *)b + header_size_BD);
    return payload;
}

static void try_coalesce_BD(block_header_BD_t *b)
{
    unsigned int o = b->order;
    while (o < max_order_BD)
    {
        size_t b_off = offset_of_block_BD(b);
        size_t b_size = block_size_for_order_BD(o);
        size_t bud_off = buddy_offset(b_off, b_size);
        block_header_BD_t *buddy = block_from_offset_BD(bud_off);

        if ((uintptr_t)buddy < (uintptr_t)arena_base_BD ||
            (uintptr_t)buddy >= (uintptr_t)arena_base_BD + arena_size_BD)
        {
            break;
        }
        if (buddy->is_free != NOTFREE_BD || buddy->order != o)
        {
            break;
        }

        remove_from_free_list_BD(buddy);
        block_header_BD_t *merged = (b_off < bud_off) ? b : buddy;
        merged->order = o + 1;
        merged->is_free = 1;
        merged->next = merged->prev = NULL;

        b = merged;
        o = b->order;
    }

    insert_to_free_list_BD(b);
}

void my_free_BD(void *ptr)
{
    if (!ptr)
        return;
    if (!arena_base_BD)
    {
        printf("free before init\n");
        return;
    }

    block_header_BD_t *b = (block_header_BD_t *)((char *)ptr - header_size_BD);

    uintptr_t b_addr = (uintptr_t)b;
    if (b_addr < (uintptr_t)arena_base_BD || b_addr >= (uintptr_t)arena_base_BD + arena_size_BD)
    {
        printf("pointer %p not from arena\n", ptr);
        return;
    }
    if (b->is_free != NOTFREE_BD)
    {
        printf("double free of %p\n", ptr);
        return;
    }

    b->is_free = 1;
    b->next = b->prev = NULL;

    try_coalesce_BD(b);
}

void print_list_BD()
{
    if (!arena_base_BD)
    {
        printf("not initialized\n");
        return;
    }
    printf("Free lists (max_order_BD=%u):\n", max_order_BD);
    for (unsigned int o = 0; o <= max_order_BD; ++o)
    {
        printf(" order %2u (size %4zu):", o, block_size_for_order_BD(o));
        block_header_BD_t *cur = free_lists_BD[o];
        while (cur)
        {
            printf(" [%p(off=%4zu)]", (void *)cur, offset_of_block_BD(cur));
            cur = cur->next;
        }
        printf("\n");
    }
}


// ========================= BEST FIT =========================

#ifndef TYPES_ENUM_H
#define TYPES_ENUM_H

enum TYPES {
    BEST,
    WORST,
    FIRST,
    NEXT,
    BUDDY,
    NONE
};

enum TYPES current_type = NONE;

#endif

#define NOTFREE_B 100

static inline size_t PASS_B(size_t sz)
{
    return sz;
}


static size_t page_size_or_default_B()
{
    return 4096;
}

typedef struct block_header_B
{
    size_t size;
    int is_free;
    struct block_header_B *linear_left_block;
    struct block_header_B *linear_right_block;
} block_header_B_t;

typedef struct avl_node_B
{
    block_header_B_t *header;
    size_t size;
    void *header_address;
    int height;
    struct avl_node_B *left;
    struct avl_node_B *right;
} avl_node_B_t;

#define KUCH_BHI_B sizeof(block_header_B_t)

static avl_node_B_t *free_tree_root_B = NULL;
static block_header_B_t *first_block_B = NULL;

static inline void *header_to_payload_B(block_header_B_t *h)
{
    return (void *)((char *)h + sizeof(block_header_B_t));
}

static inline block_header_B_t *payload_to_header_B(void *p)
{
    return (block_header_B_t *)((char *)p - sizeof(block_header_B_t));
}

static inline int max_B(int a, int b)
{
    return (a > b) ? a : b;
}

static inline int get_height_B(avl_node_B_t *node)
{
    return node ? node->height : 0;
}

static inline int get_balance_B(avl_node_B_t *node)
{
    return node ? (get_height_B(node->left) - get_height_B(node->right)) : 0;
}

static inline void update_height_B(avl_node_B_t *node)
{
    if (node)
    {
        node->height = 1 + max_B(get_height_B(node->left), get_height_B(node->right));
    }
}

static avl_node_B_t *rotate_right_B(avl_node_B_t *y)
{
    avl_node_B_t *x = y->left;
    avl_node_B_t *T2 = x->right;

    x->right = y;
    y->left = T2;

    update_height_B(y);
    update_height_B(x);

    return x;
}

static avl_node_B_t *rotate_left_B(avl_node_B_t *x)
{
    avl_node_B_t *y = x->right;
    avl_node_B_t *T2 = y->left;

    y->left = x;
    x->right = T2;

    update_height_B(x);
    update_height_B(y);

    return y;
}

static avl_node_B_t *create_avl_node_B(block_header_B_t *header)
{
    // Use mmap to allocate memory for AVL node
    avl_node_B_t *node = (avl_node_B_t *)mmap(NULL, sizeof(avl_node_B_t),
                                              PROT_READ | PROT_WRITE,
                                              MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (node == MAP_FAILED)
    {
        return NULL;
    }

    node->header = header;
    node->size = header->size;
    node->header_address = (void *)header;
    node->height = 1;
    node->left = NULL;
    node->right = NULL;

    return node;
}

static void free_avl_node_B(avl_node_B_t *node)
{
    if (node)
    {
        munmap(node, sizeof(avl_node_B_t));
    }
}

/* Compare two nodes for AVL tree ordering
   Returns: -1 if n1 < n2, 0 if n1 == n2, 1 if n1 > n2 */
static int compare_nodes_B(size_t size1, void *addr1, size_t size2, void *addr2)
{
    if (size1 < size2)
        return -1;
    if (size1 > size2)
        return 1;
    // Sizes are equal, compare addresses
    if (addr1 < addr2)
        return -1;
    if (addr1 > addr2)
        return 1;
    return 0;
}

static avl_node_B_t *avl_insert_B(avl_node_B_t *root, avl_node_B_t *new_node)
{
    if (!root)
    {
        return new_node;
    }

    int cmp = compare_nodes_B(new_node->size, new_node->header_address,
                              root->size, root->header_address);

    if (cmp <= 0)
    {
        root->left = avl_insert_B(root->left, new_node);
    }
    else
    {
        root->right = avl_insert_B(root->right, new_node);
    }

    update_height_B(root);
    int balance = get_balance_B(root);

    if (balance > 1 && compare_nodes_B(new_node->size, new_node->header_address,
                                       root->left->size, root->left->header_address) <= 0)
    {
        return rotate_right_B(root);
    }

    if (balance < -1 && compare_nodes_B(new_node->size, new_node->header_address,
                                        root->right->size, root->right->header_address) > 0)
    {
        return rotate_left_B(root);
    }

    if (balance > 1 && compare_nodes_B(new_node->size, new_node->header_address,
                                       root->left->size, root->left->header_address) > 0)
    {
        root->left = rotate_left_B(root->left);
        return rotate_right_B(root);
    }

    if (balance < -1 && compare_nodes_B(new_node->size, new_node->header_address,
                                        root->right->size, root->right->header_address) <= 0)
    {
        root->right = rotate_right_B(root->right);
        return rotate_left_B(root);
    }

    return root;
}

/* Find the node with minimum value (leftmost node) */
static avl_node_B_t *find_min_node_B(avl_node_B_t *node)
{
    while (node && node->left)
    {
        node = node->left;
    }
    return node;
}

/* Delete a specific node from AVL tree */
static avl_node_B_t *avl_delete_B(avl_node_B_t *root, size_t size, void *header_address)
{
    if (!root)
    {
        return NULL;
    }

    int cmp = compare_nodes_B(size, header_address, root->size, root->header_address);

    if (cmp < 0)
    {
        root->left = avl_delete_B(root->left, size, header_address);
    }
    else if (cmp > 0)
    {
        root->right = avl_delete_B(root->right, size, header_address);
    }
    else
    {
        // Found the node to delete
        if (!root->left || !root->right)
        {
            avl_node_B_t *temp = root->left ? root->left : root->right;

            if (!temp)
            {
                temp = root;
                root = NULL;
            }
            else
            {
                *root = *temp;
            }
            free_avl_node_B(temp);
        }
        else
        {
            avl_node_B_t *temp = find_min_node_B(root->right);

            root->header = temp->header;
            root->size = temp->size;
            root->header_address = temp->header_address;

            root->right = avl_delete_B(root->right, temp->size, temp->header_address);
        }
    }

    if (!root)
    {
        return NULL;
    }

    update_height_B(root);
    int balance = get_balance_B(root);

    // Left-Left case
    if (balance > 1 && get_balance_B(root->left) >= 0)
    {
        return rotate_right_B(root);
    }

    // Left-Right case
    if (balance > 1 && get_balance_B(root->left) < 0)
    {
        root->left = rotate_left_B(root->left);
        return rotate_right_B(root);
    }

    // Right-Right case
    if (balance < -1 && get_balance_B(root->right) <= 0)
    {
        return rotate_left_B(root);
    }

    // Right-Left case
    if (balance < -1 && get_balance_B(root->right) > 0)
    {
        root->right = rotate_right_B(root->right);
        return rotate_left_B(root);
    }

    return root;
}

/* Find the best fit block (smallest block >= requested_size) */
static avl_node_B_t *find_best_fit_B(avl_node_B_t *root, size_t requested_size)
{
    avl_node_B_t *best = NULL;
    avl_node_B_t *current = root;

    while (current)
    {
        if (current->size >= requested_size)
        {
            best = current;
            current = current->left; // Try to find smaller suitable block
        }
        else
        {
            current = current->right; // Need larger block
        }
    }

    return best;
}

/* Print AVL tree (in-order traversal) - for debugging */
static void print_avl_tree_helper_B(avl_node_B_t *node, int depth)
{
    if (!node)
        return;

    print_avl_tree_helper_B(node->right, depth + 1);

    for (int i = 0; i < depth; i++)
    {
        printf("    ");
    }
    printf("Size: %zu, Addr: %p, Height: %d\n", node->size, node->header_address, node->height);

    print_avl_tree_helper_B(node->left, depth + 1);
}

static void print_list_B()
{
    printf("\n===== FREE LIST (AVL Tree) =====\n");
    if (!free_tree_root_B)
    {
        printf("Empty\n");
    }
    else
    {
        print_avl_tree_helper_B(free_tree_root_B, 0);
    }
    printf("================================\n\n");
}

/* Print all blocks in linear order - for debugging */
static void print_linear_B()
{
    printf("\n===== LINEAR MEMORY BLOCKS =====\n");
    block_header_B_t *current = first_block_B;
    int count = 0;

    while (current)
    {
        printf("Block %d: Addr=%p, Size=%zu, Free=%s\n",
               count++, (void *)current, current->size,
               current->is_free != NOTFREE_B ? "YES" : "NO");
        current = current->linear_right_block;
    }
    printf("================================\n\n");
}

/* Initialize the memory system with one large block */
static bool initialize_memory_B()
{
    if (first_block_B)
    {
        return 1; // Already initialized
    }

    size_t page_size = page_size_or_default_B();
    void *mem = mmap(NULL, page_size, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    if (mem == MAP_FAILED)
    {
        return 0;
    }
    first_block_B = (block_header_B_t *)mem;
    first_block_B->size = page_size - sizeof(block_header_B_t);
    first_block_B->is_free = 1;
    first_block_B->linear_left_block = NULL;
    first_block_B->linear_right_block = NULL;
    avl_node_B_t *node = create_avl_node_B(first_block_B);
    if (!node)
    {
        munmap(mem, page_size);
        first_block_B = NULL;
        return 0;
    }

    free_tree_root_B = avl_insert_B(free_tree_root_B, node);

    return 1;
}

void *malloc_best_fit(size_t size)
{
    current_type = BEST ;
    
    if (size == 0)
    {
        return NULL;
    }

    if (!initialize_memory_B())
    {
        return NULL;
    }

    size_t aligned_size = PASS_B(size);

    avl_node_B_t *best_node = find_best_fit_B(free_tree_root_B, aligned_size);

    if (!best_node)
    {
        size_t page_size = page_size_or_default_B();
        size_t alloc_size = aligned_size + sizeof(block_header_B_t);

        size_t num_pages = (alloc_size + page_size - 1) / page_size;
        size_t mmap_size = num_pages * page_size;

        void *mem = mmap(NULL, mmap_size, PROT_READ | PROT_WRITE,
                         MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

        if (mem == MAP_FAILED)
        {
            return NULL;
        }

        block_header_B_t *new_block = (block_header_B_t *)mem;
        new_block->size = mmap_size - sizeof(block_header_B_t);
        new_block->is_free = 1;
        new_block->linear_left_block = NULL;
        new_block->linear_right_block = NULL;

        block_header_B_t *last = first_block_B;
        while (last && last->linear_right_block)
        {
            last = last->linear_right_block;
        }

        if (last)
        {
            last->linear_right_block = new_block;
            new_block->linear_left_block = last;
        }

        avl_node_B_t *node = create_avl_node_B(new_block);
        if (!node)
        {
            munmap(mem, mmap_size);
            return NULL;
        }

        free_tree_root_B = avl_insert_B(free_tree_root_B, node);
        best_node = node;
    }

    block_header_B_t *block = best_node->header;
    free_tree_root_B = avl_delete_B(free_tree_root_B, block->size, (void *)block);

    if (block->size >= aligned_size + sizeof(block_header_B_t) + KUCH_BHI_B)
    {
        size_t remaining_size = block->size - aligned_size - sizeof(block_header_B_t);
        block_header_B_t *new_block = (block_header_B_t *)((char *)block + sizeof(block_header_B_t) + aligned_size);
        new_block->size = remaining_size;
        new_block->is_free = 1;
        new_block->linear_left_block = block;
        new_block->linear_right_block = block->linear_right_block;

        if (block->linear_right_block)
        {
            block->linear_right_block->linear_left_block = new_block;
        }
        block->linear_right_block = new_block;
        block->size = aligned_size;

        avl_node_B_t *node = create_avl_node_B(new_block);
        if (node)
        {
            free_tree_root_B = avl_insert_B(free_tree_root_B, node);
        }
    }

    block->is_free = NOTFREE_B;

    return header_to_payload_B(block);
}

void my_free_B(void *ptr)
{
    if (!ptr)
    {
        return;
    }

    block_header_B_t *block = payload_to_header_B(ptr);

    if (block->is_free != NOTFREE_B)
    {
        return;
    }

    block->is_free = 1;

    block_header_B_t *left = block->linear_left_block;
    block_header_B_t *right = block->linear_right_block;

    block_header_B_t *merged_block = block;
    size_t merged_size = block->size;

    if (left && left->is_free != NOTFREE_B)
    {
        free_tree_root_B = avl_delete_B(free_tree_root_B, left->size, (void *)left);

        merged_size += left->size + sizeof(block_header_B_t);
        merged_block = left;
        merged_block->linear_right_block = block->linear_right_block;
        if (block->linear_right_block)
        {
            block->linear_right_block->linear_left_block = merged_block;
        }
    }

    if (right && right->is_free != NOTFREE_B)
    {
        free_tree_root_B = avl_delete_B(free_tree_root_B, right->size, (void *)right);

        merged_size += right->size + sizeof(block_header_B_t);
        merged_block->linear_right_block = right->linear_right_block;
        if (right->linear_right_block)
        {
            right->linear_right_block->linear_left_block = merged_block;
        }
    }

    merged_block->size = merged_size;
    merged_block->is_free = 1;
    avl_node_B_t *node = create_avl_node_B(merged_block);
    if (node)
    {
        free_tree_root_B = avl_insert_B(free_tree_root_B, node);
    }
}

// ======================== WORST FIT =========================

#ifndef TYPES_ENUM_H
#define TYPES_ENUM_H

enum TYPES {
    BEST,
    WORST,
    FIRST,
    NEXT,
    BUDDY,
    NONE
};

enum TYPES current_type = NONE;

#endif

#define NOTFREE_W 100

static inline size_t PASS_W(size_t sz)
{
    return sz;
}


static size_t page_size_or_default_W()
{
    return 4096;
}

typedef struct block_header_W
{
    size_t size;
    int is_free;
    struct block_header_W *linear_left_block;
    struct block_header_W *linear_right_block;
} block_header_W_t;

/* AVL Tree Node */
typedef struct avl_node_W
{
    block_header_W_t *header;
    size_t size;
    void *header_address;
    int height;
    struct avl_node_W *left;
    struct avl_node_W *right;
} avl_node_W_t;

#define WWWWW sizeof(block_header_W_t)

static avl_node_W_t *free_tree_root_W = NULL;
static block_header_W_t *first_block_W = NULL;

static inline void *header_to_payload_W(block_header_W_t *h)
{
    return (void *)((char *)h + sizeof(block_header_W_t));
}

static inline block_header_W_t *payload_to_header_W(void *p)
{
    return (block_header_W_t *)((char *)p - sizeof(block_header_W_t));
}

static inline int max_W(int a, int b)
{
    return (a > b) ? a : b;
}

static inline int get_height_W(avl_node_W_t *node)
{
    return node ? node->height : 0;
}

static inline int get_balance_W(avl_node_W_t *node)
{
    return node ? (get_height_W(node->left) - get_height_W(node->right)) : 0;
}

static inline void update_height_W(avl_node_W_t *node)
{
    if (node)
    {
        node->height = 1 + max_W(get_height_W(node->left), get_height_W(node->right));
    }
}

static avl_node_W_t *rotate_right_W(avl_node_W_t *y)
{
    avl_node_W_t *x = y->left;
    avl_node_W_t *T2 = x->right;

    x->right = y;
    y->left = T2;

    update_height_W(y);
    update_height_W(x);

    return x;
}

static avl_node_W_t *rotate_left_W(avl_node_W_t *x)
{
    avl_node_W_t *y = x->right;
    avl_node_W_t *T2 = y->left;

    y->left = x;
    x->right = T2;

    update_height_W(x);
    update_height_W(y);

    return y;
}

static avl_node_W_t *create_avl_node_W(block_header_W_t *header)
{
    avl_node_W_t *node = (avl_node_W_t *)mmap(NULL, sizeof(avl_node_W_t),
                                              PROT_READ | PROT_WRITE,
                                              MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (node == MAP_FAILED)
    {
        return NULL;
    }

    node->header = header;
    node->size = header->size;
    node->header_address = (void *)header;
    node->height = 1;
    node->left = NULL;
    node->right = NULL;

    return node;
}

static void free_avl_node_W(avl_node_W_t *node)
{
    if (node)
    {
        munmap(node, sizeof(avl_node_W_t));
    }
}

static int compare_nodes_W(size_t size1, void *addr1, size_t size2, void *addr2)
{
    if (size1 < size2)
        return -1;
    if (size1 > size2)
        return 1;
    // Sizes are equal, compare addresses
    if (addr1 < addr2)
        return -1;
    if (addr1 > addr2)
        return 1;
    return 0;
}

static avl_node_W_t *avl_insert_W(avl_node_W_t *root, avl_node_W_t *new_node)
{
    if (!root)
    {
        return new_node;
    }

    int cmp = compare_nodes_W(new_node->size, new_node->header_address,
                              root->size, root->header_address);

    if (cmp <= 0)
    {
        root->left = avl_insert_W(root->left, new_node);
    }
    else
    {
        root->right = avl_insert_W(root->right, new_node);
    }

    update_height_W(root);
    int balance = get_balance_W(root);

    // Left-Left
    if (balance > 1 && compare_nodes_W(new_node->size, new_node->header_address,
                                       root->left->size, root->left->header_address) <= 0)
    {
        return rotate_right_W(root);
    }

    // Right-Right
    if (balance < -1 && compare_nodes_W(new_node->size, new_node->header_address,
                                        root->right->size, root->right->header_address) > 0)
    {
        return rotate_left_W(root);
    }

    // Left-Right
    if (balance > 1 && compare_nodes_W(new_node->size, new_node->header_address,
                                       root->left->size, root->left->header_address) > 0)
    {
        root->left = rotate_left_W(root->left);
        return rotate_right_W(root);
    }

    // Right-Left
    if (balance < -1 && compare_nodes_W(new_node->size, new_node->header_address,
                                        root->right->size, root->right->header_address) <= 0)
    {
        root->right = rotate_right_W(root->right);
        return rotate_left_W(root);
    }

    return root;
}

static avl_node_W_t *find_min_node_W(avl_node_W_t *node)
{
    while (node && node->left)
    {
        node = node->left;
    }
    return node;
}

static avl_node_W_t *avl_delete_W(avl_node_W_t *root, size_t size, void *header_address)
{
    if (!root)
    {
        return NULL;
    }

    int cmp = compare_nodes_W(size, header_address, root->size, root->header_address);

    if (cmp < 0)
    {
        root->left = avl_delete_W(root->left, size, header_address);
    }
    else if (cmp > 0)
    {
        root->right = avl_delete_W(root->right, size, header_address);
    }
    else
    {
        if (!root->left || !root->right)
        {
            avl_node_W_t *temp = root->left ? root->left : root->right;

            if (!temp)
            {
                temp = root;
                root = NULL;
            }
            else
            {
                *root = *temp;
            }
            free_avl_node_W(temp);
        }
        else
        {
            avl_node_W_t *temp = find_min_node_W(root->right);

            root->header = temp->header;
            root->size = temp->size;
            root->header_address = temp->header_address;

            root->right = avl_delete_W(root->right, temp->size, temp->header_address);
        }
    }

    if (!root)
    {
        return NULL;
    }

    update_height_W(root);
    int balance = get_balance_W(root);

    // Left-Left
    if (balance > 1 && get_balance_W(root->left) >= 0)
    {
        return rotate_right_W(root);
    }

    // Left-Right
    if (balance > 1 && get_balance_W(root->left) < 0)
    {
        root->left = rotate_left_W(root->left);
        return rotate_right_W(root);
    }

    // Right-Right
    if (balance < -1 && get_balance_W(root->right) <= 0)
    {
        return rotate_left_W(root);
    }

    // Right-Left
    if (balance < -1 && get_balance_W(root->right) > 0)
    {
        root->right = rotate_right_W(root->right);
        return rotate_left_W(root);
    }

    return root;
}

static avl_node_W_t *find_worst_fit_W(avl_node_W_t *root, size_t requested_size)
{
    avl_node_W_t *worst = NULL;
    avl_node_W_t *current = root;

    while (current)
    {
        if (current->size >= requested_size)
        {
            // This block is suitable, remember it
            worst = current;
            // Try to find an even larger block on the right
            current = current->right;
        }
        else
        {
            // Current block is too small, go right to find larger blocks
            current = current->right;
        }
    }

    return worst;
}

/* Print AVL tree (in-order traversal) - for debugging */
static void print_avl_tree_helper_W(avl_node_W_t *node, int depth)
{
    if (!node)
        return;

    print_avl_tree_helper_W(node->right, depth + 1);

    for (int i = 0; i < depth; i++)
    {
        printf("    ");
    }
    printf("Size: %zu, Addr: %p, Height: %d , left=%p, right=%p\n", node->size, node->header_address, node->height, node->left, node->right);

    print_avl_tree_helper_W(node->left, depth + 1);
}

static void print_list_W()
{
    printf("\n===== FREE LIST (AVL Tree) =====\n");
    if (!free_tree_root_W)
    {
        printf("Empty\n");
    }
    else
    {
        print_avl_tree_helper_W(free_tree_root_W, 0);
    }
    printf("================================\n\n");
}

/* Print all blocks in linear order - for debugging */
static void print_linear_W()
{
    printf("\n===== LINEAR MEMORY BLOCKS =====\n");
    block_header_W_t *current = first_block_W;
    int count = 0;

    while (current)
    {
        printf("Block %d: Addr=%p, Size=%zu, Free=%s\n",
               count++, (void *)current, current->size,
               current->is_free == NOTFREE_W ? "YES" : "NO");
        current = current->linear_right_block;
    }
    printf("================================\n\n");
}

static bool initialize_memory_W()
{
    if (first_block_W)
    {
        return 1;
    }

    size_t page_size = page_size_or_default_W();

    void *mem = mmap(NULL, page_size, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    if (mem == MAP_FAILED)
    {
        return 0;
    }

    first_block_W = (block_header_W_t *)mem;
    first_block_W->size = page_size - sizeof(block_header_W_t);
    first_block_W->is_free = 1;
    first_block_W->linear_left_block = NULL;
    first_block_W->linear_right_block = NULL;

    avl_node_W_t *node = create_avl_node_W(first_block_W);
    if (!node)
    {
        munmap(mem, page_size);
        first_block_W = NULL;
        return 0;
    }

    free_tree_root_W = avl_insert_W(free_tree_root_W, node);

    return 1;
}

void *malloc_worst_fit(size_t size)
{

    current_type = WORST ;

    if (size == 0)
    {
        return NULL;
    }

    if (!initialize_memory_W())
    {
        return NULL;
    }

    size_t aligned_size = PASS_W(size);
    avl_node_W_t *worst_node = find_worst_fit_W(free_tree_root_W, aligned_size);

    if (!worst_node)
    {
        size_t page_size = page_size_or_default_W();
        size_t alloc_size = aligned_size + sizeof(block_header_W_t);
        size_t num_pages = (alloc_size + page_size - 1) / page_size;
        size_t mmap_size = num_pages * page_size;

        void *mem = mmap(NULL, mmap_size, PROT_READ | PROT_WRITE,
                         MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

        if (mem == MAP_FAILED)
        {
            return NULL;
        }
        block_header_W_t *new_block = (block_header_W_t *)mem;
        new_block->size = mmap_size - sizeof(block_header_W_t);
        new_block->is_free = 1;
        new_block->linear_left_block = NULL;
        new_block->linear_right_block = NULL;
        block_header_W_t *last = first_block_W;
        while (last && last->linear_right_block)
        {
            last = last->linear_right_block;
        }
        if (last)
        {
            last->linear_right_block = new_block;
            new_block->linear_left_block = last;
        }
        avl_node_W_t *node = create_avl_node_W(new_block);
        if (!node)
        {
            munmap(mem, mmap_size);
            return NULL;
        }

        free_tree_root_W = avl_insert_W(free_tree_root_W, node);
        worst_node = node;
    }
    block_header_W_t *block = worst_node->header;
    free_tree_root_W = avl_delete_W(free_tree_root_W, block->size, (void *)block);
    if (block->size >= aligned_size + sizeof(block_header_W_t) + WWWWW)
    {
        // Split the block
        size_t remaining_size = block->size - aligned_size - sizeof(block_header_W_t);
        block_header_W_t *new_block = (block_header_W_t *)((char *)block + sizeof(block_header_W_t) + aligned_size);
        new_block->size = remaining_size;
        new_block->is_free = 1;
        new_block->linear_left_block = block;
        new_block->linear_right_block = block->linear_right_block;

        if (block->linear_right_block)
        {
            block->linear_right_block->linear_left_block = new_block;
        }
        block->linear_right_block = new_block;
        block->size = aligned_size;

        // Insert the new free block into AVL tree
        avl_node_W_t *node = create_avl_node_W(new_block);
        if (node)
        {
            free_tree_root_W = avl_insert_W(free_tree_root_W, node);
        }
    }
    block->is_free = NOTFREE_W;

    return header_to_payload_W(block);
}

void my_free_W(void *ptr)
{
    if (!ptr)
    {
        return;
    }

    block_header_W_t *block = payload_to_header_W(ptr);

    if (block->is_free != NOTFREE_W)
    {
        // Double free, ignore
        return;
    }

    block->is_free = 1;

    // Try to merge with adjacent free blocks
    block_header_W_t *left = block->linear_left_block;
    block_header_W_t *right = block->linear_right_block;

    block_header_W_t *merged_block = block;
    size_t merged_size = block->size;

    // Merge with left neighbor if free
    if (left && left->is_free != NOTFREE_W)
    {
        // Remove left from AVL tree
        free_tree_root_W = avl_delete_W(free_tree_root_W, left->size, (void *)left);

        merged_size += left->size + sizeof(block_header_W_t);
        merged_block = left;

        // Update linear pointers
        merged_block->linear_right_block = block->linear_right_block;
        if (block->linear_right_block)
        {
            block->linear_right_block->linear_left_block = merged_block;
        }
    }

    if (right && right->is_free != NOTFREE_W)
    {
        free_tree_root_W = avl_delete_W(free_tree_root_W, right->size, (void *)right);

        merged_size += right->size + sizeof(block_header_W_t);

        // Update linear pointers
        merged_block->linear_right_block = right->linear_right_block;
        if (right->linear_right_block)
        {
            right->linear_right_block->linear_left_block = merged_block;
        }
    }

    merged_block->size = merged_size;
    merged_block->is_free = 1;
    avl_node_W_t *node = create_avl_node_W(merged_block);
    if (node)
    {
        free_tree_root_W = avl_insert_W(free_tree_root_W, node);
    }
}



// Function to release memory allocated using your malloc functions
void my_free(void *ptr)
{
    if (current_type == BEST)
    {
        my_free_B(ptr);
    }
    else if (current_type == WORST)
    {
        my_free_W(ptr);
    }
    else if (current_type == FIRST)
    {
        my_free_FF(ptr);
    }
    else if (current_type == NEXT)
    {
        my_free_N(ptr);
    }
    else if (current_type == BUDDY)
    {
        my_free_BD(ptr);
    }
    else
    {
        return;
    }
}
