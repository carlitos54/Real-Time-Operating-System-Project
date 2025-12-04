# Real-Time-Operating-System-Project

## Overview
A preemptive Real-Time Operating System (RTOS) ARM Cortex-M4F microcontroller featuring semaphores, mutexes, priority scheduling, memory protection, and an interactive shell.

## Features

### Kernel & Scheduler
* **Preemptive Scheduling:** Implements context switching using `PendSV` and `SysTick` interrupts.
* **Scheduling Algorithms:** Supports both **Priority Scheduling** (8 levels, 0 highest to 7 lowest) and **Round-Robin** scheduling.
* **Task Management:** Support for yielding, sleeping, and dynamic stack allocation.
* **Memory Protection:** Utilizes the Memory Protection Unit (MPU) in the TM4C to isolate task memory.

### Synchrontization
* **Semaphores:** Counting semaphores for resource tracking and signaling (`wait` / `post`)
* **Mutexes:** Binary mutal exclusion locks with onwership tracking

### Interactive Shell
A built-in shell interface using UART that allows the user to interact with the OS at runtime
Supported Commands:
|Command | Description |
| :--- | :--- |
| `ps` | Displays process info: PID, name, state, sleep ticks (ms) and CPU usage %.|
| `ipcs` | Displays status of mutexes and semaphores. |
| `kill <PID>` | Kills thread by its Process ID. |
| `pkill <Name>` | Kills a thread by its name. |
| `pidof <Name>` | Returns the PID of a specified thread name. |
| `run <Name>` | Launches a task (if not already running). |
| `reboot` | Restarts the microcontroller. |
| `sched <MODE>` | Switches scheduling mode (`prio` or `rr`). |
| `preempt <ON/OFF> ` | Toggles preemption on or off. |
| `pi <ON/OFF> ` | Toggles priority inheritance on or off. |

## Techinal Implementation
* **Context Switching:** Custom assembly handlers for `PendSV` to save/restore R4-R11 and stack pointers (PSP) as well as push exception results.
* **System Calls:** Kernel functions (sleep, yield, lock, etc...) are handled via `SVC` (Supervisor Call) exception or invoked by the kernel.
* **Timing** `SysTick` timer is used for sleep duration, preemption time slicing, and priority inheritance.

## Hardware Structure

* BLUE_LED -> Port F2
* RED_LED -> Port E0
* ORANGE_LED -> Port A2
* YELLOW_LED -> Port A3
* GREEN_LED -> Port A4
* SW0 -> Port C4
* SW1 -> Port C5
* SW2 -> Port C6
* SW3 -> Port C7
* SW4 -> Port D6
* SW5 -> Port D1
* SW6 -> Port F4

## Software Structure
* `kernel.c` : The core kernel implementation (scheduler, context switching, thread creation/killing)
* `faults.c` : Faults stack dump and print the PID of the task.
* `mm.c` : Contains mallocs first fit algorithm, memory manager, stack deallocation, and memory protection unit.
* `shell.c` : Command line interface/parsing and formatting.
* `tasks.c` : Outlines how each task interacts with the hardware.
* `asm.s` : Assembly file for stack push/pop, bit setting for PSP, and privilege level.
* `rtos.c` : Thread creations and starting the RTOS.
* `shell.c` : Command line interface/parsing and formatting.
* `tm4c123gh6pm_startup_ccs.c`: Startup code, vector table definitions, and heap declaration.

## Usage
1. **Build** Compile project using ARM toolchain (Code Composer Studio).
2. **Connect** Connect to the board using a terminal emulator (e.g., PuTTY) with a baud rate of 115200 as specified in the `rtos.c` file to access the shell.
---
*Note: This project was created for CSE4354/5392 at UT Arlington, Fall 2025.*
