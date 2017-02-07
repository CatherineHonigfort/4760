#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include "4760P1_logs.h"
#include <math.h>

//Creating a node structure
//**reference used: Unix Systems Programming (2.6, 2.7)
typedef struct list_struct {
    data_t item; //variable to hole the message object
    struct list_struct* next; //points to the next node in the linked list
} log_t;


static log_t* headptr = NULL; //head, this is the first node of the linked list 
static log_t* tailptr = NULL; //list tail, this is the very last node in the linked list 
static int n_flag = 0; // start with n flag at 0
static int l_flag = 0; // start with the l flag at 0


// This determines what addmsg [add message] will return. 
// Addmsg will return a -1 if unsuccessful, and a 0 if it is successful.
// **reference used: Unix Systems Programming (2.11, 2.12, 2.13) 
int addmsg(data_t data) {
    log_t* newNode;
    size_t nodesize;

// this will add one to the null terminator in data_t
    nodesize = sizeof(log_t) + strlen(data.string) + 1; 
//if it can't add the new node, it will return -1, which throws an error
    if ((newNode = (log_t*)(malloc(nodesize))) == NULL) {
        perror("This memory request is not able to be completed.\n");
        return(-1);
    } 


//**reference used: Unix Systems Programming (Program 2.7)
//newNode book keeping [i.e. head pointer, tail pointer, strings, etc]
  newNode->item.time = data.time;
    newNode->item.string = (char*)newNode + sizeof(log_t);
    strcpy(newNode->item.string, data.string);
    newNode->next = NULL; 
    if(headptr == NULL)
        headptr = newNode; //create new head pointer if there wasn't already one
    else
        tailptr->next = newNode; //resets (or sets) the tail pointer to the last node in the list 
    tailptr = newNode; //newest last node in the list 
    return 0; 
}

void clearlog(void);
// this releases all of the dynamically allocated memeory that was being used for logs  

//this is the static counter, initialized at 0
static char* wholeLog = NULL;

//This is the first log messsage
//**reference used: Unix Systems Programming (2.7)
wholeLog = (char*) malloc(sizeof(headptr->item.string)+1);

    log_t* nextNode = headptr;

// as long as the next node is not empty, the entire log needs to be reallocated to include the new data
while (nextNode != NULL) {
    wholeLog = reallocate(wholeLog, sizeof(nextNode->item.string)+1);
    strncat(wholeLog,nextNode->item.string, strlen(nextNode->item.string));
    //increment to next node in the linked list 
    nextNode = nextNode->next;
    }
    return wholeLog;
}

//**reference used: Unix System Programming (2.11)
// this will save the logged messages to the disk file. savelog will be 0 if it is successful, and -1 if it is not successful 
int savelog(char* filename) {
    return 0;
}

//starts the main program that takes in the number of arguments, and a string/pointer to a character
//**reference used: Unix Systems Program (2.12)
int main(int argc, char* argv[]) {
    int g_return; // variable holds getopt() return 
    int x = 42; //variable holds default for command line parameter variable 
    char* logFileName = "logfile.txt"; //output the log info into a text file
   
//**reference used: Youtube videos, quite a few of them, can provide them if needed
    while ( (g_return = getopt(argc, argv, "hn:l:")) != -1){

        switch(g_return) {
            case 'h':
                printf("-h flag - command line argument options.\n"); //test info - delete before turn in
                printf("-n flag - command line parameters to set a\nvariable to x.\n"); //test info - delete before turn in
                printf("-l flag set the name of logfile to what's stored in logFileName.\n"); //test info - delete before turn in
                break;
            case 'n':
                x = atoi(optarg);
                printf("X = %d when it is in the switch function\n", x);  //test info - delete before turn in
                n_flag = 1;
                break;
            case 'l':
                logfileName = optarg;
                printf("The value of the logfileName variable is now %s inside the switch function\n", logfileName); //test info - delete before turn in
                l_flag = 1;
                break;

            case '?':
                if (optopt == 'n' || optopt == 'l') {
                    fprintf(stderr, "Option -%c needs an argument!\n", optopt);  
                }
                else
                    fprintf(stderr, "Unkown option -%c.\n", optopt); 

                break;
            }
        }
    printf("The value of x inside main() is %d\n", x); //test info - check -n + x option -  delete before turn in
    printf("The value of the logfileName variable inside main() is %s\n", logFileName); //test info - delete before turn in

//Log object and string output 
    data_t newMessage; // creates new log message object
    time(&newMessage.time); //initialize the .time of the log message data_t object
    printf("The time in seconds is %ld\n", newMessage.time);//test info - delete before turn in


    if (n_flag == 0 && l_flag == 0){
        printf("%s: time:%ld Error: -n flag was not used [ x = 42]; -l flag not used (log file = logfile.txt)\n", argv[0], newMessage.time);
    }
    else if (n_flag == 1 && l_flag == 1){
        printf("%s: time:%ld Error: -n flag changes x to x=%d and -l flag changes log file name to %s\n", argv[0], newMessage.time, x, logFileName);
    }
    else if (n_flag == 0 && l_flag == 1){
        printf("%s: time:%ld Error: -n flag was not used; -l flag changes log file name to %s\n", argv[0], newMessage.time, logFileName);
    }
    else if (n_flag == 1 && l_flag == 0){
        printf("%s: time:%ld Error: -n flag changes x to x=%d; -l flag not used (log file = logfile.txt)\n", argv[0], newMessage.time, x);
    }

    newMessage.string = "Output #1\n";

    //testing information - delete before turn in
    printf("data_t object test:  newMessage.string = %s\n", newMessage.string); //testing creation of a log message
    printf("data_t object test:  newMessage.time = %ld\n", newMessage.time); //testing creation of a log message
    // end testing section, delete above

//error handling
    if (addmsg(newMessage) == -1) {
        perror("Memory request can't be made.\n");
        exit(1);
    }

//LOG OBJECT AND STRING OUTPUT MESSAGE
    data_t newMessage2; //new log message object; this will eventually be passed to the addmsg() function
    time(&newMessage2.time); //initialize the .time member variable of log message data_t type structure object
    printf("time in seconds = %ld\n", newMessage2.time);//test data


    newMessage2.string = "output #2..\n";

    //*testing*//
    printf("data_t object test:  newMessage.string = %s\n", newMessage2.string); //testing creation of a log message
    printf("data_t object test:  newMessage.time = %ld\n", newMessage2.time); //testing creation of a log message
    //*testing*//


    if (addmsg(newMessage2) == -1) {
        perror("Memory request is unable to be made.\n");
        exit(1);
    }

//Log object and string output 
data_t newMessage3; //new log message object
time(&newMessage3.time); //initialize the .time variable of log message data_t object
printf("The time in seconds = %ld\n", newMessage3.time);//test info -  delete before turn in 

newMessage3.string = "Output #3..\n";

    //*testing info - delete before turn in
    printf("data_t object test:  newMessage.string = %s\n", newMessage3.string); //creates log message
    printf("data_t object test:  newMessage.time = %ld\n", newMessage3.time); //creates a log message
    //*testing - delete above 

//error handling
    if (addmsg(newMessage3) == -1) {
        perror("Memory request is unable to be made.\n");
        exit(1);
    }

    printf("\nbreakpoint\n");

    //testing getlog()
    char* printLog = getlog();
    printf("%s\n", printLog);
    return 0;
}

//**references utilized: lecture notes, Unix Systems Programming book, Operating Systems book, several youtube videos (I can provide these if you need a complete list of resources utlized)