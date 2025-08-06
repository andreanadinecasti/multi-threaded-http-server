// HTTP Server
// Starter code by Eugene Chou, Andrew Quinn, Brian Zhao

#include "asgn2_helper_funcs.h"
#include "connection.h"
#include "debug.h"
#include "response.h"
#include "request.h"
#include "queue.h"
#include "rwlock.h"

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <limits.h>
#include <sys/stat.h>
#include <stdbool.h>

// ASSIGNMENT LOGIC:
// 1. main thread listens and pushes requests on to a queue
// 2. the worker threads are in infinite loop and use the queue to pop requests
// 3. execute those requests
//      a. for get requests, use reader_lock and reader_unlock
//      b. for put requests, use writer_lock and writer_unlock

////////////////////////////////////////// STRUCTS ///////////////////////////////////////
typedef struct Node Node; // node struct
typedef struct Node {
    char uri[PATH_MAX];
    rwlock_t *rwlock;
    Node *next;
    Node *prev;
} Node;

typedef struct Map Map; // map struct
typedef struct Map {
    int curr;
    pthread_mutex_t lock;
    Node *front;
    Node *back;
} Map;

struct thread_parms { // thread parameters
    queue_t *queue;
    Map *map;
};

////////////////////////////////// FUNCTION DECLARATION //////////////////////////////////
Node *node_new(void *);
Map *map_new();
Node *file_lock_find(Map *, char *);
void file_lock_push(Map *, char *);
void file_lock_new(Map *, char *);
void file_lock_read_lock(Map *, char *);
void file_lock_read_unlock(Map *, char *);
void file_lock_write_lock(Map *, char *);
void file_lock_write_unlock(Map *, char *);

void new_log_entry(conn_t *, const Response_t *);
void dispatch(queue_t *, uintptr_t);
void *work(void *);

void handle_connection(int, Map *);
void handle_get(conn_t *, Map *);
void handle_put(conn_t *, Map *);
void handle_unsupported(conn_t *, Map *);

////////////////////////////////////////// MAIN //////////////////////////////////////////
int main(int argc, char **argv) {
    if (argc < 2) { // check to see how many arguments given
        warnx("wrong arguments: %s port_num", argv[0]);
        fprintf(stderr, "usage: %s [-t threads] <port>\n", argv[0]);
        return EXIT_FAILURE;
    }

    int opt;
    int threads = 4;
    while ((opt = getopt(argc, argv, "t:")) > 0) { // if there is a -t option
        switch (opt) {
        case 't': threads = atoi(optarg); break;
        default: fprintf(stderr, "1 Usage: %s [-t threads] <port>\n", argv[0]); return 1;
        }
    }

    queue_t *req = queue_new(threads + 8); // push requests onto the queue
    Map *file_map = map_new(); // mapping through multiple files

    // multithreading
    struct thread_parms tp;
    tp.queue = req;
    tp.map = file_map;

    pthread_t thread_list[threads];
    //printf("threads: %d\n", threads); // 32770

    //printf("creating the worker threads...\n");
    for (uintptr_t i = 0; i < (uintptr_t) threads; i++) {
        //printf("thread #%lu\n", i);
        pthread_create(thread_list + i, NULL, work, (void *) &tp);
    }
    printf(
        "worker threads made!\n"); // ------------------------------------------------> never gets to this

    // get a port number
    //printf("%d - %d > 1?\n", argc, optind);
    if (argc - optind > 1) { // is there exactly one required argument (port)
        fprintf(stderr, "2 Usage: %s [-t threads] <port>\n", argv[0]);
        return 1;
    }

    char *endptr = NULL;
    size_t port = (size_t) strtoull(argv[optind], &endptr, 10);
    if (endptr && *endptr != '\0') {
        warnx("invalid port number: %s", argv[optind]);
        return EXIT_FAILURE;
    }

    // printing options
    //printf("input: ./httpserver %d %zu\n", threads, port);

    // create a listener socket
    signal(SIGPIPE, SIG_IGN);
    Listener_Socket sock;
    listener_init(&sock, port);

    // accept a connection
    while (1) {
        //fprintf(stdout, "accepting a connection...\n"); // -----------------------------> never prints
        uintptr_t connfd
            = listener_accept(&sock); // ----------------------------------> never gets to this
        //printf("connfd: %lu\n", connfd);
        dispatch(req, connfd);
    }
    return EXIT_SUCCESS;
}

///////////////////////// MAP FUNCTIONS /////////////////////////////////////////////////

Node *node_new(void *data) { // node constructor
    Node *n = (Node *) malloc(sizeof(Node));
    n->next = n->prev = NULL;
    strcpy(n->uri, data);
    n->rwlock = rwlock_new(READERS, -1);
    return n;
}

