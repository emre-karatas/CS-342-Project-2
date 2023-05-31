Emre Karataş - 22001641
İpek Öztaş - 22003250

Part A –  (60 pts):
In this project you will implement a program that will simulate multiprocessor
scheduling. The program will simulate two approaches for multiprocessor
scheduling: single-queue approach, and multi-queue approach (Figures 1 and
2). In single-queue approach, we have a single common ready queue
(runqueue) that is used by all processors. In multi-queue approach, each
processor has its own ready queue. Your program will simulate the following
scheduling algorithms: FCFS (first-come first-served), SJF (shortest job first -
nonpreemptive), and RR(Q) (round robin with time quantum Q).

In single-queue approach, a burst will be added to the tail of a single common
queue. All processors will pick bursts from that queue. In multi-queue
approach, bursts will be added to queues by using one of the following
methods:
1. Round robin Method (RM). In this case, burst 1 (first burst) will be added to
queue 1, next burst to queue 2, and so on. After queue N, we will add to
queue 1 again.
2. Load-balancing Method (LM). A burst item will be added to the queue that
has the least load at that time (total remaining length of the bursts in the
queue is smallest). In case of tie, the queue with smaller id is selected.

Part B – Experiments (20 pts):
Do experiments and try to interpret the results. OUTMODE should be 1 while
doing the experiments (nothing will be printed out while simulation is
running).

PART C - Use of condition variables (20 pts):
You will implement the same program in part A, but this time you will use a
condition variable to cause a processor thread to sleep (by using
pthread_cond_wait) as long as the respective queue is empty. When an item
is put to the queue, then the processor thread will be waken up (by using

pthread_cond_signal). For each queue, you will use another condition
variable. If we have just one queue, then one condition variable is enough. With
condition variables you no longer need to sleep for 1 ms repeatedly when the
respective queue is empty. 
