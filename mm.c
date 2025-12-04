// Memory manager functions
// J Losh

//-----------------------------------------------------------------------------
// Hardware Target
//-----------------------------------------------------------------------------

// Target uC:       TM4C123GH6PM
// System Clock:    40 MHz

//-----------------------------------------------------------------------------
// Device includes, defines, and assembler directives
//-----------------------------------------------------------------------------

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include "mm.h"
#include "shell.h"
#include "uart0.h"
#include "tm4c123gh6pm.h"
#include "kernel.h"

#define REGION2         0x20000000          //OS starting address
#define REGION3         0x20002000
#define REGION4         0x20004000
#define REGION5         0x20006000
#define HEAP_START      0x20001000          //Heap starting address
#define FLASH_REGION    0x00000000          //Flash         Region 0
#define PERIPH_REGION   0x40000000          //Peripherial   Region 1

#define SIZE8KIB    12                  //Value for 8 KiB regions
#define FLASHSIZE   17                  //PAGE 92   Flash:          0x00000000 - 0x0003FFFF     2 ^ 17 + 1 = 262144
#define PERIPHSIZE  25                  //          Peripherals:    0x40000000 - 0x44000000     2 ^ 25 + 1 = 67108864

#define BLOCK_SIZE 1024
#define TOTAL_BLOCKS 28

heap_block heap_map[TOTAL_BLOCKS] = {0};
uint64_t global_srdMask;

/*
     * MPU Region 4
     * 8 KiB
     *      1024 b  0x20007C00  31      2147483648
     *      1024 b  0x20007800  30      1073741824
     *      1024 b  0x20007400  29      536870912
     *      1024 b  0x20007000  28      268435456
     *      1024 b  0x20006C00  27      134217728
     *      1024 b  0x20006800  26      67108864
     *      1024 b  0x20006400  25      33554432
     *      1024 b  0x20006000  24      16777216
     *
     * MPU Region 3
     * 8 KiB
     *      1024 b  0x20005C00  23      8388608
     *      1024 b  0x20005800  22      4194304
     *      1024 b  0x20005400  21      2097152
     *      1024 b  0x20005000  20      1048576
     *      1024 b  0x20004C00  19      524288
     *      1024 b  0x20004800  18      262144
     *      1024 b  0x20004400  17      131072
     *      1024 b  0x20004000  16      65536
     *
     * MPU Region 2
     * 8 KiB
     *      1024 b  0x20003C00  15      32768
     *      1024 b  0x20003800  14      16384
     *      1024 b  0x20003400  13      8192
     *      1024 b  0x20003000  12      4096
     *      1024 b  0x20002C00  11      2048
     *      1024 b  0x20002800  10      1024
     *      1024 b  0x20002400  9       512
     *      1024 b  0x20002000  8       256
     *
     * MPU Region 1
     * 4 KiB
     *      1024 b  0x20001C00  7       128
     *      1024 b  0x20001800  6       64
     *      1024 b  0x20001400  5       32
     *      1024 b  0x20001000  4       16
     *
     * OS Heap Space
     * 4 KiB
     *      1024 b  0x20000C00  3       8
     *      1024 b  0x20000800  2       4
     *      1024 b  0x20000400  1       2
     *      1024 b  0x20000000  0       1
 */

//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

// REQUIRED: add your custom MPU functions here (eg to return the srd bits)
uint64_t createNoSramAccessMask(void)
{
    return 0xFFFFFFFFFFFFFFFF;          //1's no access, 0's has access
}

void applySramAccessMask(uint64_t srdBitMask)
{
    //global_srdMask = srdBitMask;
    uint8_t i = 0;
    for(i = 0; i < 4; i++)
    {
        NVIC_MPU_NUMBER_R = 2 + i;              //ensure it doesn't go into region 0 and 1 (flash and periph)
        NVIC_MPU_ATTR_R &= ~(NVIC_MPU_ATTR_SRD_M);           //clear previous mask
        uint8_t region_srd = (srdBitMask >> (i * 8));        // get 8 srd bits for each region
        NVIC_MPU_ATTR_R |= (region_srd << 8);                //writes new bits to the attributes register
    }
}

void addSramAccessWindow(uint64_t *srdBitMask, uint32_t* baseAdd, uint32_t size_in_bytes)
{
    if((uint32_t)baseAdd < REGION2 || (uint32_t)baseAdd >= (REGION2 + 32768))
    {
        return;
    }

    uint32_t index = ((uint32_t)baseAdd - REGION2) / BLOCK_SIZE;      //get index of the 1 KiB subregion needed
    uint32_t regions = (size_in_bytes + 1023) / BLOCK_SIZE;     // number of regions needed for requested size, takes care of boundaries
    uint16_t i = 0;
    for(i = 0; i < regions; i++)
    {
        *srdBitMask &= ~(1ULL << (index + i));                  //sets bits to 1 to deny access
    }
}

