#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <ctype.h>
#include <string.h>
#include <sys/time.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>

#define MAX_BUF_SIZE 256
#define MAX_NUM_PROCESSORS 10
#define MIN_BURST_LENGTH 10
#define MIN_INTERARRIVAL_TIME 10
#define MIN_QUANTUM 10
#define MAX_QUANTUM 100
#define MAX_BURSTS 1000

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
    int size;
} queue_t;

queue_t ready_queue;

void init_ready_queue(){
    pthread_mutex_init(&ready_queue.lock,NULL);
    ready_queue.head = ready_queue.tail = NULL;
    ready_queue.size = 0;
}


int num_processors;
int num_bursts=0;
int curr_pid =0;
int last_pid=0;
pthread_t processor_threads[MAX_NUM_PROCESSORS];
int timestamp;
struct timeval start_time, current_time;

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
    num_bursts++;
    pthread_mutex_unlock(&ready_queue.lock);
}



void add_burst(int burst_length) {
    pthread_mutex_lock(&ready_queue.lock);   // Acquire the lock

    // Check if the queue is full
    if (num_bursts == MAX_BURSTS) {
        printf("Queue is full, cannot add more bursts.\n");
        pthread_mutex_unlock(&ready_queue.lock);  // Release the lock before exiting
        exit(1);
    }

    // Allocate memory for a new burst item
    burst_t* new_burst = (burst_t*) malloc(sizeof(burst_t));
    if (new_burst == NULL) {
        printf("Failed to allocate memory for a new burst item.\n");
        pthread_mutex_unlock(&ready_queue.lock);  // Release the lock before exiting
        exit(1);
    }

    // Initialize the new burst item
    new_burst->pid = ++curr_pid;
    new_burst->burst_length = burst_length;
    new_burst->arrival_time = time(NULL);
    new_burst->remaining_time = 0;
    new_burst->finish_time = 0;
    new_burst->turnaround_time = 0;
    new_burst->processor_id = -1;
    new_burst->next = NULL;

    // Enqueue the new burst item
    enqueue_burst(new_burst);
    num_bursts++;

    pthread_mutex_unlock(&ready_queue.lock);  // Release the lock
}

/* Function to simulate a CPU burst */
void simulate_burst(burst_t *burst, int processor_id) {
    burst->processor_id = processor_id;
    burst->remaining_time = 0;
    burst->finish_time = time(NULL);
    burst->turnaround_time = burst->finish_time - burst->arrival_time;

    printf("Burst %d executed in processor %d. Turnaround time: %d\n", burst->pid, burst->processor_id, burst->turnaround_time);
}

burst_t* dequeue_process() {
    if (ready_queue.head == NULL) {
        return NULL;
    } else {
        burst_t* temp = ready_queue.head;
        ready_queue.head = ready_queue.head->next;
        ready_queue.size--;
        return temp;
    }
}

/* Function for the processor thread */
void *processor_thread(void *arg) {
    int processor_id = *((int *)arg);
    while (1) {
        // Remove a burst from the queue
        burst_t* burst = dequeue_process();

        // Simulate the burst
        simulate_burst(burst, processor_id);
    }
}

