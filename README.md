# Benchmarking Concurrency Models for Realtime C++ Servers

In the domain of realtime C++ server development, the selection of a concurrency model is a critical architectural decision. It significantly affects code complexity, latency, and the scalability limits of the system.

To identify the best approach for a low-level netcode project, a series of load tests were conducted comparing different architectures, including **TCP Thread-Per-Connection**, **TCP Single Thread with Coroutines**, **ZeroMQ**, and various **UDP implementations**.

The results challenge common assumptions regarding "modern" asynchronous programming, demonstrating that for specific scales, traditional models continue to offer better performance.

## Evaluated Architectures

1.  **TCP Non-ZeroMQ + 1 Thread Per Connection:** The traditional blocking I/O model where every connected client spawns a dedicated OS thread.
2.  **TCP ZeroMQ + Single Thread with Coroutines:** A hybrid approach using ZeroMQ for efficient message queuing and background I/O, coupled with C++ coroutines for asynchronous application logic on a single main thread.
3.  **TCP Non-ZeroMQ + Single Thread with Coroutines:** A purely manual implementation using raw non-blocking sockets and an event loop driving coroutines on a single thread.
4.  **UDP + Single Thread with Coroutines:** A connectionless implementation using a single thread and coroutines.
5.  **UDP + Multithread + SO_REUSEPORT:** Utilizing the `SO_REUSEPORT` socket option to allow multiple threads to bind to the same port, distributing load at the kernel level.
6.  **UDP + Multicast:** Using IP multicast to broadcast updates to client groups.

## Test Environment

All tests were conducted on a single machine (Client and Server running locally).

*   **OS:** Ubuntu 24.04.3 LTS
*   **Hardware:** Acer Predator PHN16-71
*   **CPU:** 13th Gen Intel® Core™ i5-13500HX × 20
*   **RAM:** 32.0 GiB
*   **Storage:** 1TB NVMe SSD

## Load Test Methodology

1.  **Connection Synchronization:** The program ensures that all clients are connected before starting to send a message from each client.
2.  **Broadcast Echo:** The server echoes the message back to all connected clients.
3.  **RTT Tracking:** Each client tracks the Round-Trip Time (RTT) of their own message.
4.  **Timeout:** There is a 10-second timeout, so test data exceeding 10 seconds is not considered.

## Benchmark Results

The load tests measured latency (in microseconds) across 100 and 1000 concurrent clients. Lower is better.

### 1. Top Performer: UDP Multithread + SO_REUSEPORT
Leveraging kernel-level load balancing for UDP packets proved highly effective for the target concurrency levels.

*   **100 Clients:** ~2.9ms Average (2,872 µs)
*   **1000 Clients:** ~210ms Average (210,184 µs)

### 2. Standard TCP: Thread-Per-Connection
The traditional model of dedicating a thread to every connection remains highly competitive.

*   **100 Clients:** ~5ms Average (5,072 µs)
*   **1000 Clients:** ~213ms Average (213,050 µs)

### 3. UDP Single Thread + Coroutines
Performant at scale, although exhibiting higher latency than the multithreaded approach at lower concurrency.

*   **100 Clients:** ~14.5ms Average (14,537 µs)
*   **1000 Clients:** ~45ms Average (44,820 µs)

### 4. ZeroMQ + Coroutines
ZeroMQ performed moderately, occupying the middle ground. It offered better stability than the raw implementation but did not match the raw speed of the threaded model at this scale.

*   **100 Clients:** ~19ms Average (19,402 µs)
*   **1000 Clients:** ~844ms Average (844,780 µs)

### 5. Raw TCP Sockets + Coroutines
The manual implementation of single-threaded coroutines experienced the most significant challenges under load, exhibiting the highest latency and the most drastic degradation as client count increased.

*   **100 Clients:** ~41ms Average (41,025 µs)
*   **1000 Clients:** ~1.19s Average (1,194,890 µs)

### 6. Outlier: UDP Multicast
Multicast performance was poor in this test environment.

*   **100 Clients:** ~4.87s Average (4,874,520 µs)
*   **1000 Clients:** ~4.86s Average (4,863,360 µs)

