// Kernel functions
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
#include "mm.h"
#include "kernel.h"
#include "uart0.h"
#include "shell.h"
#include "shell_func.h"
#include "asm.h"

extern int pid;
extern uint8_t curr_tcb_i = 0;           // index of tcb for heap_map allocation ownership
//-----------------------------------------------------------------------------
// RTOS Defines and Kernel Variables
//-----------------------------------------------------------------------------

// mutex
typedef struct _mutex
{
    bool lock;
    uint8_t queueSize;
    uint8_t processQueue[MAX_MUTEX_QUEUE_SIZE];
    uint8_t lockedBy;
} mutex;
mutex mutexes[MAX_MUTEXES];

// semaphore
typedef struct _semaphore
{
    uint8_t count;
    uint8_t queueSize;
    uint8_t processQueue[MAX_SEMAPHORE_QUEUE_SIZE];
} semaphore;
semaphore semaphores[MAX_SEMAPHORES];

// task states
#define STATE_INVALID           0 // no task
#define STATE_UNRUN             1 // task has never been run
#define STATE_READY             2 // has run, can resume at any time
#define STATE_DELAYED           3 // has run, but now awaiting timer
#define STATE_BLOCKED_SEMAPHORE 4 // has run, but now blocked by semaphore
#define STATE_BLOCKED_MUTEX     5 // has run, but now blocked by mutex
#define STATE_KILLED            6 // task has been killed

#define YIELD   0
#define SLEEP   1
#define LOCK    2
#define UNLOCK  3
#define WAIT    4
#define POST    5
#define PI      6
#define SCHED   7   //1 is prio 0 is rr
#define PREEMPT 8
#define REBOOT  9
#define PIDOF   10
#define IPCS    11
#define PS      12
#define PKILL   13
#define KILL    14
#define RUN     15
#define RESTART 16
#define TPRIO   17

// task
uint8_t taskCurrent = 0;          // index of last dispatched task
uint8_t taskCount = 0;            // total number of valid tasks

// control
bool priorityScheduler = false;     // priority (true) or round-robin (false)
bool priorityInheritance = false;   // priority inheritance for mutexes
bool preemption = false;            // preemption (true) or cooperative (false)
bool pingpong = false;              //pingpong buffering

// tcb
#define NUM_PRIORITIES   8
/*
struct _tcb
{
    uint8_t state;                 // see STATE_ values above
    void *pid;                     // used to uniquely identify thread (add of task fn)
    void *sp;                      // current stack pointer
    uint8_t priority;              // 0=highest
    uint8_t currentPriority;       // 0=highest (needed for pi)
    uint32_t ticks;                // ticks until sleep complete
    uint64_t srd;                  // MPU subregion disable bits
    char name[16];                 // name of task used in ps command
    uint8_t mutex;                 // index of the mutex in use or blocking the thread
    uint8_t semaphore;             // index of the semaphore that is blocking the thread
} tcb[MAX_TASKS];
*/
//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

bool initMutex(uint8_t mutex)
{
    bool ok = (mutex < MAX_MUTEXES);
    if (ok)
    {
        mutexes[mutex].lock = false;
        mutexes[mutex].lockedBy = 0;
        mutexes[mutex].queueSize = 0;
    }
    return ok;
}

bool initSemaphore(uint8_t semaphore, uint8_t count)
{
    bool ok = (semaphore < MAX_SEMAPHORES);
    if(ok)
    {
        semaphores[semaphore].count = count;
        semaphores[semaphore].queueSize = 0;
    }
    return ok;
}

// REQUIRED: initialize systick for 1ms system timer
void initRtos(void)
{
    uint8_t i;
    // no tasks running
    taskCount = 0;
    // clear out tcb records
    for (i = 0; i < MAX_TASKS; i++)
    {
        tcb[i].state = STATE_INVALID;
        tcb[i].pid = 0;
        tcb[i].timeA = 0;
        tcb[i].timeB = 0;
    }
    initWTimer();

}

