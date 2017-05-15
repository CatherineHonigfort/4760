//Just in case you check my github before you see my email, I just wanted to let you know that as soon as I get home I will load these up on the hoare server. (I'm updating the github instead of just uploading them now because the compouter with the files on it is on the other side of campus, but I had time to sprint to the lab and update this before my finals) I'm so sorry, I didn't see your email, and didn't realize that I had access to the right server so I tested my projects on my SLU server (hopper.slu.edu) and submitted them on github. I'm sorry!
#pragma once
#define SHM_KEY 9823
#define MESSAGE_QUEUE_KEY 3318

typedef struct shared_clock {
    int seconds; //sharedmemory
    int nano_seconds; //sharememory
} shared_clock_t;

//this is the message queue implementation
typedef struct message {
    long message_address; //process id of the receiving process
    int completed; //process id has terminated or finished running, whichever comes first
    shared_clock_t clock_info;
    int return_address; //address of the person who sent the message (allows the master process to ID the process that sent the message)
} message_t;

typedef struct process_info {
    int process_Num; 
    pid_t actual_Num;
} process_t;
