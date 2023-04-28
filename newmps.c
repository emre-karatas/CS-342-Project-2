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
    queue_t* queue;
    finish_list_t* finish_list;
};

struct timeval start_time, current_time;


void finish_list_init(finish_list_t* list) {
    list->head = NULL;
    list->tail = NULL;
    pthread_mutex_init(&list->lock, NULL);
    list->size = 0;
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
    printf("\n FINISH LIST %d\n", burst->pid);


    pthread_mutex_unlock(&finish_list->lock);
    printf("unlocked");
}

void queue_init(queue_t* queue){
    queue-> head = NULL;
    queue-> tail = NULL;
    pthread_mutex_init(&queue->lock, NULL);
}

void multi_queues_init(queue_t** multi_queues, int num_processors){
    // Allocate memory for the array of queues
    *multi_queues = malloc(sizeof(queue_t) * num_processors);

    // Initialize each queue in the array
    for(int i=0; i<num_processors; i++){
        queue_init(&((*multi_queues)[i]));
    }
}

time_t get_current_time() 
{
    time_t current_time;
    time(&current_time);
    return current_time;
}



burst_t* find_shortest(queue_t* ready_queue) 
{
    burst_t* min_element = NULL;
    burst_t* current_element = ready_queue->head;

    while (current_element != NULL) 
    {
        if (min_element == NULL || current_element->burst_length < min_element->burst_length) 
        {
            min_element = current_element;
        }

        current_element = current_element->next;
    }

    return min_element;
}

int get_load_balancing_index(int proc_count,queue_t** ready_queues) 
{
    int min;
    int minIndex = 0;
    for (int i = 0; i < proc_count; i++) 
    {
        pthread_mutex_lock(&ready_queues[i]->lock);
        queue_t* cur = ready_queues[i];
        int sum = 0;
        while (cur != NULL) 
        {
            sum += cur->head->remaining_time;
            cur->head = cur->head->next;
        }
        if (i == 0) 
        {
            min = sum;
        }
        if (sum < min) 
        {
            min = sum;
            minIndex = i;
        }
        pthread_mutex_unlock(&ready_queues[i]->lock);
    }
    return minIndex;
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
   // pthread_mutex_lock(&queue->lock); // lock the queue

    // check if the burst is the head of the queue
    if (queue->head == burst) {
        queue->head = burst->next;
    } else {
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
        }
    }

    // check if the burst is the tail of the queue
    if (queue->tail == burst) {
        queue->tail = burst->next;
    }

    queue->size--; // decrement the size of the queue
 //   pthread_mutex_unlock(&queue->lock); // unlock the queue
}


int get_min_queue_index(int num_queues, queue_t** ready_queues) 
{
    int min_size = MIN_BURST_LENGTH;
    int min_index = 0;
    for (int i = 0; i < num_queues; i++) 
    {
        if (ready_queues[i]->size < min_size) 
        {
            min_size = ready_queues[i]->size;
            min_index = i;
        }
    }
    return min_index;
}

int get_round_robin_index(int num_queues) 
{
    static int current_index = 0;
    current_index = (current_index + 1) % num_queues;
    return current_index;
}

int get_shortest_job_first_index(int burst_length, int num_queues,queue_t** ready_queues) 
{
    int min_burst_length = MAX_BURSTS;
    int min_index = 0;
    for (int i = 0; i < num_queues; i++) 
    {
        if (ready_queues[i]->size == 0)
        {
            continue;
        }
        burst_t* head_burst = ready_queues[i]->head;
        if (head_burst->burst_length < min_burst_length) 
        {
            min_burst_length = head_burst->burst_length;
            min_index = i;
        }
    }
    return min_index;
}

int select_queue_index(char* queue_sel_method, int num_queues, int burst_length, queue_t** ready_queues)  
{
    int index = 0;
    if (strcmp(queue_sel_method, "RM") == 0) 
    {
        index = get_min_queue_index(num_queues, ready_queues );
    } 
    else if (strcmp(queue_sel_method, "LM") == 0) 
    {
        index = get_load_balancing_index(num_queues, ready_queues); //get the queue with the least load in load-balancing approach
    }
    // for NA, index is 0
    
    return index;
}


int curr_pid =0;
int last_pid=0;
int num_bursts_inqueue;
time_t timestamp;
int end_of_simulation=0;

