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

struct thread_args {
    char* algorithm;
    int q;
    int thread_id;
};

struct timeval start_time, current_time;

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
        if (min_element == NULL || current_element->remaining_time < min_element->remaining_time) 
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

void remove_burst_from_queue(queue_t* queue, burst_t* burst) 
{
    pthread_mutex_lock(&queue->lock);  // Acquire the lock

    if (queue->head == NULL || burst == NULL) 
    {
        pthread_mutex_unlock(&queue->lock);  // Release the lock if queue or burst is empty
        return;
    }

    if (queue->head == burst) 
    {  // If burst is the head of the queue
        queue->head = burst->next;
        if (queue->tail == burst) 
        {  // If burst is also the tail of the queue
            queue->tail = NULL;
        }
    } 
    else 
    {
        burst_t* prev = queue->head;
        while (prev->next != NULL && prev->next != burst) {
            prev = prev->next;
        }
        if (prev->next != NULL) 
        {
            prev->next = burst->next;
            if (queue->tail == burst) 
            {  // If burst is the tail of the queue
                queue->tail = prev;
            }
        }
    }

    pthread_mutex_unlock(&queue->lock);  // Release the lock
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


void* processor_function(void* arg) 
{
    struct thread_args* args = (struct thread_args*) arg;
    char* algorithm = args->algorithm;
    int q = args->q;
    int index = args->thread_id;
    // rest of function

    printf("%d", index);
    while (1) 
    {
        printf("inside thread function\n");

        pthread_mutex_lock(&ready_queues[index]->lock);
        if (ready_queues[index]->head == NULL) 
        {
            pthread_mutex_unlock(&ready_queues[index]->lock);
            usleep(1);
        }
        else 
        {
            int flag = 0;
            burst_t* current_thread = NULL;
            if (strcmp(args->algorithm, "FCFS") == 0 || strcmp(args->algorithm, "SJF") == 0) 
            {
                if (ready_queues[index]->head == NULL) 
                {
                    pthread_mutex_unlock(&ready_queues[index]->lock);
                    pthread_exit(0);
                }
    
                if (strcmp(args->algorithm, "FCFS") == 0)
                {
                    current_thread = ready_queues[index]->head;
                } 
                else
                {
                    //get the element with the shortest remaining time
                    burst_t* min_element = find_shortest(ready_queues[index]);
                    current_thread = get_burst_by_pid(ready_queues[index]->head, min_element->pid);
                } 

                // remove the burst from the ready queue
                remove_burst_from_queue(ready_queues[index], current_thread);
    
                pthread_mutex_unlock(&ready_queues[index]->lock);

                // sleep for the duration of burst
                sleep(current_thread->burst_length * 1000);
                flag = 1;
            }
            else 
            {   
                // Default RR
                if (ready_queues[index]->head == NULL) 
                {
                    pthread_mutex_unlock(&ready_queues[index]->lock);
                    pthread_exit(0);
                }

                burst_t* cur = ready_queues[index]->head;
                ready_queues[index]->head = ready_queues[index]->head->next;

                if (cur->burst_length <= args->q) 
                {
                    sleep(cur->burst_length * 1000);
                    flag = 1;
                }
                else 
                {
                    sleep(args->q * 1000);
                    // update burst length and add to tail
                    cur->burst_length -= args->q;
                    if (ready_queues[index]->tail == NULL) 
                    {
                        ready_queues[index]->head = cur;
                        ready_queues[index]->tail = cur;
                    }
                    else 
                    {
                        ready_queues[index]->tail->next = cur;
                        ready_queues[index]->tail = cur;
                    }
                    ready_queues[index]->tail->next = NULL;
                }
    
                pthread_mutex_unlock(&ready_queues[index]->lock);
            }

            if (flag == 1) 
            {
                // update finish time
                current_thread->finish_time = get_current_time();
            }
        }
    }
}



void enqueue_burst(burst_t* burst, queue_t* queue) {
    pthread_mutex_lock(&queue->lock);

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


/* Function for the processor thread */
void *processor_thread(void *arg) 
{
    int processor_id = *((int *)arg);
    while (1) 
    {
        // Remove a burst from the queue
        burst_t* burst = dequeue_process(processor_id);

        // Simulate the burst
        simulate_burst(burst, processor_id);
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
	char* outfile_name = "out.txt";
	
	
	// Burst will be generated random if random > 0, read file if random == 0
    	int random = 1;

    	// Random variables
    	int iat_mean = 200;
    	int iat_min = 10;
    	int iat_max = 1000;
        int method;

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
	
    queue_t* ready_queue;
    queue_t* ready_queues[processor_number];
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
	pthread_t processor_threads[processor_number];
	for (int i = 0; i < processor_number; i++) 
	{
   		int* processor_id_ptr = malloc(sizeof(int));
    		*processor_id_ptr = i + 1;
    		pthread_create(&processor_threads[i], NULL, processor_function, (void*)processor_id_ptr);
    		printf("checkpoint 2 - thread %d creating\n", *processor_id_ptr);
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
                timestamp += (current_time.tv_sec - start_time.tv_sec)*1000 + (current_time.tv_usec- start_time.tv_usec)/1000;
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
    	
    // Add dummy bursts to each queue
    if(strcmp(sch_approach, "S")==0){
        burst_t* dummy_burst = malloc(sizeof(burst_t));
        dummy_burst->pid = -1;
        enqueue_burst(dummy_burst, &ready_queue);
    }
    else{
        burst_t* dummy_burst = malloc(sizeof(burst_t));
        dummy_burst->pid = -1;

        for (int i = 0; i < processor_number; i++) {
            enqueue_burst(dummy_burst, &(*ready_queues)[i]);
        }
    }

	
	printf("checkpoint 14 - dummy bursts are added to the end of queues \n");
	// Wait for processor threads to terminate
	for (int i = 0; i < processor_number; i++) 
	{
    		pthread_join(processor_threads[i], NULL);
    		printf("checkpoint 15 - thread %d is joining\n",i);
	}
}
