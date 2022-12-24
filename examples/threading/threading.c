#include "threading.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

// Optional: use these functions to add debug or error prints to your application
#define DEBUG_LOG(msg,...)
//#define DEBUG_LOG(msg,...) printf("threading: " msg "\n" , ##__VA_ARGS__)
#define ERROR_LOG(msg,...) printf("threading ERROR: " msg "\n" , ##__VA_ARGS__)

void* threadfunc(void* thread_param)
{
    // TODO: wait, obtain mutex, wait, release mutex as described by thread_data structure
    // hint: use a cast like the one below to obtain thread arguments from your parameter
    //struct thread_data* thread_func_args = (struct thread_data *) thread_param;
    int ret;
    struct thread_data *thread_func_args = (struct thread_data *)thread_param;

    thread_func_args->thread_complete_success = false;

    errno = 0;
    usleep(1000*thread_func_args->thread_wait_to_obtain_ms);
    if (errno) {
        perror ("usleep");
        pthread_exit(thread_param);
    }

    ret = pthread_mutex_lock(thread_func_args->thread_mutex);
   if (ret) {
        errno = ret;
        perror("pthread_mutex_lock");
        pthread_exit(thread_param);
    }

    errno = 0;
    usleep(1000*thread_func_args->thread_wait_to_release_ms);
    if (errno) {
        perror ("usleep");
        pthread_exit(thread_param);
    }

    pthread_mutex_unlock(thread_func_args->thread_mutex);

    thread_func_args->thread_complete_success = true;
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
    struct thread_data *th_data = NULL;
    int ret;

    th_data = (struct thread_data *)calloc(1, sizeof(struct thread_data));
    if (!th_data) {
        perror ("calloc");
        return false;
    }

    th_data->thread_mutex = mutex;
    th_data->thread_wait_to_obtain_ms = wait_to_obtain_ms;
    th_data->thread_wait_to_release_ms = wait_to_release_ms;
    th_data->thread_complete_success = false;

    ret = pthread_create(thread, NULL, threadfunc, th_data);
    if (ret) {
        errno = ret;
        perror("pthread_create");
        return false;
    }

    return true;
}
