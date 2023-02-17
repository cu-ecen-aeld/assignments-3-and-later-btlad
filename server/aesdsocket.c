#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <signal.h>
#include <fcntl.h>
#include <linux/fs.h>
/** Assignment-6-part-1 */
#include "queue.h"
#include <stdbool.h>
#include <pthread.h>
#include <time.h>

#define PORT "9000"
#define BACKLOG 5   // how many pending connections queue will hold
#define BUF_SIZE 1500

int sockfd;        /* listen on sockfd */
int caught_signal = 0;
char f_name[] = "/var/tmp/aesdsocketdata";

/* SIGINT, SIGTERM Signal Handler */
static void signal_handler(int signal_number)
{
    int errno_saved = errno;
    syslog(LOG_INFO, "Caught signal, exiting");
    if ((signal_number == SIGINT) || (signal_number == SIGTERM)) {
        caught_signal = 1;
        shutdown(sockfd, SHUT_RDWR);
    }
    errno = errno_saved;
}

/* SIGALRM Timer Signal Handler */
static void timer_handler(int signal_number)
{
    int errno_saved = errno;
    errno = errno_saved;
}

/* Get sockaddr, IPv4 or IPv6 */
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

/* Receive all chars from the connection file descriptor 's' into 'strbuf' */
int recvall(int s, char **strbuf)
{
    char buf[BUF_SIZE];
    int len = 0;
    int n;

    while (1) {
        if ((n = (int)recv(s, buf, BUF_SIZE, 0)) > 0) {
            *strbuf = (char *)realloc(*strbuf, (len+n+1)*sizeof(char));
            if (*strbuf == NULL) {
                perror("realloc");
                len = -1;
                break;
            }
            memset(*strbuf+len, '\0', n+1);
            strncpy(*strbuf+len, buf, n);
            len += n;

            if (NULL != strchr(*strbuf, '\n')) {
                break;
            }
        }
        else {
            perror("recv");
            if (*strbuf != NULL) {
                free(*strbuf);
                *strbuf = NULL;
            }
            len = -1;
            break;
        }
    }

    return len;
}

/* Send 'len' chars from the 'buf' to the connection file descriptor 's' */
int sendall(int s, char *buf, int *len)
{
    int total = 0;        // how many bytes we've sent
    int bytesleft = *len; // how many we have left to send
    int n;

    while(total < *len) {
        n = send(s, buf+total, bytesleft, 0);
        if (n == -1) { break; }
        total += n;
        bytesleft -= n;
    }

    *len = total; // return number actually sent here

    return n==-1?-1:0; // return -1 on failure, 0 on success
}

/** Assignment-6-part-1 */
/**
 * This structure is dynamically allocated and passed as
 * an argument to the thread using pthread_create().
 * It returned by the thread so it can be freed by the joiner thread.
 */
/** The data type for the node */
typedef struct node {
    pthread_t        thread_id;
    pthread_mutex_t *thread_mutex;
    int              thread_fd;  /* connection file descriptor in thread */
    char             thread_msg[INET6_ADDRSTRLEN];
    bool             thread_complete;
    SLIST_ENTRY(node)  nodes;
} node_t;

/** Thread function */
void* thread_func(void* thread_param)
{
    FILE *fp;
    int ret;
    int len, max_len = 0;
    char *str = NULL;

    time_t t;
    struct tm *tmp;

    node_t *thread_func_args = (node_t *)thread_param;

    pthread_mutex_lock(thread_func_args->thread_mutex);

    if (thread_func_args->thread_fd == -1) {
        fp = fopen(f_name, "a+");
        str = (char *)malloc(100*sizeof(char));
        memset(str, '\0', 100);

        t = time(NULL);
        tmp = localtime(&t);
        /* "%a, %d %b %Y %T %z" - RFC 2822-compliant date format */
        strftime(str, 100*sizeof(char), "timestamp: %a, %d %b %Y %T %z\n", tmp);

        fputs(str, fp);
        free(str);
        str = NULL;
        fclose(fp);
    }
    else {
        ret = recvall(thread_func_args->thread_fd, &str);
        if (ret == -1) {
            if (str != NULL) {
                free(str);
                str = NULL;
            }
        }
        else {
            if (max_len < ret) max_len = ret;

            fp = fopen(f_name, "a+");
            fputs(str, fp);
            fclose(fp);

            fp = fopen(f_name, "r");

            str = (char *)realloc(str, (max_len+1)*sizeof(char));
            memset(str, '\0', max_len+1);
            while (fgets(str, max_len, fp) != NULL ) {
                len = strlen(str);
                sendall(thread_func_args->thread_fd, str, &len);
                memset(str, '\0', max_len+1);
            }
            free(str);
            str = NULL;
            fclose(fp);
        }
    }

    pthread_mutex_unlock(thread_func_args->thread_mutex);
    thread_func_args->thread_complete = true;

    return thread_param;
}


