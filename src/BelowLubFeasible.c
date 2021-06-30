#define _GNU_SOURCE
#include <syslog.h>
#include <pthread.h>
#include <time.h>
#include <sched.h>
#include <semaphore.h>
#include <signal.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#define SEC_TO_USEC   1000000
#define SERVICESNO    3
#define AFFINITY_MAIN 1
#define AFFINITY_THRD 2
#define CLOCK CLOCK_REALTIME
#define LCM 30


#define FIB_LIMIT (10)
#define FIB_TEST_CYCLES (100)

#define FIB_TEST(seqCnt, iterCnt)      \
   for(int idx=0; idx < iterCnt; idx++)\
   {                                   \
      int fib0=0, fib1=1, jdx=1;       \
      int fib = fib0 + fib1;           \
      while(jdx < seqCnt)              \
      {                                \
         fib0 = fib1;                  \
         fib1 = fib;                   \
         fib = fib0 + fib1;            \
         jdx++;                        \
      }                                \
   }                                   \


/* Uncomment to switch syslog to printf */
// #define syslog(LOG_CRIT, ...) printf(__VA_ARGS__)

int quit = 0;
int runIterations = 30;
static timer_t timerID;
double startRT = 0;
struct timespec timeContainer;


/* 
    Struct holding parameters needed for each service.
*/
typedef struct
{
    sem_t           binSem;
    unsigned        threadParam[2];  // #0 thread ID, #1 thread capacity
    pthread_t       threadHandler;
    pthread_attr_t  threadAttributes;

    /*
        Function pointer to be assinged the desired service function,
        to be extended with function table when required different services.
    */
    void* (*serviceFunc) (void*);
}service_t;

/* array of parameters struct, one for each service */
service_t services[SERVICESNO];

void   fib_10ms(void);
void*  Sequencer(void* seqArg);
void*  threadLoad(void* threadID);
double realtime_ms(struct timespec *tsptr);
void   prepareAttributes(pthread_attr_t* pthrdAttr, unsigned prio);


int main(int argc, char** argv)
{
    /* Assing affinity for main thread */
    cpu_set_t affinity;
    CPU_ZERO(&affinity);
    CPU_SET(AFFINITY_MAIN, &affinity);
    struct itimerspec timerStruct;
    pthread_t seqThread;
    pthread_attr_t seqAttr;
    unsigned int seqArg = LCM;

   /* prepend string to every log message */
    system("echo > /dev/null | sudo tee /var/log/syslog && logger [COURSE:2][ASSIGNMENT:1]: `uname -a`");
    openlog("[COURSE:2][ASSIGNMENT:1]", LOG_NDELAY, LOG_USER);

    sched_setaffinity(getpid(), sizeof(cpu_set_t), &affinity);

    /* Get and process starting time */
    clock_gettime(CLOCK, &timeContainer); startRT = realtime_ms(&timeContainer);

    /*=============== Preparea and create services ====================================*/

    services[0].threadParam[1] = 1;
    services[1].threadParam[1] = 1;
    services[2].threadParam[1] = 2;

    for(int i = 0; i < SERVICESNO; i++)
    {
        unsigned prio = sched_get_priority_max(SCHED_FIFO) - i - 1;
        prepareAttributes(&services[i].threadAttributes, prio);

        services[i].threadParam[0] = i;
        sem_init(&services[i].binSem, 0, 0);
        services[i].serviceFunc = threadLoad;

        int rc = pthread_create(&services[i].threadHandler, 
                        &services[i].threadAttributes, 
                        services[i].serviceFunc, 
                        (void*)services[i].threadParam);

        if(rc)exit(-i);
    }

    prepareAttributes(&seqAttr, sched_get_priority_max(SCHED_FIFO));

    int rc = pthread_create(&seqThread, 
                    &seqAttr, 
                    Sequencer, 
                    &seqArg);

    if(rc)exit(-99);


    /*=============== Join threads and destroy attributes ==============================*/
    for(int i = 0; i < SERVICESNO; i++)
    {
        pthread_join(services[i].threadHandler, NULL);
        pthread_attr_destroy(&services[i].threadAttributes);
    }
    pthread_join(seqThread, NULL);

    pthread_exit(NULL);
}


