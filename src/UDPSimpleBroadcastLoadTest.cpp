#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <mutex>
#include <algorithm>
#include <numeric>
#include <atomic>
#include <boost/asio.hpp>
#include <functional>
#include <latch>

using boost::asio::ip::udp;

std::mutex latencies_mutex;
std::vector<long long> latencies;
std::atomic<int> errors{0};

void run_client(int id, const std::string& host, const std::string& port, std::latch& start_latch)
{
    bool connected = false;
    try
    {
        boost::asio::io_context io_context;
        udp::socket socket(io_context);
        udp::resolver resolver(io_context);
        boost::asio::connect(socket, resolver.resolve(host, port));
        
        // wait until all clients are connected before sending messages
        start_latch.arrive_and_wait();
        connected = true;

        // Prepare message: timestamp|id
        auto now = std::chrono::high_resolution_clock::now();
        long long timestamp = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()).count();
        std::string msg = std::to_string(timestamp) + "|" + std::to_string(id);

        socket.send(boost::asio::buffer(msg));

        char buffer[1024];
        boost::asio::steady_timer timer(io_context);
        timer.expires_after(std::chrono::seconds(10));
        
        bool foundMyMessage = false;
        
        // keep connecting for up to 10 seconds to receive as many broadcasted messages as possible
        timer.async_wait([&](const boost::system::error_code& ec) 
        {
            if (!ec) 
            {
                socket.close();
            }
        });

        std::function<void(boost::system::error_code, std::size_t)> read_handler;
        read_handler = [&](boost::system::error_code ec, std::size_t length) 
        {
            if (ec) 
            {
                if (!foundMyMessage && ec != boost::asio::error::operation_aborted) 
                {
                    errors++;
                }
                return;
            }

            std::string line(buffer, length);

            std::string suffix = "|" + std::to_string(id);
            if (line.find(suffix) != std::string::npos) 
            {
                auto end = std::chrono::high_resolution_clock::now();
                long long current = std::chrono::duration_cast<std::chrono::microseconds>(end.time_since_epoch()).count();
                size_t delim = line.find('|');
                if (delim != std::string::npos) 
                {
                    try 
                    {
                        long long sent_ts = std::stoll(line.substr(0, delim));
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
            
            socket.async_receive(boost::asio::buffer(buffer), read_handler);
        };

        socket.async_receive(boost::asio::buffer(buffer), read_handler);
        io_context.run();
    }
    catch (const std::exception& e)
    {
        if (!connected) 
        {
            start_latch.count_down();
        }
        errors++;
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
    std::cout << "Errors: " << errors << std::endl;

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