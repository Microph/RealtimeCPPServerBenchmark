#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <unordered_set>
#include <memory>
#include <boost/asio.hpp>
#include <chrono>

using boost::asio::ip::tcp;

struct Client {
    tcp::socket socket;
    std::mutex mutex;
    Client(boost::asio::io_context& ctx) : socket(ctx) {}
};

std::mutex clients_mutex;
std::unordered_set<std::shared_ptr<Client>> clients;

void session(std::shared_ptr<Client> client) 
{
    try 
    {
        {
            std::lock_guard<std::mutex> lock(clients_mutex);
            clients.insert(client);
        }
        std::cout << "Client connected: " << client->socket.remote_endpoint() << std::endl;

        boost::asio::streambuf buffer;
        while (true) 
        {
            boost::system::error_code ec;
            size_t len = boost::asio::read_until(client->socket, buffer, '\n', ec);
            
            if (ec) 
            {
                break; 
            }

            std::string msg(boost::asio::buffers_begin(buffer.data()), boost::asio::buffers_begin(buffer.data()) + len);
            buffer.consume(len);

            if (!msg.empty()) 
            {
                // copy current clients to avoid holding the lock during writes
                std::vector<std::shared_ptr<Client>> current_clients;
                {
                    std::lock_guard<std::mutex> lock(clients_mutex);
                    current_clients.reserve(clients.size());
                    current_clients.insert(current_clients.end(), clients.begin(), clients.end());
                }

                auto start = std::chrono::high_resolution_clock::now();
                for (auto& recipient : current_clients) 
                {
                    try 
                    {
                        std::lock_guard<std::mutex> lock(recipient->mutex);
                        boost::asio::write(recipient->socket, boost::asio::buffer(msg));
                    } 
                    catch (...) 
                    {
                        // Ignore write errors; client might be disconnected
                    }
                }
                auto end = std::chrono::high_resolution_clock::now();
                std::cout << "Broadcast took " << std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() << "us" << std::endl;
            }
        }
    } 
    catch (std::exception& e) 
    {
        std::cerr << "Exception in session: " << e.what() << std::endl;
    }

    {
        std::lock_guard<std::mutex> lock(clients_mutex);
        clients.erase(client);
    }
    std::cout << "Client disconnected" << std::endl;
}

int main(int argc, char* argv[]) 
{
    if (argc < 2) 
    {
        std::cerr << "Usage: " << argv[0] << " <port>" << std::endl;
        return 1;
    }

    unsigned short port = static_cast<unsigned short>(std::stoi(argv[1]));
    boost::asio::io_context io_context;
    tcp::acceptor acceptor(io_context, tcp::endpoint(tcp::v4(), port));

    std::cout << "Server listening on port " << port << "..." << std::endl;

    try 
    {
        while (true) 
        {
            auto client = std::make_shared<Client>(io_context);
            acceptor.accept(client->socket);
            std::thread(session, client).detach();
        }
    } 
    catch (std::exception& e) 
    {
        std::cerr << "Server error: " << e.what() << std::endl;
    }

    return 0;
}