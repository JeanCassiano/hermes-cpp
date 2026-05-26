# hermes-cpp

Asynchronous notification backend built from scratch in C++20 — the same kind of system that sends order confirmations, password resets, and registration emails. No frameworks, no ready-made abstractions.

---

## Architecture

```
POST /notify
      │
      ▼
validate + save DB (pending)
      │
      ▼
PriorityEventQueue          ← CRITICAL events processed first
      │
      ▼
Dispatcher ── RateLimiter   ← token bucket, N events/second
      │
      ▼
ThreadPool (4 workers)
      │
      ▼
SMTP (libcurl → MailHog)
      │
      ├── SUCCESS → update status (sent) + metrics
      │
      └── FAIL
           ├── retry_count < max → RetryScheduler (2^n second backoff)
           └── retry_count >= max → Dead Letter Queue
```

---

## What was built from scratch

| Component | Implementation |
|---|---|
| **Thread Pool** | `std::thread` + `std::mutex` + `condition_variable` + `std::future` |
| **Priority Queue** | Thread-safe `std::priority_queue` with `mutex` + `condition_variable` |
| **Rate Limiter** | Token bucket with refill thread and `condition_variable` |
| **Retry Scheduler** | Min-heap of `{next_retry_at, Event}` with a dedicated thread |
| **Logger** | Thread-safe with timestamp and thread ID, zero dependencies |
| **Metrics** | `std::atomic` counters + latency histogram |
| **SMTP Sender** | libcurl with authentication and TLS support |

---

## Tech stack

```
Language      C++20
HTTP          cpp-httplib (header-only)
JSON          nlohmann/json (header-only)
Database      SQLite3
Email         libcurl → MailHog (dev) / real SMTP (prod)
Threads       std::thread, std::mutex, std::condition_variable, std::atomic
Build         CMake 3.14+
```

---

## Getting started

### Dependencies

```bash
sudo apt-get install -y cmake g++ libsqlite3-dev libcurl4-openssl-dev
```

### Build

```bash
git clone https://github.com/JeanCassiano/hermes-cpp
cd hermes-cpp
mkdir build && cd build
cmake ..
cmake --build . --target run_server
```

### Start MailHog (catches emails in dev, nothing actually sent)

```bash
docker run -d -p 1025:1025 -p 8025:8025 mailhog/mailhog
```

### Run the server

```bash
# from project root
SMTP_HOST=localhost SMTP_PORT=1025 ./build/run_server
```

Server runs at `http://localhost:8080`.  
MailHog UI at `http://localhost:8025`.

---

## API Reference

### `POST /notify`
Enqueues an event for delivery.

```bash
curl -s -X POST http://localhost:8080/notify \
  -H "Content-Type: application/json" \
  -d '{"type":"order_confirmed","email":"user@example.com","priority":"critical"}'
```

```json
{"status": "queued", "id": 1, "event": "order_confirmed"}
```

`priority` field: `critical`, `high`, or `normal` (default).

---

### `GET /events`
Lists all events and their current status.

```bash
curl http://localhost:8080/events
```

```json
[
  {
    "id": 1,
    "type": "order_confirmed",
    "email": "user@example.com",
    "priority": 0,
    "status": "sent",
    "retry_count": 0,
    "last_error": "",
    "created_at": "2026-05-25 13:00:00"
  }
]
```

Possible statuses: `pending` → `sent` | `failed`

---

### `GET /events/:id`
Returns details of a single event, including `updated_at`.

```bash
curl http://localhost:8080/events/1
```

---

### `GET /dlq`
Lists events that exhausted all retry attempts.

```bash
curl http://localhost:8080/dlq
```

```json
[
  {
    "id": 1,
    "event_id": 3,
    "type": "password_reset",
    "email": "user@example.com",
    "failure_reason": "Connection refused",
    "retry_count": 3,
    "moved_at": "2026-05-25 13:05:00"
  }
]
```

---

### `POST /dlq/:id/retry`
Re-enqueues a DLQ entry from scratch (retry_count reset to 0).

```bash
curl -X POST http://localhost:8080/dlq/1/retry
```

