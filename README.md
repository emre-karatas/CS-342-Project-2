# Multiprocessor Scheduling Simulation - A Project in Three Parts

**Project Team Members**

* Emre Karataş | Student ID: 22001641
* İpek Öztaş | Student ID: 22003250

## Part A: Conceptualising Multiprocessor Scheduling - 60 pts

Unravel the complexities of multiprocessor scheduling through the creation of a simulation that encapsulates both single-queue and multi-queue scheduling approaches.

The single-queue methodology presents a shared common queue (runqueue) that is accessed by all processors. Conversely, the multi-queue methodology allows each processor its own dedicated queue. These dualistic paradigms will facilitate the simulation of various scheduling algorithms, including first-come-first-served (FCFS), shortest-job-first (SJF - non-preemptive), and round robin with time quantum Q (RR(Q)).

In the single-queue approach, a burst will be added to the tail of the shared queue, with each processor selecting bursts from the same. The multi-queue approach presents two methods of burst allocation:

1. **Round Robin Method (RM):** Sequentially distribute bursts across queues, cycling back to the first queue after reaching the final queue. For instance, Burst 1 would be added to Queue 1, the following burst to Queue 2, and so on, looping back to Queue 1 after Queue N.

2. **Load-balancing Method (LM):** Bursts are assigned to the least burdened queue at a given moment (i.e., the queue with the shortest total remaining length of bursts). In case of identical load across multiple queues, the queue with the smaller ID is chosen.

## Part B: Experimentation and Interpretation - 20 pts

Engage in a series of experiments, thoughtfully interpreting the outcomes of your trials. Ensure that OUTMODE is set to 1 for the duration of these experiments, thus refraining from producing printed output while the simulation runs its course.

## Part C: The Application of Condition Variables - 20 pts

Revisit your simulation from Part A, incorporating the use of condition variables to modulate the sleep state of a processor thread. Specifically, employ `pthread_cond_wait` to induce sleep when a respective queue is devoid of items. When an item is added to the queue, awaken the processor thread using `pthread_cond_signal`.

Each queue will operate under the guidance of a distinct condition variable. If a single queue exists, only one condition variable is necessary. Through the introduction of condition variables, repetitive sleep cycles of 1 ms duration are no longer required when a respective queue is empty.
