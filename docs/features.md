# ThreadPool — Features & Requirements

## Overview

A C++17 thread pool that manages a fixed set of worker threads,
distributes queued tasks among them, and shuts down gracefully.

---

## F1 — Construction

| ID   | Requirement                                                         |
| ---- | ------------------------------------------------------------------- |
| F1.1 | Accept a `size_t` parameter specifying the number of threads.       |
| F1.2 | Create exactly N worker threads in the constructor.                 |
| F1.3 | Each worker must start in an idle state (waiting for tasks).        |
| F1.4 | Reject construction with 0 threads (throw `std::invalid_argument`). |

---

## F2 — Task Enqueueing (`enqueue`)

| ID   | Requirement                                                                        |
| ---- | ---------------------------------------------------------------------------------- |
| F2.1 | Accept any callable with signature `void()`.                                       |
| F2.2 | Add tasks to the internal queue in a thread-safe manner.                           |
| F2.3 | Notify exactly one idle worker after enqueueing a task.                            |
| F2.4 | Reject enqueue operations after shutdown has started (throw `std::runtime_error`). |

---

## F3 — Parallel Execution

| ID   | Requirement                                                  |
| ---- | ------------------------------------------------------------ |
| F3.1 | Multiple tasks must execute simultaneously (up to N).        |
| F3.2 | Excess tasks remain queued until a worker becomes available. |
| F3.3 | Execution order across threads is not guaranteed.            |
| F3.4 | Task execution must occur outside the mutex lock.            |

---

## F4 — Graceful Shutdown

| ID   | Requirement                                                            |
| ---- | ---------------------------------------------------------------------- |
| F4.1 | The destructor sets `stop = true` and notifies all threads.            |
| F4.2 | Workers must finish currently running tasks before exiting.            |
| F4.3 | Pending tasks in the queue must be executed before shutdown completes. |
| F4.4 | The destructor must `.join()` all worker threads (no leaks).           |
| F4.5 | Destructor must never block indefinitely (deadlock-free).              |

---

## F5 — Thread Safety

| ID   | Requirement                                                     |
| ---- | --------------------------------------------------------------- |
| F5.1 | Queue access protected by `std::mutex`.                         |
| F5.2 | No data races on `stop`, `tasks`, or `workers`.                 |
| F5.3 | Use `std::condition_variable` to avoid busy waiting.            |
| F5.4 | Multiple threads calling `enqueue` simultaneously must be safe. |

---

## F6 — Resilience

| ID   | Requirement                                                                     |
| ---- | ------------------------------------------------------------------------------- |
| F6.1 | If a task throws an exception, the worker thread must not terminate.            |
| F6.2 | After an exception, the worker returns to the loop and processes the next task. |
| F6.3 | The thread pool remains functional after exceptions in any task.                |

---

## F7 — Return Values (Phase 2 — Future Work)

| ID   | Requirement                                                          |
| ---- | -------------------------------------------------------------------- |
| F7.1 | Support templated `enqueue` returning `std::future<T>`.              |
| F7.2 | The caller may block with `.get()` to retrieve the result.           |
| F7.3 | Exceptions thrown inside tasks must propagate through `std::future`. |

---