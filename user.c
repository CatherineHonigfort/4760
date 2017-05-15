#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <sys/time.h>
#include <time.h>
#include <string.h>
#include "proj5.h"

// //Ctrl+` in VisualStudio Code to display the integrated terminal
int slave_id; //name assigned to each forked process that is execl'd into the user.c source code; how we keep track of who (which child process) is currently executing on the critical section
int shmid; //holds the shared memory segment id #global; pass this shmat(...) so shmat(...) can return the address of the shared memory segment to the pointer of the data-type being stored at that location in shared memory
ssize_t user_msgrcv_result; //holds the result of msgrcv(); returned ssize_t - 1 on fail or n bytes stored in the message buffer on success
int user_msgsnd_result; // holds the result of msgsnd(); returns 0 on success or -1 on failure
shared_memory_object_t* shm_ptr; //pointer to starting address in shared memory of the shared memory structure (see proj5.h)
static int message_queue_id;

system_clock_t resourceRequestOrReleaseTime; //a clock to use as a differentiator between the logical clock that is implemented to simulate the system clock of OSS, this newProcessCreateTime clock is meant to manage a seperate time table from system time, it is meant to measure time from the perspective of the process' lifetime, not the system's lifetime


//this function controls the logical clock (controlled by oss) that is meant to represent the system clock controlled by the operating system
void clock_incrementation_function(system_clock_t *destinationClock, system_clock_t sourceClock, int additional_nano_seconds) {
    //changes the first parameter, the system_clock_t object* (passed by reference) value 
    //takes the the second parameter, the system_clock_t object (the current logical clock time calculated thus far) to use as the current state (from a value of seconds and nanoseconds properties standpoint) to update that same clock with additional nanoseconds, which is the third parameter
    destinationClock->nano_seconds = sourceClock.nano_seconds + additional_nano_seconds;
    if(destinationClock->nano_seconds > 1000000000) {
        destinationClock->seconds++;
        destinationClock->nano_seconds -= 1000000000;
    }
    //because of pass by reference this function takes care of our logical implementation of a system clock simulator without passing data heavy copies of arguments to and from our function that needs to be run many many times #memory_efficient
}

//slave.c signal handler for slave processes
void signalCallback (int signum) {
    printf("\nSIGTERM received by user_process_id %d\n", slave_id);
    // Cleanup
    shmdt(shm_ptr);
    exit(0);
}

void mail_the_message(int destination_address) {
printf("\nUSER.C: INSIDE mail");
    static int size_of_message;
    message_t message;
    message.message_address = destination_address; //this is the address on the outside of the envelope; the destination address;
    message.return_address = slave_id; //initializes the message.return_address member so the master can detect who the message was sent from
    size_of_message = sizeof(message) - sizeof(long); //specifies the number of bytes in the message contents, not counting the address variable size
    if(user_msgsnd_result == msgsnd(message_queue_id, &message, size_of_message, 0) < 0) {  // - newly added - //system function that is used to send execution context messages, should be called inside busy-loop before exiting OR entering a critical section; IPC tool for use between master and child/user/slave processes < 0){
        perror("Error: master: msgsnd()\n");
        exit(errno);
    }
}

void receive_the_message(int destination_address) {
printf("\nUSER.C: INSIDE receive()");
    static int size_of_message;
    message_t message;
    size_of_message = sizeof(message) - sizeof(long);
    if((user_msgrcv_result = msgrcv(message_queue_id, &message, size_of_message, destination_address, 0)) == (sizeof(ssize_t) - 1)) {
     perror("Error: slave: msgrcv()\n");
    }
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

    //attach each process's shared memory pointer (a pointer to a shared_memory_object_ object) to the shared_memory_object_t struct/object placed in the shared memory segment 
    shm_ptr = shmat(shmid, NULL, 0);

    //take snap shot of system time at the start of each process for request release determination
    resourceRequestOrReleaseTime.seconds = shm_ptr->clock_info.seconds;
    resourceRequestOrReleaseTime.nano_seconds = shm_ptr->clock_info.nano_seconds;

    srand(time(NULL));

    int time_until_request_or_release = rand() % TIME_FOR_RESOURCE_ACTION; //each process, when it starts, should roll a random number between 0 and TIME_PR...param; and when it occurs it should try and either claim a new resource or release a resource that it already has

    clock_incrementation_function(&resourceRequestOrReleaseTime, resourceRequestOrReleaseTime, time_until_request_or_release);



    //while(){ while loop
  
        receive_the_message(slave_id); //received the message from the master process via the message queue implemented by IPC to alert the user/slave/child process that it has the execution contenxt 
printf("\nUSER.C: after receive() called");
        sleep(1);
        //increment the clock in oss by a random amount of time between [1 and 50000 nanos] 
        clock_incrementation_function(&shm_ptr->clock_info, shm_ptr->clock_info, rand() % RANDOM_NANO_INCREMENTER + 1);
        
        //coin flip for if the process is going to request or release the resource:
        int request_or_release = rand() % 2; //if 0 request; if 1 release
        //see page 6 of notes

        //#compare system clock vs time snapshot + time_until_request_or_release
        if ((shm_ptr->clock_info.seconds >= resourceRequestOrReleaseTime.seconds) || 
        (shm_ptr->clock_info.seconds == resourceRequestOrReleaseTime.seconds && shm_ptr->clock_info.nano_seconds > resourceRequestOrReleaseTime.nano_seconds)) {

printf("\nUSER.C: system > resourceClockTimer");
//             if(request_or_release == 0) {
// printf("\nUSER.C *critical_section*: the process has REQUESTED ?");
//                 //shm_ptr->system_processes[slave_id].resources_requested
//                 //put a request in shared memory?
//                 //it will continue looping and checking to see if it is granted that resource
//                 //function that checks over each object in the array of resource descriptor objects and checks each one's 
//             }
//             else {
// printf("\nUSER.C *critical_section*: the process has RELEASED ?");
//                 //shm_ptr->system_processes[slave_id].resources_released
//             }
        printf("\nUSER: PROCESS_ID_%d *** EXECUTING ON CRITICAL SECTION @ (system_clock_time) %d:%d\n", slave_id, shm_ptr->clock_info.seconds, shm_ptr->clock_info.nano_seconds); //prints to the console which user process is currently executing on its critical section 

        mail_the_message(400); //sends the message via message queue implemented IPC alerting the master process that the execution context is being switched back to the master process from the user process because the user process has finished executing on its critical section
printf("\nUSER.C: after mailed() called");
        }
     //} while loop

     return 0;
}


