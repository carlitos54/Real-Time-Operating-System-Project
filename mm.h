// Memory manager functions
// J Losh

//-----------------------------------------------------------------------------
// Hardware Target
//-----------------------------------------------------------------------------

// Target uC:       TM4C123GH6PM
// System Clock:    40 MHz

#ifndef MM_H_
#define MM_H_

//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

typedef struct _heap_block
{
    uint8_t owner;
    uint32_t base_add;
    uint32_t req_size;
    void* pid;
    uint8_t blocks_allocated;
    bool is_allocated;
} heap_block;

extern heap_block heap_map[28];
extern uint64_t global_srdMask;

uint64_t createNoSramAccessMask(void);
void applySramAccessMask(uint64_t srdBitMask);
void addSramAccessWindow(uint64_t *srdBitMask, uint32_t* baseAdd, uint32_t size_in_bytes);
void remSramAccessWindow(uint64_t *srdBitMask, uint32_t* baseAdd, uint32_t size_in_bytes);
void * mallocHeap(uint32_t size_in_bytes);
void freeHeap(void *address_from_malloc);
void freeHeapPid(void * pid);
void turnOnMPU(void);
void allowFlashAccess(void);
void allowPeripheralAccess(void);
void setupSramAccess(void);
void initMemoryManager(void);
void initMpu(void);

#endif
