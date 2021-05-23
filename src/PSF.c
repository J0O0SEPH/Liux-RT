#define _GNU_SOURCE
#include <pthread.h>
#include <syslog.h>
#include <stdio.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>

#if SMP != 0
    #define sched_setaffinity(...)
    #define setSched(...)
#endif

#define ITERATIONS 20
// #define POLICY     SCHED_FIFO
#define POLICY     SCHED_OTHER
// #define THECLOCK   CLOCK_REALTIME
#define THECLOCK   CLOCK_MONOTONIC_RAW
// #define THECLOCK   CLOCK_REALTIME_COARSE
#define NSEC_PER_SEC    1000000000
// #define syslog(LOG_DEBUG, ...) printf(__VA_ARGS__)

#define K 4.0
#define HIGHT 300
#define WIDTH 400
#define HEADERSIZE 38
#define HIGHTTHREADS 1
#define WIDTHTHREADS 1
#define HIGHTpTHREAD HIGHT/HIGHTTHREADS
#define WIDTHpTHREAD WIDTH/WIDTHTHREADS

#define MAINAFFINITY 1
#define WORKERAFFINITY 3
#define DISPATCHERAFFINITY 2

enum process
{
    readProcess,
    writeProcess
}eprocess;

typedef struct
{
    int id, hightIdx, hight, widthIdx, width;
}PSF;

PSF threadargs[HIGHTTHREADS][WIDTHTHREADS];
pthread_t threads2D[HIGHTTHREADS][WIDTHTHREADS];
pthread_attr_t dispatcherThrdAttr, workerThrdAttr;

unsigned char R[HIGHT][WIDTH], G[HIGHT][WIDTH], B[HIGHT][WIDTH];
unsigned char Rconv[HIGHT][WIDTH], Gconv[HIGHT][WIDTH], Bconv[HIGHT][WIDTH];
unsigned char headerBuff[HEADERSIZE + 1];

float avg[3][3] = {{-K/8.0, -K/8.0, -K/8.0}, 
                   {-K/8.0,  K+1.0, -K/8.0}, 
                   {-K/8.0, -K/8.0, -K/8.0}};

void* sharpen(void* thrdArg);
void* dispatcherThread(void* arg);
void processFile(unsigned operation, char* fileName);
#if SMP == 0
void setSched(pthread_attr_t *thrdAttrs, unsigned affinity); 
#endif
void delta_time(const struct timespec* end, const struct timespec* start, struct timespec* duration);


int main(int argc, char** argv)
{
    if(argc < 3)
    {
        printf("enter at least 2 input argument\n");
        exit(-1);
    }
    int rc = system("echo > /dev/null | sudo tee /var/log/syslog && logger [COURSE:1][ASSIGNMENT:5]: `uname -a`");
    if(rc)perror("error cleaning syslog\n");

    openlog("[COURSE:1][ASSIGNMENT:5]",LOG_NDELAY,LOG_DAEMON);
    syslog(LOG_DEBUG, "Image size is: %d * %d, threads count: %d, iterations %d\n", 
                           HIGHT, WIDTH, HIGHTTHREADS*WIDTHTHREADS, ITERATIONS);

    cpu_set_t mainAffin;
    CPU_ZERO(&mainAffin);
    CPU_SET(MAINAFFINITY, &mainAffin);

    // sched_setaffinity(getpid(), sizeof(cpu_set_t), &mainAffin);
    // setSched(&dispatcherThrdAttr, DISPATCHERAFFINITY);
    // setSched(&workerThrdAttr, WORKERAFFINITY);

    eprocess = readProcess;
    processFile(eprocess, argv[1]);

    pthread_t dispatcherThrd;
    rc = pthread_create(&dispatcherThrd,          // pointer to thread descriptor
                            &dispatcherThrdAttr,      // use default attributes
                            dispatcherThread,         // thread function entry point
                            NULL);                    // parameters to pass in
    /* if failed, print error and exit */
    if(rc != 0)
    {
        perror("creating thread faild!\n");
        exit(-1);
    }

    pthread_join(dispatcherThrd, NULL);

    eprocess = writeProcess;
    processFile(eprocess, argv[2]);
}


/*
  Thread body.
  Thread fetches grid parameters from input argument, and convolutes on
  3 color buffers caculating wighted average for 3*3 matrix corresponding to
  each pixel.
*/
void* sharpen(void* thrdArg)
{
    PSF* pPsf = (PSF*)thrdArg;
    for(int iIteration = 0; iIteration < ITERATIONS; iIteration++)
    {
        for(int iHight = (pPsf->hight * pPsf->hightIdx); iHight < (pPsf->hight * pPsf->hightIdx) + pPsf->hight; iHight++)
        {
            for(int iWidth = (pPsf->width * pPsf->widthIdx); iWidth < (pPsf->width * pPsf->widthIdx) + pPsf->width; iWidth++)
            {
                float tempR = 0, tempG = 0, tempB = 0; int paddingH = -1;
                for(int iMatrixH = 0; iMatrixH < 3; iMatrixH++)
                {
                    int paddingW = -1;
                    for(int iMatrixW = 0; iMatrixW < 3; iMatrixW++)
                    {
                        int Hindex= iHight + paddingH, Windex = iWidth + paddingW;
                        tempR += avg[iMatrixH][iMatrixW] * (float)R[Hindex][Windex];
                        tempG += avg[iMatrixH][iMatrixW] * (float)G[Hindex][Windex];
                        tempB += avg[iMatrixH][iMatrixW] * (float)B[Hindex][Windex];
                        paddingW++;
                    }
                    paddingH++;
                }
                if(tempR > 255)tempR = 255; 
                if(tempR<0)tempR = 0;
                if(tempG > 255)tempG = 255; 
                if(tempG<0)tempG = 0;
                if(tempB > 255)tempB = 255; 
                if(tempB<0)tempB = 0;

                Rconv[iHight][iWidth] = (char)tempR;
                Gconv[iHight][iWidth] = (char)tempG;
                Bconv[iHight][iWidth] = (char)tempB;
            }
        }
    }

    pthread_exit(NULL);
}


