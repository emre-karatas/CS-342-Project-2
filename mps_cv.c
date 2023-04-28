#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <ctype.h>
#include <string.h>
#include <sys/time.h>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <limits.h>
#include <math.h>

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
    int waiting_time;
    int processor_id;
    struct burst_t* next;
} burst_t;

typedef struct queue_t {
    burst_t* head;
    burst_t* tail;
    pthread_mutex_t lock;
    int size;
} queue_t;


typedef struct finish_list_t {
    burst_t* head;
    burst_t* tail;
    pthread_mutex_t lock;
    int size;
} finish_list_t;



struct thread_args {
    char* algorithm;
    int q;
    int thread_id;
    int out;
    queue_t* queue;
    finish_list_t* finish_list;
};

struct timeval start, burstFinishTime;


void finish_list_init(finish_list_t* list) {
    list->head = NULL;
    list->tail = NULL;
    pthread_mutex_init(&list->lock, NULL);
    list->size = 0;
}

int timeval_diff_ms(struct timeval* t1, struct timeval* t2) {
    int diff_sec = t2->tv_sec - t1->tv_sec;
    int diff_usec = t2->tv_usec - t1->tv_usec;
    int diff_us = diff_sec * 1000 + diff_usec / 1000;
    return diff_us;
}

int generateRandomInt(int min, int max, int mean)
{
    double lambda = 1.0 / mean;
    double u, x;
    int a;

    do {
        // Generate exponential random value x
        u = (double)rand() / RAND_MAX;
        x = -log(1 - u) / lambda;

        // Round x to an integer
        a = (int)round(x);

    } while (a < min || a > max);

    return a;
}

void push_to_finish_list(finish_list_t* finish_list, burst_t* burst) {
    int lock_result;
    do {
        lock_result = pthread_mutex_trylock(&finish_list->lock);
        if (lock_result == EBUSY) {
            printf("WAIT LOCK");
            // The lock is currently held by another thread, so sleep for a short time and try again.
            usleep(1000);
        } else if (lock_result != 0) {
            // An error occurred while trying to obtain the lock, handle it as appropriate
            return;
        }
    } while (lock_result == EBUSY);

    // Add the burst to the end of the finish list
    if (finish_list->head == NULL) {
        finish_list->head = burst;
        finish_list->tail = burst;
        burst->next = NULL;
    } else {
        finish_list->tail->next = burst;
        finish_list->tail = burst;
        burst->next = NULL;
    }


    pthread_mutex_unlock(&finish_list->lock);
}

void queue_init(queue_t* queue){
    queue-> head = NULL;
    queue-> tail = NULL;
    pthread_mutex_init(&queue->lock, NULL);
}

void multi_queues_init(queue_t** multi_queues, int num_processors) {
    // Allocate memory for the queue array
    *multi_queues = malloc(num_processors * sizeof(queue_t));
    if (*multi_queues == NULL) {
        fprintf(stderr, "Error: memory allocation failed\n");
        exit(EXIT_FAILURE);
    }

    // Initialize each queue
    for (int i = 0; i < num_processors; i++) {
        (*multi_queues)[i].head = NULL;
        (*multi_queues)[i].tail = NULL;
        (*multi_queues)[i].size = 0;
        pthread_mutex_init(&((*multi_queues)[i].lock), NULL);
    }
}





burst_t* find_shortest(queue_t* ready_queue) 
{
    burst_t* current_element = ready_queue->head;
    burst_t* min_element = current_element;

    while (current_element != NULL && current_element->remaining_time >= 0) 
    {
        if (current_element->remaining_time < min_element->remaining_time ) 
        {
            min_element = current_element;
        }

        current_element = current_element->next;
    }

    return min_element;
}




burst_t* get_burst_by_pid(burst_t* head, int pid) 
{
    burst_t* curr = head;
    while (curr != NULL) 
    {
        if (curr->pid == pid) 
        {
            return curr;
        }
        curr = curr->next;
    }
    return NULL;
}

