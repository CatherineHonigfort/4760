#include <time.h>

//**reference used: Unix Systems Programming (2.11)
////typedef struct list_struct {
//
//
//
  //  data_t item;
//      time_t time;
       char *string;
                //struct list_struct* next;
               //}
                 data_t; //log_t;
                 //int addmsg(data_t data);
                 void clearlog(void);
                 char *getlog(void);
                int savelog(char *logname);
