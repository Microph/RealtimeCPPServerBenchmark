#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <map>
#include <set>
#include <queue>
#include <condition_variable>
#include <memory>
#include <boost/asio.hpp>
#include <chrono>
#include <algorithm>

#ifdef _WIN32
#include <winsock2.h>
#else
#include <sys/socket.h>
#endif

using boost::asio::ip::udp;
using boost::asio::ip::make_address;

void run_server(unsigned short port, const std::string& multicast_group)
{
    boost::asio::io_context io_context;
    udp::socket socket(io_context);
    socket.open(udp::v4());
    socket.set_option(udp::socket::reuse_address(true));

#ifdef SO_REUSEPORT
    int opt = 1;
    if (setsockopt(socket.native_handle(), SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt)) < 0)
    {
        std::cerr << "Failed to set SO_REUSEPORT" << std::endl;
    }
#endif

    socket.bind(udp::endpoint(udp::v4(), port));

    // Define the multicast group endpoint
    udp::endpoint multicast_endpoint(make_address(multicast_group), port);
    std::cout << "Broadcasting to multicast group: " << multicast_endpoint << std::endl;

    char data[1024];
    try 
    {
        while (true) 
        {
            udp::endpoint sender_endpoint;
            size_t len = socket.receive_from(boost::asio::buffer(data), sender_endpoint);

            if (len > 0)
            {
                std::string msg(data, len);
                auto start = std::chrono::high_resolution_clock::now();
                socket.send_to(boost::asio::buffer(msg), multicast_endpoint);
                auto end = std::chrono::high_resolution_clock::now();
                std::cout << "Broadcast took " << std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() << "us" << std::endl;
            }
        }
    } 
    catch (std::exception& e) 
    {
        std::cerr << "Server error: " << e.what() << std::endl;
    }
}

int main(int argc, char* argv[]) 
{
    if (argc < 3) 
    {
        std::cerr << "Usage: " << argv[0] << " <port> <multicast_group>" << std::endl;
        return 1;
    }

    unsigned short port = static_cast<unsigned short>(std::stoi(argv[1]));
    std::string multicast_group = argv[2];
    std::cout << "Server listening on port " << port << "..." << std::endl;

    unsigned int thread_count = std::thread::hardware_concurrency();
    if (thread_count == 0) thread_count = 4;

    std::vector<std::thread> threads;
    for (unsigned int i = 0; i < thread_count; ++i)
    {
        threads.emplace_back(run_server, port, multicast_group);
    }

    for (auto& t : threads)
    {
        t.join();
    }

    return 0;
}