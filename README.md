# Multi-threaded HTTP Server

## Overview

This project involves the development of a multi-threaded HTTP server that integrates functionality from previous assignments, with a focus on concurrency, synchronization, and efficient request handling.

The server can be executed using the following syntax:

[./httpserver [-t threads] <port>]

## Description

The primary challenge of this assignment lay in understanding and applying the core concurrency logic required to build a functional multi-threaded server. Once the design concepts were clearly defined, the implementation became more straightforward.

Collaboration was essential throughout the development process—particularly during the brainstorming and planning phases. Team discussions helped refine the approach and led to more effective solutions.

## Key Concepts

- **Thread-safe queue management** and use of **read-write locks (rwlocks)**
- **Linearization** of concurrent client requests
- **Total ordering** of tasks to maintain consistency
- Design and implementation of a **thread pool** for scalable request handling

## Learning Outcomes

This project provided hands-on experience in building concurrent systems and deepened understanding of:

- Synchronization mechanisms
- Parallel processing
- Systems-level architecture and design


