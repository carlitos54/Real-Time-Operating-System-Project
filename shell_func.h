/*
 * shell_func.h
 *
 *  Created on: Aug 31, 2024
 *      Author: carli
 */

#ifndef SHELL_FUNC_H_
#define SHELL_FUNC_H_

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include "kernel.h"

typedef struct _mutexINFO
{
    bool lock;
    char lockedBy[16];      //name of task
    char processQueue[MAX_MUTEX_QUEUE_SIZE][16];
    uint8_t queueSize;
} MutexINFO;

typedef struct _semINFO
{
    uint8_t count;
    char processQueue[MAX_SEMAPHORE_QUEUE_SIZE][16];
    uint8_t queueSize;
} SemINFO;

typedef struct _ipcsINFO
{
    MutexINFO mutexes[MAX_MUTEXES];
    SemINFO semaphores[MAX_SEMAPHORES];
} IPCS_INFO;

typedef struct _TaskInfo
{
    void* pid;
    char name[16];
    uint8_t ticks;
    uint8_t state;
    uint64_t cpu;
} TaskInfo;

typedef struct _PS_INFO
{
    TaskInfo tasks[MAX_TASKS];
} PS_INFO;

void printState(uint8_t state);
void ps();
void reboot();
void ipcs();
void kill(uint32_t pid);
void pi(bool on);
void preempt(bool on);
void sched(bool prio_on);
void* pidof(const char name[]);
void pkill(const char name[]);
void run_proc(const char name[]);

#endif