void remSramAccessWindow(uint64_t *srdBitMask, uint32_t* baseAdd, uint32_t size_in_bytes)
{
    if((uint32_t)baseAdd < REGION2 || (uint32_t)baseAdd >= (REGION2 + 32768))           //check if it's in range
    {
        return;
    }

    uint32_t index = ((uint32_t)baseAdd - REGION2) / BLOCK_SIZE;        //get index of the 1 KiB subregion needed
    uint32_t regions = (size_in_bytes + 1023) / BLOCK_SIZE;         // number of regions needed for requested size, takes care of boundaries
    uint16_t i = 0;
    for(i = 0; i < regions; i++)
    {
        *srdBitMask |= (1ULL << (index + i));                       //sets bits to 1 to deny access
    }
}


// REQUIRED: add your malloc code here and update the SRD bits for the current thread
void * mallocHeap(uint32_t size_in_bytes)
{
    uint32_t blocks_needed = 0;
    if(size_in_bytes == 0)
        return NULL;
    else if (size_in_bytes > 0 && size_in_bytes <= 32768)               //Ensure it is in range
        blocks_needed = (size_in_bytes + BLOCK_SIZE - 1) / BLOCK_SIZE;  //round the blocks needed
    else
        return NULL;

    uint32_t i = 0;
    for(i = 0; i < TOTAL_BLOCKS - blocks_needed ; i++)
    {
        bool contiguous = 1;
        uint32_t j;
        for (j = 0; j < blocks_needed; j++)             //loops to check if next blocks needed are free
        {
            if(heap_map[i + j].is_allocated)            //finds contiguous block and updates index
            {
                contiguous = 0;
                i = i + j;
                break;
            }
        }

        if(contiguous)
        {
            uint8_t owner = taskCurrent;
            void * pid = (void *)getPid();
            for(j = 0; j < blocks_needed; j++)                          //update heapmap if contiguous blocks are found
            {
                heap_map[i + j].is_allocated = true;
                heap_map[i + j].owner = (int)owner;                     //task that owns the stack
                heap_map[i + j].pid = pid;                              //fn of that task
                heap_map[i + j].req_size = size_in_bytes;               //bytes requested
                heap_map[i + j].base_add = (HEAP_START + (i * BLOCK_SIZE)); //base address for freeing
            }

            heap_map[i].blocks_allocated = blocks_needed;
            return (void *)(HEAP_START + (i * BLOCK_SIZE));
        }
    }
    return NULL;
}

// REQUIRED: add your free code here and update the SRD bits for the current thread
void freeHeap(void *address_from_malloc)
{
    //Ensure it's in heap range
    if(address_from_malloc < (void*)HEAP_START || address_from_malloc >= (void*)(HEAP_START + (TOTAL_BLOCKS * BLOCK_SIZE)))
        return;


    uint32_t offset = (uint32_t) address_from_malloc - HEAP_START;       //calculate index of the block p falls into
    uint32_t index = offset / BLOCK_SIZE;

    if((offset % BLOCK_SIZE) != 0)                //makes sure block is not in the middle of allocation
        return;

    if (!heap_map[index].is_allocated || heap_map[index].blocks_allocated == 0)   //make sure it's allocated
        return;

    uint32_t blocks_freed = heap_map[index].blocks_allocated;
    //uint32_t size_in_bytes = blocks_freed * BLOCK_SIZE;
    uint32_t i = 0;

    for(i = 0; i < blocks_freed; i++)                           //free all blocks
    {
        heap_map[index + i].is_allocated = false;
        heap_map[index + i].pid = 0;                            // Clear the PID
    }

    heap_map[index].blocks_allocated = 0;
    //remSramAccessWindow(&global_srdMask, address_from_malloc, size_in_bytes);
    //applySramAccessMask(global_srdMask);

}

void freeHeapPid(void * pid)
{
    uint32_t i = 0;
    for(i = 0; i < TOTAL_BLOCKS; i++)
    {
        if(heap_map[i].is_allocated && (heap_map[i].pid == pid))
        {
            if(heap_map[i].blocks_allocated > 0)
            {
                remSramAccessWindow(&global_srdMask, (uint32_t*)heap_map[i].base_add, heap_map[i].req_size);
            }
            heap_map[i].is_allocated = false;
            heap_map[i].pid = 0;
            heap_map[i].owner = 0;
            heap_map[i].blocks_allocated = 0;
            heap_map[i].base_add = 0;
            heap_map[i].req_size = 0;
        }
    }
}

