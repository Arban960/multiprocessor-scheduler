/*
 * student.c
 * Multithreaded OS Simulation for CS 2200
 * Spring 2026
 *
 * This file contains the CPU scheduler for the simulation.
 */

#include <assert.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "student.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"

/** Function prototypes **/
extern void idle(unsigned int cpu_id);
extern void on_preempt(unsigned int cpu_id);
extern void yield(unsigned int cpu_id);
extern void terminate(unsigned int cpu_id);
extern void wake_up(pcb_t *process);

static unsigned int cpu_count;

/*
 * current[] is an array of pointers to the currently running processes.
 * There is one array element corresponding to each CPU in the simulation.
 *
 * current[] should be updated by schedule() each time a process is scheduled
 * on a CPU.  Since the current[] array is accessed by multiple threads, you
 * will need to use a mutex to protect it.  current_mutex has been provided
 * for your use.
 *
 * rq is a pointer to a struct you should use for your ready queue
 * implementation. The head of the queue corresponds to the process
 * that is about to be scheduled onto the CPU, and the tail is for
 * convenience in the enqueue function. See student.h for the
 * relevant function and struct declarations.
 *
 * Similar to current[], rq is accessed by multiple threads,
 * so you will need to use a mutex to protect it. ready_mutex has been
 * provided for that purpose.
 *
 * The condition variable queue_not_empty has been provided for you
 * to use in conditional waits and signals.
 *
 * Please look up documentation on how to properly use pthread_mutex_t
 * and pthread_cond_t.
 *
 * A scheduler_algorithm variable and sched_algorithm_t enum have also been
 * supplied to you to keep track of your scheduler's current scheduling
 * algorithm. You should update this variable according to the program's
 * command-line arguments. Read student.h for the definitions of this type.
 */
static pcb_t **current;
static queue_t *rq;

static pthread_mutex_t current_mutex;
static pthread_mutex_t queue_mutex;
static pthread_cond_t queue_not_empty;

static sched_algorithm_t scheduler_algorithm;
static unsigned int cpu_count;
static int utimeslice = -1;

/** ------------------------Problem 0 & 3-----------------------------------
 * Checkout PDF Section 2 and 5 for this problem
 *
 * enqueue() is a helper function to add a process to the ready queue.
 *
 * NOTE: For Priority, FCFS, and SRTF scheduling, you will need to have
 * additional logic in this function and/or the dequeue function to pick the
 * process with the smallest priority.
 *
 *
 * @param queue pointer to the ready queue
 * @param process process that we need to put in the ready queue
 */
void enqueue(queue_t *queue, pcb_t *process) {
  process->next = NULL;
  if (is_empty(queue)) {
    queue->head = process;
    queue->tail = process;
  } else {
    // just add to tail
    queue->tail->next = process;
    queue->tail = process;
  }
}

/**
 * dequeue() is a helper function to remove a process to the ready queue.
 *
 * NOTE: For Priority, FCFS, and SRTF scheduling, you will need to have
 * additional logic in this function and/or the enqueue function to pick the
 * process with the smallest priority.
 *
 *
 * @param queue pointer to the ready queue
 */
pcb_t *dequeue(queue_t *queue) {
  if (is_empty(queue)) {
    return NULL;
  }
  pcb_t *best_prev = NULL;
  pcb_t *best = queue->head;
  pcb_t *prev = NULL;
  pcb_t *curr = queue->head;
  if (scheduler_algorithm == FCFS) {
    while (curr != NULL) {
      if (curr->arrival_time < best->arrival_time) {
        best = curr;
        best_prev = prev;
      }
      prev = curr;
      curr = curr->next;
    }
  }
  if (scheduler_algorithm == PRIORITY) {
    while (curr != NULL) {
        if (curr->priority < best->priority) {
          best = curr;
          best_prev = prev;
        }
        prev = curr;
        curr = curr->next;
      
    }} else if (scheduler_algorithm == SRTF) {
      while (curr != NULL) {
        if (curr->total_time_remaining < best->total_time_remaining) {
          best = curr;
          best_prev = prev;
        }
        prev = curr;
        curr = curr->next;
      }
    }
    
  if (best_prev == NULL) {
    queue->head = best->next;
  } else {
    best_prev->next = best->next;
  }

  if (queue->tail == best) {
    queue->tail = best_prev;
  }

  best->next = NULL;
  return best;
}