int main(int argc, char* argv[]) {
    // Get and set args
    gettimeofday(&start_time, NULL);
    int proccessor_number = 2;
    char* sch_approach = "M";
    char* queue_sel_method = "RM";
    char* algorithm = "RR";
    int quantum_number = 20; // default value
    char* infile_name = "in.txt"; //bunu silebiliriz de default value olmak zorunda degilmis korpenin yazdigina gore
    int out_mode = 1;
    char* outfile_name = "out.txt";

    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "-n") == 0) {
            proccessor_number = atoi(argv[i + 1]);
        }
        else if (strcmp(argv[i], "-a") == 0) {
            sch_approach = argv[i + 1];
            queue_sel_method = argv[i + 2];
        }
        else if (strcmp(argv[i], "-s") == 0) {
            algorithm = argv[i + 1];
            quantum_number = atoi(argv[i + 2]);
        }
        else if (strcmp(argv[i], "-i") == 0) {
            infile_name = argv[i + 1];
        }
        else if (strcmp(argv[i], "-m") == 0) {
            out_mode = atoi(argv[i + 1]);
        }
        else if (strcmp(argv[i], "-o") == 0) {
            outfile_name = argv[i + 1];
        }
        else if (strcmp(argv[i], "-r") == 0) {
            // anlamadim tam bunu 
        }
    } 
    //outputting data(testing icin)
    printf("Num of processors = %d\n", proccessor_number);
    printf("Sched approach = %s\n", sch_approach);
    printf("Queue selection method = %s\n", queue_sel_method);
    printf("Algorithm = %s\n", algorithm);
    printf("quantum = %d\n", quantum_number);
    printf("infile name = %s\n", infile_name);
    printf("out mode = %d\n", out_mode);
    printf("outfile name = %s\n", outfile_name);
    fflush(stdout);
   
    FILE* input_file = fopen(infile_name, "r");
    if (input_file == NULL) {
        perror("Error opening input file");
        exit(EXIT_FAILURE);
    }  

    FILE* output_file = fopen(outfile_name,"w");
    if (output_file == NULL) {
        perror("Error opening output file");
        exit(EXIT_FAILURE);
    }
    char buffer[MAX_BUF_SIZE];
    int pid_counter = 1;
   


    // initialize the ready queue and processor threads
    init_ready_queue();
    //ready_queue.head = ready_queue.tail = NULL;
    //pthread_mutex_init(&ready_queue.lock, NULL);

    // create N processor threads
    for (int i = 0; i < proccessor_number; i++) {
        int* processor_id_ptr = malloc(sizeof(int));
        *processor_id_ptr = i + 1;
        printf("pid is %d \n", *processor_id_ptr);
        //printf("outfile name = %s\n", outfile_name);
        pthread_create(&processor_threads[i], NULL, processor_function, processor_id_ptr);
    }
    char line[80];
    // Process the bursts sequentially
    while (fgets(line, sizeof(line), input_file)) {
        if (strncmp(line, "PL", 2) == 0) { // a new burst
            // Parse the burst length from the line
            int burst_length = atoi(line + 3);
            printf("BL %d \n",burst_length);
            fflush(stdout);
            // Create a new burst item and fill in its fields
            burst_t* burst = malloc(sizeof(burst_t));
            burst->pid = ++last_pid;
            printf("burst id %d \n",burst-> pid);
            burst->burst_length = burst_length;
            gettimeofday(&current_time,NULL);
            timestamp += (current_time.tv_sec - start_time.tv_sec)*1000 + (current_time.tv_usec- start_time.tv_usec)/1000;
            burst->arrival_time = timestamp;
            printf("arrival time: %d \n",burst->arrival_time);

            burst->remaining_time = burst_length;
            burst->finish_time = 0;
            burst->turnaround_time = 0;
            burst->processor_id = 0;
            
            // Add the burst item to the tail of the queue
           
            enqueue_burst(burst);
           

        }
        else if (strncmp(line, "IAT", 3) == 0) { // an interarrival time
            // Parse the interarrival time from the line
            int interarrival_time = atoi(line + 4);
            printf("IAT: %d\n",interarrival_time);
            fflush(stdout);
            // Sleep for the interarrival time
            usleep(interarrival_time);
            
            // Update the timestamp
            
            timestamp += interarrival_time;
        }
    }
    // add a dummy item to the end of the queue
    burst_t* end_marker = malloc(sizeof(burst_t));
    end_marker-> pid = -1;
    end_marker-> burst_length = 0;
    end_marker-> arrival_time = 0;
    end_marker-> remaining_time =0;
    end_marker-> finish_time = 0;
    end_marker-> turnaround_time =0;
    end_marker->processor_id =0;
    enqueue_burst(end_marker);

    // wait for all processor threads to terminate
    //for(int i =0; i <proccessor_number; i++){
      //  pthread_join(&processor_threads[i], NULL);
    //}

    // Close the input file
    fclose(input_file); 
}