void displayFinishList(finish_list_t* root) 
{
    if (root == NULL || root->head == NULL) 
    {
        printf("Queue is empty.\n");
    }
    else 
    {
        burst_t* current = root->head;
        int turnAroundSum = 0;
        int processCount = 0;

        while (current != NULL) 
        {
            turnAroundSum += current->finish_time - current->arrival_time;
            processCount++;
            printf("%-10d %-10d %-10d %-10d %-10d %-12d %-10d\n", current->pid, current->processor_id, current->burst_length, current->arrival_time, current->finish_time, current->finish_time - current->arrival_time - current->burst_length, current->finish_time - current->arrival_time);
            //printf("----------------\n");
            //printf("- id: %d\n", current->pid);
            //printf("- burst length: %d\n", current->burst_length);
            //printf("- arrival time: %d\n", current->arrival_time);
            //printf("- finish time: %d\n", current->finish_time);
            //printf("- waiting time: %d\n", current->finish_time - current->arrival_time - current->burst_length);
            //printf("- turnaround time: %d\n", current->finish_time - current->arrival_time);
            current = current->next;
   
        }
        //printf("----------------\n");
        //printf("\n");
        printf("average turnaround time: %d ms\n", turnAroundSum / processCount);
    }
}
void displayList(queue_t* root) 
{
    if (root == NULL || root->head == NULL) 
    {
        printf("Queue is empty.\n");
    }
    else 
    {
        burst_t* current = root->head;

        while (current != NULL) 
        {
            printf("----------------\n");
            printf("- id: %d\n", current->pid);
            printf("- burst length: %d\n", current->burst_length);
            printf("- arrival time: %d\n", current->arrival_time);
            printf("- finish time: %d\n", current->finish_time);
            printf("- waiting time: %d\n", current->finish_time - current->arrival_time - current->burst_length);
            printf("- turnaround time: %d\n", current->finish_time - current->arrival_time);
            current = current->next;
        }
        printf("----------------\n");
        printf("\n");
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
                queue->head = selected_burst->next;
                if (queue->head == NULL) {
                    queue->tail = NULL;
                }
                // decrement the size of the queue
                queue->size--;
            } else if (strcmp(algorithm, "SJF") == 0) {
                // if there is a tie, the item closer to head is picked
                selected_burst = find_shortest(queue);
                printf("\n SHORTES %d \n", selected_burst->pid);
                remove_burst_from_queue(queue, selected_burst);
            } else if (strcmp(algorithm, "RR") == 0) {
                selected_burst = queue->head;
                queue->head = selected_burst->next;
                if (queue->head == NULL) {
                    queue->tail = NULL;
                }
                // set the remaining time of the burst to the quantum number
                selected_burst->remaining_time = selected_burst->burst_length;
                // decrement the size of the queue
                queue->size--;
            }
            
        }

        pthread_mutex_unlock(&queue->lock); // unlock the queue

        // if no process is picked, sleep for 1 ms and try again
        if (selected_burst == NULL) {
            usleep(1);
        }
    }
    printf("\n XXXX     %d\n", selected_burst->pid);
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
    int remaining_time;
    burst_t* current_burst;

    while (end_of_simulation==0) {
        // Pick a process from the queue based on the scheduling algorithm
        //pthread_mutex_lock(&queue->lock);
        if (queue->size == 0) {
            // Sleep for 1 ms if the queue is empty
            //pthread_mutex_unlock(&queue->lock);
            usleep(1000);
            continue;
        }
        current_burst = pick_from_queue(queue, algorithm);
        printf("\n !!! BURST ID%d !!! \n", current_burst->pid);
        //pthread_mutex_unlock(&queue->lock);

        if (current_burst->pid == -1 && queue->size ==0) {
            // This is a dummy burst indicating end of simulation
            end_of_simulation=1;
            break;
        }

        // Simulate the running of the process by sleeping for a while
        remaining_time = current_burst->remaining_time;
        if (strcmp(algorithm, "RR") == 0 && remaining_time > q) {
            // For RR, if remaining time is greater than quantum, set remaining time to quantum
            remaining_time = q;
        }
        printf("%d", remaining_time);
        usleep(remaining_time * 1000);

        // Update the remaining time and put it back into the queue
        //pthread_mutex_lock(&queue->lock);
        current_burst->remaining_time -= remaining_time;
        printf("\n------------%d------------------%d---", current_burst->pid, current_burst->remaining_time);
        if (current_burst->remaining_time <= 0) {
            // Burst is finished
            gettimeofday(&current_time,NULL);
            timestamp = (current_time.tv_sec - start_time.tv_sec)*1000 + (current_time.tv_usec- start_time.tv_usec)/1000;

            current_burst->finish_time = timestamp;
            current_burst->turnaround_time = current_burst->finish_time - current_burst->arrival_time;
            //current_burst->waiting_time = current_burst->turnaround_time - current_burst->burst_length;
            push_to_finish_list(finish_list, current_burst);
            printf("pushed");
        } else {
            // Burst is not finished, put it back into the queue
            enqueue_burst(current_burst,queue);
        }
        //pthread_mutex_unlock(&queue->lock);
    }

    // Thread has finished its work
    pthread_exit(NULL);
}