void initWTimer(void)
{
    SYSCTL_RCGCWTIMER_R |= SYSCTL_RCGCWTIMER_R0;        //WTimer 0 clock
    _delay_cycles(3);
    WTIMER0_CTL_R &= ~TIMER_CTL_TAEN;   //timer off for configuration
    WTIMER0_CFG_R = TIMER_CFG_32_BIT_TIMER;
    WTIMER0_TAMR_R = TIMER_TAMR_TAMR_1_SHOT | TIMER_TAMR_TACDIR;
    WTIMER0_TAILR_R = 40000000;
    WTIMER0_TAV_R = 0;
    WTIMER0_CTL_R |= TIMER_CTL_TAEN;

    NVIC_ST_CTRL_R = 0;             //turn off for configuration
    NVIC_ST_RELOAD_R |= 40000;
    NVIC_ST_CURRENT_R = 0;
    NVIC_ST_CTRL_R |= NVIC_ST_CTRL_CLK_SRC | NVIC_ST_CTRL_INTEN | NVIC_ST_CTRL_ENABLE;  //use sys clock and enable interrupt
}

// REQUIRED: Implement prioritization to NUM_PRIORITIES
uint8_t rtosScheduler(void)
{
    if(priorityScheduler)       //priority
    {
        static uint8_t taskArr[NUM_PRIORITIES] = {0};
        uint8_t prio = 0;
        for(prio = 0; prio < NUM_PRIORITIES; prio++)    //go through all 8 prio levels
        {
            uint8_t task = taskArr[prio];
            uint8_t i;
            for(i = 0; i < MAX_TASKS; i++)              //check all tasks at that level
            {
                bool task_state = (tcb[task].state == STATE_READY || tcb[task].state == STATE_UNRUN);
                if(task_state && tcb[task].currentPriority == prio)
                {
                    taskArr[prio] = (task + 1) % taskCount;
                    return task;
                }
                task = (task + 1) % taskCount;
            }
        }
    }

    bool ok;
    static uint8_t task = 0xFF;
    ok = false;
    while (!ok)
    {
        task++;
        if (task >= MAX_TASKS)
            task = 0;
        ok = (tcb[task].state == STATE_READY || tcb[task].state == STATE_UNRUN);
    }
    return task;
}

// REQUIRED: modify this function to start the operating system
// by calling scheduler, set srd bits, setting PSP, ASP bit, call fn with fn add in R0      set srd bits according to task about to run
// fn set TMPL bit, and PC <= fn
// _fn fn = tdc[taskcurrent].pid;       fn();       idle is orange led
// _fnx() = _fn fn;         fn = tcb[taskcurrent].pid;       fn();
// PSP will not point to 0x20008000, it will commit to using IDLE stack
void startRtos(void)
{
    taskCurrent = rtosScheduler();
    applySramAccessMask(tcb[taskCurrent].srd);
    setPSP(tcb[taskCurrent].sp);
    setASP();
    _fn fn = (_fn)tcb[taskCurrent].pid;
    setTMPL(1);
    fn();
}

// REQUIRED:
// add task if room in task list        max task is 12
// store the thread name
// allocate stack space and store top of stack in sp
// set the srd bits based on the memory allocation
bool createThread(_fn fn, const char name[], uint8_t priority, uint32_t stackBytes)     //not allow re-entrancy         run before startRTOS in priv
{
    bool ok = false;
    uint8_t i = 0;
    bool found = false;
    if (taskCount < MAX_TASKS)
    {
        // make sure fn not already in list (prevent reentrancy)
        while (!found && (i < MAX_TASKS))       //not found and i is less than 12
        {
            found = (tcb[i++].pid ==  fn);      //pid address in memory when function is at
        }
        if (!found)
        {
            //move sp to the requested value, if request is 700, malloc gives 1024, move sp up 700 from base address given from malloc
            //sp will be decrementing by 4 bytes
            // TO DO: 0 won't work, should call malloc, but not value of pointer, adjust by size of stack
            // find first available tcb record
            i = 0;
            while (tcb[i].state != STATE_INVALID) {i++;}
            tcb[i].state = STATE_UNRUN;     //just created, unrun means hasn't run yet
            tcb[i].pid = fn;                //pid is function pointer       new pid in malloc table to validate pid matches owner
            taskCurrent = i;
            uint64_t srdMask = createNoSramAccessMask();                           //no access for any subregion in unpriv
            uint32_t * base_add = mallocHeap(stackBytes);     //will return pointer of base address
            addSramAccessWindow(&srdMask, base_add, stackBytes);
            uint32_t i_sp = ((uint32_t)base_add + stackBytes) & (~0x7);
            tcb[i].sp = (void *)i_sp;     //store top of stack in sp
            //tcb[i].base_add = (uint32_t)base_add;
            tcb[i].priority = priority;
            tcb[i].currentPriority = priority;
            tcb[i].srd = srdMask;                 //16 min in, listen again 10/14
            tcb[i].req_size = stackBytes;
            int8_t j = 0;
            for(j = 0; j < 15; j ++)        //stores name of thread in tcb
            {
                if(name[j] == '\0') break;
                tcb[i].name[j] = name[j];
            }
            tcb[i].name[j] = '\0';

            // increment task count
            taskCount++;
            ok = true;
        }
    }
    return ok;
}

