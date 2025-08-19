// Christian Garces — httpserver.c

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>
#include <unistd.h>

#include "request.h"
#include "listener_socket.h"   // ls_new / ls_accept / ls_delete

// worker thread that handles exactly one connection
static void *worker_main(void *arg) {
    int client_fd = (int)(intptr_t)arg;
    handle_connection(client_fd);
    close(client_fd);
    return NULL;
}

// main: create listener, accept loop, thread-per-connection
int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        return EXIT_FAILURE;
    }

    signal(SIGPIPE, SIG_IGN); // avoid SIGPIPE, write() will return -1/EPIPE

    int port = atoi(argv[1]);
    if (port <= 0 || port > 65535) {
        fprintf(stderr, "Invalid port: %s\n", argv[1]);
        return EXIT_FAILURE;
    }

    Listener_Socket_t *ls = ls_new(port);
    if (!ls) {
        fprintf(stderr, "Failed to listen on port %d\n", port);
        return EXIT_FAILURE;
    }
    printf("httpserver listening on port %d\n", port);

    for (;;) {
        int client_fd = ls_accept(ls); // helper sets a 5s timeout
        if (client_fd < 0) {
            if (errno == EINTR) continue;
            // timeout or transient error: keep accepting
            continue;
        }

        pthread_t tid;
        int rc = pthread_create(&tid, NULL, worker_main, (void *)(intptr_t)client_fd);
        if (rc != 0) {
            // fallback: handle in this thread on failure
            fprintf(stderr, "pthread_create failed: %s\n", strerror(rc));
            handle_connection(client_fd);
            close(client_fd);
            continue;
        }
        pthread_detach(tid);
    }

    // not reached
    // ls_delete(&ls);
    // return EXIT_SUCCESS;
}