void enqueue_burst_multi(burst_t* burst, queue_t** queue_array, int num_processors, int method) {
    if (method == 1) {  // round-robin method
        int queue_index = burst->pid % num_processors;  // select queue based on PID
        pthread_mutex_lock(&queue_array[queue_index]->lock);
        if (queue_array[queue_index]->tail == NULL) {
            queue_array[queue_index]->head = queue_array[queue_index]->tail = burst;
        } else {
            queue_array[queue_index]->tail->next = burst;
            queue_array[queue_index]->tail = burst;
        }
        queue_array[queue_index]->size++;
        pthread_mutex_unlock(&queue_array[queue_index]->lock);
    } else {  // load-balancing method
        int smallest_queue_index = 0;
        int smallest_queue_size = queue_array[0]->size;
        for (int i = 1; i < num_processors; i++) {  // find queue with smallest size
            if (queue_array[i]->size < smallest_queue_size) {
                smallest_queue_index = i;
                smallest_queue_size = queue_array[i]->size;
            }
        }
        pthread_mutex_lock(&queue_array[smallest_queue_index]->lock);
        if (queue_array[smallest_queue_index]->tail == NULL) {
            queue_array[smallest_queue_index]->head = queue_array[smallest_queue_index]->tail = burst;
        } else {
            queue_array[smallest_queue_index]->tail->next = burst;
            queue_array[smallest_queue_index]->tail = burst;
        }
        queue_array[smallest_queue_index]->size++;
        pthread_mutex_unlock(&queue_array[smallest_queue_index]->lock);
    }
}


