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

typedef struct threads
{
    int pid;
    int burst_length;
    int arrival_time;
    int remaining_time;
    int finish_time;
    int turnaround_time;
    int processor_id;
} threads;

typedef struct schleduling_arguments
{
    char* algorithm;
    int q;
    int index;
} schleduling_arguments;


typedef struct queue_elements
{
    struct queue_elements* previous;
    struct queue_elements* next;
    struct threads* current_thread;
    
    struct queue_elements* head;
    struct queue_elements* tail;
    
    pthread_mutex_t lock;
    int size;
} queue_elements;


struct queue_elements** queues;
struct queue_elements** tails;
struct queue_elements* doneProcessesHead;
struct queue_elements* doneProcessesTail;
struct timeval start;

int num_processors;
int num_bursts=0;
int curr_pid =0;
int last_pid=0;
pthread_t processor_threads[MAX_NUM_PROCESSORS];
int timestamp;
struct timeval start_time, current_time;

struct queue_elements* createQueueElement(struct threads* t) 
{
    struct queue_elements* newElement = (struct queue_elements*) malloc(sizeof(struct queue_elements));

    newElement->previous = newElement->next = NULL;
    newElement->current_thread = t;
    newElement->size = 0; 
    return newElement;
}


void displayList(struct queue_elements* head) 
{
    if (head == NULL || head->current_thread == NULL) 
    {
        printf("Queue is empty.\n");
    }
    else 
    {
        struct queue_elements* current = head;

        while (current != NULL) 
        {
            printf("----------------\n");
            printf("- id: %d\n", current->current_thread->pid);
            printf("- burst length: %d\n", current->current_thread->burst_length);
            printf("- arrival time: %d\n", current->current_thread->arrival_time);
            printf("- finish time: %d\n", current->current_thread->finish_time);
            printf("- waiting time: %d\n", current->current_thread->finish_time - current->current_thread->arrival_time - current->current_thread->burst_length);
            printf("- turnaround time: %d\n", current->current_thread->finish_time - current->current_thread->arrival_time);
            current = current->next;
        }
        printf("----------------\n");
        printf("\n");
    }
}


int get_index_by_element(struct queue_elements* queue, struct queue_elements* element) 
{
    struct queue_elements* current_element = queue->head;
    int index = 0;
    while (current_element != NULL) 
    {
        if (current_element == element) 
        {
            return index;
        }
        current_element = current_element->next;
        index++;
    }
    // element not found in queue
    return -1;
}

struct queue_elements* find_shortest(struct queue_elements* queue) 
{
    struct queue_elements* current_element = queue->head;
    struct queue_elements* min_element = current_element;

    while (current_element != NULL) 
    {
        if (current_element->current_thread->remaining_time < min_element->current_thread->remaining_time) 
        {
            min_element = current_element;
        }
        current_element = current_element->next;
    }
    
    return min_element;
}


struct queue_elements* get_first_item(struct queue_elements* queue) 
{
    pthread_mutex_lock(&queue->lock);
    struct queue_elements* first_item = NULL;
    if (queue->head != NULL) 
    {
        first_item = queue->head;
        queue->head = queue->head->next;
        if (queue->head == NULL) 
        {
            queue->tail = NULL;
        } else 
        {
            queue->head->previous = NULL;
        }
        queue->size--;
    }
    pthread_mutex_unlock(&queue->lock);
    return first_item;
}

struct queue_elements* get_element_by_index(struct queue_elements* queue, int  index) 
{
    if (index < 0 || index >= queue->size) 
    {
        return NULL;
    }
    pthread_mutex_lock(&queue->lock);
    struct queue_elements* current_element = queue->head;
    for (int i = 0; i < index; i++) 
    {
        current_element = current_element->next;
    }
    if (current_element == queue->head) 
    {
        if (current_element == queue->tail) 
        {
            queue->head = NULL;
            queue->tail = NULL;
        } 
        else 
        {
            current_element->next->previous = NULL;
            queue->head = current_element->next;
        }
    } 
    else if (current_element == queue->tail) 
    {
        current_element->previous->next = NULL;
        queue->tail = current_element->previous;
    } 
    else 
    {
        current_element->previous->next = current_element->next;
        current_element->next->previous = current_element->previous;
    }
    queue->size--;
    pthread_mutex_unlock(&queue->lock);
    return current_element;
}

