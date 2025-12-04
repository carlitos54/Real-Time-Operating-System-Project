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
#include <stdbool.h>
#include <stdlib.h>
#include "shell.h"
#include "shell_func.h"
#include "clock.h"
#include "uart0.h"
#include "tm4c123gh6pm.h"
#include "kernel.h"


// REQUIRED: Add header files here for your strings functions, ...
#define RED_LED      (*((volatile uint32_t *)(0x42000000 + (0x400253FC-0x40000000)*32 + 1*4)))

#define MAX_CHARS 80
#define MAX_FIELDS 5

void getsUart0(USER_DATA *data)
{
    int count = 0;
    while(true)
    {
        if(kbhitUart0())
        {
            char letter = getcUart0();

            if(letter >= 32 && letter != 127)
            {
                data->buffer[count] = letter;
                count++;
                if(count == MAX_CHARS)
                {
                    data->buffer[MAX_CHARS] = 0;
                    break;
                }
            }
            else if(letter == 8 || letter == 127)
            {
                if(count > 0)
                {
                    count--;
                }
            }
            else if(letter == 13)
            {
                data->buffer[count] = 0;
                break;
            }
        }
        else
        {
            yield();
        }
    }
}

void parseFields(USER_DATA *data)
{
    int counter = 0;
    char prev_char = 'd';
    int i = 0;
    data->fieldCount = 0;
    for(i = 0; data->buffer[i] != '\0'; i++)
    {
        if(data->fieldCount < MAX_FIELDS)
        {
            if((data->buffer[i] > 47 && data->buffer[i] < 58) || data->buffer[i] == 45 || data->buffer[i] == 46)      //numbers
            {
                if(prev_char == 'd')
                {
                    data->fieldPosition[counter] = i;
                    data->fieldCount++;
                    data->fieldType[counter] = 'n';
                    counter++;
                }

                prev_char = 'n';

            }
            else if((data->buffer[i] > 64 && data->buffer[i] < 91) || (data->buffer[i] < 123 && data->buffer[i] > 96))  // alpha
            {
                if(prev_char == 'd')
                {
                    data->fieldPosition[counter] = i;
                    data->fieldCount++;
                    data->fieldType[counter] = 'a';
                    counter++;
                }
                prev_char = 'a';
            }
            else
            {
                data->buffer[i] = '\0';
                prev_char = 'd';
            }

        }
        else
        {
            return;
        }

    }

}

char* getFieldString(USER_DATA *data, uint8_t fieldNumber)              //returns pointer to where string exist
{
    char *ret;

    if((fieldNumber <= data->fieldCount || fieldNumber == 0) && (data->fieldType[fieldNumber] == 'a' || data->fieldType[fieldNumber] == 'n'))
    {
        ret = &data->buffer[data->fieldPosition[fieldNumber]];
    }
    else
    {
        return NULL;
    }

    return ret;

}

int32_t getFieldInteger(USER_DATA* data, uint8_t fieldNumber)
{

    int32_t ret;

    if((fieldNumber <= data->fieldCount || fieldNumber == 0) && data->fieldType[fieldNumber] == 'n')
    {
        ret = atoi(&data->buffer[data->fieldPosition[fieldNumber]]);
    }
    else
    {
        return NULL;
    }

    return ret;

}


bool isCommand(USER_DATA* data, const char strCommand[], uint8_t minArguments)
{
    bool ret = 0;
    int counter = 0;
    int pos = data->fieldPosition[0];
    if( (data->fieldType[data->fieldPosition[0]] == 'a') && ( (data->fieldCount - 1) >= minArguments) )
    {
        while(data->buffer[counter] != '\0')
        {
            if(data->buffer[pos] == strCommand[counter])
            {
                counter++;
                pos++;
                ret = 1;
            }
            else
            {
                ret = 0;
                break;
            }
        }
    }
    else
    {
        ret = 0;
    }
    return ret;
}

bool strcompare(const char str1[], const char str2[])
{
    while(*str1 != '\0' && *str2 != '\0')
    {
        if(*str1 != *str2)
        {
            return false;
        }
        str1++;
        str2++;
    }
    if(*str1 == '\0' && *str2 == '\0')
    {
        return true;
    }
    else
    {
        return false;
    }
}