#if SMP == 0
/*
  Helper function to assing desired scheduling policy and affinity.
*/
void setSched(pthread_attr_t *thrdAttrs, unsigned affinity)
{
    struct sched_param schedPriority;                                  // Declare sched_param to hold desired priority
    schedPriority.sched_priority = sched_get_priority_max(POLICY);     // Fetch maximum priority for desired scheduling policy

    cpu_set_t cpuAffin;             // Maskable varialble holding desired affinity
    CPU_ZERO(&cpuAffin);            // Clear all garbage masks
    CPU_SET(affinity, &cpuAffin);   // Set desired affinity

    /* Initialize, set inheritance, affinity, scheduling policy, and priority to thread attribute in question */
    pthread_attr_init(thrdAttrs);
    pthread_attr_setschedpolicy(thrdAttrs,  POLICY);
    pthread_attr_setschedparam(thrdAttrs,   &schedPriority);
    pthread_attr_setinheritsched(thrdAttrs, PTHREAD_EXPLICIT_SCHED);
    pthread_attr_setaffinity_np(thrdAttrs,  sizeof(cpu_set_t), &cpuAffin);
}
#endif

/*
  Helper function, calculating time difference between 2 time structures,
  and fill a third structure with delta time.
*/
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


/*
  Create threads corresponding to required grid parameters.
  Assign grid dimensions to each thread.
  Calculate duration spent by all threads.
*/
void* dispatcherThread(void* arg)
{
    struct timespec start, end, duration;

    clock_gettime(THECLOCK, &start);

    /* Create desired number of threads within a loop with specified thread routine */
    for(int hThread = 0; hThread < HIGHTTHREADS; hThread++)
    {
        for(int wThread = 0; wThread < WIDTHTHREADS; wThread++)
        {
            threadargs[hThread][wThread].id = hThread + wThread;                  // assign thread number to thread attribute
            threadargs[hThread][wThread].hight = HIGHTpTHREAD;
            threadargs[hThread][wThread].hightIdx = hThread;
            threadargs[hThread][wThread].width = WIDTHpTHREAD;
            threadargs[hThread][wThread].widthIdx = wThread;

            int rc = pthread_create(&threads2D[hThread][wThread],                 // pointer to thread descriptor
                                    &workerThrdAttr,                              // use default attributes
                                    sharpen,                                      // thread function entry point
                                    (void*)&threadargs[hThread][wThread]);        // parameters to pass in
        
            /* if failed, print error and exit */
            if(rc != 0)
            {
                perror("creating thread faild!\n");
                exit(-threadargs[hThread][wThread].id);
            }
        }
    }

    /* wait on created threads to join dispatching thread */
    for(int hThread = 0; hThread < HIGHTTHREADS; hThread++)
    {
        for(int wThread = 0; wThread < WIDTHTHREADS; wThread++)
        { 
            pthread_join(threads2D[hThread][wThread], NULL);
        }
    }

    clock_gettime(THECLOCK, &end);
    delta_time(&end, &start, &duration);
    syslog(LOG_DEBUG, "All threads consumed %ld seconds, and %ld nanoseconds to finish.\n", duration.tv_sec, duration.tv_nsec);

    /* exit dispatcher thread */
    pthread_exit(NULL);
}


/* 
  Open file for read/write, 
  parse data from/to 3 individual buffers, 
  one for each color RGB 
  */
void processFile(enum process operation, char* fileName)
{
    int rc, inFile, outFile, readBytes, writtenBytes = 0, bytesLeft = HEADERSIZE; 

    if (operation == readProcess)
    {
        syslog(LOG_DEBUG, "Started Reading\n");
        inFile = open(fileName, O_RDONLY, 0644);

        while(bytesLeft)
        {
            readBytes = read(inFile, (void*)&headerBuff, bytesLeft);
            bytesLeft -= readBytes;
        }
        headerBuff[HEADERSIZE] = '\0';

        for(int iMatrixH = 0; iMatrixH < HIGHT; iMatrixH++)
        {
            for(int iMatrixW = 0; iMatrixW < WIDTH; iMatrixW++)
            {
                rc += read(inFile, (void*)&R[iMatrixH][iMatrixW], 1);
                rc += read(inFile, (void*)&G[iMatrixH][iMatrixW], 1);
                rc += read(inFile, (void*)&B[iMatrixH][iMatrixW], 1);
            }
        }
        close(inFile);
        syslog(LOG_DEBUG, "Just finished reading\n");
    }
    else if (operation == writeProcess)
    {
        syslog(LOG_DEBUG, "Started Writing\n");

        outFile = open(fileName, (O_RDWR | O_CREAT), 0666);

        while(bytesLeft)
        {
            writtenBytes = write(outFile, (void*)&headerBuff, bytesLeft);
            bytesLeft -= writtenBytes;
        }

        for(int iMatrixH = 0; iMatrixH < HIGHT; iMatrixH++)
        {
            for(int iMatrixW = 0; iMatrixW < WIDTH; iMatrixW++)
            {
                rc += write(outFile, (void*)&Rconv[iMatrixH][iMatrixW], 1);
                rc += write(outFile, (void*)&Gconv[iMatrixH][iMatrixW], 1);
                rc += write(outFile, (void*)&Bconv[iMatrixH][iMatrixW], 1);
            }
        }
        close(outFile);
    }
    else
    {
        printf("wrong operation request.\n");
    }
}
