#!/usr/bin/env python3

import asyncio
import aiohttp
import json
import time
import statistics
import argparse
import sys
from pathlib import Path

async def benchmark(base_url, n_requests, concurrency, output_file):
    """
    Run benchmark: send N POST /notify requests with controlled concurrency.
    Measure latency, compute p50/p95/p99, and save results as JSON.
    """

    print(f"Starting benchmark:")
    print(f"  URL: {base_url}")
    print(f"  Requests: {n_requests}")
    print(f"  Concurrency: {concurrency}")
    print(f"  Output: {output_file}")
    print()

    # Fetch metrics before
    async with aiohttp.ClientSession() as session:
        try:
            async with session.get(f"{base_url}/metrics") as resp:
                before_metrics = await resp.json()
        except Exception as e:
            print(f"Failed to get initial metrics: {e}")
            before_metrics = None

    # Run benchmark
    latencies = []
    errors = 0
    start_time = time.time()

    async def send_request(session, request_num):
        payload = {
            "type": f"benchmark_test_{request_num}",
            "email": f"test_{request_num}@example.com",
            "priority": "normal"
        }
        try:
            req_start = time.time()
            async with session.post(
                f"{base_url}/notify",
                json=payload,
                timeout=aiohttp.ClientTimeout(total=10)
            ) as resp:
                req_latency = (time.time() - req_start) * 1000  # milliseconds
                if resp.status == 200:
                    data = await resp.json()
                    return req_latency, None
                else:
                    return None, f"HTTP {resp.status}"
        except asyncio.TimeoutError:
            return None, "Timeout"
        except Exception as e:
            return None, str(e)

    async def worker(session, queue):
        nonlocal errors
        while True:
            try:
                request_num = queue.get_nowait()
            except asyncio.QueueEmpty:
                break

            latency, error = await send_request(session, request_num)
            if latency is not None:
                latencies.append(latency)
            else:
                errors += 1
                print(f"  Error on request {request_num}: {error}")

    async with aiohttp.ClientSession() as session:
        queue = asyncio.Queue()
        for i in range(n_requests):
            queue.put_nowait(i)

        workers = [asyncio.create_task(worker(session, queue)) for _ in range(concurrency)]
        await asyncio.gather(*workers)

    total_time = time.time() - start_time

    # Fetch metrics after
    async with aiohttp.ClientSession() as session:
        try:
            async with session.get(f"{base_url}/metrics") as resp:
                after_metrics = await resp.json()
        except Exception as e:
            print(f"Failed to get final metrics: {e}")
            after_metrics = None

    # Compute statistics
    if latencies:
        latencies.sort()
        p50 = statistics.median(latencies)
        p95 = latencies[int(len(latencies) * 0.95)]
        p99 = latencies[int(len(latencies) * 0.99)]
        avg_latency = statistics.mean(latencies)
        min_latency = min(latencies)
        max_latency = max(latencies)
    else:
        p50 = p95 = p99 = avg_latency = min_latency = max_latency = 0

    throughput = len(latencies) / total_time if total_time > 0 else 0

    # Compute per-broker sent counts (if metrics available)
    internal_sent_before = 0
    internal_sent_after = 0
    redis_sent_before = 0
    redis_sent_after = 0

    if before_metrics and "internal" in before_metrics:
        internal_sent_before = before_metrics.get("internal", {}).get("sent", 0)
        redis_sent_before = before_metrics.get("redis", {}).get("sent", 0)

    if after_metrics and "internal" in after_metrics:
        internal_sent_after = after_metrics.get("internal", {}).get("sent", 0)
        redis_sent_after = after_metrics.get("redis", {}).get("sent", 0)

    internal_sent = internal_sent_after - internal_sent_before
    redis_sent = redis_sent_after - redis_sent_before

    results = {
        "timestamp": time.strftime("%Y-%m-%d %H:%M:%S"),
        "benchmark": {
            "requests": n_requests,
            "concurrency": concurrency,
            "successful": len(latencies),
            "errors": errors,
            "total_time_seconds": round(total_time, 2)
        },
        "latency_ms": {
            "p50": round(p50, 2),
            "p95": round(p95, 2),
            "p99": round(p99, 2),
            "avg": round(avg_latency, 2),
            "min": round(min_latency, 2),
            "max": round(max_latency, 2)
        },
        "throughput": {
            "events_per_second": round(throughput, 2)
        },
        "broker_metrics": {
            "internal_sent": internal_sent,
            "redis_sent": redis_sent
        }
    }

    # Save to file
    Path(output_file).parent.mkdir(parents=True, exist_ok=True)
    with open(output_file, 'w') as f:
        json.dump(results, f, indent=2)

    # Print results
    print(f"\nBenchmark Results:")
    print(f"  Successful: {len(latencies)}/{n_requests}")
    print(f"  Errors: {errors}")
    print(f"  Total time: {total_time:.2f}s")
    print()
    print(f"Latency (ms):")
    print(f"  P50:  {p50:.2f}")
    print(f"  P95:  {p95:.2f}")
    print(f"  P99:  {p99:.2f}")
    print(f"  Avg:  {avg_latency:.2f}")
    print(f"  Min:  {min_latency:.2f}")
    print(f"  Max:  {max_latency:.2f}")
    print()
    print(f"Throughput: {throughput:.2f} events/second")
    print()
    print(f"Broker Metrics:")
    print(f"  Internal: {internal_sent} sent")
    print(f"  Redis:    {redis_sent} sent")
    print()
    print(f"Results saved to: {output_file}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Hermes benchmark tool")
    parser.add_argument("--url", default="http://localhost:8080", help="Base URL of Hermes server")
    parser.add_argument("--n", type=int, default=1000, help="Number of requests to send")
    parser.add_argument("--concurrency", type=int, default=50, help="Number of concurrent requests")
    parser.add_argument("--output", default="docs/results.json", help="Output file for results (JSON)")

    args = parser.parse_args()

    try:
        asyncio.run(benchmark(args.url, args.n, args.concurrency, args.output))
    except KeyboardInterrupt:
        print("\nBenchmark cancelled by user")
        sys.exit(1)
    except Exception as e:
        print(f"Benchmark failed: {e}", file=sys.stderr)
        sys.exit(1)
