#define _GNU_SOURCE
#include <syslog.h>
#include <pthread.h>
#include <time.h>
#include <sched.h>
#include <semaphore.h>
#include <signal.h>
#include <unistd.h>
#include <stdio.h>

#define _10_MSEC      10000000
#define SERVICESNO    3
#define AFFINITY_MAIN 1
#define AFFINITY_THRD 2
#define CLOCK CLOCK_REALTIME


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
    unsigned        threadID;
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

void   Sequencer();
void*  threadLoad(void* threadID);
double realtime(struct timespec *tsptr);
void   prepareAttributes(pthread_attr_t* pthrdAttr, unsigned prio);


int main(int argc, char** argv)
{
    /* Assing affinity for main thread */
    cpu_set_t affinity;
    CPU_ZERO(&affinity);
    CPU_SET(AFFINITY_MAIN, &affinity);
    struct itimerspec timerStruct;
    sched_setaffinity(getpid(), sizeof(cpu_set_t), &affinity);

    /* Get and process starting time */
    clock_gettime(CLOCK, &timeContainer); startRT = realtime(&timeContainer);

    /*=============== Preparea and create services ====================================*/
    for(int i = 0; i < SERVICESNO; i++)
    {
        unsigned prio = sched_get_priority_max(SCHED_FIFO) - i - 1;
        prepareAttributes(&services[i].threadAttributes, prio);

        services[i].threadID = i;
        sem_init(&services[i].binSem, 0, 0);
        services[i].serviceFunc = threadLoad;

        int rc = pthread_create(&services[i].threadHandler, 
                        &services[i].threadAttributes, 
                        services[i].serviceFunc, 
                        (void*)&services[i].threadID);
    }

    /*=============== Preparea and create timer ========================================*/
    timerStruct.it_interval.tv_sec = 0;
    timerStruct.it_interval.tv_nsec = _10_MSEC;
    timerStruct.it_value.tv_sec = 0;
    timerStruct.it_value.tv_nsec = _10_MSEC;

    signal(SIGALRM, (void(*)()) Sequencer);
    timer_create(CLOCK, NULL, &timerID);
    timer_settime(timerID, 0, &timerStruct, NULL);

    /*=============== Join threads and destroy attributes ==============================*/
    for(int i = 0; i < SERVICESNO; i++)
    {
        pthread_join(services[i].threadHandler, NULL);
        pthread_attr_destroy(&services[i].threadAttributes);
    }

    pthread_exit(NULL);
}


void Sequencer()
{
    static int seqCnt = 0;
    struct itimerspec disArmTimer;

    seqCnt++;

    if(seqCnt % 2 == 0)sem_post(&services[0].binSem);

    if(seqCnt % 10 == 0)sem_post(&services[1].binSem);

    if(seqCnt % 15 == 0)sem_post(&services[2].binSem);

    /* 
        When exceeded execution time, 
        reset the timer halting signal invokation, 
        give the semaphore for services to quit
    */
    if(seqCnt >= runIterations)
    {
        quit = 1;
        disArmTimer.it_value.tv_sec = 0;
        disArmTimer.it_value.tv_nsec = 0;
        disArmTimer.it_interval.tv_sec = 0;
        disArmTimer.it_interval.tv_nsec = 0;
        timer_settime(timerID, 0, &disArmTimer, NULL);


        for(int i = 0; i < SERVICESNO; i++)
        {
            sem_post(&services[i].binSem);
        }

        syslog(LOG_CRIT, "Disabling sequencer interval timer with quit=%d\n", seqCnt);
    }
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
    clock_gettime(CLOCK, &localTime); currentRT = realtime(&localTime);
    syslog(LOG_CRIT, "Service NO: %d created @ sec=%6.9lf\n", myID+1, currentRT-startRT);

    while(!quit)
    {
        /* wait semaphore signal, when received decrement and execute one loop iteration */
        sem_wait(&services[myID].binSem);
        counter++;

        clock_gettime(CLOCK, &localTime); currentRT=realtime(&localTime);
        syslog(LOG_CRIT, "Service NO: %d on core %d for release %d @ sec=%6.9lf\n", myID+1, sched_getcpu(), counter, currentRT-startRT);
    }

    pthread_exit(NULL);
}

/* Convert clock time from seconds and nano-seconds into fractional seconds */
double realtime(struct timespec *tsptr)
{
    return ((double)(tsptr->tv_sec) + (((double)tsptr->tv_nsec)/1000000000.0));
}
