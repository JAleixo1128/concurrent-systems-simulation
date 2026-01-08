# Concurrent Systems Simulation (C++)

This project is a multithreaded systems simulation implemented in C++.
It models multiple concurrent entities interacting with shared resources,
requiring careful synchronization to ensure correctness and avoid race conditions.

## Key Concepts
- Multithreading and concurrency
- Shared resource management
- Synchronization (mutexes / locks / semaphores)
- Deadlock and race condition prevention
- Systems-level debugging

## Implementation Overview
The system spawns multiple threads that operate concurrently while accessing
shared data structures. Synchronization mechanisms are used to ensure that
critical sections are protected and that the system remains in a valid state
under concurrent execution.

## Build & Run
```bash
chmod +x build.sh
./build.sh
./test_all.sh