_fn getPid(void)
{
    return (_fn)tcb[taskCurrent].pid;
}

// REQUIRED: modify this function to kill a thread
// REQUIRED: free memory, remove any pending semaphore waiting,
//           unlock any mutexes, mark state as killed
void killThread(_fn pid)
{
    __asm(" SVC #14");
}

void killT(_fn pid)
{
    uint8_t i = 0,j = 0;
    for(i = 0; i < 12; i++)
    {
        if(tcb[i].pid == pid && tcb[i].state != STATE_KILLED)
        {
            freeHeapPid(tcb[i].pid);                   //memory freed
            tcb[i].srd = createNoSramAccessMask();              //clear srd, base add, and sp
            tcb[i].sp = NULL;
            if(tcb[i].state == STATE_BLOCKED_MUTEX)             //removes from queue if blocked
            {
                for(j = 0;j < mutexes[0].queueSize; j++)
                {
                    if(mutexes[0].processQueue[j] == i)
                    {
                        if(j == 0)
                        mutexes[0].processQueue[0] = mutexes[0].processQueue[1];
                        mutexes[0].processQueue[1] = 0;
                        mutexes[0].queueSize--;
                        break;
                    }
                }
            }
            else if(tcb[i].state == STATE_BLOCKED_SEMAPHORE)
            {
                if(tcb[i].semaphore < MAX_SEMAPHORES)
                {
                    for(j = 0;j < semaphores[tcb[i].semaphore].queueSize; j++)
                    {
                        if(semaphores[tcb[i].semaphore].processQueue[j] == i)
                        {
                            if(j == 0)
                            semaphores[tcb[i].semaphore].processQueue[0] = semaphores[tcb[i].semaphore].processQueue[1];
                            semaphores[tcb[i].semaphore].processQueue[1] = 0;
                            semaphores[tcb[i].semaphore].queueSize--;
                            break;
                        }
                    }
                }
            }
            tcb[i].state = STATE_KILLED;
            if(i == taskCurrent)                    //task switch in case curr task is killed
            {
                NVIC_INT_CTRL_R |= NVIC_INT_CTRL_PEND_SV;
            }
        }
    }
}

// REQUIRED: modify this function to restart a thread, including creating a stack
void restartThread(_fn fn)
{
    __asm(" SVC #16");
}

// REQUIRED: modify this function to set a thread priority
void setThreadPriority(_fn fn, uint8_t priority)
{
    __asm(" SVC #17");
}

// REQUIRED: modify this function to yield execution back to scheduler using pendsv
void yield(void)
{
    __asm(" SVC #0");
}

// REQUIRED: modify this function to support 1ms system timer
// execution yielded back to scheduler until time elapses using pendsv
void sleep(uint32_t tick)
{
    __asm(" SVC #1");
}

// REQUIRED: modify this function to wait a semaphore using pendsv
void wait(int8_t semaphore)
{
    __asm(" SVC #4");
}

// REQUIRED: modify this function to signal a semaphore is available using pendsv
void post(int8_t semaphore)
{
    __asm(" SVC #5");
}

// REQUIRED: modify this function to lock a mutex using pendsv
void lock(int8_t mutex)
{
    __asm(" SVC #2");
}

// REQUIRED: modify this function to unlock a mutex using pendsv
void unlock(int8_t mutex)
{
    __asm(" SVC #3");
}

