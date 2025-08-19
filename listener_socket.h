// listener_socket.h

#pragma once

typedef struct Listener_Socket Listener_Socket_t;

Listener_Socket_t *ls_new(int port);
void ls_delete(Listener_Socket_t **ppls);
int ls_accept(Listener_Socket_t *pls);