/** ------------------------Problem 0-----------------------------------
 * Checkout PDF Section 2 for this problem
 *
 * is_empty() is a helper function that returns whether the ready queue
 * has any processes in it.
 *
 * @param queue pointer to the ready queue
 *
 * @return a boolean value that indicates whether the queue is empty or not
 */
bool is_empty(queue_t *queue) { 
  return queue->head == NULL;
 }

/** ------------------------Problem 1B-----------------------------------
 * Checkout PDF Section 3 for this problem
 *
 * schedule() is your CPU scheduler.
 *
 * Remember to specify the timeslice if the scheduling algorithm is Round-Robin
 *
 * @param cpu_id the target cpu we decide to put our process in
 */
static void schedule(unsigned int cpu_id) { 
  pthread_mutex_lock(&queue_mutex);
  if (is_empty(rq)) {
    pthread_mutex_unlock(&queue_mutex);
    pthread_mutex_lock(&current_mutex);
    current[cpu_id] = NULL;
    pthread_mutex_unlock(&current_mutex);
    context_switch(cpu_id, NULL, -1);
    return;
  }

  pcb_t *next = dequeue(rq);
  pthread_mutex_unlock(&queue_mutex);
  next->state = PROCESS_RUNNING;
  pthread_mutex_lock(&current_mutex);
  current[cpu_id] = next;
  pthread_mutex_unlock(&current_mutex);

  int slice = -1;
  if (scheduler_algorithm == RR) {
    slice = timeslice;
  }
  context_switch(cpu_id, next, slice);
}

/**  ------------------------Problem 1A-----------------------------------
 * Checkout PDF Section 3 for this problem
 *
 * idle() is your idle process.  It is called by the simulator when the idle
 * process is scheduled. This function should block until a process is added
 * to your ready queue.
 *
 * @param cpu_id the cpu that is waiting for process to come in
 */
extern void idle(unsigned int cpu_id) {
  pthread_mutex_lock(&queue_mutex);

  while (is_empty(rq)) {
    pthread_cond_wait(&queue_not_empty, &queue_mutex);
  }

  pthread_mutex_unlock(&queue_mutex);

  schedule(cpu_id);
  /*
   * REMOVE THE LINE BELOW AFTER IMPLEMENTING IDLE()
   *
   * idle() must block when the ready queue is empty, or else the CPU threads
   * will spin in a loop.  Until a ready queue is implemented, we'll put the
   * thread to sleep to keep it from consuming 100% of the CPU time.  Once
   * you implement a proper idle() function using a condition variable,
   * remove the call to mt_safe_usleep() below.
   */
}

/** ------------------------Problem 2 & 3-----------------------------------
 * Checkout Section 4 and 5 for this problem
 *
 * on_preempt() is the handler used in Round-robin, Preemptive Priority, and
 * SRTF scheduling.
 *
 * This function should place the currently running process back in the
 * ready queue, and call schedule() to select a new runnable process.
 *
 * @param cpu_id the cpu in which we want to preempt process
 */
extern void on_preempt(unsigned int cpu_id) {
  pthread_mutex_lock(&current_mutex);
  pcb_t *preempted = current[cpu_id];
  current[cpu_id] = NULL;
  pthread_mutex_unlock(&current_mutex);

  if (preempted != NULL) {
    preempted->state = PROCESS_READY;
    pthread_mutex_lock(&queue_mutex);
    enqueue(rq, preempted);
    pthread_cond_signal(&queue_not_empty);
    pthread_mutex_unlock(&queue_mutex);
  }

  schedule(cpu_id);
}

/**  ------------------------Problem 1A-----------------------------------
 * Checkout PDF Section 3 for this problem
 *
 * yield() is the handler called by the simulator when a process yields the
 * CPU to perform an I/O request.
 *
 * @param cpu_id the cpu that is yielded by the process
 */
extern void yield(unsigned int cpu_id) {
  pthread_mutex_lock(&current_mutex);
  pcb_t *proc = current[cpu_id];
  current[cpu_id] = NULL;
  pthread_mutex_unlock(&current_mutex);

  if (proc != NULL) {
    proc->state = PROCESS_WAITING;
  }
  schedule(cpu_id);
 }

/**  ------------------------Problem 1A-----------------------------------
 * Checkout PDF Section 3
 *
 * terminate() is the handler called by the simulator when a process completes.
 *
 * @param cpu_id the cpu we want to terminate
 */
