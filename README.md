# Hermes V3 — Async Notification System in C++20

High-performance notification backend with dual-pipeline architecture (internal thread pool + Redis Pub/Sub), Prometheus observability, and automated benchmarking.

Built from scratch in C++20. No frameworks, no abstractions — just clean, efficient code that handles millions of notifications.

---

## What's New in V3

```
V1 → V2: Added persistence, retries, priority queue, rate limiting
V2 → V3: Added Redis broker, dual pipelines, Prometheus metrics, Docker Compose
```

**V3 Features:**
- **Redis Pub/Sub broker** running side by side with the internal thread pool
- **Dual-pipeline architecture** — compare both approaches under identical load
- **Prometheus metrics** exposed at `/metrics/prometheus`
- **Prometheus + Grafana** in docker-compose for visualization
- **Automated benchmarking** script (Python asyncio)
- **GitHub Pages frontend** with live benchmark results and latency charts

---

## Quick Start

### With Docker Compose (Recommended)

```bash
git clone https://github.com/JeanCassiano/hermes-cpp.git
cd hermes-cpp
docker-compose up
```

This brings up:
- **Hermes** (C++ server) at `http://localhost:8080`
- **Redis** at `localhost:6379`
- **MailHog** (email catcher) at `http://localhost:8025`
- **Prometheus** at `http://localhost:9090`
- **Grafana** at `http://localhost:3000` (admin / admin)

### Without Docker (Build from source)

**Dependencies:**
```bash
# Ubuntu/Debian
sudo apt-get install -y cmake g++ libsqlite3-dev libcurl4-openssl-dev libhiredis-dev

# macOS (with Homebrew)
brew install cmake sqlite3 curl hiredis
```

**Build:**
```bash
mkdir build && cd build
cmake ..
cmake --build . --target run_server
```

**Run (need MailHog for dev):**
```bash
# Terminal 1: MailHog
docker run -d -p 1025:1025 -p 8025:8025 mailhog/mailhog

# Terminal 2: Hermes
export SMTP_HOST=localhost SMTP_PORT=1025 REDIS_HOST=localhost REDIS_PORT=6379
./build/run_server
```

Server runs at `http://localhost:8080`.

---

## Architecture

### High Level

```
POST /notify
  ├─ Internal Queue (V2)
  │   → Thread Pool (4 workers)
  │   → SMTP
  │   → DB + metrics_internal
  │
  └─ Redis Pub/Sub (V3)
      → Redis Dispatcher
      → Thread Pool (4 workers)
      → SMTP
      → DB + metrics_redis

Metrics aggregated → Prometheus → Grafana
```

### Data Flow

1. **Ingest** → POST /notify validates and saves event to DB
2. **Dual Publish** → Event pushed to internal queue AND published to Redis
3. **Dispatcher A** (Internal) → consumes from priority queue, rate-limited
4. **Dispatcher B** (Redis) → subscribes to "notifications" channel
5. **Processing** → Both workers send via SMTP, update DB, record metrics
6. **Retry** → On failure, exponential backoff (1s, 2s, 4s)
7. **DLQ** → After 3 retries, moved to dead letter queue

### Thread Safety

- **Database**: Single `std::mutex` protects all SQLite operations
- **Metrics**: `std::atomic` counters (lock-free)
- **Queues**: `std::priority_queue` + `std::mutex` + `std::condition_variable`
- **Rate Limiter**: `std::atomic` token count + refill thread

---

## API Endpoints

### POST /notify
Enqueue a notification.

```bash
curl -X POST http://localhost:8080/notify \
  -H "Content-Type: application/json" \
  -d '{
    "type": "order_confirmed",
    "email": "user@example.com",
    "priority": "critical"
  }'
```

Response:
```json
{"status": "queued", "id": 1, "event": "order_confirmed"}
```

Priority values: `critical`, `high`, `normal` (default).

### GET /events
List all events with status (pending / sent / failed).

```bash
curl http://localhost:8080/events
```

### GET /events/:id
Details of a single event.

```bash
curl http://localhost:8080/events/1
```

### GET /dlq
Dead Letter Queue (exhausted retries).

```bash
curl http://localhost:8080/dlq
```

### POST /dlq/:id/retry
Re-enqueue a dead-lettered event.

```bash
curl -X POST http://localhost:8080/dlq/1/retry
```