Map *map_new() { // map constructor
    Map *m = (Map *) malloc(sizeof(Map));
    pthread_mutex_init(&(m->lock), NULL);
    m->front = m->back = NULL;
    m->curr = 0;
    return m;
}

Node *file_lock_find(Map *m, char *uri) { // goes through and find node
    // returns pointer to found node else NULL
    Node *temp = m->front;
    while (temp != NULL) {
        if (strcmp(uri, temp->uri) == 0) {
            return temp;
        }
        temp = temp->next;
    }
    return NULL;
}

void file_lock_push(Map *m, char *uri) { // adds a node
    // when adding a new node, use strcopy to add uri
    int size = strlen(uri);
    char uri_cpy[size];
    strcpy(uri_cpy, uri);
    //printf("file_lock_push(): after strcpy()\n");

    Node *new = node_new(uri_cpy);
    if (m->curr == 0) { // empty
        m->front = m->back = new;
    } else {
        Node *temp = m->front;
        m->front = new;
        new->next = temp;
        temp->prev = new;
    }
    m->curr += 1;
}

void file_lock_new(Map *m, char *uri) { // add new node if it doesn't exist already
    pthread_mutex_lock(&(m->lock));
    Node *found = file_lock_find(m, uri);
    //printf("file_lock_new(): after calling file_lock_find\n");
    if (found == NULL) {
        file_lock_push(m, uri);
        //printf("file_lock_new(): after calling file_lock_push\n");
    }
    pthread_mutex_unlock(&(m->lock));
}

void file_lock_read_lock(Map *m, char *uri) { // finds the node with uri and does reader_lock
    pthread_mutex_lock(&(m->lock));
    Node *found = file_lock_find(m, uri);
    if (found == NULL) {
        //printf("file read lock: tried to read a file that hasn't been added to map\n");
    }
    pthread_mutex_unlock(&(m->lock));
    reader_lock(found->rwlock);
}

void file_lock_read_unlock(Map *m, char *uri) { // finds the node with uri and does reader_unlock
    pthread_mutex_lock(&(m->lock));
    Node *found = file_lock_find(m, uri);
    if (found == NULL) {
        //printf("file read unlock: tried to read a file that hasn't been added to map\n");
    }
    pthread_mutex_unlock(&(m->lock));
    reader_unlock(found->rwlock);
}

void file_lock_write_lock(Map *m, char *uri) { // finds the node with uri and does writer_lock
    pthread_mutex_lock(&(m->lock));
    Node *found = file_lock_find(m, uri);
    if (found == NULL) {
        //printf("file write lock: tried to read a file that hasn't been added to map\n");
    }
    pthread_mutex_unlock(&(m->lock));
    writer_lock(found->rwlock);
}

void file_lock_write_unlock(Map *m, char *uri) { // finds the node with uri and does writer_unlock
    pthread_mutex_lock(&(m->lock));
    Node *found = file_lock_find(m, uri);
    if (found == NULL) {
        //printf("file write unlock: tried to read a file that hasn't been added to map\n");
    }
    pthread_mutex_unlock(&(m->lock));
    writer_unlock(found->rwlock);
}

///////////////////////// ADD A NEW LOG ENTRY INTO THE AUDIT LOG /////////////////////////
void new_log_entry(conn_t *conn, const Response_t *res) {

    char *request = NULL;
    const Request_t *req = conn_get_request(conn);
    if (req == &REQUEST_GET) {
        request = "GET";
    } else if (req == &REQUEST_PUT) {
        request = "PUT";
    }
    char *uri = conn_get_uri(conn);
    uint16_t status = response_get_code(res);
    char *request_id = conn_get_header(conn, "Request-Id");

    fprintf(stderr, "%s,/%s,%hu,%s\n", request, uri, status, request_id);
}

///////////////////////// DISPATCHER THREAD FUNCTION /////////////////////////////////////
void dispatch(queue_t *q, uintptr_t connfd) {
    //printf("about to push...\n");
    queue_push(q, (void *) connfd);
    //printf("pushed!\n");
}

///////////////////////// WORKER THREAD FUNCTION /////////////////////////////////////////
void *work(void *args) {
    struct thread_parms *tp = (struct thread_parms *) args;
    uintptr_t connfd;

    while (1) {
        //printf("about to pop...\n");
        queue_pop(tp->queue, (void **) &connfd);
        //printf("connfd: %lu\n", connfd);
        //printf("popped!\n");
        handle_connection(connfd, tp->map);
        //printf("successfully handled connection!\n");
    }
}

///////////////////////// HANDLING EACH CLIENT CONNECTION ////////////////////////////////

