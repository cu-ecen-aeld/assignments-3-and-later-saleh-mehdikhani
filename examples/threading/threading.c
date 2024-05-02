#include "threading.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

// Optional: use these functions to add debug or error prints to your application
#define DEBUG_LOG(msg,...)
//#define DEBUG_LOG(msg,...) printf("threading: " msg "\n" , ##__VA_ARGS__)
#define ERROR_LOG(msg,...) printf("threading ERROR: " msg "\n" , ##__VA_ARGS__)

void* threadfunc(void* thread_param)
{

    // TODO: wait, obtain mutex, wait, release mutex as described by thread_data structure
    // hint: use a cast like the one below to obtain thread arguments from your parameter
    //struct thread_data* thread_func_args = (struct thread_data *) thread_param;
    struct thread_data * dataPtr = (struct thread_data *)thread_param;
    dataPtr->thread_complete_success = false;

    // Wait before taking the mutex
    usleep(dataPtr->wait_to_obtain_ms * 1000);
    // lock the mutex
    if (pthread_mutex_lock(dataPtr->mutex) == 0) {
        // wait
        usleep(dataPtr->wait_to_release_ms * 1000);
        // Unlock the mutex
        if (pthread_mutex_unlock(dataPtr->mutex) == 0) {
            // ob successfully done!
            dataPtr->thread_complete_success = true;
        }
    }

    return thread_param;
}


bool start_thread_obtaining_mutex(pthread_t *thread, pthread_mutex_t *mutex,int wait_to_obtain_ms, int wait_to_release_ms)
{
    /**
     * TODO: allocate memory for thread_data, setup mutex and wait arguments, pass thread_data to created thread
     * using threadfunc() as entry point.
     *
     * return true if successful.
     *
     * See implementation details in threading.h file comment block
     */
    struct thread_data * dataPtr = malloc(sizeof(struct thread_data));
    if (dataPtr != NULL) {
        dataPtr->mutex = mutex;
        dataPtr->wait_to_obtain_ms = wait_to_obtain_ms;
        dataPtr->wait_to_release_ms = wait_to_release_ms;
        // Create and run the thread
        if (pthread_create(thread, NULL, threadfunc, dataPtr) == 0) {
            return true;
        }
    }

    return false;
}

