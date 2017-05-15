#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/msg.h>
#include <signal.h>
#include <sys/shm.h>
#include <string.h>
#include "proj5.h"
// // #include <math.h>

// /*getopt() flags:*/
static int h_flag = 0;
static char* fileName = "log_file.txt"; //the filename in -l filename; declaring char* fileName character pointer as a const string affords us the priviledge of not having to end our string with a NULL terminator '\0'
static int computationShotClock = 20; //the z in -t z; is the time in seconds when the master will terminate itself and all children
// /*getopt() flags:*/
static int array_pos = 1; //called in main while(1) for makeUserProcesses(array_pos)
static int message_queue_id;
int msgsnd_result; //holds the result of msgsnd(); 0 on success and -1 on fail()
ssize_t msgrcv_result; //holds the result of msgrcv(); n bytes on sucess and ssize_t - 1 on fail
int shmid; //holds the shared memory segment id - - #virtual_SHMSnametag
shared_memory_object_t* shm_ptr; //pointer to starting address in shared memory of the shared memory structure (see proj4.h)

system_clock_t newProcessCreateTime; //a clock to use as a differentiator between the logical clock that is implemented to simulate the system clock of OSS, this newProcessCreateTime clock is meant to manage a seperate time table from system time, it is meant to measure time from the perspective of the process' lifetime, not the system's lifetime
static int terminated_processes = 0;

//FILE* fp; //global FILE* pointer, fp, will be used to assign the address of a stream file descriptor segment in memory that controls the i/o for this write operation
//int line_counter = 1; //this counts every line that is printed, so we can monitor our log's total line count (want to keep it at a max of 10,000 lines)

static pid_t pid_array[MAX_PROCESS_NUM + 1]; //stores the pid_t of each child in a pid_t array; 1's based
static int process_count = 0;

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

//function that spawns/generates/forks slave/user/child processes; takes the position of the first open position in the process table and spawns a user process that is assigned that same position value in our array of pid_t(s) (array of spawned user processes)
void makeUserProcesses(int pid_array_position) {
    char slave_id[3]; //char array (via the sprintf below) #process_nametag
    int i; //iterate for-loop
    
    //Fork (loop) ~ generate/spawn/fork a new user/slave/child process
    for (i = 1; i < MAX_PROCESS_NUM + 1; i++) {
        pid_array[pid_array_position] = fork();

        //slave/child/user process code - - remember that fork() returns twice!
        if (pid_array[pid_array_position] < 0) {
            //fork failed
            perror("Fork failed");
            exit(errno);
        }
        //this catches the child process that is executing after fork because a child process has a self-referencing ID of 0 where as the Master process holds the child process's actual pid_t
        else if (pid_array[pid_array_position] == 0) {
        shm_ptr->system_processes[pid_array_position].pid_literal = pid_array[pid_array_position];
        shm_ptr->system_processes[pid_array_position].process_num_logical = i;
        ++process_count;
            sprintf(slave_id, "%d", pid_array_position); 
            execl("./user", "user", slave_id, NULL); //replaces the memory copied to each process after fork to start over with nothing
            perror("Child failed to execl slave exe");
            printf("THIS SHOULD NEVER EXECUTE\n");
        }
    }
}

//master.c signal handler for master process
void signalCallback (int signum) {
    int i, status;
    if (signum == SIGINT)
        printf("\nSIGINT received by master\n");
    else
        printf("\nSIGALRM received by master\n");
   for (i = 1; i <= MAX_PROCESS_NUM + 1; i++){
        kill(pid_array[i], SIGINT);
    }
    while(waitpid(pid_array[i], &status, 0) > 0) {
        if (WIFEXITED(status))	/* process exited normally */
		printf("slave process exited with value %d\n", WEXITSTATUS(status));
    }
    detachandremove(shmid,shm_ptr); //cleans up shared memory IPC
    msgctl(message_queue_id, IPC_RMID, NULL); //cleans up message queue IPC
    printf("master done\n");
//printf("\nline count = %d\n", line_counter);
    //fclose(fp); //close the write to file method until we are ready for logging to be taken care of at the end
    exit(0);
//check ipcs after this; if there is shared memory still there then type ipcrm -h and it will show you how to remove each type (Message Queue, Shared Mem, etc...)
}