int timeval_difference(struct timeval* t1, struct timeval* t2) {
    int diff_sec = t2->tv_sec - t1->tv_sec;
    int diff_usec = t2->tv_usec - t1->tv_usec;
    int diff_ms = diff_sec * 1000 + diff_usec / 1000;
    return diff_ms;
}

int findLeastLoad(int proc_count) 
{
    int min;
    int minIndex = 0;

    for (int i = 0; i < proc_count; i++) 
    {
        queue_elements* cur = queues[i];
        int sum = 0;
        while (cur != NULL) 
        {
            sum += cur->current_thread->remaining_time;
            cur = cur->next;
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
    }
    return minIndex;
}

void insertToEnd(queue_elements** head, queue_elements** tail, queue_elements* newElement) 
{
    // If the queue is empty, set the head and tail to the new element
    if (*head == NULL) 
    {
        *head = newElement;
        *tail = newElement;
    }
    else 
    {
        // Insert the new element to the end of the queue
        newElement->next = (*tail)->next;
        newElement->previous = *tail;
        (*tail)->next = newElement;
        *tail = newElement;
    }
}

void insertAscendingOrder(struct queue_elements** head, struct queue_elements** tail, struct queue_elements* newElement) {
    if (*head == NULL) {
        *head = newElement;
        *tail = newElement;
    }
    else if ((*head)->current_thread->pid < newElement->current_thread->pid) 
    {
        newElement->next = *head;
        newElement->next->previous = newElement;
        *head = newElement;
    }
    else 
    {
        // insert in the middle or end
        struct queue_elements* cur = *head;

        while (cur->next != NULL && cur->next->current_thread->pid >= newElement->current_thread->pid) 
        {
            cur = cur->next;
        }

        if (cur->next != NULL) 
        {
            newElement->next = cur->next;
            newElement->next->previous = newElement;
            cur->next = newElement;
            newElement->previous = cur;
        }
        else {
            newElement->next = (*tail)->next;
            (*tail)->next = newElement;
            newElement->previous = *tail;
            *tail = newElement;
        }
    }
}




void* process_thread(void *arg) {
    struct schleduling_arguments* args = (schleduling_arguments*)arg;
    int index = args->index;
    printf("Created thread with q index %d\n", index);
    printf("%d", index);
    while (1) 
    {
        pthread_mutex_lock(&queues[index]->lock);
        if (queues[index] == NULL) 
        {
            pthread_mutex_unlock(&queues[index]->lock);
            usleep(1);
        }
        else 
        {
            int flag = 0;
            struct queue_elements* current;

            if (strcmp(args->algorithm, "FCFS") == 0 || strcmp(args->algorithm, "SJF") == 0) 
            {
            

                if (strcmp(args->algorithm, "FCFS") == 0)
                {
                	 current = get_first_item(queues[index]);
                } 
                else
                {
                	//get the element with the shortest remaining time
			struct queue_elements* min_element = find_shortest(queues[index]);

                	current = get_element_by_index(queues[index],get_index_by_element(queues[index],min_element));
                } 
                
                pthread_mutex_unlock(&queues[index]->lock);

                if (current->current_thread->pid == -1)
                {
                	pthread_exit(0);
                }
               
                // sleep for the duration of burst
                sleep(current->current_thread->burst_length / 1000);
                flag = 1;
            }
            else 
            { 	
		// Default RR
		pthread_mutex_lock(&queues[index]->lock);
		struct queue_elements* cur = get_element_by_index(queues[index], 0);
		pthread_mutex_unlock(&queues[index]->lock);
		printf("pid: %d\n", cur->current_thread->pid);
		if (cur->current_thread->pid == -1) 
		{
    			pthread_exit(0);
		}

		if (cur->current_thread->remaining_time <= args->q) 
		{
    			sleep(cur->current_thread->remaining_time / 1000);
    			flag = 1;
		}
		else 
		{
    			sleep(args->q / 1000);
    			// update remaining time and add to tail
    			cur->current_thread->remaining_time -= args->q;
    			pthread_mutex_lock(&queues[index]->lock);
    			insertToEnd(&queues[index], &tails[index], cur);
    			pthread_mutex_unlock(&queues[index]->lock);
		}

            }
            

            if (flag) 
            {
                // update information of process
                struct threads* pointer= current->current_thread;
                struct timeval burstFinishTime;
                pointer->finish_time = gettimeofday(&burstFinishTime, NULL);
                pointer->turnaround_time = pointer->finish_time - pointer->arrival_time;
                pointer->remaining_time = 0;
                pointer->processor_id = index;

                // add to finished processes list
                pthread_mutex_lock(&doneProcessesHead->lock);
                insertAscendingOrder(&doneProcessesHead, &doneProcessesTail, current);
                pthread_mutex_unlock(&doneProcessesHead->lock);
            }
        }
    }       
}


int main(int argc, char* argv[]) 
{
    // Get and set args
    gettimeofday(&start_time, NULL);
    int processor_number = 2;
    char* sch_approach = "M";
    char* queue_sel_method = "RM";
    char* algorithm = "RR";
    int quantum_number = 20; // default value
    char* infile_name = "in.txt"; //bunu silebiliriz de default value olmak zorunda degilmis korpenin yazdigina gore
    int out_mode = 1;
    char* outfile_name = "out.txt";

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
            // anlamadim tam bunu 
        }
    } 
    //outputting data(testing icin)
    printf("Num of processors = %d\n", processor_number);
    printf("Sched approach = %s\n", sch_approach);
    printf("Queue selection method = %s\n", queue_sel_method);
    printf("Algorithm = %s\n", algorithm);
    printf("quantum = %d\n", quantum_number);
    printf("infile name = %s\n", infile_name);
    printf("out mode = %d\n", out_mode);
    printf("outfile name = %s\n", outfile_name);
    fflush(stdout);
    
     // Get and record start time
    gettimeofday(&start, NULL);
    
   
    // Create queue(s)
    if (strcmp(sch_approach, "S") == 0) 
    {
    	queues = (queue_elements**)malloc(sizeof(queue_elements*));
    	*queues = (queue_elements*)malloc(sizeof(queue_elements));
    	(*queues)->head = NULL;
    	(*queues)->tail = NULL;
    	(*queues)->size = 0;
    	pthread_mutex_t* mutex = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
    	pthread_mutex_init(mutex, NULL);
    	(*queues)->lock = *mutex;
    }	
    else  
    {
    	queues = (queue_elements**)malloc(processor_number * sizeof(queue_elements*));
    	for (int i = 0; i < processor_number; i++) 
    	{
        	*(queues+i) = (queue_elements*)malloc(sizeof(queue_elements));
        	(*(queues+i))->head = NULL;
        	(*(queues+i))->tail = NULL;
        	(*(queues+i))->size = 0;
        	pthread_mutex_t* mutex = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
        	pthread_mutex_init(mutex, NULL);
        	(*(queues+i))->lock = *mutex;
    	}
     }
     printf("checkpoint 1\n");
     struct schleduling_arguments args[processor_number];
    // Create processor threads
    pthread_t threads[processor_number];
    int count = 0;
    for (int i = 0; i < processor_number; i++)
    {
	if (strcmp(sch_approach, "S") == 0) 
	{
            args[i].index = 0;
            if(count == 1)
            	break;
            count++;
        }
        else 
        {
        
            args[i].index = i;
            args[i].algorithm = algorithm;
            args[i].q = quantum_number;
            pthread_create(&threads[i], NULL, process_thread, (void*) &args[i]);
        }
            
        args[i].algorithm = algorithm;
        args[i].q = quantum_number;
        pthread_create(&threads[i], NULL, process_thread, (void*) &args[i]);
    }
    printf("checkpoint 2\n");
    char* line = NULL;
    size_t len = 0;
    ssize_t read;
    FILE* input_file = fopen(infile_name, "r");
    if (input_file == NULL) 
    {
        perror("Error opening input file");
        exit(EXIT_FAILURE);
    }  

    FILE* output_file = fopen(outfile_name,"w");
    if (output_file == NULL) 
    {
        perror("Error opening output file");
        exit(EXIT_FAILURE);
    }
    int cur_id = 0;
    printf("checkpoint 3\n");

    while ((read = getline(&line, &len, input_file)) != -1) 
    {
        // Process the line
        char* keyword = strtok(line, " ");
        char* burst = strtok(NULL, " ");
        int burst_len = atoi(burst);

        struct timeval arrival;
        gettimeofday(&arrival, NULL);
        
        struct threads *t = (struct threads*)malloc(sizeof(struct threads));
        t->pid = cur_id;
        t->burst_length = burst_len;
        t->remaining_time = burst_len;
        t->arrival_time = timeval_difference(&start, &arrival);
        printf("checkpoint 4\n");
        
        if (strcmp(sch_approach,"S") == 0) 
        {
        
            pthread_mutex_lock(&queues[0]->lock);
            insertToEnd(&queues[0],&tails[0],createQueueElement(t));
            pthread_mutex_unlock(&queues[0]->lock);
            printf("checkpoint S\n");
        }
        else if (strcmp(sch_approach,"M") == 0) 
        {
            int q_index;
            if (strcmp(queue_sel_method,"RM") == 0) 
            {
                printf("\nIn queue selection\n");
                printf("Cur id = %d\n", cur_id);
                q_index = cur_id % processor_number;
                printf("Selected queue = %d\n", q_index);
                printf("checkpoint RM\n");
            }
             else if (strcmp(queue_sel_method,"LM") == 0) 
             {
                printf("\nIn load queue selection\n");
                q_index = findLeastLoad(processor_number);
                printf("Selected queue = %d\n", q_index);
                printf("checkpoint LM\n");
            }
            pthread_mutex_lock(&queues[q_index]->lock);
            insertToEnd(&queues[q_index],&tails[q_index],createQueueElement(t));
            pthread_mutex_unlock(&queues[q_index]->lock);
            
            read = getline(&line, &len, input_file);
            if (read == -1) 
            {
            	break;
            }
            keyword = strtok(line, " ");
            char* inter_arrival = strtok(NULL, " ");
            int iat = atoi(inter_arrival);
            cur_id++;
	    sleep(iat / 1000);
        }
        // Add dummy Nodes
    	if (strcmp(sch_approach, "S") == 0) 
    	{
        	struct threads *t = (struct threads*)malloc(sizeof(struct threads));
        	t->pid = -1;
        	t->burst_length = 10000000;
        	t->remaining_time = 10000000;
        	t->arrival_time = 100000000;
        	queue_elements* dummy = createQueueElement(t);
        	pthread_mutex_lock(&queues[0]->lock);
        	insertToEnd(&queues[0],&tails[0],dummy);
        	pthread_mutex_unlock(&queues[0]->lock);
    	}
    	else 
    	{
        	for (int i = 0; i < processor_number; i++) 
        	{
        	    	struct threads *t = (struct threads*)malloc(sizeof(struct threads));
        	    	t->pid = -1;
        		t->burst_length = 10000000;
        		t->remaining_time = 1000000;
        		t->arrival_time = 10000000;
        		queue_elements* dummy = createQueueElement(t);
        		pthread_mutex_lock(&queues[i]->lock);
        		insertToEnd(&queues[i],&tails[i],dummy);
        		pthread_mutex_unlock(&queues[i]->lock);
        	}
    	}
    	// Free the memory allocated for the line
    	free(line);

    	// Close the file
    	fclose(input_file);

    	// Wait for threads
    	for (int i = 0; i < processor_number; i++) 
    	{
        	printf("Thread %d has finished", i);
        	pthread_join(threads[i], NULL);
    	}

    	for (int i = 0; i < processor_number; i++)
    	{
        	printf("Queue %d\n", i);
        	displayList(queues[i]);
    	}
    }


    return 0;
}