void remove_burst_from_queue(queue_t* queue, burst_t* burst) {

    // check if the burst is the head of the queue
    if (queue->head == burst) {
        queue->head = burst->next;
        if (queue->tail == burst) {
            queue->tail = NULL;
        }
        queue->size--;
        return;
    }

    // find the burst in the queue
    burst_t* curr = queue->head;
    burst_t* prev = NULL;
    while (curr != NULL && curr != burst) {
        prev = curr;
        curr = curr->next;
    }

    // check if the burst was found
    if (curr != NULL) {
        prev->next = curr->next;
        if (queue->tail == burst) {
            queue->tail = prev;
        }
        queue->size--;
    }

}





void enqueue_burst_rr(burst_t* burst, queue_t* queue) {
    int lock_result;
    do {
        lock_result = pthread_mutex_trylock(&queue->lock);
        if (lock_result == EBUSY) {
            printf("WAITING FOR THE LOCK");
            // The lock is currently held by another thread, so sleep for a short time and try again.
            usleep(1000);
        } else if (lock_result != 0) {
            // An error occurred while trying to obtain the lock, handle it as appropriate
            return;
        }
    } while (lock_result == EBUSY);

    if (queue->tail == NULL) {
        queue->head = queue->tail = burst;
    } else {
        // Add the new burst before the dummy burst
        if (queue->tail->pid == -1) {
            // The dummy burst is the last burst in the list
            burst->next = queue->tail;
            queue->head = burst;
        } else {
            burst->next = queue->tail;
            queue->tail = burst;
        }
    }

    //num_bursts_inqueue++;
    queue->size++;

    pthread_mutex_unlock(&queue->lock);
}


int curr_pid =0;
int last_pid=0;
int num_bursts_inqueue;
time_t timestamp;
int end_of_simulation=0;
pthread_mutex_t end_of_simulation_lock = PTHREAD_MUTEX_INITIALIZER;

// Function to set end_of_simulation to 1
void set_end_of_simulation() {
    pthread_mutex_lock(&end_of_simulation_lock);
    end_of_simulation = 1;
    pthread_mutex_unlock(&end_of_simulation_lock);
}

// Function to check if end_of_simulation is set to 1
int is_end_of_simulation() {
    int result;
    pthread_mutex_lock(&end_of_simulation_lock);
    result = end_of_simulation;
    pthread_mutex_unlock(&end_of_simulation_lock);
    return result;
}

//int num_bursts_inqueue = 0;
void sortFinishListByPid(finish_list_t* root)
{
    if (root == NULL || root->head == NULL)
    {
        return;
    }

    burst_t* current;
    burst_t* next;
    burst_t temp;

    for (current = root->head; current != NULL; current = current->next)
    {
        for (next = current->next; next != NULL; next = next->next)
        {
            if (current->pid > next->pid)
            {
                temp = *current;
                *current = *next;
                *next = temp;

                // Fix the 'next' pointers after swapping
                temp.next = current->next;
                current->next = next->next;
                next->next = temp.next;
            }
        }
    }
}


void displayFinishList(finish_list_t* root) 
{
    if (root == NULL || root->head == NULL) 
    {
        printf("Queue is empty.\n");
    }
    else 
    {
    	sortFinishListByPid(root);
        burst_t* current = root->head;
        int turnAroundSum = 0;
        int processCount = 0;

        while (current != NULL) 
        {
            turnAroundSum += current->finish_time - current->arrival_time;
            processCount++;
            printf("%-10d %-10d %-10d %-10d %-10d %-12d %-10d\n", current->pid, current->processor_id, current->burst_length, current->arrival_time, current->finish_time, current->waiting_time, current->finish_time - current->arrival_time);
            current = current->next;
   
        }
        printf("average turnaround time: %d ms\n", turnAroundSum / processCount);
    }
}

void displayFinishListToFile(finish_list_t* root, FILE* out) 
{
    if (root == NULL || root->head == NULL) 
    {
        fprintf(out,"Queue is empty.\n");
    }
    else 
    {
    	sortFinishListByPid(root);
        burst_t* current = root->head;
        int turnAroundSum = 0;
        int processCount = 0;
        fprintf(out,"%-10s %-10s %-10s %-10s %-10s %-12s %-10s\n", "pid", "cpu", "bustlen", "arv", "finish", "waitingtime", "turnaround");

        while (current != NULL) 
        {
            turnAroundSum += current->finish_time - current->arrival_time;
            processCount++;
            fprintf(out,"%-10d %-10d %-10d %-10d %-10d %-12d %-10d\n", current->pid, current->processor_id, current->burst_length, current->arrival_time, current->finish_time, current->waiting_time, current->finish_time - current->arrival_time);
            current = current->next;
   
        }
        fprintf(out,"average turnaround time: %d ms\n", turnAroundSum / processCount);
    }
}




