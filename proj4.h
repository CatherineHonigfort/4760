#pragma once
#define MAX_USER_PROCESSES 18
#define CONTEXT_SWITCH_TIME 500
#define SHM_KEY 9823
#define MESSAGE_QUEUE_KEY 3318
#define MASTER_PROCESS_ADDRESS 999
#define SetBit(A,k)     ( A[((k-1)/32)] |= (1 << ((k-1)%32)) ) 
#define ClearBit(A,k)   ( A[((k-1)/32)] &= ~(1 << ((k-1)%32)) ) 
#define TestBit(A,k)    ( A[((k-1)/32)] & (1 << ((k-1)%32)) )
#define TIME_QUANT_QUEUE_0 2000000
#define TIME_QUANT_QUEUE_1 4000000
#define TIME_QUANT_QUEUE_2 8000000
#define LINE_MAX 10000

typedef struct system_clock {
    int seconds; //#sharedmemory
    int nano_seconds; //#sharememory
} system_clock_t;

//message queue implementation
typedef struct message {
    long message_address; //process id of the receiving process
    int return_address; //address of the person who sent the message (allows the master process to ID the process that sent the message)
} message_t;

typedef struct process_control_block {
    system_clock_t process_starts; //keeps track of when the process is first generated
    system_clock_t process_arrives; //keeps track of the time when process gains control of the CPU
    int turn_around_time; //keeps track of the time it took from between when the process was created and when the process finished executing;
    int wait_time; //keeps track of the total time the process existed in the system 
    int priority; //queue number ~ 0 has the highest priority 
    int burst; //time spent executing its instructions 
    int time_quantum; //time quantum allocated to the process 
    int finished; //signifies that the process has finished executing its instructions
    int cpu_usage_time; //total time the process had controll of the CPU
} process_control_block_t; //this represents a individual process control block; 

typedef struct shared_memory_object {
    process_control_block_t process_control_block [MAX_USER_PROCESSES + 1]; 
    system_clock_t clock_info; //simulate a systems internal clock
} shared_memory_object_t; //this is the system data structure that will be placed in the shared memory (which is allocated by the OSS) to facilitate IPC

