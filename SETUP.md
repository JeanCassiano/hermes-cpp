# Setup Instructions for Hermes V3

## Local Development Setup

### Prerequisites

#### Linux (Ubuntu/Debian)

```bash
sudo apt-get update
sudo apt-get install -y \
  cmake \
  g++ \
  libsqlite3-dev \
  libcurl4-openssl-dev \
  libhiredis-dev
```

#### macOS

```bash
brew install cmake sqlite3 curl hiredis
```

### Build from Source

```bash
git clone https://github.com/JeanCassiano/hermes-cpp.git
cd hermes-cpp

mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . --target run_server
```

The executable will be at `./build/run_server`.

### Run Locally

**Terminal 1 — Start MailHog (email catcher for dev):**
```bash
docker run -d -p 1025:1025 -p 8025:8025 mailhog/mailhog
```

**Terminal 2 — Start Redis:**
```bash
docker run -d -p 6379:6379 redis:7-alpine
```

**Terminal 3 — Start Hermes:**
```bash
cd build
export SMTP_HOST=localhost SMTP_PORT=1025 REDIS_HOST=localhost REDIS_PORT=6379
./run_server
```

Server runs at `http://localhost:8080`.

### Test

```bash
# Send a test notification
curl -X POST http://localhost:8080/notify \
  -H "Content-Type: application/json" \
  -d '{"type":"test","email":"user@example.com"}'

# View all events
curl http://localhost:8080/events | jq

# Check metrics
curl http://localhost:8080/metrics | jq
```

---

## Docker Compose (Recommended)

**One command brings up the entire stack:**

```bash
docker-compose up
```

This starts:
- **Hermes** at `http://localhost:8080`
- **Redis** at `localhost:6379`
- **MailHog** at `http://localhost:8025`
- **Prometheus** at `http://localhost:9090`
- **Grafana** at `http://localhost:3000` (admin / admin)

**Stop everything:**
```bash
docker-compose down
```

**Clear volumes (databases):**
```bash
docker-compose down -v
```

---

## Running Benchmarks

### Prerequisites

```bash
pip3 install aiohttp
```

### Run Benchmark

```bash
python3 benchmark/bench.py \
  --url http://localhost:8080 \
  --n 1000 \
  --concurrency 50 \
  --output docs/results.json
```

Results are saved to `docs/results.json` and automatically displayed at `docs/index.html`.

### Benchmark Options

```
--url URL              Base URL of Hermes server (default: http://localhost:8080)
--n N                  Total requests to send (default: 1000)
--concurrency N        Concurrent requests (default: 50)
--output FILE          Output file for results (default: docs/results.json)
```

### Example: Full Benchmark Suite

```bash
# Run benchmark with 5000 requests, 100 concurrent
python3 benchmark/bench.py --n 5000 --concurrency 100 --output docs/results.json

# Wait for results...
# Results saved to docs/results.json

# View results
cat docs/results.json | jq

# Open frontend to see charts
open docs/index.html  # macOS
xdg-open docs/index.html  # Linux
```

---

## Production Deployment

### Using Real SMTP

Set environment variables before running:

```bash
export SMTP_HOST=smtp.gmail.com
export SMTP_PORT=587
export SMTP_USER=your-email@gmail.com
export SMTP_PASS=your-app-password
export REDIS_HOST=redis.example.com
export REDIS_PORT=6379

./build/run_server
```

### Docker Production Deployment

1. Build your own image:
```bash
docker build -t hermes:latest .
```

2. Run with persistent storage:
```bash
docker run -d \
  --name hermes \
  -p 8080:8080 \
  -v hermes-db:/app \
  -e SMTP_HOST=smtp.gmail.com \
  -e SMTP_PORT=587 \
  -e SMTP_USER=your-email@gmail.com \
  -e SMTP_PASS=your-app-password \
  -e REDIS_HOST=redis \
  -e REDIS_PORT=6379 \
  hermes:latest
```

3. For full stack with Prometheus + Grafana, use docker-compose (see above).

---

## Troubleshooting

### Build fails: `hiredis/hiredis.h: No such file or directory`

**Solution:** Install libhiredis-dev

```bash
# Ubuntu/Debian
sudo apt-get install libhiredis-dev

# macOS
brew install hiredis
```

### "Redis connection refused"

**Solution:** Make sure Redis is running

```bash
# Check if Redis is up
redis-cli ping

# Or start it
docker run -d -p 6379:6379 redis:7-alpine
```

### "Connection to SMTP failed"

**Solution:** Make sure MailHog (dev) or real SMTP server is running and accessible

```bash
# Dev: start MailHog
docker run -d -p 1025:1025 -p 8025:8025 mailhog/mailhog

# Verify
telnet localhost 1025
```

### Benchmark script fails with "aiohttp not found"

**Solution:** Install aiohttp

```bash
pip3 install aiohttp
```

---

## Next Steps

1. **Read the architecture**: Check [README.md](README.md) for design decisions
2. **Explore the code**: Start with `src/server.cpp` for the dual-pipeline logic
3. **Run the benchmark**: See real latency measurements
4. **View the frontend**: Open `docs/index.html` to see results with charts
5. **Monitor with Grafana**: Use docker-compose to spin up monitoring stack

---

## Support

Found an issue? [Open an issue on GitHub](https://github.com/JeanCassiano/hermes-cpp/issues).