void remove_burst(queue_t* queue, burst_t* burst_to_remove) {
    burst_t* curr = queue->head;
    burst_t* prev = NULL;

    while (curr != NULL) {
        if (curr == burst_to_remove) {
            if (prev == NULL) {
                queue->head = curr->next;
            } else {
                prev->next = curr->next;
            }
            if (curr == queue->tail) {
                queue->tail = prev;
            }
            queue->size--;
            break;
        }
        prev = curr;
        curr = curr->next;
    }
}


burst_t* pick_from_queue(queue_t* queue, char* algorithm) {
    burst_t* selected_burst = NULL;
    while (selected_burst == NULL) { // loop until a process is picked
        pthread_mutex_lock(&queue->lock); // lock the queue

        if (queue->size > 0) {
            // pick a process from the queue based on the scheduling algorithm
            if (strcmp(algorithm, "FCFS") == 0) {
                selected_burst = queue->head;
                //queue->head = selected_burst->next;
                //if (queue->head == NULL) {
                  //  queue->tail = NULL;
                //}
                if(selected_burst->pid !=-1){
                    remove_burst_from_queue(queue, selected_burst);

                }
                // decrement the size of the queue
                //queue->size--;
            } else if (strcmp(algorithm, "SJF") == 0) {
                // if there is a tie, the item closer to head is picked
                selected_burst = find_shortest(queue);
                if(selected_burst->pid !=-1){
                    remove_burst_from_queue(queue, selected_burst);

                }
            } else if (strcmp(algorithm, "RR") == 0) {
                selected_burst = queue->head;
                //queue->head = selected_burst->next;
                //if (queue->head == NULL) {
                  //  queue->tail = NULL;
                //}
                if(selected_burst->pid !=-1){
                    remove_burst_from_queue(queue, selected_burst);

                }
                // decrement the size of the queue
                //queue->size--;
            }
            
        }
        

        pthread_mutex_unlock(&queue->lock); // unlock the queue

        // if no process is picked, sleep for 1 ms and try again
        if (selected_burst == NULL) {
            usleep(1);
        }
    }
    return selected_burst;
}



void enqueue_burst(burst_t* burst, queue_t* queue) {
    int lock_result;
    do {
        lock_result = pthread_mutex_trylock(&queue->lock);
        if (lock_result == EBUSY) {
            printf("WAITING FOR THE LOCK");
            // The lock is currently held by another thread, so sleep for a short time and try again.
            usleep(1000);
        } else if (lock_result != 0) {
            // An error occurred while trying to obtain the lock, handle it as appropriate
            return;
        }
    } while (lock_result == EBUSY);

    if (queue->tail == NULL) {
        queue->head = queue->tail = burst;
    } else {
        queue->tail->next = burst;
        queue->tail = burst;
    }
    num_bursts_inqueue++;
    queue->size++;

    pthread_mutex_unlock(&queue->lock);
}

void* processor_function(void* arg) {
    struct thread_args* args = (struct thread_args*) arg;
    char* algorithm = args->algorithm;
    int q = args->q;
    int thread_id = args->thread_id;
    finish_list_t* finish_list = args->finish_list;
    queue_t* queue = args->queue;
    int sleep_time;
    burst_t* current_burst;

    while (1) {
        
        current_burst = pick_from_queue(queue, algorithm);
        //pthread_mutex_unlock(&queue->lock);

        if (current_burst->pid == -1 ) {
            // This is a dummy burst indicating end of simulation
            break;
        }

        
        // Simulate the running of the process by sleeping for a while
        sleep_time = current_burst->remaining_time;
        if (strcmp(algorithm, "RR") == 0 && sleep_time > q) 
        {
            // For RR, if remaining time is greater than quantum, set remaining time to quantum
            sleep_time = q;
            
        }
        printf("%d", sleep_time);
        usleep(sleep_time * 1000);

       
        current_burst->remaining_time -= sleep_time;
       
        struct timeval cur_time;
        gettimeofday(&cur_time, NULL);
        if (args->out == 2) {
                
                printf("time=%d, cpu=%d, pid=%d, burstlen=%d, remainingtime=%d\n", timeval_diff_ms(&start, &cur_time), current_burst->processor_id, current_burst->pid, current_burst->burst_length, current_burst->remaining_time);
                
         }
        if (current_burst->remaining_time <= 0) {
            // Burst is finished
           
            struct timeval burstFinishTime;
            gettimeofday(&burstFinishTime, NULL);
            current_burst->finish_time = timeval_diff_ms(&start, &burstFinishTime);
            current_burst->turnaround_time = current_burst->finish_time - current_burst->arrival_time;
            current_burst->processor_id = thread_id;
            current_burst->waiting_time = current_burst->turnaround_time - current_burst->burst_length;
            push_to_finish_list(finish_list, current_burst);
        } else 
        {
        	
      
            enqueue_burst_rr(current_burst,queue);
            		
        	
        }
        
    }

    // Thread has finished its work
    pthread_exit(NULL);
}