### GET /metrics
JSON format (both brokers).

```bash
curl http://localhost:8080/metrics | jq
```

Response:
```json
{
  "internal": {
    "sent": 42,
    "failed": 3,
    "dlq_total": 1,
    "rate_limit_hits": 7,
    "avg_latency_ms": 214
  },
  "redis": {
    "sent": 42,
    "failed": 3,
    "dlq_total": 1,
    "rate_limit_hits": 0,
    "avg_latency_ms": 312
  }
}
```

### GET /metrics/prometheus
Prometheus text format (scraped by Prometheus).

```bash
curl http://localhost:8080/metrics/prometheus
```

### GET /health
Simple health check.

```bash
curl http://localhost:8080/health
```

### DELETE /events
Clear all events and DLQ (dev only).

```bash
curl -X DELETE http://localhost:8080/events
```

---

## Benchmark

Run automated latency analysis:

```bash
python3 benchmark/bench.py \
  --url http://localhost:8080 \
  --n 1000 \
  --concurrency 50 \
  --output docs/results.json
```

This measures:
- **Latency**: p50, p95, p99, min, max, average
- **Throughput**: events/second
- **Per-broker metrics**: how many events each pipeline processed
- **Writes results** to `docs/results.json` (consumed by the frontend at `/docs/index.html`)

### Interpreting Results

**Dual Processing:** Each event is sent **twice** (once by each pipeline). This is intentional for V3 — it shows the cost and benefit of each broker approach under identical load.

**Internal (Thread Pool) typically shows:**
- Lower latency (no network hop)
- Slightly higher throughput
- Perfect reliability (no persistence needed)

**Redis typically shows:**
- Higher latency (network overhead)
- Survives server crashes
- Works across multiple processes

---

## Environment Variables

| Variable | Default | Description |
|---|---|---|
| `SMTP_HOST` | `localhost` | SMTP server host |
| `SMTP_PORT` | `1025` | SMTP port |
| `SMTP_USER` | _(empty)_ | SMTP username |
| `SMTP_PASS` | _(empty)_ | SMTP password |
| `SMTP_MAX_RETRIES` | `3` | Max retries before DLQ |
| `REDIS_HOST` | `localhost` | Redis server host |
| `REDIS_PORT` | `6379` | Redis port |

### Using Real SMTP

**Gmail:**
```bash
SMTP_HOST=smtp.gmail.com \
SMTP_PORT=587 \
SMTP_USER=you@gmail.com \
SMTP_PASS=your-app-password \
./build/run_server
```

**Brevo / SendGrid / AWS SES:**
```bash
SMTP_HOST=smtp-relay.brevo.com \
SMTP_PORT=587 \
SMTP_USER=you@company.com \
SMTP_PASS=api-key \
./build/run_server
```

---

## Database Schema

### Events Table
```sql
CREATE TABLE events (
    id            INTEGER PRIMARY KEY AUTOINCREMENT,
    type          TEXT NOT NULL,
    email         TEXT NOT NULL,
    priority      INTEGER DEFAULT 2,   -- 0=CRITICAL, 1=HIGH, 2=NORMAL
    status        TEXT DEFAULT 'pending',  -- pending | sent | failed
    retry_count   INTEGER DEFAULT 0,
    max_retries   INTEGER DEFAULT 3,
    next_retry_at DATETIME,
    last_error    TEXT,
    created_at    DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at    DATETIME DEFAULT CURRENT_TIMESTAMP
);
```

