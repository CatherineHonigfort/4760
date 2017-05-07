#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/msg.h>
#include <signal.h>
#include <sys/shm.h>
#include <string.h>
#include "proj4.h"
#include <math.h>



/*getopt() flags:*/
static int h_flag = 0;
static char* fileName = "log_file.txt"; //the filename in -l filename
static int computationShotClock = 20; //the z in -t z; is time in seconds when the master will terminate itself and all children
/*getopt() flags:*/
static int message_queue_id; //holds message queue id 
static pid_t actual[MAX_USER_PROCESSES + 1]; //array of pid_t(s);
system_clock_t newProcessCreateTime; //a clock to use as differentiator between the logical clock and the system clock of OSS


int process_table[32]; //process table
int process_table_reservation; //index of the process control blocks in the process table 

int shmid; //holds the shared memory segment id 
shared_memory_object_t* shm_ptr; //pointer to starting address in shared memory of the shared memory structure 
FILE* fp; //global FILE* pointer
int line_counter = 1; //counts every line printed

// REFERENCED from textbook -- 3-28
int terminated_process_count = 0; 
double avg_turn_around_time = 0.00; //nano_seconds
unsigned long long total_turn_around_time = 0;
double avg_wait_time = 0.00; //average time a process in the system spends waiting for control of the CPU
unsigned int cpu_idle_itme = 0; // computing the average wait time


//REFERENCE 3-25
// Define queue type
typedef struct Node_Object node_object_t; //forward declaration 

//REFERENCE3-25
typedef struct Node_Object {
	int process_id;
	node_object_t* next_node;
} node_object_t; //node for a linked-list implementation of a queue

typedef struct Queue_Object {
    node_object_t* front;
    node_object_t* back;
    int time_quantum;
} queue_object_t; //this is a queue data structure for implementing a multi-level queue schedluing system

static queue_object_t* multilevel_queue_system[3]; //multi-level queue ptr

queue_object_t* create_a_new_queue(int time_quantum_amt) {
    queue_object_t* new_queue = (queue_object_t*) malloc(sizeof(queue_object_t));

    //malloc failed 
    if (new_queue == NULL) {
        return NULL;
    }

    new_queue->time_quantum = time_quantum_amt;
    new_queue->front = NULL;
    new_queue->back = NULL;

    return new_queue;
}

int push_enqueue(queue_object_t* destinationQueue, int processID) {

    //Check if the queue is a valid queue:
    if (destinationQueue == NULL) {
        return -1; //invalid queue
    }

    //Add a node to the queue:
    node_object_t* new_queue_node = (node_object_t*) malloc(sizeof(node_object_t));
    new_queue_node->process_id = processID;
    new_queue_node->next_node = NULL;

    //Empty Queue Processing:
    if (destinationQueue->front == NULL) {
        destinationQueue->front = new_queue_node;
        destinationQueue->back = new_queue_node;
    }
    //Non-Empty Queue Processing:
    else {
        destinationQueue->back->next_node = new_queue_node; //pointing to node_object_t pointer 
        destinationQueue->back = new_queue_node;
    }

    //return a 0 if push was successful:
    return 0;
}

int pop_dequeue(queue_object_t* sourceQueue) {

    //Empty Queue Processing:
    if(sourceQueue->front == NULL){
        return -1; //this is an empty queue
    }

    int processID = sourceQueue->front->process_id; //return the process_id value to identify which process is being popped off the queue
    
    //Remove Node From Queue:
    node_object_t* temporary_node = sourceQueue->front;
    sourceQueue->front = sourceQueue->front->next_node;
    free(temporary_node);

    return processID; 
}


void destroyQueue(queue_object_t* sourceQueue) {

    node_object_t* current_node = sourceQueue->front;
    node_object_t* temporary_node;

    //Free nodes in Queue:
    while (current_node != NULL) {
        temporary_node = current_node->next_node;
        free(current_node);
        current_node = temporary_node;
    }

    //Free The Queue Itself:
    free(sourceQueue);
}

int process_selection(void) {
    int i; 
    int selected_process;

//3 = the number of queues in our multilevel queue system
    for(i = 0; i < 3; ++i) {
        selected_process = pop_dequeue(multilevel_queue_system[i]);

        //Check If pop_dequeue(queue_array[]) Returned -1 because of an Empty/Non-Valid Queue
        if (selected_process == -1) {
            continue; //this queue is empty - try the next queue 
        }

        return selected_process;
    }
    
    //Report Back a -1 if no processes in queues 
    return -1;
}


