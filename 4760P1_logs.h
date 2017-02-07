#include <time.h>

//**reference used: Unix Systems Programming (2.11)
typedef struct list_struct {
    data_t item; 
    struct list_struct* next;
} log_t;

int addmsg(data_t data);
void clearlog(void);
char *getlog(void);
int savelog(char *logname);