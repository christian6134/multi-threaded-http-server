// Christian Garces — req.c

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "request.h"
#include "iowrapper.h" // read_n_bytes / write_n_bytes

// limits
#define LINE_MAX_LEN  8192
#define PATH_MAX_LEN  4096
#define IO_BUF_SIZE   16384

// write all bytes helper using provided helper
static int write_all(int fd, const void *buf, size_t n) {
    ssize_t w = write_n_bytes(fd, (char *)buf, n);
    if (w < 0 || (size_t)w != n) return -1;
    return 0;
}

// read exactly n bytes using provided helper
static int read_exact(int fd, void *buf, size_t n) {
    ssize_t r = read_n_bytes(fd, (char *)buf, n);
    if (r < 0 || (size_t)r != n) return -1;
    return 0;
}

// read a CRLF-terminated line (without CRLF). returns length, 0 on EOF-before-any, -1 on error
static ssize_t read_line(int fd, char *out, size_t cap) {
    size_t i = 0;
    int got_any = 0;
    for (;;) {
        char c;
        ssize_t n = read(fd, &c, 1);
        if (n == 0) return got_any ? -1 : 0;
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        got_any = 1;
        if (c == '\r') {
            ssize_t m = read(fd, &c, 1);
            if (m <= 0 || c != '\n') return -1;
            out[i] = '\0';
            return (ssize_t)i;
        }
        if (i + 1 >= cap) return -1;
        out[i++] = c;
    }
}

// safe path check to avoid traversal and absolute paths
static int is_safe_path(const char *p) {
    if (!p || p[0] == '\0') return 0;
    if (p[0] == '/') return 0;
    if (strstr(p, "..")) return 0;
    if (strchr(p, '\\')) return 0;
    return 1;
}

// directory check
static int is_directory(const char *p) {
    struct stat st;
    if (stat(p, &st) < 0) return 0;
    return S_ISDIR(st.st_mode) ? 1 : 0;
}

// send minimal HTTP response line
static int send_status_line(int fd, int code, const char *reason) {
    char buf[128];
    int len = snprintf(buf, sizeof(buf), "HTTP/1.1 %d %s\r\n", code, reason);
    if (len < 0 || (size_t)len >= sizeof(buf)) return -1;
    return write_all(fd, buf, (size_t)len);
}

// send header
static int send_header(int fd, const char *name, const char *value) {
    char line[512];
    int len = snprintf(line, sizeof(line), "%s: %s\r\n", name, value);
    if (len < 0 || (size_t)len >= sizeof(line)) return -1;
    return write_all(fd, line, (size_t)len);
}

// end headers
static int end_headers(int fd) {
    return write_all(fd, "\r\n", 2);
}

// GET handler
static int do_get(int fd, const char *path) {
    if (!is_safe_path(path) || is_directory(path)) {
        if (send_status_line(fd, 403, "Forbidden") < 0) return -1;
        if (send_header(fd, "Content-Length", "0") < 0) return -1;
        if (end_headers(fd) < 0) return -1;
        return 0;
    }

    int f = open(path, O_RDONLY);
    if (f < 0) {
        if (errno == ENOENT) {
            if (send_status_line(fd, 404, "Not Found") < 0) return -1;
        } else if (errno == EACCES) {
            if (send_status_line(fd, 403, "Forbidden") < 0) return -1;
        } else {
            if (send_status_line(fd, 500, "Internal Server Error") < 0) return -1;
        }
        if (send_header(fd, "Content-Length", "0") < 0) return -1;
        if (end_headers(fd) < 0) return -1;
        return 0;
    }

    struct stat st;
    if (fstat(f, &st) < 0 || !S_ISREG(st.st_mode)) {
        close(f);
        if (send_status_line(fd, 403, "Forbidden") < 0) return -1;
        if (send_header(fd, "Content-Length", "0") < 0) return -1;
        if (end_headers(fd) < 0) return -1;
        return 0;
    }

    char lenbuf[64];
    snprintf(lenbuf, sizeof(lenbuf), "%lld", (long long)st.st_size);
    if (send_status_line(fd, 200, "OK") < 0) { close(f); return -1; }
    if (send_header(fd, "Content-Length", lenbuf) < 0) { close(f); return -1; }
    if (send_header(fd, "Content-Type", "application/octet-stream") < 0) { close(f); return -1; }
    if (end_headers(fd) < 0) { close(f); return -1; }

    char buf[IO_BUF_SIZE];
    for (;;) {
        ssize_t n = read(f, buf, sizeof(buf));
        if (n == 0) break;
        if (n < 0) { close(f); return -1; }
        if (write_all(fd, buf, (size_t)n) < 0) { close(f); return -1; }
    }
    close(f);
    return 0;
}