void intToString(int val)
{
    char str[11];
    int index = 0;
    if (val < 0 || val > 2147483647) {
        putsUart0("Error: Out of range\n");
        return;
    }
    else
    {
        if (val >= 1000000000 || index > 0) {       //checks val to see if it's at least 1000000000
            str[index++] = '0' + val / 1000000000;  //val divided by 1 billion, adds ascii val of 48, adds to str
            val %= 1000000000;                      //removes 1000000000 digit place
        }
        if (val >= 100000000 || index > 0) {
            str[index++] = '0' + val / 100000000;
            val %= 100000000;
        }
        if (val >= 10000000 || index > 0) {
            str[index++] = '0' + val / 10000000;
            val %= 10000000;
        }
        if (val >= 1000000 || index > 0) {
            str[index++] = '0' + val / 1000000;
            val %= 1000000;
        }
        if (val >= 100000 || index > 0) {
            str[index++] = '0' + val / 100000;
            val %= 100000;
        }
        if (val >= 10000 || index > 0) {
            str[index++] = '0' + val / 10000;
            val %= 10000;
        }
        if (val >= 1000 || index > 0) {
            str[index++] = '0' + val / 1000;
            val %= 1000;
        }
        if (val >= 100 || index > 0) {
            str[index++] = '0' + val / 100;
            val %= 100;
        }
        if (val >= 10 || index > 0) {
            str[index++] = '0' + val / 10;
            val %= 10;
        }
        str[index++] = '0' + val;

        str[index] = '\0';
        putsUart0(str);
    }

}

void intToHex(uint32_t val)
{
    uint32_t i;
    uint32_t temp_val = val;

    putsUart0("0x");

    for (i = 0; i < 8; i++) {
        uint8_t nibble = (temp_val >> 28) & 0xF;
        if (nibble < 10)
            putcUart0(nibble + '0');
        else
            putcUart0(nibble - 10 + 'A');
        temp_val <<= 4;
    }
}

uint32_t strHexToInt(const char * str)
{
    uint32_t ret = 0;
    str += 2;   //skip 0x
    while (*str)
    {
        char c = *str;
        uint8_t val = 0;
        if (c >= '0' && c <= '9')
            val = c - '0';
        else if (c >= 'a' && c <= 'f')
            val = 10 + (c - 'a');
        else if (c >= 'A' && c <= 'F')
            val = 10 + (c - 'A');
        else
            break;
        ret = (ret * 16) + val;
        str++;
    }
    return ret;
}
//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

