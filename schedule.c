#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

#define MAX_BUF_SIZE 256
#define MAX_NUM_PROCESSORS 10

typedef struct burst_t {
    int pid;
    int burst_length;
    int arrival_time;
    int remaining_time;
    int finish_time;
    int turnaround_time;
    int processor_id;
    struct burst_t* next;
} burst_t;

typedef struct queue_t {
    burst_t* head;
    burst_t* tail;
    pthread_mutex_t lock;
} queue_t;

queue_t ready_queue;
int num_processors;
pthread_t processor_threads[MAX_NUM_PROCESSORS];

void* processor_function(void* arg) {
    int processor_id = *(int*) arg;

    while (1) {
        pthread_mutex_lock(&ready_queue.lock);

        if (ready_queue.head == NULL) {
            pthread_mutex_unlock(&ready_queue.lock);
            break;
        }

        burst_t* current_burst = ready_queue.head;
        ready_queue.head = ready_queue.head->next;

        pthread_mutex_unlock(&ready_queue.lock);

        printf("Processor %d executing burst %d (length %d) at time %d\n",
               processor_id, current_burst->pid, current_burst->burst_length, current_burst->arrival_time);

        current_burst->processor_id = processor_id;
        current_burst->remaining_time = current_burst->burst_length;

        int time_slice = 50; // for RR algorithm
        int quantum = time_slice;

        while (current_burst->remaining_time > 0) {
            if (current_burst->remaining_time <= quantum) {
                usleep(current_burst->remaining_time * 1000); // sleep in microseconds
                current_burst->remaining_time = 0;
            } else {
                usleep(quantum * 1000);
                current_burst->remaining_time -= quantum;
            }

            current_burst->arrival_time += time_slice;
        }

        current_burst->finish_time = current_burst->arrival_time;
        current_burst->turnaround_time = current_burst->finish_time - current_burst->arrival_time;

        printf("Processor %d finished burst %d at time %d\n",
               processor_id, current_burst->pid, current_burst->finish_time);

        free(current_burst);
    }

    pthread_exit(NULL);
}

void enqueue_burst(burst_t* burst) {
    pthread_mutex_lock(&ready_queue.lock);

    if (ready_queue.tail == NULL) {
        ready_queue.head = ready_queue.tail = burst;
    } else {
        ready_queue.tail->next = burst;
        ready_queue.tail = burst;
    }

    pthread_mutex_unlock(&ready_queue.lock);
}
void* processor_function(void* arg) {
    int processor_id = *(int*) arg;

    while (1) {
        pthread_mutex_lock(&ready_queue.lock);

        if (ready_queue.head == NULL) {
            pthread_mutex_unlock(&ready_queue.lock);
            break;
        }

        burst_t* current_burst = ready_queue.head;
        ready_queue.head = ready_queue.head->next;

        pthread_mutex_unlock(&ready_queue.lock);

        printf("Processor %d executing burst %d (length %d) at time %d\n",
               processor_id, current_burst->pid, current_burst->burst_length, current_burst->arrival_time);

        current_burst->processor_id = processor_id;
        current_burst->remaining_time = current_burst->burst_length;

        int time_slice = 50; // for RR algorithm
        int quantum = time_slice;

        while (current_burst->remaining_time > 0) {
            if (current_burst->remaining_time <= quantum) {
                usleep(current_burst->remaining_time * 1000); // sleep in microseconds
                current_burst->remaining_time = 0;
            } else {
                usleep(quantum * 1000);
                current_burst->remaining_time -= quantum;
            }

            current_burst->arrival_time += time_slice;
        }

        current_burst->finish_time = current_burst->arrival_time;
        current_burst->turnaround_time = current_burst->finish_time - current_burst->arrival_time;

        printf("Processor %d finished burst %d at time %d\n",
               processor_id, current_burst->pid, current_burst->finish_time);

        free(current_burst);
    }

    pthread_exit(NULL);
}

