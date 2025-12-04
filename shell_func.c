/*
 * shell_func.c
 *
 *  Created on: Aug 31, 2024
 *      Author: carli
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include "kernel.h"
#include "uart0.h"
#include "shell.h"
#include "shell_func.h"
#include "tm4c123gh6pm.h"

#define RED_LED      (*((volatile uint32_t *)(0x42000000 + (0x400253FC-0x40000000)*32 + 1*4)))

void printState(uint8_t state)
{
    switch(state)
    {
    case 0:
        putsUart0("INVALID");
        break;
    case 1:
        putsUart0("UNRUN");
        break;
    case 2:
        putsUart0("READY");
        break;
    case 3:
        putsUart0("DELAYED");
        break;
    case 4:
        putsUart0("BLOCKED(SEM)");
        break;
    case 5:
        putsUart0("BLOCKED(MUTEX)");
        break;
    case 6:
        putsUart0("KILLED");
        break;
    default:
        putsUart0("UNKNOWN");
    }
}

void ps(PS_INFO *data)
{
    __asm(" SVC #12");
}

void reboot()
{
    __asm(" SVC #9");
}

void ipcs(IPCS_INFO* data)
{
    __asm(" SVC #11");
}

void kill(uint32_t pid)
{
    __asm(" SVC #14");
    putsUart0("Task killed");
    putcUart0('\n');
}

void pi(bool on)
{
    __asm(" SVC #6");
    if(on == 1)
    {
        putsUart0("Priority Inheritance ON");
        putsUart0("\n\n");
    }
    else
    {
        putsUart0("Priority Inheritance OFF");
        putsUart0("\n\n");
    }
}

void sched(bool prio_on)
{
    __asm(" SVC #7");
    if(prio_on == 1)
    {
        putsUart0("Priority Scheduling");
        putsUart0("\n\n");
    }
    else
    {
        putsUart0("Round Robin Scheduling");
        putsUart0("\n\n");
    }
}

void preempt(bool on)
{
    __asm(" SVC #8");
    if(on == 1)
    {
        putsUart0("Preemption ON");
        putsUart0("\n\n");
    }
    else
    {
        putsUart0("Preemption OFF");
        putsUart0("\n\n");
    }
}

void* pidof(const char name[])
{
    __asm(" SVC #10");
}

void pkill(const char name[])
{
    __asm(" SVC #13");
    putsUart0("Task Killed: ");
    putsUart0((char*) name);
    putsUart0("\n\n");
}

void run_proc(const char name[])
{
    __asm(" SVC #15");
    putsUart0((char*) name);
    putsUart0(" Restarted");
    putsUart0("\n\n");
}


