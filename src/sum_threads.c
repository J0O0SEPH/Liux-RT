#define _GNU_SOURCE
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <sched.h>
#include <unistd.h>

#define CPUAFFINITY    3
#define THREADS_COUNT  128
#define REQUIREDPOLICY SCHED_FIFO

typedef struct
{
    unsigned int thread_no;
}thread_args_t;

pthread_t threads[THREADS_COUNT];                     // Array of pthread type used to create desired number of executing threads
thread_args_t threads_arg[THREADS_COUNT];             // Array of structs each to hold corresponding thread id
pthread_attr_t fifoAttributes;                        // Attribute variable to hold, affinity, priority, and policy

void* threadRoutine(void *threadAttributes)
{
    int sum = 0;          //local variable saving summation result

    /* cast input argument to its original type */
    thread_args_t *thrdAttrib = (thread_args_t*) threadAttributes;

    for(unsigned int index = 0; index <= thrdAttrib->thread_no; index++)
    {
        sum+=index;
    }

    /* generate a statement from spawned thread */
    syslog(LOG_CRIT,"Thread idx=%d, sum[0...%d]=%d, Running on core : %d\n", thrdAttrib->thread_no, thrdAttrib->thread_no, sum, sched_getcpu());

    pthread_exit(NULL);
}

void prepareAttributes()
{
    cpu_set_t cpuAffin;                 // Maskable varialbel holding desired affinity
    CPU_ZERO(&cpuAffin);                // Clear all garbage masks
    CPU_SET(CPUAFFINITY, &cpuAffin);    // Set desired affinity
    struct sched_param priorityStruct;  // Declare sched_param to hold desired priority

    priorityStruct.sched_priority = sched_get_priority_max(REQUIREDPOLICY);  // Fetch maximum priority for specified scheduling policy

    /* Initialize, set inheritance, affinity, scheduling policy, and priority to thread attribute in question */
    pthread_attr_init(&fifoAttributes);
    pthread_attr_setinheritsched(&fifoAttributes, PTHREAD_EXPLICIT_SCHED);
    pthread_attr_setaffinity_np(&fifoAttributes,  sizeof(cpu_set_t), &cpuAffin);
    pthread_attr_setschedpolicy(&fifoAttributes,  REQUIREDPOLICY);
    pthread_attr_setschedparam(&fifoAttributes,   &priorityStruct);
}

void* dispatcherThread(void* arg)
{
    /* Create desired number of threads within a loop with specified thread routine */
    for(int iThread = 0; iThread < THREADS_COUNT; iThread++)
    {
        threads_arg[iThread].thread_no = iThread;                  // assign thread number to thread attribute

        int rc = pthread_create(&threads[iThread],                 // pointer to thread descriptor
                                &fifoAttributes,                    // use default attributes
                                threadRoutine,                     // thread function entry point
                                (void*)&threads_arg[iThread]);     // parameters to pass in


        /* if failed, print error and exit */
        if(rc != 0)
        {
            perror("creating thread faild!\n");
            exit(-iThread);
        }
    }

    /* wait on created threads to join dispatching thread */
    for(unsigned int iThread = 0; iThread < THREADS_COUNT; iThread++)
    {   
        pthread_join(threads[iThread], NULL);
    }

    /* exit dispatcher thread */
    pthread_exit(NULL);
}

int main(int argc,char **argv)
{
    pthread_t dispatcherThrd;

    /* prepend string to every log message */
    openlog ("[COURSE:1][ASSIGNMENT:4]", LOG_NDELAY, LOG_DAEMON); 

    prepareAttributes();  // Prepare threads attributes, same for dispatcher and worker threads

    pthread_create(&dispatcherThrd, &fifoAttributes, dispatcherThread, NULL);

    pthread_join(dispatcherThrd, NULL);  // Wait dispatcher thread to join main thread

    printf("TEST COMPLETE\n");

    /* exit main thread */
    pthread_exit(NULL);
}