### Dead Letter Queue
```sql
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

## Architecture: What Was Built from Scratch

| Component | Implementation |
|---|---|
| **Thread Pool** | `std::thread` + `std::mutex` + `condition_variable` + `std::future` |
| **Priority Queue** | Thread-safe `std::priority_queue` (CRITICAL events first) |
| **Rate Limiter** | Token bucket with refill thread |
| **Retry Scheduler** | Min-heap of `{next_retry_at, Event}` with background thread |
| **Redis Broker** | `hiredis` C client with non-blocking subscribe + publish |
| **Logger** | Thread-safe with timestamps and thread ID |
| **Metrics** | `std::atomic` counters + latency histograms |
| **SMTP Sender** | libcurl with TLS support |

---

## Tech Stack

```
Language      C++20 (C++17 compatible)
HTTP          cpp-httplib (header-only)
JSON          nlohmann/json (header-only)
Database      SQLite3
Redis         hiredis (C client)
Email         libcurl → SMTP
Threading     std::thread, std::mutex, std::atomic
Build         CMake 3.14+
Observability Prometheus + Grafana
Benchmarking  Python 3.8+ with asyncio + aiohttp
```

---

## Performance Characteristics

### Latency (Typical)

**Internal (Thread Pool):**
- P50: ~0.1–0.5ms
- P99: ~1–2ms
- Under heavy load: remains stable (rate limited)

**Redis (Pub/Sub):**
- P50: ~1–2ms (network latency)
- P99: ~5–10ms (depends on network quality)
- Works across multiple instances

### Throughput

- **Rate Limiter**: 5 events/second (configurable)
- **Worker Pool**: 4 concurrent processors
- **MailHog**: Handles thousands of emails in dev
- **Real SMTP**: Depends on downstream server

### Scalability

- **Single Process**: thread pool + internal queue
- **Multi-Process**: Redis broker (multiple Hermes instances can subscribe to same Redis)
- **Disk**: SQLite persists ~100K events per GB

---

## Retry Flow

On SMTP failure:

```
Attempt 1 → fail → scheduled for 2s later
Attempt 2 → fail → scheduled for 4s later
Attempt 3 → fail → scheduled for 8s later
Attempt 4 → fail → moved to DLQ (no more retries)
```

DLQ items can be manually retried via `POST /dlq/:id/retry`.

---

## Design Decisions

### Why Dual Pipelines?

Both approaches are valid:
- **Thread pool**: Fast, simple, zero-config → perfect for single-instance services
- **Redis**: Durable, multi-process, standard → perfect for distributed systems

V3 doesn't pick a winner — it shows the trade-offs side by side. The benchmark proves the difference isn't theoretical.

### Why C++?

- **Explicit Performance**: No GC pauses, no hidden allocations
- **Low Latency**: Can sustain <1ms p99 latencies at scale
- **Resource Efficiency**: SQLite + thread pool fit on a small VPS
- **Learning Value**: See exactly what's happening under the hood

### Why Both Queues Exist?

The internal queue + Redis queue are independent. Each event gets enqueued to both simultaneously. This is intentional:
- Shows architectural impact in isolation
- Allows gradual migration (v1→v2→v3 pattern)
- Demonstrates the real cost of adding a broker

---

## Monitoring with Grafana

1. Start docker-compose: `docker-compose up`
2. Open Grafana: `http://localhost:3000` (admin / admin)
3. Add Prometheus data source: `http://prometheus:9090`
4. Import dashboard or create custom panels

**Key metrics:**
- `hermes_sent_total` — total events sent (per broker)
- `hermes_failed_total` — total failures (per broker)
- `hermes_avg_latency_ms` — average latency (per broker)
- `hermes_dlq_total` — dead letter queue size

---

## Testing

Unit tests for thread pool (Google Test):
```bash
cd build
cmake ..
cmake --build . --target run_tests
./run_tests
```

Manual integration test:
```bash
./build/run_server &
sleep 1
curl -X POST http://localhost:8080/notify \
  -d '{"type":"test","email":"a@b.com"}'
curl http://localhost:8080/events | jq
killall run_server
```

---

## Production Checklist

- [ ] Use real SMTP (Gmail, Brevo, etc.) instead of MailHog
- [ ] Enable Redis persistence (`RDB` or `AOF`)
- [ ] Set `SMTP_MAX_RETRIES` appropriately for your domain
- [ ] Monitor `DLQ` table — alerts when entries accumulate
- [ ] Monitor Prometheus metrics in Grafana
- [ ] Regular backups of `notifications.db`
- [ ] Run benchmark periodically to catch performance regressions

---

## Roadmap

- [ ] Batch SMTP (send multiple recipients per request)
- [ ] Custom retry strategies (pluggable backoff functions)
- [ ] Kafka as alternative broker
- [ ] Event templating (Handlebars / Jinja2)
- [ ] Web UI for DLQ management
- [ ] Multi-region failover

---

## License

MIT — see [LICENSE](LICENSE)

---

## Feedback

Found a bug or have a suggestion? [Open an issue on GitHub](https://github.com/JeanCassiano/hermes-cpp/issues).

---

**Built with 💜 in C++20**