---

### `GET /metrics`
Real-time counter snapshot.

```bash
curl http://localhost:8080/metrics
```

```json
{
  "sent": 42,
  "failed": 3,
  "dlq_total": 1,
  "rate_limit_hits": 7,
  "avg_latency_ms": 214
}
```

---

### `GET /health`
```bash
curl http://localhost:8080/health
# ok
```

### `DELETE /events`
Clears all tables (dev only).

```bash
curl -X DELETE http://localhost:8080/events
```

---

## Retry flow

When SMTP fails, the event is rescheduled with exponential backoff:

```
Attempt 1 → fail → retry in 2s
Attempt 2 → fail → retry in 4s
Attempt 3 → fail → retry in 8s
Attempt 4 → fail → moved to DLQ
```

Default `max_retries`: 3. Configurable via `SMTP_MAX_RETRIES` env var.

---

## Environment variables

| Variable | Default | Description |
|---|---|---|
| `SMTP_HOST` | `localhost` | SMTP server host |
| `SMTP_PORT` | `1025` | SMTP port |
| `SMTP_USER` | _(empty)_ | SMTP username |
| `SMTP_PASS` | _(empty)_ | Password / App Password |
| `SMTP_MAX_RETRIES` | `3` | Attempts before moving to DLQ |

---

## Using a real SMTP server

The project uses MailHog by default — it captures all emails without actually sending them, which is ideal for development. To send real emails, just swap the env vars. The code doesn't change.

**Gmail (personal projects):**
```bash
# 1. Enable App Password on your Google account
# 2. Google Account → Security → App Passwords

SMTP_HOST=smtp.gmail.com \
SMTP_PORT=587 \
SMTP_USER=you@gmail.com \
SMTP_PASS=xxxx-xxxx-xxxx-xxxx \
./build/run_server
```

**Brevo / SendGrid / AWS SES (production):**
```bash
SMTP_HOST=smtp-relay.brevo.com \
SMTP_PORT=587 \
SMTP_USER=you@company.com \
SMTP_PASS=api-key \
./build/run_server
```

> In production, companies rarely call SMTP directly — they use libraries like `spring-mail` (Java), `ActionMailer` (Rails), or `nodemailer` (Node). What this project implements is exactly what those libraries do under the hood: enqueue, attempt, retry on failure, and record what couldn't be delivered.

---

## Database schema

```sql
CREATE TABLE events (
    id            INTEGER PRIMARY KEY AUTOINCREMENT,
    type          TEXT NOT NULL,
    email         TEXT NOT NULL,
    priority      INTEGER DEFAULT 2,   -- 0=CRITICAL, 1=HIGH, 2=NORMAL
    status        TEXT DEFAULT 'pending',
    retry_count   INTEGER DEFAULT 0,
    max_retries   INTEGER DEFAULT 3,
    next_retry_at DATETIME,
    last_error    TEXT,
    created_at    DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at    DATETIME DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE dlq (
    id             INTEGER PRIMARY KEY AUTOINCREMENT,
    event_id       INTEGER REFERENCES events(id),
    type           TEXT NOT NULL,
    email          TEXT NOT NULL,
    failure_reason TEXT,
    retry_count    INTEGER,
    moved_at       DATETIME DEFAULT CURRENT_TIMESTAMP
);
```

---

## Project structure

```
include/
  thread_pool.hpp       # thread pool with future and enqueue template
  priority_queue.hpp    # thread-safe queue ordered by priority
  rate_limiter.hpp      # token bucket with refill thread
  retry_scheduler.hpp   # min-heap of events scheduled for retry
  smtp_sender.hpp       # SMTP sender interface
  logger.hpp            # thread-safe logger (header-only)
  metrics.hpp           # atomic counters (header-only)
  event.hpp             # Event struct + Priority enum + EventComparator

src/
  server.cpp            # main: HTTP server + dispatcher + all endpoints
  thread_pool.cpp
  retry_scheduler.cpp
  smtp_sender.cpp       # libcurl implementation
  smtp_sender_mock.cpp  # mock with 20% failure rate (for tests without SMTP)
```