void process_scheduler(int process_table_reservation) {
    //Priority for user at process_table_reservation
    int current_priority = shm_ptr->process_control_block[process_table_reservation].priority;

    // CPU BOUND PROCESS:
    if (shm_ptr->process_control_block[process_table_reservation].burst == multilevel_queue_system[shm_ptr->process_control_block[process_table_reservation].priority]->time_quantum) {
        shm_ptr->process_control_block[process_table_reservation].priority = (current_priority + 1 >= 3) ? current_priority : ++current_priority;
        shm_ptr->process_control_block[process_table_reservation].time_quantum = multilevel_queue_system[current_priority]->time_quantum;
    }

    // I/O Bound
    else {
        // Output message 
        fprintf(fp, "%d: OSS: PROCESS_PID_%d TIME_QUANTUM NOT EXHAUSTED ~ I/O BOUND PROCESS\n", line_counter, process_table_reservation);
        ++line_counter;

        current_priority = 0;
        shm_ptr->process_control_block[process_table_reservation].priority = current_priority;
        shm_ptr->process_control_block[process_table_reservation].time_quantum = multilevel_queue_system[current_priority]->time_quantum;
    }
    //Push to right queue 
    push_enqueue(multilevel_queue_system[current_priority], process_table_reservation);

    //Output Message:
    fprintf(fp, "%d: OSS: Putting PROCESS_PID_%d INTO QUEUE %d\n", line_counter, process_table_reservation, current_priority);
    ++line_counter;
}

//this function controls the logical clock (controlled by oss)
void clock_incrementation_function(system_clock_t *destinationClock, system_clock_t sourceClock, int additional_nano_seconds) {
    destinationClock->nano_seconds = sourceClock.nano_seconds + additional_nano_seconds;
    if(destinationClock->nano_seconds > 1000000000) {
        destinationClock->seconds++;
        destinationClock->nano_seconds -= 1000000000;
    }
}

//function detaches the shared memory segment shmaddr and then removes the shared memory segment specified by shmid
int detachandremove (int shmid, void* shmaddr){
    int error = 0;
    if (shmdt(shmaddr) == - 1)
        error = errno;
    if ((shmctl(shmid, IPC_RMID, NULL) == -1) && !error)
        error = errno;
    if (!error)
        return 0;
    errno = error;
    return -1;
}

//function that spawns/generates/forks slave processes; 
void makeUserProcesses(int process_table_position) {
    char slave_id[3]; //char array to be used to hold a copy of the process_table_position value 
        shm_ptr->process_control_block[process_table_position].priority = 0;
        shm_ptr->process_control_block[process_table_position].cpu_usage_time = 0;
        shm_ptr->process_control_block[process_table_position].burst = 0;
        shm_ptr->process_control_block[process_table_position].time_quantum = multilevel_queue_system[shm_ptr->process_control_block[process_table_position].priority]->time_quantum;
        shm_ptr->process_control_block[process_table_position].finished = 0;
        shm_ptr->process_control_block[process_table_position].process_starts.seconds = shm_ptr->clock_info.seconds; //start process life-span (time in system tracker) 
        shm_ptr->process_control_block[process_table_position].process_starts.nano_seconds = shm_ptr->clock_info.nano_seconds; //start process life-span (time in system tracker) clock at process generation time
        shm_ptr->process_control_block[process_table_position].process_arrives.seconds = 0; //this time will get initialized when the process is returned back from the Critical Section to the Master OSS
        shm_ptr->process_control_block[process_table_position].process_arrives.nano_seconds = 0; 

       actual[process_table_position] = fork();

        if (actual[process_table_position] < 0) {
            //fork failed
            perror("Fork failed");
            exit(errno);
        }
        else if (actual[process_table_position] == 0) { //child process executing after fork
            //slave/child/user process code - - remember that fork() returns twice!
            sprintf(slave_id, "%d", process_table_position); 
            execl("./user", "user", slave_id, NULL); //replaces memory copied to each process after fork 
            perror("Child failed to execl slave exe");
            printf("Error - this shouldn't display \n");
        }
    }

void signalCallback (int signum) {
    int j, status; 

    if (signum == SIGINT)
        printf("\nSIGINT received by master\n");
    else
        printf("\nSIGALRM received by master\n");


   for (j = 1; j <= MAX_USER_PROCESSES; j++){ 
        kill(actual[j], SIGINT);
    }

   while(wait(&status) > 0);  //wait for all of the slaves processes return exit statuses
   
   //Remove All Queues
   int i;
   for (i = 0; i < 3; ++i) {
       destroyQueue(multilevel_queue_system[i]);
   }

   avg_wait_time = cpu_idle_itme/terminated_process_count;
printf("\navg_wait_time = %.0f nano_seconds per process\n", avg_wait_time);
    avg_turn_around_time = total_turn_around_time/terminated_process_count;
printf("\navg_turn_around_time = %.0f nano_seconds per process\n", avg_turn_around_time);
printf("\nCPU_Idle_Time = %u nano_seconds\n", cpu_idle_itme); 
    //clean up program before exit 
    detachandremove(shmid,shm_ptr); //cleans up shared memory IPC
    msgctl(message_queue_id, IPC_RMID, NULL); //cleans up message queue IPC
    printf("master done\n");
    printf("\nline count = %d\n", line_counter);
    fclose(fp);
    exit(0);
}