// PUT handler
static int do_put(int fd, const char *path, long long content_length) {
    if (!is_safe_path(path) || is_directory(path)) {
        if (send_status_line(fd, 403, "Forbidden") < 0) return -1;
        if (send_header(fd, "Content-Length", "0") < 0) return -1;
        if (end_headers(fd) < 0) return -1;
        return 0;
    }
    if (content_length < 0) {
        if (send_status_line(fd, 400, "Bad Request") < 0) return -1;
        if (send_header(fd, "Content-Length", "0") < 0) return -1;
        if (end_headers(fd) < 0) return -1;
        return 0;
    }

    int existed = 0;
    struct stat st;
    if (stat(path, &st) == 0 && S_ISREG(st.st_mode)) existed = 1;

    int f = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (f < 0) {
        if (send_status_line(fd, errno == EACCES ? 403 : 500,
                             errno == EACCES ? "Forbidden" : "Internal Server Error") < 0) return -1;
        if (send_header(fd, "Content-Length", "0") < 0) return -1;
        if (end_headers(fd) < 0) return -1;
        return 0;
    }

    char buf[IO_BUF_SIZE];
    long long remaining = content_length;
    while (remaining > 0) {
        size_t chunk = (remaining > (long long)sizeof(buf)) ? sizeof(buf) : (size_t)remaining;
        if (read_exact(fd, buf, chunk) < 0) { close(f); return -1; }
        if (write_all(f, buf, chunk) < 0) { close(f); return -1; }
        remaining -= (long long)chunk;
    }
    if (close(f) < 0) {
        if (send_status_line(fd, 500, "Internal Server Error") < 0) return -1;
        if (send_header(fd, "Content-Length", "0") < 0) return -1;
        if (end_headers(fd) < 0) return -1;
        return 0;
    }

    if (!existed) {
        if (send_status_line(fd, 201, "Created") < 0) return -1;
    } else {
        if (send_status_line(fd, 200, "OK") < 0) return -1;
    }
    if (send_header(fd, "Content-Length", "0") < 0) return -1;
    if (end_headers(fd) < 0) return -1;
    return 0;
}

// public entry: read one request, parse, and dispatch
void handle_connection(int fd) {
    char line[LINE_MAX_LEN];

    ssize_t n = read_line(fd, line, sizeof(line));
    if (n <= 0) return;

    char method[16], target[PATH_MAX_LEN], version[16];
    method[0] = target[0] = version[0] = '\0';
    if (sscanf(line, "%15s %4095s %15s", method, target, version) != 3) {
        send_status_line(fd, 400, "Bad Request");
        send_header(fd, "Content-Length", "0");
        end_headers(fd);
        return;
    }

    long long content_length = -1;
    for (;;) {
        ssize_t m = read_line(fd, line, sizeof(line));
        if (m < 0) {
            send_status_line(fd, 400, "Bad Request");
            send_header(fd, "Content-Length", "0");
            end_headers(fd);
            return;
        }
        if (m == 0) {
            send_status_line(fd, 400, "Bad Request");
            send_header(fd, "Content-Length", "0");
            end_headers(fd);
            return;
        }
        if (line[0] == '\0') break;
        if (strncasecmp(line, "Content-Length:", 15) == 0) {
            const char *v = line + 15;
            while (*v == ' ' || *v == '\t') v++;
            char *end = NULL;
            long long tmp = strtoll(v, &end, 10);
            if (end == v || tmp < 0) {
                send_status_line(fd, 400, "Bad Request");
                send_header(fd, "Content-Length", "0");
                end_headers(fd);
                return;
            }
            content_length = tmp;
        }
    }

    if (strcmp(version, "HTTP/1.1") != 0) {
        send_status_line(fd, 400, "Bad Request");
        send_header(fd, "Content-Length", "0");
        end_headers(fd);
        return;
    }

    if (strcmp(method, "GET") == 0) {
        do_get(fd, target);
    } else if (strcmp(method, "PUT") == 0) {
        do_put(fd, target, content_length);
    } else {
        send_status_line(fd, 405, "Method Not Allowed");
        send_header(fd, "Allow", "GET, PUT");
        send_header(fd, "Content-Length", "0");
        end_headers(fd);
    }
}