void mail_the_message(int destination_address){
printf("\nOSS.C: after mail() called");
    static int size_of_message;
    message_t message;
    message.message_address = destination_address;
    size_of_message = sizeof(message) - sizeof(message.message_address); //specifies the number of bytes in the message contents, not counting the address variable size
    //if successful, msgsnd returns 0
    //msg_result = msgsnd(message_queue_id, &message, size_of_message, 0);  //system function that is used to send execution context messages, should be called inside busy-loop before exiting OR entering a critical section; IPC tool for use between master and child/user/slave processes
    if(msgsnd_result == msgsnd(message_queue_id, &message, size_of_message, 0) < 0) {  // - newly added - //system function that is used to send execution context messages, should be called inside busy-loop before exiting OR entering a critical section; IPC tool for use between master and child/user/slave processes < 0){
        perror("Error: master: msgsnd()\n");
        exit(errno);
    }
}

void receive_the_message(int destination_address){
    static int size_of_message;
    message_t message;
    size_of_message = sizeof(message) - sizeof(long);
    if((msgrcv_result = msgrcv(message_queue_id, &message, size_of_message, destination_address, 0)) == (sizeof(ssize_t) - 1)) {
     perror("Error: master: msgrcv()\n");  
    }

printf("\nfrom_id=%d\n", message.return_address);
printf("\nOSS: RECEIVE() EXECUTES HERE #tracer\n");

clock_incrementation_function(&shm_ptr->clock_info, shm_ptr->clock_info, rand() % RANDOM_NANO_INCREMENTER + 1);

    //clock_incrementation_function(&shm_ptr->clock_info, shm_ptr->clock_info, //?); //increment logical clock again upon message receipt from user.c

    //kill the process on return from the critical section
    int i, status; //i is for-loop iterator, status is to hold the exit signal after master invokes kill on the infinitely running slave processes
    
    /*might not need any of this [FRONT BOOK END FOR CODE BLOCK IN QUESTION]*/
    for (i = 1; i <= MAX_PROCESS_NUM + 1; i++){
        kill(shm_ptr->system_processes[i].pid_literal, SIGTERM);
        ++terminated_processes; //counts processes that have been killed...useful metric?
printf("terminated_processes = %d", terminated_processes);
        while(waitpid(shm_ptr->system_processes[i].pid_literal, &status, 0) > 0) { 
        if (WIFEXITED(status))	/* process exited normally */
		printf("slave process exited with value %d\n", WEXITSTATUS(status));
        }
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
        printf("'-h' flag: This provides a help menu\n");
        printf("'-l filename' flag: This sets the name of the file that the slave processes will write into (default file name = %s).  Current name  is %s\n", fileName, fileName);
        printf("'-t z' flag: This flag determines the amount time that will pass until the master process terminates itself (default value = %d). Current value of the timer variable = %d\n", computationShotClock, computationShotClock );
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

    //master process creates and assigns shared memory segment; assigns id to shmid
    if ((shmid = shmget(SHM_KEY, sizeof(shared_memory_object_t), IPC_CREAT | 0600)) < 0) {
        perror("Error: shmget");
        exit(errno);
    }

    //master process creates a message queue; it returns a queue_id which you can use to access the message queue
    if ((message_queue_id = msgget(MESSAGE_QUEUE_KEY, IPC_CREAT | 0600)) < 0) {
        perror("Error: mssget");
        exit(errno);
    }

    //attach shared memory segment address to our pointer to a shared_memory_object_t struct; we've initialized our shared memory segment
    shm_ptr = shmat(shmid, NULL, 0);

    /* If you need more initial setup area:*/
    //master initializes shared memory variables in shared memory structure; has to be done AFTER shmat:
    shm_ptr->clock_info.seconds = 0; //must be declared and initialized to zero before any logical time is kept by the simulated system clock
    shm_ptr->clock_info.nano_seconds = 0;  //must be declared and initialized to zero before any logical time is kept by the simulated system clock

  //master initializes time (to zero to start) till next user process is to be generated; will start off at zero for the initial comparison and that is why we have initialized its seconds and nano-seconds systel_clock_t object attributes both to 0 here
    newProcessCreateTime.seconds = 0;
    newProcessCreateTime.nano_seconds = 0;

//     //open up a new file to write to for our log, assign the control of this stream to the FILE* file stream manager object pointer, fp:
//     fp = fopen(fileName, "w");

     //infinite loop won't terminate without a signal (wont terminate naturally)
    //while(1) { //may need to change this to a conditional set forth by the project 5 requirements

//         if(line_counter > LINE_MAX) {
//             fclose(fp);
//         }
    sleep(1);
        //increment the clock in oss by a random amount of time between [1,500,000,000 nanos] 
        clock_incrementation_function(&shm_ptr->clock_info, shm_ptr->clock_info, rand() % RANDOM_NANO_INCREMENTER + 1);
printf("OSS: clock_incrementer() = %d:%d", shm_ptr->clock_info.seconds, shm_ptr->clock_info.seconds);
        //if elapsed (simulated) system (logical clock) time has eclipsed the amount of time necessary to wait until next user process is generated/spawned (newProcessCreateTime) then try to spawn a user new process
        if ((shm_ptr->clock_info.seconds > newProcessCreateTime.seconds) || 
        (shm_ptr->clock_info.seconds == newProcessCreateTime.seconds && shm_ptr->clock_info.nano_seconds > newProcessCreateTime.nano_seconds)) {

printf("\nsystem clock > newClock");

            //temporary conditional: do not call makeUserProcesses(int array_pos) if there are more processes than allowed in the system currently
            if (process_count <= MAX_PROCESS_NUM) { //should MAX_PROCESS_NUM have a + 1 after it?  not sure right now come back to this later
printf("\nINprocess_count <= MAX_PROCESS_NUM");
                makeUserProcesses(array_pos);
            }
printf("\nJUSTOUTprocess_count <= MAX_PROCESS_NUM");
            //Determine next time to generate a process...after a random amount of time between [1 and 500million nano-seconds] has passed
            //this works because we are adding a random amount of time to another copy of the logical clock (system clock simulator) with all of the same information as the current state of that clock except for that it will be given a potentially much larger (between 1 and 500 million nanoseconds) amount of additional time, as the loop continues to loop the next process won't be able to be generated (because of an if-conditional) that won't allow the code to advance until enough extra context switch time has been incremented to our original logical clock that it has a greater value than the new copy of that logical clock meaning that the amount of time on average that should pass before oss generates a new process has passed and a new process should be generated...at that point the if-conditional will be satisfied and a new process will be generated'
            clock_incrementation_function(&newProcessCreateTime, shm_ptr->clock_info, rand() % RANDOM_FORK_TIME_BOUND + 1);
printf("\nCLOCK JUST INCREMENTED RANDOMLY");


    int i;
    for (i = 1; i <= MAX_PROCESS_NUM + 1; i++) {
        //if (shm_ptr->system_processes[i].process_num_logical > 0 && shm_ptr->system_processes[i].process_num_logical <= MAX_PROCESS_NUM) {
            sleep(1);
            //USER: update the clock in the user.c critical section
            //after obtaining access to the message queue with msgget, a program inserts messages into the message queue with msgsnd(...) (which is in the mail_the_message ud-type)
            mail_the_message(shm_ptr->system_processes[i].process_num_logical); //master/OSS sends the logical id of the process that is currently allowed into the crtical section 
printf("\nOSS.C: after mail() called");
            //OSS: update the clock in the oss.c receive()
            receive_the_message(400); //user/child/slave process has given up the critical section back to the OS (oss.c)
printf("\nOSS.C: after receive() called");
            sleep(1);     
        }


        }

    //} while loop

    printf("OSS.c is done running...\n");
    //Cleanup
    detachandremove(shmid,shm_ptr); 
    msgctl(message_queue_id, IPC_RMID, NULL);
    free(pid_array);
    /**might not need any of this [FRONT BOOK END FOR CODE BLOCK IN QUESTION]*/
     

    return 0; 

}


