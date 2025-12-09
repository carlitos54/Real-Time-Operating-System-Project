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

## Demo Application & User Interface

The project includes a sample application (`tasks.c`) demonstrating the RTOS capabilities. The system utilizes the board's LEDs to visualize thread states and uses the push buttons to trigger kernel events, context switches, and faults.

### LED Status Indicators
| LED Color | Behavior | Description |
| :--- | :--- | :--- |
| **Orange** | Toggles constantly | Indicates the **Idle Task** is running (lowest priority). |
| **Green** | Toggles @ 4Hz | Indicates the `Flash4Hz` task is running. |
| **Yellow** | Toggles/One-Shot | Controlled by **SW0** (Toggle) and **SW1** (One-shot pulse). |
| **Red** | Toggles/Solid | Toggled by `LengthyFn` task; set Solid/Off by **SW0/SW1**. |
| **Blue** | Toggles | controlled by the `Important` task (Highest Priority). |

### Button Controls
The 6 push buttons are mapped to specific kernel interactions to demonstrate synchronization, priority changes, and fault handling:

| Button | Action | RTOS Feature |
| :--- | :--- | :--- |
| **SW0** | Toggle Yellow LED & Turn ON Red LED | Basic GPIO and Task communication. |
| **SW1** | Signal `flashReq` semaphore & Turn OFF Red LED | **Semaphores:** Signals the `OneShot` task to run once. |
| **SW2** | Restart `Flash4Hz` thread | **Thread Management:** Re-enables a killed task. |
| **SW3** | Kill `Flash4Hz` thread | **Thread Management:** Terminates an active task. |
| **SW4** | Change `LengthyFn` priority to 4 | **Priority Scheduling:** Dynamically changes task priority at runtime. |
| **SW5** | Trigger Memory Write | **MPU Fault:** Attempts to write to protected OS memory (`0x20000000`), triggering a Hard Fault or MPU Fault to test exception handlers. |

### 3. Shell Controls (Runtime Configuration)
Connect via a terminal (115200 baud) to issue these commands that alter Kernel behavior on the fly:

| Command | Arguments | Description | Example Usage |
| :--- | :--- | :--- | :--- |
| `ps` | N/A | Prints out Process Status, this includes the PID (hex), process name, state, sleep ticks (ms), and CPU % | `ps` |
| `ipcs` | N/A | Prints out the status of mutexes and semaphores | `ipcs` |
| `preempt` | `ON` \| `OFF` | Toggles Preemption. When **OFF**, tasks only switch when they `yield()` or block. When **ON**, the SysTick handler forces context switches. | `preempt OFF` (Observe Orange LED blink pattern change) |
| `sched` | `PRIO` \| `RR` | Switches the scheduler algorithm. <br>**PRIO**: Highest priority task runs. <br>**RR**: Round-Robin scheduling (time slicing). | `sched RR` (See tasks share CPU equally regardless of priority) |
| `pi` | `ON` \| `OFF` | Toggles **Priority Inheritance**. Prevents priority inversion when high-priority tasks wait on mutexes held by low-priority tasks. | `pi ON` |
| `pidof` | `<Process_Name>` | Finds the Process ID (PID) of a named task. | `pidof Flash4Hz` |
| `kill` | `<PID>` | Kills a task using its ID (hex). | `kill 0x20002150` |
| `run` | `<Process_Name>` | Restarts a task using its name. | `run Idle` |
| `reboot` | N/A | Performs a system reset. | `reboot` |

---
*Note: This project was created for CSE4354/5392 at UT Arlington, Fall 2025.*