extern void terminate(unsigned int cpu_id) {
  pthread_mutex_lock(&current_mutex);
  pcb_t *proc = current[cpu_id];
  current[cpu_id] = NULL;
  pthread_mutex_unlock(&current_mutex);

  if (proc != NULL) {
    proc->state = PROCESS_TERMINATED;
  }
  schedule(cpu_id);
 }

/**  ------------------------Problem 1A & 3---------------------------------
 * Checkout PDF Section 3 and 5 for this problem
 *
 * wake_up() is the handler called by the simulator when a process's I/O
 * request completes.
 * This method will also need to handle priority and SRTF preemption.
 * Look in section 5 of the PDF for more info.
 *
 * We've provided an API for marking a CPU for preemption via
 * `mark_for_preemption(unsigned int cpu_id)`
 *
 * @param process the process that finishes I/O and is ready to run on CPU
 */
extern void wake_up(pcb_t *process) {
  process->state = PROCESS_READY;
  pthread_mutex_lock(&queue_mutex);
  enqueue(rq, process);
  pthread_cond_signal(&queue_not_empty);
  pthread_mutex_unlock(&queue_mutex);
  if (scheduler_algorithm != PRIORITY && scheduler_algorithm != SRTF) {
    return;
  }
  pthread_mutex_lock(&current_mutex);
  
  for (unsigned int i = 0; i<cpu_count; i++) {
      if(current[i]==NULL) {
        pthread_mutex_unlock(&current_mutex);
        return;
      }
    }
  int victim_cpu = -1;
  unsigned int worst_priority = (unsigned int)-1;
  unsigned int most_time = -1;
  if (scheduler_algorithm == PRIORITY) {


    for (unsigned int i = 0; i < cpu_count; i++) {
      if (current[i] != NULL) {
        if (victim_cpu == -1 || current[i]->priority > worst_priority) {
          worst_priority = current[i]->priority;
          victim_cpu = (int)i;
        }
      }
    }
      


      if (victim_cpu != -1 && process->priority < current[victim_cpu]->priority) {
        pthread_mutex_unlock(&current_mutex);
        mark_for_preemption((unsigned int)victim_cpu);
        return;
      }} else {
        for (unsigned int i = 0; i < cpu_count; i++) {
          if (current[i] != NULL) {
            if (victim_cpu == -1 || current[i]->total_time_remaining>most_time) {
              most_time = current[i]->total_time_remaining;
              victim_cpu=(int)i;
            }
          }
        }

        if (victim_cpu != -1 && process->total_time_remaining < current[victim_cpu]->total_time_remaining) {
        pthread_mutex_unlock(&current_mutex);
        mark_for_preemption((unsigned int) victim_cpu);
        return;
      }
      }

      
    

    pthread_mutex_unlock(&current_mutex);
  }



/**
 * main() simply parses command line arguments, then calls start_simulator().
 *
 * You don't need
 */
int main(int argc, char *argv[]) {
  /* FIX ME */
  scheduler_algorithm = FCFS;

  if (argc == 3 && !strcmp(argv[2], "-p")) {
    scheduler_algorithm = PRIORITY;
  } else if (argc == 4 && !strcmp(argv[2], "-r")) {
    scheduler_algorithm = RR;
    timeslice = atoi(argv[3]);
  } else if (argc == 3 && !strcmp(argv[2], "-s")) {
    scheduler_algorithm = SRTF;
  } else if (argc != 2) {
    fprintf(stderr, "CS 2200 Project 4 -- Multithreaded OS Simulator\n"
                    "Usage: ./os-sim <# CPUs> [ -r <time slice> | -p | -s ]\n"
                    "    Default : FCFS Scheduler\n"
                    "         -p : Priority Aging Scheduler\n"
                    "         -r : Round Robin Scheduler\n"
                    "         -s : Shortest Remaining Time First\n");
    return -1;
  }

  /* Parse the command line arguments */
  cpu_count = strtoul(argv[1], NULL, 0);

  /* Allocate the current[] array and its mutex */
  current = malloc(sizeof(pcb_t *) * cpu_count);
  assert(current != NULL);
  pthread_mutex_init(&current_mutex, NULL);
  pthread_mutex_init(&queue_mutex, NULL);
  pthread_cond_init(&queue_not_empty, NULL);
  rq = (queue_t *)malloc(sizeof(queue_t));
  assert(rq != NULL);
  rq->head = NULL;
  rq->tail = NULL;
  /* Start the simulator in the library */
  start_simulator(cpu_count);

  return 0;
}

#pragma GCC diagnostic pop