// REQUIRED: add processing for the shell commands through the UART here
void shell(void)
{
    USER_DATA data;
    while(true)
    {
        if(kbhitUart0())
        {
            bool valid = false;
            yield();
            getsUart0(&data);
            parseFields(&data);

            if (isCommand(&data, "pi", 1))
            {
                char* str = getFieldString(&data, 1);
                valid = true;
                bool power = 0;
                if(strcompare(str, "on") || strcompare(str, "ON"))
                {
                    power = 1;
                    pi(power);
                }
                else if(strcompare(str, "off") || strcompare(str, "OFF"))
                {
                    power = 0;
                    pi(power);
                }
                else
                {
                    putsUart0("Invalid field for pi\n");
                }
            }
            else if (isCommand(&data, "preempt", 1))
            {
                char* str = getFieldString(&data, 1);
                valid = true;
                bool pree = 0;
                if(strcompare(str, "on") || strcompare(str, "ON"))
                {
                    pree = 1;
                    preempt(pree);
                }
                else if(strcompare(str, "off") || strcompare(str, "OFF"))
                {
                    pree = 0;
                    preempt(pree);
                }
                else
                {
                    putsUart0("Invalid field for preempt\n");
                }
            }
            else if (isCommand(&data, "sched", 1))
            {
                char* str = getFieldString(&data, 1);
                valid = true;
                bool prio_on = 0;
                if(strcompare(str, "prio") || strcompare(str, "PRIO"))
                {
                    prio_on = 1;
                    sched(prio_on);
                }
                else if(strcompare(str, "rr") || strcompare(str, "RR"))
                {
                    prio_on = 0;
                    sched(prio_on);
                }
                else
                {
                    putsUart0("Invalid field for sched\n");
                }
            }
            else if(isCommand(&data, "reboot", 0))
            {
                valid = true;
                reboot();
            }
            else if(isCommand(&data, "ps", 0))
            {
                valid = true;
                PS_INFO data;
                ps(&data);
                uint8_t i = 0;
                uint32_t cputime = 0;
                uint32_t integer = 0;
                uint32_t fraction = 0;

                putsUart0("\nPID \t\tName\t\tTicks\tState\t\t%CPU\n");
                putsUart0("--------------------------------------------------------------------\n");
                for(i = 0; i < 12; i++)
                {
                    if(data.tasks[i].state != 0)   //check if task is valid
                    {
                        intToHex((uint32_t)data.tasks[i].pid);
                        putsUart0("\t");

                        if(strcompare(data.tasks[i].name, "Idle") || strcompare(data.tasks[i].name, "OneShot") || strcompare(data.tasks[i].name, "Uncoop") || strcompare(data.tasks[i].name, "Errant") || strcompare(data.tasks[i].name, "Shell"))
                        {
                            putsUart0(data.tasks[i].name);
                            putsUart0("\t\t");
                        }
                        else
                        {
                            putsUart0(data.tasks[i].name);
                            putsUart0("\t");
                        }

                        intToString(data.tasks[i].ticks);
                        putsUart0(" ms\t");
                        if(data.tasks[i].state == 4 || data.tasks[i].state == 5)
                        {
                            printState(data.tasks[i].state);
                            putsUart0("\t");
                        }
                        else
                        {
                            printState(data.tasks[i].state);
                            putsUart0("\t\t");
                        }

                        cputime = data.tasks[i].cpu;
                        integer = cputime / 100;
                        fraction = cputime % 100;
                        intToString(integer);
                        putsUart0(".");
                        if(fraction < 10)
                        {
                            putsUart0("0");
                        }
                        intToString(fraction);
                        putsUart0("%\n");
                    }
                }
                putsUart0("\n");
            }
            else if(isCommand(&data, "ipcs", 0))
            {
                valid = true;
                IPCS_INFO data;
                uint8_t i = 0;
                ipcs(&data);
                putsUart0("\nMutex Status\n");
                putsUart0("State\tLockedBy  QSize  Queue\n");
                putsUart0("----------------------------------------\n");
                if(data.mutexes[0].lock)
                {
                    putsUart0("Locked\t");
                    putsUart0(data.mutexes[0].lockedBy);
                    putsUart0(" ");
                    if(data.mutexes[0].queueSize != 0)
                    {
                        intToString(data.mutexes[0].queueSize);
                        putsUart0("\t ");
                        putsUart0(data.mutexes[0].processQueue[0]);
                    }
                    else
                    {
                        putsUart0("0");
                    }
                    putsUart0("\n\n");
                }
                else
                {
                    putsUart0("Unlocked");
                }

                //Semaphore Info
                putsUart0("Semaphore Status\n");
                putsUart0("Sem\tCount\tQSize\tQueue\n");
                putsUart0("----------------------------------------\n");
                for(i = 0;i < MAX_SEMAPHORES; i++)
                {
                    intToString(i);
                    putsUart0("\t");
                    intToString(data.semaphores[i].count);
                    putsUart0("\t");
                    if(data.semaphores[i].queueSize != 0)
                    {
                        intToString(data.semaphores[i].queueSize);
                        putsUart0("\t");
                        putsUart0(data.semaphores[i].processQueue[0]);
                    }
                    else
                    {
                        putsUart0("0");
                    }
                    putsUart0("\n");
                }
                putsUart0("\n");

            }
            else if(isCommand(&data, "kill", 1))
            {
                valid = true;
                char* strpid = getFieldString(&data, 1);
                uint32_t pid = strHexToInt(strpid);
                kill(pid);
            }
            else if(isCommand(&data, "pkill", 1))
            {
                valid = true;
                valid = true;
                char* taskname = getFieldString(&data, 1);
                pkill(taskname);

            }
            else if(isCommand(&data, "pidof", 1))
            {
                valid = true;
                char* taskname = getFieldString(&data, 1);
                char* pid = pidof(taskname);
                uint32_t taskpid = (uint32_t)pid;
                putsUart0("PID: ");
                intToHex(taskpid);
                putsUart0("\n\n");
            }
            else if(isCommand(&data, "run", 1))
            {
                valid = true;
                char* taskname = getFieldString(&data, 1);
                run_proc(taskname);
            }
            else if(isCommand(&data, "help", 0))
            {
                valid = true;
                char* str = getFieldString(&data, 1);
                putsUart0("\nreboot           Reboots the Microcontroller\n");
                putsUart0("ps               Displays process(thread) status\n");
                putsUart0("ipcs             Displays the inter-process (thread) communication status.\n");
                putsUart0("kill pid         Kills the process (thread) with the matching PID\n");
                putsUart0("pkill proc_name  Kills the thread based on the process name\n");
                putsUart0("pi ON|OFF        Turns priority inheritance on or off\n");
                putsUart0("preempt ON|OFF   Turns preemption on or off\n");
                putsUart0("sched PRIO | RR  Selected priority or round-robin scheduling\n");
                putsUart0("pidof proc_name  Displays the PID of the process (thread)\n");
                putsUart0("run proc_name    Runs the selected program in the background\n");

            }
            else if(!valid)
            {
                putsUart0("Invalid Input\n");
            }

        }
        yield();
    }
}