## Analysis: Performance Factors

### 1. Parallelism vs. Concurrency
A significant observation is the performance gap at 1,000 clients. The **Thread-Per-Connection** model (Avg: 0.21s) was roughly **4x faster** than ZeroMQ (0.84s) and **5.6x faster** than raw coroutines (1.19s).

This is likely due to hardware utilization. In the "Single Thread with Coroutines" models, all application logic (packet parsing, application state updates) is serialized on a single CPU core. Even if the I/O is non-blocking, the CPU becomes the bottleneck. The Thread-Per-Connection model allows the operating system to schedule 1,000 threads across all available CPU cores, processing multiple client requests in parallel.

### 2. ZeroMQ Performance Characteristics
ZeroMQ outperformed the raw coroutine implementation because it is not strictly single-threaded. While the application logic ran on one thread, ZeroMQ uses a background I/O thread pool (configurable via `ZMQ_IO_THREADS`) to handle system calls and buffer management.

This offloading ensures that while the main thread processes a coroutine, ZeroMQ continues to retrieve data from the network in the background. The raw implementation likely handled both the `recv` calls and the logic on the same thread, leading to increased jitter and latency.

### 3. Context Switching Overhead
A common argument against Thread-Per-Connection is the overhead of context switching. However, modern Linux schedulers are highly efficient. At 1,000 threads, the context switch overhead is often negligible compared to the latency introduced by serializing 1,000 clients' worth of logic onto a single core. The "C10K problem" (where threads become unviable) typically applies to 10,000+ connections; at 1,000, threads often remain the most performant choice.

### 4. Effectiveness of SO_REUSEPORT
The **UDP Multithread** implementation utilized `SO_REUSEPORT`, which allows multiple sockets (and thus multiple threads) to bind to the same port. The Linux kernel then distributes incoming packets across these sockets. This effectively applies the parallelism of the "Thread-Per-Connection" model to UDP without requiring a complex userspace dispatcher, resulting in the lowest latency at 100 clients.

### 5. Practicality of Multicast
Using multicast is generally impractical for active user interaction because, in practice, each client requires a unique data stream tailored to their perspective. Multicast broadcasts identical data to all recipients, limiting its utility to scenarios like spectator groups where all clients view the same state.

## Conclusion

For a realtime server targeting approximately 100 concurrent users per instance, **UDP with SO_REUSEPORT** appears to be the optimal choice, offering the lowest latency (~2.9ms). **Thread-Per-Connection (TCP)** remains a highly viable alternative (~5ms) if reliable streams are required.

The **ZeroMQ + Coroutine** model shows promise for architectural simplicity, avoiding synchronization complexities. To address the performance gap, the ZeroMQ model would likely need to evolve into a **Thread-Pool + Coroutine** architecture, distributing the coroutines across multiple worker threads to match the parallelism of the threaded models.

## Installation & Usage

### Ubuntu Dependencies
```bash
sudo apt install build-essential cmake ninja-build pkg-config git curl zip unzip tar
```

### Building
Run CMake build at the project root:
```bash
cmake --preset linux-debug
cmake --build out/build/linux-debug
```

### Running
It is recommended to use **Visual Studio Code** as the IDE to run the launch options.

### Client-Server Pairs
Ensure you match the correct load test client with the corresponding server architecture:

*   **UDP Broadcast**
    *   Load Test: `UDPSimpleBroadcastLoadTest`
    *   Servers: `UDPSimpleBroadcastAsyncServer`, `UDPSimpleBroadcastSO_REUSEPORTServer`

*   **UDP Multicast**
    *   Load Test: `UDPSimpleMulticastLoadTest`
    *   Server: `UDPSimpleMulticastServer`

*   **TCP Broadcast**
    *   Load Test: `TCPSimpleBroadcastLoadTest`
    *   Servers: `TCPSimpleBroadcastAsyncServer`, `TCPSimpleBroadcastThreadPerClientServer`

*   **ZeroMQ**
    *   Load Test: `TCPZeroMQLoadTest`
    *   Server: `TCPZeroMQBroadcastServer`