void mail_the_message(int destination_address){
    static int size_of_message;
    message_t message;
    message.message_address = destination_address;
    //specifies the number of bytes in the message contents, not counting the address variable size
    size_of_message = sizeof(message) - sizeof(message.message_address);
    msgsnd(message_queue_id, &message, size_of_message, 0);  //send execution context messages
}

void receive_the_message(int destination_address){
    static int size_of_message;
    message_t message;
    size_of_message = sizeof(message) - sizeof(long);
    msgrcv(message_queue_id, &message, size_of_message, destination_address, 0);

    int process_table_reservation = message.return_address;

printf("\nid=%d\n", message.return_address);
    fprintf(fp, "\n%d: ID %d received time:  %d:%d\n", line_counter, process_table_reservation, shm_ptr->clock_info.seconds, shm_ptr->clock_info.nano_seconds);
    ++line_counter;

    int status; //holds the exiting code for terminated process 
    clock_incrementation_function(&shm_ptr->clock_info, shm_ptr->clock_info, shm_ptr->process_control_block[process_table_reservation].burst);
    
    if(shm_ptr->process_control_block[process_table_reservation].finished == 1) {

        shm_ptr->process_control_block[process_table_reservation].wait_time = ((shm_ptr->clock_info.seconds + shm_ptr->clock_info.nano_seconds) - (shm_ptr->process_control_block[process_table_reservation].process_starts.seconds + shm_ptr->process_control_block[process_table_reservation].process_starts.nano_seconds) - shm_ptr->process_control_block[process_table_reservation].cpu_usage_time);                     
        cpu_idle_itme += shm_ptr->process_control_block[process_table_reservation].wait_time;

        shm_ptr->process_control_block[process_table_reservation].turn_around_time = (((shm_ptr->clock_info.seconds * 1000000000) + shm_ptr->clock_info.nano_seconds) - ((shm_ptr->process_control_block[process_table_reservation].process_arrives.seconds * 1000000000) + shm_ptr->process_control_block[process_table_reservation].process_arrives.nano_seconds));
        fprintf(fp, "\n%d: PROCESS_ID_%d's Turn_Around_Time = %d nano_seconds\n", line_counter, process_table_reservation, shm_ptr->process_control_block[process_table_reservation].turn_around_time);        
        ++line_counter;
        total_turn_around_time += shm_ptr->process_control_block[process_table_reservation].turn_around_time;
        kill(actual[process_table_reservation], SIGINT);
        ++terminated_process_count;
        waitpid(actual[process_table_reservation], &status, 0);
        ClearBit(process_table, process_table_reservation);
        
        fprintf(fp, "%d: ID%d  termination time: %d:%d\n", line_counter, process_table_reservation, shm_ptr->clock_info.seconds, shm_ptr->clock_info.nano_seconds);
        ++line_counter;
        fprintf(fp, "%d: ID:%d BURST = %d\n", line_counter, process_table_reservation, shm_ptr->process_control_block[process_table_reservation].burst);
        ++line_counter;
    }
    else {
        process_scheduler(process_table_reservation);                    
    }

}

//search the process table
int get_process_table_index_state(int array[]) {
int i;
    for (i = 1; i <= MAX_USER_PROCESSES; i++) {
        if(!TestBit(array, i)) {
printf("\ni = %d\n", i);
            return i; //return index number of position
        }
printf("**PROCESS_TABLE_FULL**\n");
        return -1; //return a -1 if the process_table is full 
    }
}

