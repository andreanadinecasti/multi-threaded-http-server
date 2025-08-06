# 🧵 Multi-threaded HTTP Server

This project is a simple yet powerful **multi-threaded HTTP/1.0 server** written in C. It supports `GET` and `PUT` requests using concurrent threads and a thread-safe queue. Each file is protected by a reader-writer locking mechanism to ensure thread-safe file access across multiple connections.

> Developed as part of an operating systems/networking coursework assignment.

---

## 🚀 Features

- HTTP/1.0 support with basic `GET` and `PUT` functionality
- Thread pool with configurable number of worker threads
- Thread-safe request queue
- Per-URI locking using custom reader-writer locks (`rwlock`)
- Linearization and total ordering of requests
- Detailed audit logging
- POSIX-compliant and built for Unix-based systems

---

## 🛠️ Technologies & Concepts

This project leverages **Unix system programming** and **concurrency** techniques:

- 🧵 `pthreads` for multithreading
- 🔄 Thread-safe queues for producer-consumer design
- 🔐 Reader-writer locks for synchronized file access
- 📄 System calls: `open`, `read`, `write`, `fstat`, `close`, `access`
- 🌐 Networking: sockets, signals (`SIGPIPE`)
- 🔒 Mutexes for protecting shared data structures

---

## 🖥️ System Requirements

- OS: **Ubuntu** or any Unix-like system (macOS, Linux, WSL on Windows)
- Compiler: `gcc` or `clang` with POSIX support
- Tools: `curl` for basic testing

---

## 🧩 Compilation

### Using `gcc` manually:

```bash
gcc -Wall -pthread -o httpserver httpserver.c asgn2_helper_funcs.c connection.c response.c request.c queue.c rwlock.c
