#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <sys/time.h>
#include <time.h>
#include <string.h>
#include "proj4.h"


int slave_id; //name assigned to each forked process 
int shmid; //holds the shared memory segment id #global;
shared_memory_object_t* shm_ptr; //pointer to starting address in shared memory 
static int message_queue_id;


void signalCallback (int signum) {
    printf("\nSIGTERM received by user_process_id %d\n", slave_id);
    // Cleanup
    shmdt(shm_ptr);
    exit(0);
}

void mail_the_message(int destination_address) {
    static int size_of_message;
    message_t message;
    message.message_address = destination_address; //address on the outside of the envelope
    message.return_address = slave_id; //initializes the message.return_address member 
    size_of_message = sizeof(message) - sizeof(long); //specifies the number of bytes in the message contents
    msgsnd(message_queue_id, &message, size_of_message, 0);
}

void receive_the_message(int destination_address) {
    static int size_of_message;
    message_t message;
    size_of_message = sizeof(message) - sizeof(long);
    msgrcv(message_queue_id, &message, size_of_message, destination_address, 0);
}

int main(int argc, char* argv[])
{

    slave_id = atoi(argv[1]);

    if (signal(SIGINT, signalCallback) == SIG_ERR) {
        perror("Error: slave: signal().\n");
        exit(errno);
    }

    //slave process is assigned shared memory segment; returns the ID unique to this shared memory segment for correct-access assurance
    if ((shmid = shmget(SHM_KEY, sizeof(shared_memory_object_t), 0600)) < 0) {
        perror("Error: shmget");
        exit(errno);
    }

    //slave process is assigned the message queue id created by the master process
    if ((message_queue_id = msgget(MESSAGE_QUEUE_KEY, 0600)) < 0) {
        perror("Error: msgget");
        exit(errno);
    }

    //attach each process's shared memory pointer to the shared_memory_object_t struct 
    shm_ptr = shmat(shmid, NULL, 0);
   
    srand(time(NULL));

    int time_needed_to_execute_instructions = rand() % 40000000; //this determines how much time each process will need to run for to "finish" it's job



    while(1){
        receive_the_message(slave_id);

        printf("USER: PROCESS_ID_%d *** EXECUTING ON CRITICAL SECTION (HAS CPU CONTROL) @ (system_clock_time) %d:%d\n", slave_id, shm_ptr->clock_info.seconds, shm_ptr->clock_info.nano_seconds); //prints to the console which user process is currently executing on its critical section 
        
//user processes uses up the entire time quantum is a 50/50 chance
        int was_time_quantum_used_up = rand() % 2; //if 0 then yes, if 1 then no
       //Determine run/burst time 
        if (was_time_quantum_used_up == 0) {
            shm_ptr->process_control_block[slave_id].burst = shm_ptr->process_control_block[slave_id].time_quantum;
        }
        else {
            shm_ptr->process_control_block[slave_id].burst = rand() % shm_ptr->process_control_block[slave_id].time_quantum;
        }
        //Check if burst time exceeds time left:
        int execution_time_remaining = time_needed_to_execute_instructions - shm_ptr->process_control_block[slave_id].cpu_usage_time;
        if (shm_ptr->process_control_block[slave_id].burst > execution_time_remaining) {
            shm_ptr->process_control_block[slave_id].burst = execution_time_remaining;
        }
      
        //update time
        shm_ptr->process_control_block[slave_id].cpu_usage_time += shm_ptr->process_control_block[slave_id].burst;
        //Check if process should terminate
        if (shm_ptr->process_control_block[slave_id].cpu_usage_time >= time_needed_to_execute_instructions) {
            shm_ptr->process_control_block[slave_id].finished = 1;
        }

        mail_the_message(MASTER_PROCESS_ADDRESS); //sends the message via message queue 
printf("\nslave_id=%d\n", slave_id);
    }

    return 0;
}