void handle_connection(int connfd, Map *m) {

    //printf("handling connection\n");
    conn_t *conn = conn_new(connfd);
    const Response_t *res = conn_parse(conn);
    if (res != NULL) {
        conn_send_response(conn, res);
    } else {
        debug("%s", conn_str(conn));
        const Request_t *req = conn_get_request(conn);
        if (req == &REQUEST_GET) {
            handle_get(conn, m);
            //printf("successfully handled get!\n");
        } else if (req == &REQUEST_PUT) {
            handle_put(conn, m);
            //printf("successfully handled put!\n");
        } else {
            handle_unsupported(conn, m);
        }
    }

    conn_delete(&conn);
    close(connfd);
}

///////////////////////// HANDLING GET REQUESTS //////////////////////////////////////////
struct stat file_info;
void handle_get(conn_t *conn, Map *m) {
    //printf("handling get request...\n");

    char *uri = conn_get_uri(conn);
    file_lock_new(m, uri);
    //printf("handle_get(): after file_lock_new\n");
    file_lock_read_lock(m, uri);

    const Response_t *res = NULL;
    debug("GET request not implemented. But, we want to get %s", uri);
    int fd = open(uri, O_RDONLY);
    if (fd < 0) {
        if (errno == EACCES || errno == EISDIR || errno == ENOENT) { // cannot access
            res = &RESPONSE_FORBIDDEN;
            goto out;
        } else if (errno == ENAMETOOLONG || errno == ENOTDIR) { // cannot find the file
            res = &RESPONSE_NOT_FOUND;
            goto out;
        } else { // other error
            res = &RESPONSE_INTERNAL_SERVER_ERROR;
            goto out;
        }
    }
    if (fstat(fd, &file_info) < 0) { // get the size of the file
        res = &RESPONSE_FORBIDDEN;
        goto out;
    } else if (file_info.st_mode & S_IFDIR) { // check if file is directory
        res = &RESPONSE_BAD_REQUEST;
        goto out;
    }

    uint64_t file_size = (uint64_t) file_info.st_size; // file size
    //printf("file_size: %lu\n", file_size);
    pthread_mutex_lock(&(m->lock));
    res = conn_send_file(conn, fd, file_size); // send the file in a lock
    pthread_mutex_unlock(&(m->lock));

    if (res == NULL) {
        res = &RESPONSE_OK;
    }

    close(fd);
    //file_lock_read_unlock(m, uri); // reader unlock
out:
    pthread_mutex_lock(&(m->lock)); // lock maps mutex
    //conn_send_response(conn, res);
    new_log_entry(conn, res); // add the log entry
    pthread_mutex_unlock(&(m->lock)); // unlock maps mutex
    file_lock_read_unlock(m, uri); // reader unlock
}

///////////////////////// HANDLING UNSUPPORTED REQUESTS //////////////////////////////////
void handle_unsupported(conn_t *conn, Map *m) { // int **status) {
    debug("handling unsupported request");
    const Response_t *res = &RESPONSE_NOT_IMPLEMENTED;
    pthread_mutex_lock(&(m->lock)); // lock maps mutex
    conn_send_response(conn, res); // send response
    new_log_entry(conn, res); //add the log entry
    pthread_mutex_unlock(&(m->lock)); //unlock maps mutex
}

///////////////////////// HANDLING PUT REQUESTS //////////////////////////////////////////
void handle_put(conn_t *conn, Map *m) {
    //printf("handling put request...\n");

    char *uri = conn_get_uri(conn);
    file_lock_new(m, uri);
    file_lock_write_lock(m, uri);

    const Response_t *res = NULL;
    debug("handling put request for %s", uri);

    bool existed = access(uri, F_OK) == 0;
    debug("%s existed? %d", uri, existed);

    int fd = open(uri, O_CREAT | O_TRUNC | O_WRONLY, 0600);
    if (fd < 0) {
        debug("%s: %d", uri, errno);
        if (errno == EACCES || errno == EISDIR || errno == ENOENT) {
            res = &RESPONSE_FORBIDDEN;
            goto out;
        } else {
            res = &RESPONSE_INTERNAL_SERVER_ERROR;
            goto out;
        }
    }

    //pthread_mutex_lock(&(m->lock));
    res = conn_recv_file(conn, fd);
    //pthread_mutex_unlock(&(m->lock));

    if (res == NULL && existed) {
        res = &RESPONSE_OK;
    } else if (res == NULL && !existed) {
        res = &RESPONSE_CREATED;
    }

    close(fd);
    //file_lock_write_unlock(m, uri); //writer unlock

out:
    pthread_mutex_lock(&(m->lock)); //lock maps mutex
    conn_send_response(conn, res);
    new_log_entry(conn, res); //add the log entry
    pthread_mutex_unlock(&(m->lock)); //unlock maps mutex
    file_lock_write_unlock(m, uri); //writer unlock
}
