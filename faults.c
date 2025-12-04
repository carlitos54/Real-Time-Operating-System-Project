// Shell functions
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
#include "tm4c123gh6pm.h"
#include "faults.h"
#include "kernel.h"
#include "uart0.h"
#include "shell.h"
#include "asm.h"

//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

// REQUIRED: If these were written in assembly
//           omit this file and add a faults.s file

// REQUIRED: code this function
void mpuFaultIsr(void)
{
    uint32_t* psp;
    uint32_t* msp;

    putsUart0("MPU fault in process ");
    intToHex((uint32_t)tcb[taskCurrent].pid);
    putsUart0("\n");

    psp = getPSP();
    putsUart0("PSP:          ");
    intToHex((uint32_t)psp);
    putsUart0("\n");

    msp = getMSP();
    putsUart0("MSP:          ");
    intToHex((uint32_t)msp);
    putsUart0("\n");

    putsUart0("mfault:       ");
    intToHex(NVIC_FAULT_STAT_R);
    putsUart0("\n");

    putsUart0("MMFAULTADDR:  ");
    intToHex(NVIC_MM_ADDR_R);
    putsUart0("\n");

    uint32_t pc = psp[6];
    uint32_t* off_addr = (uint32_t*)(pc - 2);         // before stacked PC
    uint32_t opcode = *off_addr;

    putsUart0("Off Opcode:   ");
    intToHex(opcode);
    putsUart0("\n");

    putsUart0("xPSR:         ");
    intToHex(psp[7]);
    putsUart0("\n");

    putsUart0("PC:           ");
    intToHex(pc);
    putsUart0("\n");

    putsUart0("LR:           ");
    intToHex(psp[5]);
    putsUart0("\n");

    putsUart0("R12:          ");
    intToHex(psp[4]);
    putsUart0("\n");

    putsUart0("R3:           ");
    intToHex(psp[3]);
    putsUart0("\n");

    putsUart0("R2:           ");
    intToHex(psp[2]);
    putsUart0("\n");

    putsUart0("R1:           ");
    intToHex(psp[1]);
    putsUart0("\n");

    putsUart0("R0:           ");
    intToHex(psp[0]);
    putsUart0("\n\n");

    killT(getPid());
    NVIC_SYS_HND_CTRL_R &= ~(NVIC_SYS_HND_CTRL_MEMP| NVIC_SYS_HND_CTRL_MEMA);
    if((NVIC_FAULT_STAT_R & (NVIC_FAULT_STAT_DERR | NVIC_FAULT_STAT_IERR)) != 0)
        NVIC_FAULT_STAT_R |= (NVIC_FAULT_STAT_DERR | NVIC_FAULT_STAT_IERR);
    NVIC_INT_CTRL_R |= NVIC_INT_CTRL_PEND_SV;
}

// REQUIRED: code this function
void hardFaultIsr(void)
{
    uint32_t* psp;
    uint32_t* msp;

    putsUart0("Hard fault in process ");
    intToHex((uint32_t)tcb[taskCurrent].pid);
    putsUart0("\n");

    psp = getPSP();
    putsUart0("PSP:          ");
    intToHex((uint32_t)psp);
    putsUart0("\n");

    msp = getMSP();
    putsUart0("MSP:          ");
    intToHex((uint32_t)msp);
    putsUart0("\n");

    putsUart0("hfault:       ");
    intToHex(NVIC_HFAULT_STAT_R);
    putsUart0("\n");

    uint32_t pc = psp[6];
    uint32_t* off_addr = (uint32_t*)(pc - 2);
    uint32_t opcode = *off_addr;

    putsUart0("Off Opcode:   ");
    intToHex(opcode);
    putsUart0("\n");

    putsUart0("xPSR:         ");
    intToHex(psp[7]);
    putsUart0("\n");

    putsUart0("PC:           ");
    intToHex(pc);
    putsUart0("\n");

    putsUart0("LR:           ");
    intToHex(psp[5]);
    putsUart0("\n");

    putsUart0("R12:          ");
    intToHex(psp[4]);
    putsUart0("\n");

    putsUart0("R3:           ");
    intToHex(psp[3]);
    putsUart0("\n");

    putsUart0("R2:           ");
    intToHex(psp[2]);
    putsUart0("\n");

    putsUart0("R1:           ");
    intToHex(psp[1]);
    putsUart0("\n");

    putsUart0("R0:           ");
    intToHex(psp[0]);
    putsUart0("\n\n");

    while(1)
    {
    }
}

// REQUIRED: code this function
void busFaultIsr(void)
{
    putsUart0("Bus fault in process ");
    intToHex((uint32_t)tcb[taskCurrent].pid);
    putsUart0("\n\n");
    while(1)
    {
    }
}

// REQUIRED: code this function
void usageFaultIsr(void)
{
    putsUart0("Usage fault in process ");
    intToHex((uint32_t)tcb[taskCurrent].pid);
    putsUart0("\n\n");
    while(1)
    {
    }
}