// REQUIRED: modify this function to add support for the system timer
// REQUIRED: in preemptive code, add code to request task switch
void systickIsr(void)               //goes off every ms
{
    uint8_t i = 0;
    static uint32_t time = 0;
    for(i = 0; i < taskCount; i++)
    {
        if(tcb[i].state == STATE_DELAYED)           //if delayed increment ticks till 0
        {
            tcb[i].ticks--;
            if(tcb[i].ticks == 0)
            {
                tcb[i].state = STATE_READY;
            }
        }
    }
    if(priorityInheritance && mutexes[0].lock)
    {
       uint8_t owner = mutexes[0].lockedBy;
       uint8_t highprio = 100;
       if(mutexes[0].queueSize != 0)
       {
           if(tcb[mutexes[0].processQueue[0]].currentPriority < tcb[mutexes[0].processQueue[1]].currentPriority)
               highprio = tcb[mutexes[0].processQueue[0]].currentPriority;
           else
               highprio = tcb[mutexes[0].processQueue[1]].currentPriority;
           if(highprio < tcb[owner].priority)
           {
               tcb[owner].currentPriority = highprio;
           }
           else
           {
               tcb[owner].currentPriority = tcb[owner].priority;
           }
       }
    }

    time++;
    if(time > 500)
    {
        time = 0;
        pingpong = !pingpong;
        for(i = 0; i < taskCount; i++)
        {
            if(pingpong)
                tcb[i].timeA = 0;
            else
                tcb[i].timeB = 0;
        }
    }
    if(preemption)
    {
        NVIC_INT_CTRL_R |= NVIC_INT_CTRL_PEND_SV;
    }
}

// REQUIRED: in coop and preemptive, modify this function to add support for task switching
// REQUIRED: process UNRUN and READY tasks differently
__attribute__((naked))
void pendSvIsr(void)
{
    pushRegs();                                     //push SW regs
    tcb[taskCurrent].sp = (void*)getPSP();          //sync tcb.sp w/ PSP
    if(pingpong)
        tcb[taskCurrent].timeA += WTIMER0_TAV_R;
    else
        tcb[taskCurrent].timeB += WTIMER0_TAV_R;
    WTIMER0_CTL_R &= ~TIMER_CTL_TAEN;
    taskCurrent = rtosScheduler();                  //gets current task
    WTIMER0_TAV_R = 0;
    WTIMER0_CTL_R |= TIMER_CTL_TAEN;
    applySramAccessMask(tcb[taskCurrent].srd);
    if(tcb[taskCurrent].state == STATE_UNRUN)
    {
        tcb[taskCurrent].state = STATE_READY;       //set state to ready
        uint32_t * sp = (uint32_t *)tcb[taskCurrent].sp;
        sp -= 17;
        sp[0] = 0xFFFFFFFD;                         //LR Exception Result   lowest mem address
        sp[1] = 111;                                //R4
        sp[2] = 110;
        sp[3] = 109;
        sp[4] = 108;
        sp[5] = 107;
        sp[6] = 106;
        sp[7] = 105;
        sp[8] = 104;                                //R11
        sp[9] = 100;                                //R0
        sp[10] = 101;
        sp[11] = 102;
        sp[12] = 103;                               //R3
        sp[13] = 112;                               //R12
        sp[14] = 0x11111111;                        //LR
        sp[15] = (uint32_t)tcb[taskCurrent].pid;    //PC
        sp[16] = 0x01000000;                        //xPSR      highest mem address
        tcb[taskCurrent].sp = (void*)sp;            //update tcb.sp
    }
    setPSP((uint32_t*)tcb[taskCurrent].sp);
    popRegs();
}

void restart(uint8_t task)
{
uint32_t req_size = 0;
    if(tcb[task].state == STATE_KILLED)
    {
        tcb[task].state = STATE_UNRUN;
        tcb[task].currentPriority = tcb[task].priority;   //set priority to original prio
        tcb[task].mutex = 0;
        tcb[task].semaphore = 0;
        req_size = tcb[task].req_size;
        uint32_t * base_add = mallocHeap(req_size);     //will return pointer of base address
        global_srdMask = createNoSramAccessMask();
        addSramAccessWindow(&global_srdMask, base_add, req_size);
        uint32_t i_sp = ((uint32_t)base_add + req_size) & (~0x7);
        tcb[task].sp = (void *)i_sp;     //store top of stack in sp
        tcb[task].srd = global_srdMask;
    }
}

