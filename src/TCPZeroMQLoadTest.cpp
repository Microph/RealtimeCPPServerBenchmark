#include <zmq.hpp>
#include <string>
#include <iostream>
#include <thread>
#include <vector>
#include <chrono>
#include <atomic>
#include <mutex>
#include <algorithm>
#include <numeric>
#include <latch>
#include <functional>

std::mutex latencies_mutex;
std::vector<long long> latencies;

void run_client(int id, const std::string& host, const std::string& port, std::latch& start_latch)
{
  bool connected = false;
  try 
  {
    // Create a dedicated context per thread to simulate distinct clients/processes
    zmq::context_t context(1);
    zmq::socket_t socket(context, zmq::socket_type::dealer);

    // Set a unique identity for the DEALER socket
    std::string identity = "load_client_" + std::to_string(id);
    socket.set(zmq::sockopt::routing_id, identity);

    socket.set(zmq::sockopt::probe_router, 1);
    std::string endpoint = "tcp://" + host + ":" + port;
    socket.connect(endpoint);

    // wait until all clients are connected before sending messages
    start_latch.arrive_and_wait();
    connected = true;

    auto start_time = std::chrono::steady_clock::now();

    // Create message with timestamp (compatible with the format used in ZeroMQClient.cpp)
    auto now = std::chrono::high_resolution_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()).count();
    std::string payload = std::to_string(timestamp) + "|Load Test Message from " + std::to_string(id);

    // Send message
    socket.send(zmq::buffer(payload), zmq::send_flags::none);

    bool foundMyMessage = false;

    while (true)
    {
      auto current_time = std::chrono::steady_clock::now();
      // keep connecting for up to 10 seconds to receive as many broadcasted messages as possible
      if (current_time - start_time >= std::chrono::seconds(10)) 
      {
        break;
      }

      int timeout = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::seconds(10) - (current_time - start_time)).count());
      if (timeout < 0) 
      {
        timeout = 0;
      }

      socket.set(zmq::sockopt::rcvtimeo, timeout);
      zmq::message_t reply;
      auto res = socket.recv(reply, zmq::recv_flags::none);

      if (res)
      {
        std::string msg(static_cast<char*>(reply.data()), reply.size());
        std::string suffix = "|Load Test Message from " + std::to_string(id);
        if (msg.find(suffix) != std::string::npos)
        {
          if (!foundMyMessage)
          {
            auto now_recv = std::chrono::high_resolution_clock::now();
            auto current = std::chrono::duration_cast<std::chrono::microseconds>(now_recv.time_since_epoch()).count();
            size_t delimiterPos = msg.find('|');
            if (delimiterPos != std::string::npos)
            {
              try 
              {
                long long sent_ts = std::stoll(msg.substr(0, delimiterPos));
                long long rtt = current - sent_ts;
                std::lock_guard<std::mutex> lock(latencies_mutex);
                // save latency data for this client
                latencies.push_back(rtt);
                foundMyMessage = true;
              } 
              catch (...) 
              {
              }
            }
          }
        }
      }
    }
  }
  catch (const std::exception& e) 
  {
    if (!connected) 
    {
      start_latch.count_down();
    }
    std::cerr << "Client " << id << " error: " << e.what() << std::endl;
  }
}

int main(int argc, char* argv[]) 
{
  if (argc < 4) 
  {
    std::cerr << "Usage: " << argv[0] << " <host> <port> <clients>" << std::endl;
    return 1;
  }

  std::string host = argv[1];
  std::string port = argv[2];
  int num_clients = std::stoi(argv[3]);

  std::cout << "Spawning " << num_clients << " clients connecting to " << host << ":" << port << "..." << std::endl;
  std::vector<std::thread> threads;
  threads.reserve(num_clients);

  auto start = std::chrono::high_resolution_clock::now();
  std::latch start_latch(num_clients);

  for (int i = 0; i < num_clients; ++i) 
  {
    threads.emplace_back(run_client, i, host, port, std::ref(start_latch));
  }

  for (auto& t : threads) 
  {
    if (t.joinable())
    {
      t.join();
    }
  }

  auto end = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

  std::cout << "Finished " << num_clients << " clients in " << duration << "ms" << std::endl;

  if (!latencies.empty())
  {
    long long min_val = *std::min_element(latencies.begin(), latencies.end());
    long long max_val = *std::max_element(latencies.begin(), latencies.end());
    long long sum = std::accumulate(latencies.begin(), latencies.end(), 0LL);
    double avg = static_cast<double>(sum) / latencies.size();
    std::cout << "Latency (us) -> Min: " << min_val << ", Max: " << max_val << ", Avg: " << avg << std::endl;
  }

  return 0;
}