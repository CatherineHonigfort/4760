#include <time.h>
#include "logs.h"
#include <string.h>
//**reference used: Unix Systems Programming (2.11)
//typedef struct list_struct {
    data_t item;
        struct list_struct* next;
        } log_t;

        int addmsg(data_t data);
        void clearlog(void){
                }
                char *getlog(void){
                        return NULL;
                                }
                                int savelog(char *logname);