void enqueue_burst(burst_t* burst) {
    pthread_mutex_lock(&ready_queue.lock);

    if (ready_queue.tail == NULL) {
        ready_queue.head = ready_queue.tail = burst;
    } else {
        ready_queue.tail->next = burst;
        ready_queue.tail = burst;
    }

    pthread_mutex_unlock(&ready_queue.lock);
}

/* Function to add a burst to the queue */
void add_burst(int burst_length) {
    pthread_mutex_lock(&queue_lock);   // Acquire the lock

    // Check if the queue is full
    if (num_bursts == MAX_BURSTS) {
        printf("Queue is full, cannot add more bursts.\n");
        exit(1);
    }

    // Create a new burst item
    burst_t new_burst;
    new_burst.pid = ++curr_pid;
    new_burst.burst_length = burst_length;
    new_burst.arrival_time = time(NULL);
    new_burst.remaining_time = burst_length;
    new_burst.finish_time = 0;
    new_burst.turnaround_time = 0;
    new_burst.processor_id = 0;

    // Add the burst to the queue
    rear = (rear + 1) % MAX_BURSTS;
    burst_queue[rear] = new_burst;
    num_bursts++;

    pthread_mutex_unlock(&queue_lock); // Release the lock
}
/* Function to simulate a CPU burst */
void simulate_burst(burst_t *burst, int processor_id) {
    burst->processor_id = processor_id;
    burst->remaining_time = 0;
    burst->finish_time = time(NULL);
    burst->turnaround_time = burst->finish_time - burst->arrival_time;

    printf("Burst %d executed in processor %d. Turnaround time: %d\n", burst->pid, burst->processor_id, burst->turnaround_time);
}

/* Function for the processor thread */
void *processor_thread(void *arg) {
    int processor_id = *((int *)arg);
    while (1) {
        // Remove a burst from the queue
        burst_t burst = remove_burst();

        // Simulate the burst
        simulate_burst(&burst, processor_id);
    }
}

int main(int argc, char* argv[]) {
    char* input_file_name = argv[1];
    FILE* input_file = fopen(input_file_name, "r");
    char buffer[MAX_BUF_SIZE];
    int current_time = 0;
    int pid_counter = 1;

    // initialize the ready queue and processor threads
    ready_queue.head = ready_queue.tail = NULL;
    pthread_mutex_init(&ready_queue.lock, NULL);

    num_processors = atoi(argv[2]);
    for (int i = 0; i < num_processors; i++) {
        int* processor_id_ptr = malloc(sizeof(int));
        *processor_id_ptr = i + 1;
        pthread_create(&processor_threads[i], NULL, processor_function, processor_id_ptr);
    }

    // Process the bursts sequentially
while (fgets(line, sizeof(line), file)) {
    if (strncmp(line, "PL", 2) == 0) { // a new burst
        // Parse the burst length from the line
        int burst_length = atoi(line + 3);
        
        // Create a new burst item and fill in its fields
        BurstItem* burst = malloc(sizeof(BurstItem));
        burst->pid = ++last_pid;
        burst->burst_length = burst_length;
        burst->arrival_time = timestamp;
        burst->remaining_time = burst_length;
        burst->finish_time = 0;
        burst->turnaround_time = 0;
        burst->processor_id = 0;
        
        // Add the burst item to the tail of the queue
        pthread_mutex_lock(&queue_mutex);
        enqueue(&queue, burst);
        pthread_mutex_unlock(&queue_mutex);
    }
    else if (strncmp(line, "IAT", 3) == 0) { // an interarrival time
        // Parse the interarrival time from the line
        int interarrival_time = atoi(line + 4);
        
        // Sleep for the interarrival time
        usleep(interarrival_time * 1000);
        
        // Update the timestamp
        timestamp += interarrival_time;
    }
}

// Close the input file
fclose(file); }