int main(int argc, char* argv[]) {
    //holds the return value of getopt()
    int c; 
    while ( ( c = getopt(argc, argv, "hs:l:t:")) != -1)
    {
        switch(c) {
            case 'h':
                h_flag = 1;
                break;
            case 'l':
                fileName = optarg;
                break;
            case 't':
                computationShotClock = atoi(optarg);
                break;
            case '?':
                if (optopt == 'l' || optopt == 't')
                    fprintf(stderr, "Option %c requires an argument.\n", optopt);
                else
                    fprintf(stderr, "Unknown option -%c\n", optopt);
                return -1;
        }
    }

    if (h_flag == 1)
    {
        printf("'-h' flag: help menu\n");
        printf("'-l filename' flag: sets name of the file that slave processes will write into (default file name = %s).  Current name  is %s\n", fileName, fileName);
        printf("'-t z' flag: determines amount time until the master process terminates itself (default value = %d). Current value of timer variable = %d\n", computationShotClock, computationShotClock );
        exit(0);
    }

    //generate SIGINT via Ctrl+c
    if (signal(SIGINT, signalCallback) == SIG_ERR) {
        perror("Error: master: signal(): SIGINT\n");
        exit(errno);
    }
    //generate via alarm()
    if (signal(SIGALRM, signalCallback) == SIG_ERR) {
        perror("Error: slave: signal(): SIGALRM\n");
        exit(errno);
    }

    //this timer generates SIGALRM after computationShotClock seconds
    alarm(computationShotClock);

    //master process creates shared memory segment
    if ((shmid = shmget(SHM_KEY, sizeof(shared_memory_object_t), IPC_CREAT | 0600)) < 0) {
        perror("Error: shmget");
        exit(errno);
    }

    //master process creates a message queue;
    if ((message_queue_id = msgget(MESSAGE_QUEUE_KEY, IPC_CREAT | 0600)) < 0) {
        perror("Error: mssget");
        exit(errno);
    }

    //shared memory segment address 
    shm_ptr = shmat(shmid, NULL, 0);
    //master initializes shared memory
    shm_ptr->clock_info.seconds = 0; //zero before time is kept by the simulated system fact 
    shm_ptr->clock_info.nano_seconds = 0;  //zero before time is kept by the simulated system clock 
    newProcessCreateTime.seconds = 0;
    newProcessCreateTime.nano_seconds = 0;

    int golden_boy; //selected process

    //Create the Multi-Level Queue System:
    int k=0;
    for (k = 0; k < 3; k++){ //3 = the number of queues in system
        multilevel_queue_system[k] = create_a_new_queue(TIME_QUANT_QUEUE_0 * pow(2, k)); 
    }

    // Initialize Process Table:
    int j = 0; 
    for (j = 0; j <= 32; j++) {
        process_table[j] = 0; //32 bits in process table
    }

    //open  new file to write for log
    fp = fopen(fileName, "w");

    //infinite loop won't terminate without signal
    while(1) {

        if(line_counter > LINE_MAX) {
            fclose(fp);
        }
        
        //increment clock 
        clock_incrementation_function(&shm_ptr->clock_info, shm_ptr->clock_info, rand() % CONTEXT_SWITCH_TIME + 1);
        if ((shm_ptr->clock_info.seconds > newProcessCreateTime.seconds) || 
        (shm_ptr->clock_info.seconds == newProcessCreateTime.seconds && shm_ptr->clock_info.nano_seconds > newProcessCreateTime.nano_seconds)) {

                //return proces_table_reservation, from get_process_table_index_state
                process_table_reservation = get_process_table_index_state(process_table);
                printf("\nprocess_table_reservation = %d\n", process_table_reservation);

                //call user process generation function 
            if (process_table_reservation != -1) {
                makeUserProcesses(process_table_reservation);
                
            //put user process in process_table
            fprintf(fp, "%d: ID%d in process table in position: %d\n", line_counter, actual[process_table_reservation], process_table_reservation); //prints the pid of the child 
            ++line_counter;
            SetBit(process_table, process_table_reservation); //place a logical process in bit-vector from process_table simulation

            //place new process in queue based on priority
            push_enqueue(multilevel_queue_system[shm_ptr->process_control_block[process_table_reservation].priority], process_table_reservation);
            clock_incrementation_function(&newProcessCreateTime, shm_ptr->clock_info, rand() % 2000000000);

            //log message 
            fprintf(fp, "%d: ID%d generated; time: %d:%d\n", line_counter, process_table_reservation, shm_ptr->clock_info.seconds, shm_ptr->clock_info.nano_seconds);
            ++line_counter;
            }
        }
        golden_boy = process_selection(); //returns the process id of process exiting the queue 
        
        if(golden_boy != -1){
    
            mail_the_message(golden_boy); //master sends the id of the process currently scheduled 
            fprintf(fp, "%d: OSS: ID%d  placed into queue: %d System_clock_t: %d:%d \n", line_counter, golden_boy, shm_ptr->process_control_block[process_table_reservation].priority, shm_ptr->clock_info.seconds, shm_ptr->clock_info.nano_seconds);
            ++line_counter;

            //return address to master 
            receive_the_message(MASTER_PROCESS_ADDRESS);
                shm_ptr->process_control_block[process_table_reservation].process_arrives.seconds = shm_ptr->clock_info.seconds;
                shm_ptr->process_control_block[process_table_reservation].process_arrives.nano_seconds = shm_ptr->clock_info.nano_seconds;
            }
        }

    //no cleanup -  signal handler 
    return 0; 
}