void enqueue_burst_multi(burst_t* burst, queue_t** queue_array, int num_processors, int method) {
    if (method == 1) {  // round-robin method
        int queue_index = burst->pid % num_processors;  // select queue based on PID
        enqueue_burst_rr(burst, &(*queue_array)[queue_index]);
        printf("\n *** \n burst %d is enqueued in list %d \n **** \n", burst->pid, queue_index);
    } else {  // load-balancing method
        int smallest_queue_index = 0;
        int smallest_queue_size = queue_array[0]->size;
        for (int i = 1; i < num_processors; i++) {  // find queue with smallest size
            if (queue_array[i]->size < smallest_queue_size) {
                smallest_queue_index = i;
                smallest_queue_size = queue_array[i]->size;
            }
        }
        enqueue_burst_rr(burst, &(*queue_array)[smallest_queue_index]);
    }
}




int main(int argc, char* argv[])
{
	// Set default values
	int processor_number = 2;
	char* sch_approach = "M";
	char* queue_sel_method = "RM";
	char* algorithm = "RR";
	int quantum_number = 20;
	char* infile_name = "in.txt";
	int out_mode = 1;
	char* outfile_name = "nothing";
	int method=0;

	
	// Burst will be generated random if random > 0, read file if random == 0
    	int random = 0;

    	// Random variables
    	int iat_mean = 200;
    	int iat_min = 10;
    	int iat_max = 1000;

    	int burst_mean = 100;
    	int burst_min = 10;
    	int burst_max = 500;

    	int pc = 10;
    	
	// Parse command line arguments
	for (int i = 0; i < argc; i++) 
	{
    		if (strcmp(argv[i], "-n") == 0) 
    		{
        		processor_number = atoi(argv[i + 1]);
    		}
    		else if (strcmp(argv[i], "-a") == 0) 
    		{
        		sch_approach = argv[i + 1];
        		queue_sel_method = argv[i + 2];
    		}	
    		else if (strcmp(argv[i], "-s") == 0) 
    		{
        		algorithm = argv[i + 1];
        		quantum_number = atoi(argv[i + 2]);
    		}
    		else if (strcmp(argv[i], "-i") == 0) 
    		{
    		    infile_name = argv[i + 1];
    		}
    		else if (strcmp(argv[i], "-m") == 0) 
    		{
    		    out_mode = atoi(argv[i + 1]);
    		}
    		else if (strcmp(argv[i], "-o") == 0) 
    		{
    		    outfile_name = argv[i + 1];
    		}
    		else if (strcmp(argv[i], "-r") == 0) 
    		{
    		    random++;
            	    iat_mean = atoi(argv[i + 1]);
            	    iat_min = atoi(argv[i + 2]);
            	    iat_max = atoi(argv[i + 3]);

                    burst_mean = atoi(argv[i + 4]);
                    burst_min = atoi(argv[i + 5]);
                    burst_max = atoi(argv[i + 6]);

                    pc = atoi(argv[i + 7]);
    		}
	} 
	
	int original_stdout;

    	// Store the original stdout file descriptor
    	original_stdout = dup(fileno(stdout));
	
	//to print nothing to the screen
    	if(out_mode == 1)
    	{
    		 freopen("/dev/null", "w", stdout);
    	}

	if(out_mode == 3)
	{
	// Output parsed arguments (for testing purposes)
	printf("Num of processors = %d\n", processor_number);
	printf("Sched approach = %s\n", sch_approach);
	printf("Queue selection method = %s\n", queue_sel_method);
	printf("Algorithm = %s\n", algorithm);
	printf("quantum = %d\n", quantum_number);
	printf("infile name = %s\n", infile_name);
	printf("out mode = %d\n", out_mode);
	printf("outfile name = %s\n", outfile_name);
	}
	
    
    if(strcmp(queue_sel_method, "RM") == 0){
        method = 1;
    }
    else if(strcmp(queue_sel_method, "LM") == 0){
        method = 2;
    }
    fflush(stdout);

    queue_t* ready_queue= malloc(sizeof(queue_t));
    queue_t* ready_queues[processor_number];
    finish_list_t* finish_list;

    finish_list =(finish_list_t*)malloc(sizeof(finish_list_t));
    finish_list_init(finish_list);
    fflush(stdout);

	// Initialize ready queues
    if(strcmp(sch_approach, "S")==0){
        queue_init(ready_queue);
    }
    else{
        //use *multi_queues to access the array, and 
        //(*multi_queues)[i] to access the i-th queue in the array.
        multi_queues_init(ready_queues, processor_number);
    }
	if(out_mode == 3)
	{
		printf("checkpoint 1- initialized ready queues\n");
	}

	// Create processor threads
    pthread_t threads[processor_number];
   
    if(strcmp(sch_approach, "S")==0){
        for (int i = 0; i < processor_number; i++) {
            struct thread_args* args = (struct thread_args*) malloc(sizeof(struct thread_args));
            args->algorithm = algorithm;
            args->q = quantum_number;
            args->finish_list=finish_list;
            args->thread_id = i+1;
            args->out = out_mode;
            args->queue = ready_queue; // Add this line to pass the ready queue
            pthread_create(&threads[i], NULL, processor_function, (void*) args);
        }

    }
    else{
        pthread_t threads[processor_number];
        for (int i = 0; i < processor_number; i++) {
            struct thread_args* args = (struct thread_args*) malloc(sizeof(struct thread_args));
            args->algorithm = algorithm;
            args->q = quantum_number;
            args->finish_list=finish_list;
            args->thread_id = i+1;
            args->out = out_mode;
            args->queue = &(*ready_queues)[i]; // Add this line to pass the ready queue
            pthread_create(&threads[i], NULL, processor_function, (void*) args);
        }

    }
    

	if (random == 0)
	{
	// Open input file and initialize variables
	FILE* input_file = fopen(infile_name, "r");
	char line[MAX_BUF_SIZE];
	int pid_counter = 1;
	if(out_mode == 3)
	{
		printf("checkpoint 3- starting processing bursts \n");
	}
	int first = 1;
	// Process the bursts sequentially
	while (fgets(line, sizeof(line), input_file)) 
	{
	 if (first) 
	 {
                // Get and record start time
                
                gettimeofday(&start, NULL);
                first = 0;
        }
        //struct timeval arrival;
        //gettimeofday(&arrival, NULL);
        if (strncmp(line, "PL", 2) == 0) 
        { 
            if(out_mode == 3)
            {
            	printf("checkpoint 4- PL Reading \n");
            }
            // a new burst
            // Parse the burst length from the line
            int burst_length = atoi(line + 3);
    	    if(out_mode == 3)
    	    {
            	printf("Burst length: %d \n",burst_length);
            }
            fflush(stdout);
            
            // Create a new burst item and fill in its fields
            burst_t* burst = malloc(sizeof(burst_t));
            burst->pid = ++last_pid;
            if(out_mode == 3)
            {
            	printf("burst id: %d \n",burst-> pid);
            }
            burst->burst_length = burst_length;
            struct timeval arrival;
            gettimeofday(&arrival, NULL);
            burst->arrival_time = timeval_diff_ms(&start,&arrival);
            if(out_mode == 3)
            {
            	printf("arrival time: %d \n",burst->arrival_time);
            }

            burst->remaining_time = burst_length;
            burst->finish_time = 0;
            burst->turnaround_time = 0;
            burst->processor_id = 0;
            burst->waiting_time = 0;
    

            if (strcmp(sch_approach, "S") == 0) 
            {
                enqueue_burst(burst, ready_queue);
            }
            else{
                enqueue_burst_multi(burst, ready_queues, processor_number,method);
            }
        		if(out_mode == 3)
        		{
				printf("checkpoint 8- burst is enqueued \n");
			}
		}
		else if (strncmp(line, "IAT", 3) == 0) 
        { 
            // an interarrival time
            // Parse the interarrival time from the line
            int interarrival_time = atoi(line + 4);
            if(out_mode == 3)
            {
            	printf("IAT: %d\n",interarrival_time);
            	fflush(stdout);
            	printf("checkpoint 10 - inside IAT before sleeping \n");
            }
    
            // Sleep for the interarrival time
            usleep(interarrival_time*1000);
        }
	} 
	
	
	// Close the input file
    fclose(input_file); 
    }
    else
    {
    	int cur_id = 0;
        int count = 0;
        gettimeofday(&start, NULL);
         while (count < pc) 
         {
            burst_t* burst = malloc(sizeof(burst_t));
            burst->pid = ++last_pid;
            
            burst->burst_length = generateRandomInt(burst_min, burst_max, burst_mean);
            struct timeval arrival;
            gettimeofday(&arrival, NULL);
            burst->arrival_time = timeval_diff_ms(&start,&arrival);
           

            burst->remaining_time = burst->burst_length;
            burst->finish_time = 0;
            burst->turnaround_time = 0;
            burst->processor_id = 0;
            burst->waiting_time = 0;
    

            if (strcmp(sch_approach, "S") == 0) 
            {
                enqueue_burst(burst, ready_queue);
            }
            else{
                enqueue_burst_multi(burst, ready_queues, processor_number,method);
            }
            int iat = generateRandomInt(iat_min, iat_max, iat_mean);
        
            cur_id++;

            usleep(iat * 1000);
            count++;
         
         }

     }
    // Add dummy bursts to each queue
    if(strcmp(sch_approach, "S")==0){
        burst_t* dummy_burst = malloc(sizeof(burst_t));
        dummy_burst->pid = -1;
        dummy_burst->arrival_time=-1;
        dummy_burst->burst_length=-1;
        dummy_burst->finish_time=-1;
        dummy_burst->remaining_time=-1;
        dummy_burst->turnaround_time=-1;
        enqueue_burst(dummy_burst, ready_queue);
    }
    else{
       burst_t* dummy_burst = malloc(sizeof(burst_t));
        dummy_burst->pid = -1;
        dummy_burst->arrival_time=-1;
        dummy_burst->burst_length=-1;
        dummy_burst->finish_time=-1;
        dummy_burst->remaining_time=-1;
        dummy_burst->turnaround_time=-1;

        for (int i = 0; i < processor_number; i++) {
            enqueue_burst(dummy_burst, &(*ready_queues)[i]);
            if(out_mode == 3)
            {
            	printf("\n *** \n burst %d is enqueued in list %d \n **** \n", dummy_burst->pid, i);
            }

        }
    }
	if(out_mode == 3)
	{
		printf("checkpoint 14 - dummy bursts are added to the end of queues \n");
	}
	// Wait for processor threads to terminate
	for (int i = 0; i < processor_number; i++) 
	{
    		pthread_join(threads[i], NULL);
    		if (out_mode == 3)
    		{
    			printf("checkpoint 15 - thread %d is joining\n",i);
    		}
	}
    
    if (strcmp(outfile_name, "out.txt") == 0)
    {
    	// Write the result to the specified output file
    	FILE *fp = fopen(outfile_name, "w");
    	if (fp == NULL) {
        	fprintf(stderr, "Error: Unable to open output file %s\n", outfile_name);
       		exit(EXIT_FAILURE);
    	}
    	displayFinishListToFile(finish_list,fp);
    	fclose(fp);
    }
    else
    {
    	// Restore the original stdout behavior
    	fflush(stdout);
    	dup2(original_stdout, fileno(stdout));
    	close(original_stdout);
    	printf("%-10s %-10s %-10s %-10s %-10s %-12s %-10s\n", "pid", "cpu", "bustlen", "arv", "finish", "waitingtime", "turnaround");
    	displayFinishList(finish_list);
    }
    
}
