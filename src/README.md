# Multiprocessor CPU Scheduler Simulator

A multithreaded operating system simulator written in C, implementing a CPU scheduler on top of a pthreads-based multiprocessor framework.

## What it does

Simulates an OS running multiple processes across multiple CPU cores, where each CPU is represented by its own thread. The scheduler decides which process runs on which CPU at any given moment, tracking process state transitions (NEW → READY → RUNNING → WAITING → TERMINATED) and enforcing thread-safe access to shared data structures via mutexes and condition variables.

## Scheduling algorithms implemented

- **FCFS (First Come, First Serve)**: non-preemptive, processes run to completion or until they block for I/O
- **Round-Robin**: preemptive, fixed timeslice per process, configurable via `-r <timeslice>`
- **Preemptive Priority Scheduling**: higher-priority processes preempt lower-priority ones running on any CPU
- **Shortest Remaining Time First (SRTF)**: preemptive, always runs the process with the least total remaining burst time

## Key implementation details

- Custom thread-safe ready queue (enqueue/dequeue) shared across CPU threads, protected by `ready_mutex`
- Per-CPU `current[]` array tracking the running PCB on each core, protected by `current_mutex`
- Condition variable based idle process that blocks until work is available (no busy-waiting)
- Preemption handled via `mark_for_preemption()` for non-timeslice-based algorithms (Priority, SRTF)
- Verified race-condition-free using GDB (deadlock diagnosis) and Valgrind (Helgrind/DRD)

## Files

- `student.c`: scheduler implementation (all of the above)
- `os-sim.c` / `os-sim.h`: simulator framework (provided)
- `process.c` / `process.h`: simulated process definitions (provided)
- `answers.txt`: written analysis of scheduler behavior across CPU counts and timeslice lengths

## Running it

```bash
make debug
./os-sim <num_cpus>              # FCFS
./os-sim <num_cpus> -r <ms>      # Round-Robin
```