int main(int argc, char *argv[])
{
    openlog(NULL, 0, LOG_USER);
    int yes=1;        // For setsockopt() SO_REUSEADDR, below
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage their_addr;  /* connector's address information */
    socklen_t their_addr_len;
    struct sigaction sa;
    struct sigaction satm;
    int success = 1;
    int new_fd;  /* new connection on new_fd */
    char s[INET6_ADDRSTRLEN];
    int rv;
    struct itimerval delay;
    pid_t pid;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;        /* Allow IPv4 or IPv6 */
    hints.ai_socktype = SOCK_STREAM;    /* Stream socket */
    hints.ai_flags = AI_PASSIVE;        /* For wildcard IP address */
    hints.ai_protocol = 0;              /* Any protocol */
    hints.ai_canonname = NULL;
    hints.ai_addr = NULL;
    hints.ai_next = NULL;

    rv = getaddrinfo(NULL, PORT, &hints, &servinfo);
    if ( rv != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        syslog(LOG_ERR, "getaddrinfo: %s\n", gai_strerror(rv));
        return -1;
    }

    for(p = servinfo; p != NULL; p = p->ai_next) {
        sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if ( sockfd == -1) {
            perror("socket");
            continue;
        }

        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
            perror("setsockopt");
            exit(-1);
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            perror("bind");
            close(sockfd);
            continue;
        }

        break;                      /* Success */
    }

    freeaddrinfo(servinfo);         /* No longer needed */

    if (p == NULL)  {               /* No address succeeded */
        fprintf(stderr, "aesdsocket: failed to bind\n");
        exit(-1);
    }

    /* Start application as a daemon if -d is used */
    if ( (argc == 2) && (0 == strcmp(argv[1],"-d"))) {

        /* create new process */
        pid = fork();
        if (pid == -1)
            exit(-1);
        else if (pid != 0)
            exit (0);

        /* create new session and process group */
        if (setsid () == -1)
            exit(-1);

        /* set the working directory to the root directory */
        if (chdir ("/") == -1)
            exit(-1);

        /* close all open files */
        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);

        /* redirect fd's 0,1,2 to /dev/null */
        open ("/dev/null", O_RDWR);  /* stdin */
        dup (0);                     /* stdout */
        dup (0);                     /* stderror */
    }

    if (listen(sockfd, BACKLOG) == -1) {
        perror("listen");
        exit(-1);
    }

    memset(&sa,0,sizeof(struct sigaction));
    sa.sa_handler=signal_handler;
    if( sigaction(SIGTERM, &sa, NULL) != 0) {
        fprintf(stderr, "Error %d (%s) registering for SIGTERM", errno, strerror(errno));
        success = 0;
    }
    if( sigaction(SIGINT, &sa, NULL) != 0) {
        fprintf(stderr, "Error %d (%s) registering for SIGINT",errno,strerror(errno));
        success = 0;
    }
    /* Timer action */
    memset(&satm,0,sizeof(struct sigaction));
    satm.sa_handler=timer_handler;
    if( sigaction(SIGALRM, &satm, NULL) != 0) {
        fprintf(stderr, "Error %d (%s) registering for SIGALRM",errno,strerror(errno));
        success = 0;
    }
    delay.it_value.tv_sec = 0;
    delay.it_value.tv_usec = 1000;
    delay.it_interval.tv_sec = 10;
    delay.it_interval.tv_usec = 0;
    rv = setitimer(ITIMER_REAL, &delay, NULL);
    if (rv == -1) {
        perror ("setitimer");
        exit(-1);
    }

/** Assignment-6-part-1 */
    node_t *np;
    node_t *np_temp;

    SLIST_HEAD(head_t, node) head; // = SLIST_HEAD_INITIALIZER(head);  /* Singly-linked List head. */
    SLIST_INIT(&head);                /* Initialize the list. */

    pthread_t       thread;
    pthread_mutex_t mutex;
    pthread_mutex_init(&mutex, NULL);
/**  **/
    if (success) {
        while(!caught_signal) {  // main accept() loop
            their_addr_len = sizeof(their_addr);
            new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &their_addr_len);
/** Assignment-6-part-1 */
            np = (node_t *)malloc(sizeof(node_t));
            if (np == NULL)
            {
                perror("malloc");
                continue;
            }

            np->thread_mutex    = &mutex;
            np->thread_complete = false;
            np->thread_fd       = new_fd;

            if (new_fd != -1) {
                inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr), s, sizeof(s));
                syslog(LOG_INFO, "Accepted connection from %s\n", s);
                strncpy(np->thread_msg, s, INET6_ADDRSTRLEN);
            }

            rv = pthread_create(&thread, NULL, thread_func, np);
            if (rv) {
                errno = rv;
                perror("pthread_create");
                if (np->thread_fd != -1) {
                    close(np->thread_fd);
                    syslog(LOG_INFO, "Closed connection from %s\n", np->thread_msg);
                }
                free(np);
                np = NULL;
                continue;
            }

            np->thread_id = thread;

            SLIST_INSERT_HEAD(&head, np, nodes);
            np = NULL;
            SLIST_FOREACH_SAFE(np, &head, nodes, np_temp) {
                if (np->thread_complete) {
                    if (np->thread_fd != -1) {
                        close(np->thread_fd);
                        syslog(LOG_INFO, "Closed connection from %s\n", np->thread_msg);
                    }
                    rv = pthread_join(np->thread_id, NULL);
                    if (rv) {
                        errno = rv;
                        perror ("pthread_join");
                    }
                    SLIST_REMOVE(&head, np, node, nodes);   /* Deletion. */
                    free(np);
                }
            }
/**       **/
        }
        remove(f_name);
    }
        close(sockfd);
        closelog();

    return success ? 0 : -1;
}