void* Sequencer(void* seqArg)
{
    static int seqCnt = 0;
    struct itimerspec disArmTimer;
    unsigned int* lcm = seqArg;
    
    while(seqCnt < *lcm)
    {
        usleep(SEC_TO_USEC/100); //sleep 10 millisecond

        if(seqCnt % 2 == 0)sem_post(&services[0].binSem);

        if(seqCnt % 10 == 0)sem_post(&services[1].binSem);

        if(seqCnt % 15 == 0)sem_post(&services[2].binSem);

        seqCnt++;
    }

    quit = 1;
    for(int i = 0; i < SERVICESNO; i++)
    {
        sem_post(&services[i].binSem);
    }

    // syslog(LOG_CRIT, "Disabling sequencer interval timer with quit=%d\n", seqCnt);
}


void prepareAttributes(pthread_attr_t* pthrdAttr, unsigned prio)
{
    cpu_set_t affinity;
    CPU_ZERO(&affinity);
    CPU_SET(AFFINITY_THRD, &affinity);
    struct sched_param priority;
    priority.sched_priority = prio;

    /* 
        Initialize, set inheritance, affinity, scheduling policy, 
        and priority to thread attribute in question 
    */
    pthread_attr_init(pthrdAttr);
    pthread_attr_setdetachstate(pthrdAttr, PTHREAD_CREATE_JOINABLE);
    pthread_attr_setaffinity_np(pthrdAttr, sizeof(cpu_set_t), &affinity);
    pthread_attr_setinheritsched(pthrdAttr, PTHREAD_EXPLICIT_SCHED);
    pthread_attr_setschedpolicy(pthrdAttr, SCHED_FIFO);
    pthread_attr_setschedparam(pthrdAttr, &priority);
}


void* threadLoad(void* threadID)
{
    int counter = 0;
    unsigned myID = *(unsigned*)threadID;
    struct timespec localTime;
    double currentRT = 0;

    /* Get current time and calulate the duration since starting time in fractional seconds */
    clock_gettime(CLOCK, &localTime); currentRT = realtime_ms(&localTime);
    syslog(LOG_CRIT, "Service NO: %d created @ sec=%6.9lf\n", myID+1, currentRT-startRT);

    while(!quit)
    {
        if(!quit)
        {
            /* wait semaphore signal, when received decrement and execute one loop iteration */
            sem_wait(&services[myID].binSem);
            counter++;

            clock_gettime(CLOCK, &localTime); currentRT=realtime_ms(&localTime);
            syslog(LOG_CRIT, "Thread %d start %d @ %lf on core %d\n", myID+1, counter, currentRT-startRT, sched_getcpu());

            for(unsigned i = 0; i < *((unsigned*)threadID + 1); i++)
            {
                fib_10ms();
            }
        }
    }

    pthread_exit(NULL);
}


/* Convert clock time from seconds and nano-seconds into fractional milliseconds */
double realtime_ms(struct timespec *tsptr)
{
    return ((double)(tsptr->tv_sec * 1000) + (((double)tsptr->tv_nsec)/1000000.0));
}


void fib_10ms(void)
{
    int limit=0, release=0, cpucore, i;
    struct timespec localTime;

    clock_gettime(CLOCK, &localTime);
    double event_time = realtime_ms(&localTime);

    FIB_TEST(FIB_LIMIT, FIB_TEST_CYCLES);

    clock_gettime(CLOCK, &localTime);
    double run_time = realtime_ms(&localTime) - event_time;
    

    unsigned int required_test_cycles = (int)(10.0/run_time);

    do
    {
        FIB_TEST(FIB_LIMIT, FIB_TEST_CYCLES);
    }
    while(limit++ < required_test_cycles);

}