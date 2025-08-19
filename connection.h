// conn.h

#pragma once

#include "response.h"
#include "request.h"

#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>

typedef struct Conn conn_t;

conn_t *conn_new(int connfd);
void conn_delete(conn_t **conn);
const Response_t *conn_parse(conn_t *conn);

const Request_t *conn_get_request(conn_t *conn);
char *conn_get_uri(conn_t *conn);
char *conn_get_header(conn_t *conn, char *header);

const Response_t *conn_recv_file(conn_t *conn, int fd);

const Response_t *conn_send_file(conn_t *conn, int fd, uint64_t count);
const Response_t *conn_send_response(conn_t *conn, const Response_t *res);

char *conn_str(conn_t *conn);

