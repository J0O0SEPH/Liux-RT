#define _GNU_SOURCE
#include <pthread.h>
#include <time.h>
#include <sched.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>


#define AFFINITY   2
#define TESTLOOP   100
#define ITERATIONS 3
#define POLICY     SCHED_FIFO
// #define THECLOCK   CLOCK_REALTIME
// #define THECLOCK   CLOCK_MONOTONIC_RAW
#define THECLOCK   CLOCK_REALTIME_COARSE

#define SLEEPING_SEC    0
#define SLEEPING_N_SEC  10000000
#define NSEC_PER_SEC    1000000000
#define ERROR (-1)
#define OK (0)

struct timespec start, end, duration, rectify, sleepingTime, error;   // Time structures to for real time execution time/duration.


int delta_t(struct timespec *stop, struct timespec *start, struct timespec *delta_t)
{
  int dt_sec=stop->tv_sec - start->tv_sec;
  int dt_nsec=stop->tv_nsec - start->tv_nsec;

  //printf("\ndt calcuation\n");

  // case 1 - less than a second of change
  if(dt_sec == 0)
  {
	  //printf("dt less than 1 second\n");

	  if(dt_nsec >= 0 && dt_nsec < NSEC_PER_SEC)
	  {
	          //printf("nanosec greater at stop than start\n")
		  delta_t->tv_sec = 0;
		  delta_t->tv_nsec = dt_nsec;
	  }

	  else if(dt_nsec > NSEC_PER_SEC)
	  {
        //printf("nanosec overflow\n");
        delta_t->tv_sec = 1;
        delta_t->tv_nsec = dt_nsec-NSEC_PER_SEC;
	  }

	  else // dt_nsec < 0 means stop is earlier than start
	  {
	         printf("stop is earlier than start\n");
		 return(ERROR);  
	  }
  }

  // case 2 - more than a second of change, check for roll-over
  else if(dt_sec > 0)
  {
	  //printf("dt more than 1 second\n");

	  if(dt_nsec >= 0 && dt_nsec < NSEC_PER_SEC)
	  {
	          //printf("nanosec greater at stop than start\n");
		  delta_t->tv_sec = dt_sec;
		  delta_t->tv_nsec = dt_nsec;
	  }

	  else if(dt_nsec > NSEC_PER_SEC)
	  {
	          //printf("nanosec overflow\n");
		  delta_t->tv_sec = delta_t->tv_sec + 1;
		  delta_t->tv_nsec = dt_nsec-NSEC_PER_SEC;
	  }

	  else // dt_nsec < 0 means roll over
	  {
	          //printf("nanosec roll over\n");
		  delta_t->tv_sec = dt_sec-1;
		  delta_t->tv_nsec = NSEC_PER_SEC + dt_nsec;
	  }
  }

  return(OK);
}

/* Helper function clearing received time structure */
void resetTimeStamp(struct timespec* clk)
{
    clk->tv_sec = clk->tv_nsec = 0;
}

void delta_time(const struct timespec* end, const struct timespec* start, struct timespec* duration)
{
    /*  
        Unify time units to nano-seconds, 
        calculate spent time in nano-seconds, 
        then Convert back to seconds, and nano-seconds
    */
    const long localStartTime = (start->tv_sec * NSEC_PER_SEC) + start->tv_nsec;
    const long localEndTime = (end->tv_sec * NSEC_PER_SEC) + end->tv_nsec;
    const long localDeltaTime = localEndTime - localStartTime;

    duration->tv_nsec = localDeltaTime % NSEC_PER_SEC;   // the remaider of delta time in nano-seconds
    duration->tv_sec = localDeltaTime / NSEC_PER_SEC;    // The quotient of delta time in seconds
}

