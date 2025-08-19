# Multithreaded HTTP Server

--------

## Overview
This project implements a multithreaded HTTP/1.1 server in C.  
The server supports `GET` and `PUT` requests, handles multiple clients concurrently, and uses synchronization primitives for thread safety.

--------

## Features
- HTTP/1.1 compliant request parsing
- Supports `GET` and `PUT` methods
- Returns correct status codes (200, 201, 400, 403, 404, 405, 500)
- Multithreading with pthreads
- Reader-writer lock (`rwlock.c`)
- Bounded blocking queue (`queue.c`)

--------

## File Structure
- `httpserver.c` — main server and threading logic
- `req.c` — request parsing and method handling
- `queue.c` / `queue.h` — thread-safe queue implementation
- `rwlock.c` / `rwlock.h` — reader-writer lock implementation
- `listener_socket.h` — provided library for managing sockets
- `asgn2_helper_funcs.h` — provided I/O helpers
- `request.h` / `response.h` — request and response definitions
- `Makefile` — build instructions

--------

## Build
Run the following command to build the server:

```bash
make

________

## Usage

Start the server on a port:

./httpserver <port>


Example:

./httpserver 8080

_______