/* Function to simulate a CPU burst */
void simulate_burst(burst_t *burst, int processor_id) 
{
    burst->processor_id = processor_id;
    burst->remaining_time = 0;
    burst->finish_time = time(NULL);
    burst->turnaround_time = burst->finish_time - burst->arrival_time;

    printf("Burst %d executed in processor %d. Turnaround time: %d\n", burst->pid, burst->processor_id, burst->turnaround_time);
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
	char* outfile_name = "out.txt";
	int method=0;

	
	// Burst will be generated random if random > 0, read file if random == 0
    	int random = 1;

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

	// Output parsed arguments (for testing purposes)
	printf("Num of processors = %d\n", processor_number);
	printf("Sched approach = %s\n", sch_approach);
	printf("Queue selection method = %s\n", queue_sel_method);
	printf("Algorithm = %s\n", algorithm);
	printf("quantum = %d\n", quantum_number);
	printf("infile name = %s\n", infile_name);
	printf("out mode = %d\n", out_mode);
	printf("outfile name = %s\n", outfile_name);
    
    if(strcmp(queue_sel_method, "RM") == 0){
        method = 1;
    }
    else if(strcmp(queue_sel_method, "LM") == 0){
        method = 2;
    }
    printf("%d", method);
    fflush(stdout);

    queue_t* ready_queue= malloc(sizeof(queue_t));
    queue_t* ready_queues[processor_number];
    finish_list_t* finish_list;

    finish_list =(finish_list_t*)malloc(sizeof(finish_list_t));
    finish_list_init(finish_list);
    printf("test1");
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

	printf("checkpoint 1- initialized ready queues\n");

	// Create processor threads
    pthread_t threads[processor_number];
   
    if(strcmp(sch_approach, "S")==0){
        for (int i = 0; i < processor_number; i++) {
            struct thread_args* args = (struct thread_args*) malloc(sizeof(struct thread_args));
            args->algorithm = algorithm;
            args->q = quantum_number;
            args->finish_list=finish_list;
            args->thread_id = i+1;
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
            args->queue = &(*ready_queues)[i]; // Add this line to pass the ready queue
            pthread_create(&threads[i], NULL, processor_function, (void*) args);
        }

    }
    

	
	// Open input file and initialize variables
	FILE* input_file = fopen(infile_name, "r");
	char line[MAX_BUF_SIZE];
	int pid_counter = 1;
	printf("checkpoint 3- starting processing bursts \n");
 	gettimeofday(&start_time, NULL);

	// Process the bursts sequentially
	while (fgets(line, sizeof(line), input_file)) 
	{
        if (strncmp(line, "PL", 2) == 0) 
        { 
            printf("checkpoint 4- PL Reading \n");
            // a new burst
            // Parse the burst length from the line
            int burst_length = atoi(line + 3);
    
            printf("Burst length: %d \n",burst_length);
            fflush(stdout);
            
            // Create a new burst item and fill in its fields
            burst_t* burst = malloc(sizeof(burst_t));
            burst->pid = ++last_pid;
            printf("burst id: %d \n",burst-> pid);
            burst->burst_length = burst_length;
            gettimeofday(&current_time,NULL);
            timestamp = (current_time.tv_sec - start_time.tv_sec)*1000 + (current_time.tv_usec- start_time.tv_usec)/1000;
            burst->arrival_time = timestamp;
            printf("arrival time: %d \n",burst->arrival_time);

            burst->remaining_time = burst_length;
            burst->finish_time = 0;
            burst->turnaround_time = 0;
            burst->processor_id = 0;
    

            if (strcmp(sch_approach, "S") == 0) 
            {
                enqueue_burst(burst, ready_queue);
            }
            else{
                enqueue_burst_multi(burst, ready_queues, processor_number,method);
            }
        		
			printf("checkpoint 8- burst is enqueued \n");
			printf("checkpoint 9 - unlocked \n");
		}
		else if (strncmp(line, "IAT", 3) == 0) 
        { 
            // an interarrival time
            // Parse the interarrival time from the line
            int interarrival_time = atoi(line + 4);
            printf("IAT: %d\n",interarrival_time);
            fflush(stdout);
            printf("checkpoint 10 - inside IAT before sleeping \n");
    
            // Sleep for the interarrival time
            usleep(interarrival_time*1000);
            printf("checkpoint 11 - inside IAT after sleeping \n");
    
            //timestamp += interarrival_time;
            printf("checkpoint 12 - inside IAT timestamp updated \n");
        }
	} 
	
	
	// Close the input file
    fclose(input_file); 

    //display ready queues
    //if(strcmp(sch_approach, "S")==0){
      //  displayList(ready_queue);
    //}
    

    // Add dummy bursts to each queue
    if(strcmp(sch_approach, "S")==0){
        burst_t* dummy_burst = malloc(sizeof(burst_t));
        dummy_burst->pid = -1;
        dummy_burst->arrival_time=-1;
        dummy_burst->burst_length=1000000;
        dummy_burst->finish_time=-1;
        dummy_burst->remaining_time=-1;
        dummy_burst->turnaround_time=-1;
        enqueue_burst(dummy_burst, ready_queue);
    }
    else{
        burst_t* dummy_burst = malloc(sizeof(burst_t));
        dummy_burst->pid = -1;
        dummy_burst->arrival_time=-1;
        dummy_burst->burst_length=1000000;
        dummy_burst->finish_time=-1;
        dummy_burst->remaining_time=-1;
        dummy_burst->turnaround_time=-1;

        for (int i = 0; i < processor_number; i++) {
            enqueue_burst(dummy_burst, &(*ready_queues)[i]);
        }
    }
	
	printf("checkpoint 14 - dummy bursts are added to the end of queues \n");
	// Wait for processor threads to terminate
	for (int i = 0; i < processor_number; i++) 
	{
    		pthread_join(threads[i], NULL);
    		printf("checkpoint 15 - thread %d is joining\n",i);
	}
    displayFinishList(finish_list);
}