// REQUIRED: modify this function to add support for the service call
// REQUIRED: in preemptive code, add code to handle synchronization primitives
void svCallIsr(void)
{
    uint32_t * psp = getPSP();
    uint8_t *pc = (uint8_t*) psp[6];
    uint32_t R0 = psp[0];
    uint32_t R1 = psp[1];
    uint32_t PID = 0;
    uint32_t tasktime = 0;
    uint64_t totaltime = 0;
    uint8_t num = *(pc - 2);
    uint8_t ID = (uint8_t)R0;
    uint8_t i = 0, j = 0;
    IPCS_INFO *IPSCdata;
    PS_INFO *PSdata;
    char * name;
    bool power;
    _fn fn;
    switch(num)
    {
    case YIELD:
        NVIC_INT_CTRL_R |= NVIC_INT_CTRL_PEND_SV;
        break;
    case SLEEP:
        tcb[taskCurrent].ticks = R0;
        tcb[taskCurrent].state = STATE_DELAYED;
        NVIC_INT_CTRL_R |= NVIC_INT_CTRL_PEND_SV;
        break;
    case LOCK:
        if(ID < MAX_MUTEXES)
        {
            if(mutexes[ID].lock == true)
            {
                if(mutexes[ID].queueSize < MAX_MUTEX_QUEUE_SIZE)
                {
                    tcb[taskCurrent].state = STATE_BLOCKED_MUTEX;   //task state to blocked
                    tcb[taskCurrent].mutex = ID;
                    mutexes[ID].processQueue[mutexes[ID].queueSize] = taskCurrent;
                    mutexes[ID].queueSize++;
                    NVIC_INT_CTRL_R |= NVIC_INT_CTRL_PEND_SV;
                }
            }
            else
            {
                mutexes[ID].lock = true;
                mutexes[ID].lockedBy = taskCurrent;
                tcb[taskCurrent].mutex = ID;                //ownership recorded in tcb
            }
        }
        break;
    case UNLOCK:
        if((mutexes[ID].lockedBy == taskCurrent) && (ID < MAX_MUTEXES))
        {
            mutexes[ID].lock = false;
            mutexes[ID].lockedBy = 0;
            tcb[taskCurrent].mutex = 0;
            if(mutexes[ID].queueSize > 0)
            {
                mutexes[ID].lock = true;
                mutexes[ID].lockedBy = mutexes[ID].processQueue[0];
                tcb[mutexes[ID].processQueue[0]].mutex = 0;
                tcb[mutexes[ID].processQueue[0]].state = STATE_READY;
                mutexes[ID].processQueue[0] = mutexes[ID].processQueue[1];
                mutexes[ID].queueSize--;
                NVIC_INT_CTRL_R |= NVIC_INT_CTRL_PEND_SV;
            }
        }
        else
        {
            killThread((_fn)tcb[taskCurrent].pid);
            NVIC_INT_CTRL_R |= NVIC_INT_CTRL_PEND_SV;
        }
        break;
    case WAIT:
        if(semaphores[ID].count > 0)
        {
            semaphores[ID].count --;
        }
        else
        {
            tcb[taskCurrent].state = STATE_BLOCKED_SEMAPHORE;
            tcb[taskCurrent].semaphore = ID;
            semaphores[ID].processQueue[semaphores[ID].queueSize] = taskCurrent;
            semaphores[ID].queueSize++;
            NVIC_INT_CTRL_R |= NVIC_INT_CTRL_PEND_SV;
        }
        break;
    case POST:
        if(semaphores[ID].queueSize > 0)
        {
            tcb[semaphores[ID].processQueue[0]].state = STATE_READY;
            tcb[semaphores[ID].processQueue[0]].semaphore = 0;
            semaphores[ID].processQueue[0] = semaphores[ID].processQueue[1];
            semaphores[ID].processQueue[1] = 0;
            semaphores[ID].queueSize--;
            NVIC_INT_CTRL_R |= NVIC_INT_CTRL_PEND_SV;
        }
        else
        {
            semaphores[ID].count++;
        }
        break;
    case PI:
        power = (bool)R0;
        if(power)
            priorityInheritance = true;
        else
            priorityInheritance = false;
        break;
    case SCHED:
        power = (bool)R0;
        if(power)
            priorityScheduler = true;       //1 is prio
        else
            priorityScheduler = false;      //0 is rr
        break;
    case PREEMPT:
        power = (bool)R0;
        if(power)
            preemption = true;
        else
            preemption = false;
        break;
    case REBOOT:
        NVIC_APINT_R = (NVIC_APINT_VECTKEY | 4);
        break;
    case PIDOF:
        name = (char*)R0;
        power = false;
        for(i = 0; i < taskCount; i++)
        {
            if(tcb[i].state != STATE_INVALID && strcompare(tcb[i].name, name))
            {
                power = true;
                psp[0] = (uint32_t)tcb[i].pid;
                return;
            }
        }
        if(!power)
        {
            psp[0] = 0;
        }
        break;
    case IPCS:
        IPSCdata = (IPCS_INFO*)R0;

        //fill mutex data
        IPSCdata->mutexes[0].lock = mutexes[0].lock;
        if(mutexes[0].lock)
        {
            char* source = tcb[mutexes[0].lockedBy].name;
            char* dest = IPSCdata->mutexes[0].lockedBy;
            for(i = 0; (i < 15 && source[i] != 0); i++)
            {
                dest[i] = source[i];
            }
            dest[i] = 0;
        }
        else
        {
            IPSCdata->mutexes[0].lockedBy[0] = '\0';
        }

        IPSCdata->mutexes[0].queueSize = mutexes[0].queueSize;
        char* source = tcb[mutexes[0].processQueue[0]].name;
        char* dest = IPSCdata->mutexes[0].processQueue[0];
        for(i = 0; (i < 15 && source[i] != 0); i++)
        {
            dest[i] = source[i];
        }
        dest[i] = 0;

        //fill semaphore data
        for(i = 0; i < MAX_SEMAPHORES; i++)
        {
            IPSCdata->semaphores[i].count = semaphores[i].count;
            IPSCdata->semaphores[i].queueSize = semaphores[i].queueSize;
            char* source = tcb[semaphores[i].processQueue[0]].name;
            char* dest = IPSCdata->semaphores[i].processQueue[0];
            for(j = 0;(j < 15 && source[j] != 0); j++)
            {
                dest[j] = source[j];
            }
            dest[j] = 0;
        }
        break;
    case PS:
        PSdata = (PS_INFO*)R0;
        totaltime = 0;
        for(i = 0; i < taskCount; i++)
        {
            if(pingpong)
                totaltime += tcb[i].timeA;
            else
                totaltime += tcb[i].timeB;
        }
        for(i = 0; i < MAX_TASKS; i++)
        {
            PSdata->tasks[i].pid = tcb[i].pid;
            PSdata->tasks[i].ticks = tcb[i].ticks;
            PSdata->tasks[i].state = tcb[i].state;

            if(pingpong)
                tasktime = tcb[i].timeA;
            else
                tasktime = tcb[i].timeB;

            if(totaltime > 0)
                PSdata->tasks[i].cpu = ((uint64_t)tasktime * 10000) / totaltime;
            else
                PSdata->tasks[i].cpu = 0;

            char* source = tcb[i].name;
            char* dest = PSdata->tasks[i].name;
            for(j = 0;(j < 15 && source[j] != 0); j++)
            {
                dest[j] = source[j];
            }
            dest[j] = 0;
        }
        break;
    case PKILL:
        name = (char*)R0;
        for(i = 0; i < taskCount; i++)
        {   //if state valid, matches the name, and has not already been killed
            if(tcb[i].state != STATE_INVALID && strcompare(tcb[i].name, name) && tcb[i].state != STATE_KILLED)
            {
                killT((_fn)tcb[i].pid);
                break;
            }
        }
        break;
    case KILL:                  //void killT(_fn pid)   void killThread(_fn pid)
        PID = (uint32_t)R0;
        for(i = 0; i < taskCount; i++)
        {   //if state is not invalid, pid matches one in the tcb, and has not already been killed
            if(tcb[i].state != STATE_INVALID && ((uint32_t)tcb[i].pid == PID) && tcb[i].state != STATE_KILLED)
            {
                killT((_fn)tcb[i].pid);
                break;
            }
        }
        break;
    case RUN:
        name = (char*)R0;
        for(i = 0; i < taskCount; i++)
        {   //if state valid, matches the name, and has not already been killed
            if(tcb[i].state != STATE_INVALID && strcompare(tcb[i].name, name))
            {
                restart(i);
                break;
            }
        }
        break;
    case RESTART:
        fn = (_fn)R0;
        uint8_t i = 0;
        for(i = 0; i < taskCount; i++)
        {
            if(tcb[i].pid == fn)    //task found
            {
                restart(i);
                break;
            }
        }
        break;
    case TPRIO:                         //setThreadPriority(_fn fn, uint8_t priority)
        fn = (_fn)R0;
        uint8_t prio = (uint8_t)R1;
        for(i = 0; i < taskCount; i++)
        {
            if(tcb[i].pid == fn)    //task found
            {
                tcb[i].currentPriority = prio;      //update current prio
                break;
            }
        }
        break;
    }
    //NVIC_INT_CTRL_R |= NVIC_INT_CTRL_PEND_SV;
}


