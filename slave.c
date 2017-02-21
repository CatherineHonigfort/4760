

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <sys/shm.h>

char slave_id[3];

int shmid_sharedNum; //holds the shared memory segment id #global
int* sharedNum; //pointer to starting address in shared memory of the shared int variable #global
key_t key_sharedNum = 9823; //a name for your shared memory segment

int shmid_sharedNum2; //holds the shared memory segment id #global
int* sharedNum2; //pointer to starting address in shared memory of an array of shared int variables #global
key_t key_sharedNum2 = 9824; //a name for your shared memory segment

//slave.c signal handler for slave processes
void signalCallback (int signum)
{
    printf("\nSIGTERM received by slave %d\n", atoi(slave_id));

    // Cleanup
    shmdt(sharedNum);
    shmdt(sharedNum2);
    exit(0);
}

int main(int argc, char* argv[])
{

    strcpy(slave_id, argv[1]);
    if (signal(SIGTERM, signalCallback) == SIG_ERR) {
        perror("Error: slave: signal().\n");
        exit(errno);
    }

    //master process creates and assigns shared memory segment; assigns id to shmid_sharedNum
    if ((shmid_sharedNum = shmget(key_sharedNum, sizeof(int), 0600)) < 0) {
        perror("Error: shmget");
        exit(errno);
    }

    //attach shared memory
    sharedNum = shmat(shmid_sharedNum, NULL, 0);

    //master process creates and assigns a second shared memory segment; assigns id to shmid_sharedNum2
    if ((shmid_sharedNum2 = shmget(key_sharedNum2, sizeof(int) * (*sharedNum +1), IPC_CREAT | 0600)) < 0) {
        perror("Error: shmget");
        exit(errno);
    }

    //attach shared memory to sharedNum2
    sharedNum2 = shmat(shmid_sharedNum2, NULL, 0);

    int i;
    for(i = 0; i <= *sharedNum; i++) {
        sharedNum2[i] = i;
    }

    printf("%s %d array size = %d\n\tarray value = %d\n",argv[0], atoi(slave_id), *sharedNum, sharedNum2[atoi(slave_id)]);

    sleep(3);

    //Clean up
    shmdt(sharedNum);
    shmdt(sharedNum2);
    return 0;
}