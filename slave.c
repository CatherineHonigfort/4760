//References used: UNIX Programming Textbook AND several youtube videos (can provide list and urls if necessary)

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <sys/shm.h>

char slave_id[3];

int sharedmemid_sharedNum; //Shared memory segment id #global
int* sharedNum; //pointer to shared memory of #global
key_t key_sharedNum = 9823; //shared memory segment

int sharedmemid_sharedGlobal; //holds the shared memory segment id #global
int* sharedGlobal; //pointer of shared int variables #global
key_t key_sharedGlobal = 9824; //shared memory segment

//slave process handler
void signalCallback (int signum)
{
    printf("\nSIGTERM from slave %d\n", atoi(slave_id));

    // Cleanup the shared memory allocated
    shmdt(sharedNum);
    shmdt(sharedGlobal);
    exit(0);
}

int main(int argc, char* argv[])
{

    strcpy(slave_id, argv[1]);
    if (signal(SIGTERM, signalCallback) == SIG_ERR) {
        perror("Error: slave: signal().\n");
        exit(errno);
    }

    //master assigns shared memory segment
    if ((sharedmemid_sharedNum = shmget(key_sharedNum, sizeof(int), 0600)) < 0) {
        perror("Error: sharedmem_get");
        exit(errno);
    }

    //attach the shared memory
    sharedNum = shmat(sharedmemid_sharedNum, NULL, 0);

    //master process assigns second shared memory segment
    if ((sharedmemid_sharedGlobal = shmget(key_sharedGlobal, sizeof(int) * (*sharedNum +1), IPC_CREAT | 0600)) < 0) {
        perror("Error: sharedmem_get");
        exit(errno);
    }

    //attach shared memory to sharedGlobal
    sharedGlobal = shmat(sharedmemid_sharedGlobal, NULL, 0);

    int i;
    for(i = 0; i <= *sharedNum; i++) {
        sharedGlobal[i] = i;
    }

    printf("%s %d array size = %d\n\tarray value = %d\n",argv[0], atoi(slave_id), *sharedNum, sharedGlobal[atoi(slave_id)]);

    sleep(3);

    //Clean up shared memory
    shmdt(sharedNum);
    shmdt(sharedGlobal);
    return 0;
}
