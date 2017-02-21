#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/shm.h>

/*getopt() flags:*/
static int h_flag = 0;
static int maxSlaveProcessesSpawned = 5; //user sets a maximum for the number of slave processes spawned with '-s x' flag where x sets maxSlaveProcessesSpawned
static char* fileName = "test.out"; //declaring char* fileName character pointer as a const string affords us the priviledge of not having to end our string with a NULL terminator '\0'
static int criticalRegionEntranceAllowance = 3; //the default number of times each slave/child process is allowed to enter its critical region to increment the sharedNum variable and write
static int computationShotClock = 20;
/*getopt() flags:*/

pid_t *slavePointer;
int shmid_sharedNum; //holds the shared memory segment id #global
int* sharedNum; //pointer to starting address in shared memory of the shared int variable #global
key_t key_sharedNum = 9823; //a name for your shared memory segment

int shmid_sharedNum2; //holds the shared memory segment id #global
int* sharedNum2; //pointer to starting address in shared memory of an array of shared int variables #global
key_t key_sharedNum2 = 9824; //a name for your shared memory segment

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

//function that spawns slave processes
void makeSlaveProcesses(int numberOfSlaves)
{
    char slave_id[3];
    int i, status;

    //Allocate space for slave process array
    slavePointer = (pid_t*) malloc(sizeof(pid_t) * maxSlaveProcessesSpawned);

    // Fork loop
    for (i = 0; i < numberOfSlaves; i++)
    {
        slavePointer[i] = fork();

        if (slavePointer[i] < 0) {
            //fork failed
            perror("Fork failed");
            exit(errno);
        }
        else if (slavePointer[i] == 0) {
            //slave (child) process code - - remember that fork() returns twice...
            sprintf(slave_id, "%d", i + 1);
            execl("./slave", "slave", slave_id, NULL);
            perror("Child failed to execl slave exe");
            printf("THIS WILL NEVER EXECUTE\n");
        }
    }

    //All slaves have been forked. Wait calling process waits (blocks/goes to sleep) for them to terminate (or change state i.e., terminate or get interrupted)
    while(wait(&status) > 0) {
        if (WIFEXITED(status))  /* process exited normally */
                printf("slave process exited with value %d\n", WEXITSTATUS(status));
        else if (WIFSIGNALED(status))   /* child exited on a signal */
                printf("slave process exited due to signal %d\n", WTERMSIG(status));
        else if (WIFSTOPPED(status))    /* child was stopped */
                printf("slave process was stopped by signal %d\n", WIFSTOPPED(status));
    }

}

//master.c signal handler for master process
void signalCallback (int signum)
{
    int i, status;

    if (signum == SIGINT)
        printf("\nSIGINT received by master\n");
    else
        printf("\nSIGALRM received by master\n");

   for (i = 0; i < maxSlaveProcessesSpawned; i++){
        kill(slavePointer[i], SIGTERM);
    }
    while(wait(&status) > 0) {
        if (WIFEXITED(status))  /* process exited normally */
                printf("slave process exited with value %d\n", WEXITSTATUS(status));
        else if (WIFSIGNALED(status))   /* child exited on a signal */
                printf("slave process exited due to signal %d\n", WTERMSIG(status));
        else if (WIFSTOPPED(status))    /* child was stopped */
                printf("slave process was stopped by signal %d\n", WIFSTOPPED(status));
    }

    //clean up program before exit (via interrupt signal)
    detachandremove(shmid_sharedNum,sharedNum);
    detachandremove(shmid_sharedNum2,sharedNum2);
    free(slavePointer);
    exit(0);
}

int main(int argc, char* argv[])
{

    //holds the return value of getopt()
    int c;
    while ( ( c = getopt(argc, argv, "hs:l:i:t:")) != -1)
    {
        switch(c) {
            case 'h':
                h_flag = 1;
                break;
            case 's':
                maxSlaveProcessesSpawned = atoi(optarg);
                break;
            case 'l':
                fileName = optarg;
                break;
            case 'i':
                criticalRegionEntranceAllowance = atoi(optarg);
                break;
            case 't':
                computationShotClock = atoi(optarg);
                break;
            case '?':
                if (optopt == 's' || optopt == 'l' || optopt == 'i' || optopt == 't')
                    fprintf(stderr, "Option %c requires an argument.\n", optopt);
                else
                    fprintf(stderr, "Unknown option -%c\n", optopt);
                return -1;
        }
    }

    if (h_flag == 1)
    {
        printf("'-h' flag: This provides a help menu\n");
        printf("'-s x' flag: This sets the number of slave processes spawned (default value = %d).  Current value = %d\n", maxSlaveProcessesSpawned,maxSlaveProcessesSpawned);
        printf("'-l filename' flag: This sets the name of the file that the slave processes will write into (default file name = %s).  Current name  is %s\n", fileName, fileName);
        printf("'-i y' flag: This sets the amount of times each slave should increment and write to the file (default value = %d). Current value = %d\n",criticalRegionEntranceAllowance, criticalRegionEntranceAllowance);
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

    //master process creates and assigns shared memory segment; assigns id to shmid_sharedNum
    if ((shmid_sharedNum = shmget(key_sharedNum, sizeof(int), IPC_CREAT | 0600)) < 0) {
        perror("Error: shmget");
        exit(errno);
    }

    //attach shared memory to sharedNum
    sharedNum = shmat(shmid_sharedNum, NULL, 0);

    //master process creates and assigns a second shared memory segment; assigns id to shmid_sharedNum2
    if ((shmid_sharedNum2 = shmget(key_sharedNum2, sizeof(int) * (maxSlaveProcessesSpawned +1), IPC_CREAT | 0600)) < 0) {
        perror("Error: shmget");
        exit(errno);
    }

    //attach shared memory to sharedNum2
    sharedNum2 = shmat(shmid_sharedNum2, NULL, 0);

    //master initializes shared memory variable to number of spawn processes to create proper array size
    *sharedNum = maxSlaveProcessesSpawned;

    int i;
    for(i = 0; i <= maxSlaveProcessesSpawned; i++) {
        sharedNum2[i] = i;
    }

    //spawn slave process
    makeSlaveProcesses(maxSlaveProcessesSpawned);

    sleep(5);
    printf("[master]: Done\n");

    //Cleanup
    detachandremove(shmid_sharedNum,sharedNum);
    detachandremove(shmid_sharedNum2,sharedNum2);
    free(slavePointer);
    return 0;
}