void* sleepingThread(void* thrdArgs)
{
    struct timespec rtclk_resolution;

    if(clock_getres(THECLOCK, &rtclk_resolution))
    {
        perror("clock_getres");
        exit(-1);
    }
    else
    {
        printf("\n\nPOSIX Clock demo using system RT clock with resolution:\n\t%ld secs, %ld microsecs, %ld nanosecs\n", 
        rtclk_resolution.tv_sec, (rtclk_resolution.tv_nsec/1000), rtclk_resolution.tv_nsec);

        printf("Sleeping thread assinged affinity to core: %d\n", sched_getcpu());
    }

    /* Initialize all time structure in question with (0) */
    resetTimeStamp(&start);resetTimeStamp(&end);
    resetTimeStamp(&duration);resetTimeStamp(&rectify);
    resetTimeStamp(&sleepingTime);resetTimeStamp(&error);

    /* Set required sleeping time */
    sleepingTime.tv_sec = SLEEPING_SEC;
    sleepingTime.tv_nsec = SLEEPING_N_SEC;


    for(int i = 0; i < TESTLOOP; i++)
    {
        int retry = ITERATIONS;             // Initialize number of rectifying iterations, in case nanosleep() yield earlier that requested

        /* Execution block, sleep would be replaced by real-time application load */
        {
            clock_gettime(THECLOCK, &start);                                 // Collect time stamp before execution
            while((nanosleep(&sleepingTime, &rectify) != 0) && (retry > 0))  // In case of non-completion, re-try to sleep the remaining time
            {
                sleepingTime.tv_sec = rectify.tv_sec;
                sleepingTime.tv_nsec = rectify.tv_nsec;
                retry--;
            }
            clock_gettime(THECLOCK, &end);                                   // Collect time stamp after execution
        }

        /* Calculate spent duration of execution */
        delta_time(&end, &start, &duration);

        /* Reset required sleeping time for next iteration, and compare it with the actuall execution */
        sleepingTime.tv_sec = SLEEPING_SEC;
        sleepingTime.tv_nsec = SLEEPING_N_SEC;
        delta_time(&duration, &sleepingTime, &error);

        printf("MY_CLOCK clock DT seconds = %ld, msec=%ld, usec=%ld, nsec=%ld\n", 
                duration.tv_sec, duration.tv_nsec/1000000, duration.tv_nsec/1000, duration.tv_nsec);

        printf("MY_CLOCK delay error = %ld, nanoseconds = %ld\n", 
                error.tv_sec, error.tv_nsec);

        /*======================= Compare with original fuction =========================*/
        struct timespec original_duration = {0, 0}, original_error = {0, 0};

        delta_t(&end, &start, &original_duration);
        delta_t(&duration, &sleepingTime, &original_error);

        printf("_____Original DT seconds = %ld, msec=%ld, usec=%ld, nsec=%ld\n", 
                original_duration.tv_sec, original_duration.tv_nsec/1000000, original_duration.tv_nsec/1000, original_duration.tv_nsec);

        printf("_____Original delay error = %ld, nanoseconds = %ld\n", 
                original_error.tv_sec, original_error.tv_nsec);

        printf("\n");
        // assert((original_duration.tv_nsec == duration.tv_nsec) && (original_duration.tv_sec == duration.tv_sec));
        // assert((original_error.tv_nsec == error.tv_nsec) && (original_error.tv_sec == error.tv_sec));
    }

    pthread_exit(NULL);
}

void setSched(pthread_attr_t *thrdAttrs)
{
    struct sched_param schedPriority;                                  // Declare sched_param to hold desired priority
    schedPriority.sched_priority = sched_get_priority_max(POLICY);     // Fetch maximum priority for desired scheduling policy

    cpu_set_t cpuAffin;             // Maskable varialble holding desired affinity
    CPU_ZERO(&cpuAffin);            // Clear all garbage masks
    CPU_SET(AFFINITY, &cpuAffin);   // Set desired affinity

    /* Initialize, set inheritance, affinity, scheduling policy, and priority to thread attribute in question */
    pthread_attr_init(thrdAttrs);
    pthread_attr_setschedpolicy(thrdAttrs, POLICY);
    pthread_attr_setschedparam(thrdAttrs, &schedPriority);
    pthread_attr_setinheritsched(thrdAttrs, PTHREAD_EXPLICIT_SCHED);
    pthread_attr_setaffinity_np(thrdAttrs, sizeof(cpu_set_t), &cpuAffin);
}

int main(int argc, char** argv)
{
    pthread_t sleepthread;            // Thread handle
    pthread_attr_t threadAttributes;  // Attribute variable to hold, affinity, priority, and policy

    setSched(&threadAttributes);      // Helper funciton assigning affinity, priority, and scheduling policy to thread attribute

    /* Create thread and throw an error if not successful */
    if(pthread_create(&sleepthread, &threadAttributes, sleepingThread, NULL))
        perror("Error creating sleeping thread\n");

    pthread_join(sleepthread, NULL);            // Wait on created thread to join main thread
    pthread_attr_destroy(&threadAttributes);    // Free created attribute

    pthread_exit(NULL);                         // Exit main thread

}