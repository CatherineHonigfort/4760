//References used: UNIX Programming Textbook AND several youtube videos (can provide list and urls if necessary)

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/shm.h>

//getopt() from before -- flags:
static int h_flag = 0;
static int maxSlaveSpawned = 5; //-s x flag sets maximum number of the slaves spawned
static char* fileName = "testdata.out"; //chose char because then NULL character isn't necessary
static int criticalRegionEntranceAllowance = 3;
static int computationClock = 20;



pid_t *slavePointer;
int shmem_id_sharedNum; //shared memory segment id
int* sharedNum; //pointer to starting address in shared memory of the shared int variable #global
key_t key_sharedNum = 9823; //a name for your shared memory segment

int shmem_id_sharedNum2; //holds the shared memory segment id #global
int* sharedNum2;
key_t key_sharedMemSeg = 9824;


int detachandremove (int shmid, void* shmem_addr){ //detaches the shared memory then removes the shared memory segment
    int error = 0;

    if (shmdt(shmem_addr) == - 1)
        error = errno;
    if ((shmctl(shmid, IPC_RMID, NULL) == -1) && !error)
        error = errno;
    if (!error)
        return 0;
    errno = error;
    return -1;
}


void makeSlaveProcesses(int numberOfSlaves) //spawn slaves
{
    char slave_id[3]; //3 was specified earlier
    int i, status;


    slavePointer = (pid_t*) malloc(sizeof(pid_t) * maxSlaveSpawned); //allocate slave arrays

    // This is where the Fork loop happens
    for (i = 0; i < numberOfSlaves; i++)
    {
        slavePointer[i] = fork();

        if (slavePointer[i] < 0) {
            perror("The fork failed"); // error check
            exit(errno);
        }
        else if (slavePointer[i] == 0) { // if fork doesn't fail, process
            sprintf(slave_id, "%d", i + 1);
            execl("./slave", "slave", slave_id, NULL);
            perror("The child process has failed");
            printf("Will not execute\n");
        }
    }

    //Slaves have been forked. Wait  for them to terminate
    while(wait(&status) > 0) {
        if (WIFEXITED(status)) // normal exit
                printf("slave process properly exited %d\n", WEXITSTATUS(status));
        else if (WIFSIGNALED(status)) //child exited
                printf("slave process exited by user %d\n", WTERMSIG(status));
        else if (WIFSTOPPED(status))
                printf("slave process was stopped by user %d\n", WIFSTOPPED(status));
    }

}


void signalCallback (int signum) //master process
{
    int i, status;

    if (signum == SIGINT)
        printf("\n signal was received by the master\n");
    else
        printf("\n signal alarm was received by the master\n");

   for (i = 0; i < maxSlaveSpawned; i++){
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
                maxSlaveSpawned = atoi(optarg);
                break;
            case 'l':
                fileName = optarg;
                break;
            case 'i':
                criticalRegionEntranceAllowance = atoi(optarg);
                break;
            case 't':
                computationClock = atoi(optarg);
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
        printf("'-h' flag: Help menu\n");
        printf("'-s x' flag: Sets number of slave processes spawned -- default value = %d. -- Current value = %d\n", maxSlaveSpawned,maxSlaveSpawned);
        printf("'-l filename' flag: Sets the name of file that the slave processes -- default file name = %s --  Current name  is %s\n", fileName, fileName);
        printf("'-i y' flag: Sets the amount of times each slave increments and writes to file  --default value = %d --  Current value = %d\n",criticalRegionEntranceAllowance, criticalRegionEntranceAllowance);
        printf("'-t z' flag: Amount of time until the master process terminates itself -- default value = %d == Current value of the timer variable = %d\n", computationClock, computationClock );
        exit(0);
    }

    //generate SIGINT -- Ctrl+c
    if (signal(SIGINT, signalCallback) == SIG_ERR) {
        perror("Error: master: signal(): SIGINT\n");
        exit(errno);
    }
    //generates alarm()
    if (signal(SIGALRM, signalCallback) == SIG_ERR) {
        perror("Error: slave: signal(): SIGALRM\n");
        exit(errno);
    }

    //Generates SIGALRM after computationClock seconds
    alarm(computationClock);


    if ((shmid_sharedNum = shmget(key_sharedNum, sizeof(int), IPC_CREAT | 0600)) < 0) { // master creates and assigns shared memory
        perror("Error: shmem_get");
        exit(errno);
    }

    //SharedNum attached to shared memory
    sharedNum = shmat(shmid_sharedNum, NULL, 0);

    //master process creates and assigns a second shared memory segment; assigns id to shmid_sharedNum2
    if ((shmid_sharedNum2 = shmget(key_sharedMemSeg, sizeof(int) * (maxSlaveSpawned +1), IPC_CREAT | 0600)) < 0) {
        perror("Error: shmget");
        exit(errno);
    }

    //attach shared memory to sharedNum2
    sharedNum2 = shmat(shmid_sharedNum2, NULL, 0);

    //master initializes shared memory variable to number of spawn processes to create proper array size
    *sharedNum = maxSlaveSpawned;

    int i;
    for(i = 0; i <= maxSlaveSpawned; i++) {
        sharedNum2[i] = i;
    }

    //spawn slave process
    makeSlaveProcesses(maxSlaveSpawned);

    sleep(5);
    printf("[master]: Done\n");

    //Cleanup shared memory allocated
    detachandremove(shmid_sharedNum,sharedNum);
    detachandremove(shmid_sharedNum2,sharedNum2);
    free(slavePointer);
    return 0;
}