void turnOnMPU()
{
    NVIC_MPU_CTRL_R |= NVIC_MPU_CTRL_PRIVDEFEN | NVIC_MPU_CTRL_ENABLE;
}


void allowFlashAccess(void)
{
    NVIC_MPU_NUMBER_R = 0;                                 //pg 189
    NVIC_MPU_BASE_R = FLASH_REGION;     //pg 190
    //                   AP                      C                     SIZE                 ENABLE
    NVIC_MPU_ATTR_R = (3 << 24) | NVIC_MPU_ATTR_CACHEABLE | (FLASHSIZE << 1) | NVIC_MPU_ATTR_ENABLE;   //pg 192 and 129 table 3.5
}

void allowPeripheralAccess(void)
{
    NVIC_MPU_NUMBER_R = 1;
    NVIC_MPU_BASE_R = PERIPH_REGION;
    //                   AP                 S                         B                     SIZE                ENABLE                  XN
    NVIC_MPU_ATTR_R = (3 << 24) | NVIC_MPU_ATTR_SHAREABLE | NVIC_MPU_ATTR_BUFFRABLE | (PERIPHSIZE << 1) | NVIC_MPU_ATTR_ENABLE | NVIC_MPU_ATTR_XN;   //pg 192 and 129 table 3.5
}

void setupSramAccess(void)
{
    NVIC_MPU_NUMBER_R = 2;
    NVIC_MPU_BASE_R = REGION2;
    //                   AP                  S                         C                        B                   SIZE              SRD               ENABLE               XN
    NVIC_MPU_ATTR_R = (3 << 24) | NVIC_MPU_ATTR_SHAREABLE | NVIC_MPU_ATTR_CACHEABLE |NVIC_MPU_ATTR_BUFFRABLE | (SIZE8KIB << 1) | (0xFF << 8) |NVIC_MPU_ATTR_ENABLE | NVIC_MPU_ATTR_XN;   //pg 192 and 129 table 3.5

    NVIC_MPU_NUMBER_R = 3;
    NVIC_MPU_BASE_R = REGION3;
    //                   AP                  S                         C                        B                   SIZE              SRD               ENABLE               XN
    NVIC_MPU_ATTR_R = (3 << 24) | NVIC_MPU_ATTR_SHAREABLE | NVIC_MPU_ATTR_CACHEABLE |NVIC_MPU_ATTR_BUFFRABLE | (SIZE8KIB << 1) | (0xFF << 8) | NVIC_MPU_ATTR_ENABLE | NVIC_MPU_ATTR_XN;   //pg 192 and 129 table 3.5

    NVIC_MPU_NUMBER_R = 4;
    NVIC_MPU_BASE_R = REGION4;
    //                   AP                  S                         C                        B                   SIZE              SRD               ENABLE                XN
    NVIC_MPU_ATTR_R = (3 << 24) | NVIC_MPU_ATTR_SHAREABLE | NVIC_MPU_ATTR_CACHEABLE | NVIC_MPU_ATTR_BUFFRABLE | (SIZE8KIB << 1) | (0xFF << 8) | NVIC_MPU_ATTR_ENABLE | NVIC_MPU_ATTR_XN;   //pg 192 and 129 table 3.5

    NVIC_MPU_NUMBER_R = 5;
    NVIC_MPU_BASE_R = REGION5;
    //                   AP                  S                         C                        B                   SIZE              SRD               ENABLE                XN
    NVIC_MPU_ATTR_R = (3 << 24) | NVIC_MPU_ATTR_SHAREABLE | NVIC_MPU_ATTR_CACHEABLE | NVIC_MPU_ATTR_BUFFRABLE | (SIZE8KIB << 1) | (0xFF << 8) | NVIC_MPU_ATTR_ENABLE | NVIC_MPU_ATTR_XN;   //pg 192 and 129 table 3.5
}

// REQUIRED: add code to initialize the memory manager
void initMemoryManager(void)
{
    NVIC_CFG_CTRL_R |= NVIC_CFG_CTRL_DIV0;              //Enable div by 0
    NVIC_SYS_HND_CTRL_R |= NVIC_SYS_HND_CTRL_USAGE;     //NVIC_SYS_HND_CTRL_USGA(1 << 18)Usage
    NVIC_SYS_HND_CTRL_R |= NVIC_SYS_HND_CTRL_BUS;       //NVIC_SYS_HND_CTRL_BUSA(1 << 17)Bus
    NVIC_SYS_HND_CTRL_R |= NVIC_SYS_HND_CTRL_MEM;       //MPU

}


// REQUIRED: initialize MPU here
void initMpu(void)
{
    // REQUIRED: call your MPU functions here
    allowFlashAccess();
    allowPeripheralAccess();
    setupSramAccess();
    turnOnMPU();
}

