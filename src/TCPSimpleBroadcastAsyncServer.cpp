#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <boost/asio.hpp>
#include <boost/asio/io_context.hpp>
#include <unordered_set>
#include <chrono>

using boost::asio::awaitable;
using boost::asio::co_spawn;
using boost::asio::detached;
using boost::asio::ip::tcp;
using boost::asio::use_awaitable;
using boost::asio::io_context;
using namespace std;

static unordered_set< std::shared_ptr<tcp::socket> > connectedSockets;

awaitable<void> session(std::shared_ptr<tcp::socket> socket)
{
  connectedSockets.insert(socket);
  cout << "Client connected: " << socket->remote_endpoint() << '\n';
  try
  {
    string data;
    while (true)
    {
      data.clear();
      co_await boost::asio::async_read_until(*socket, boost::asio::dynamic_buffer(data), '\n', use_awaitable); // line-by-line reading
      if (data != "")
      {
        // Copy to avoid iterator invalidation and handle concurrent disconnects
        vector<std::shared_ptr<tcp::socket>> recipients(connectedSockets.begin(), connectedSockets.end());
        auto start = std::chrono::high_resolution_clock::now();
        for (auto& recipient : recipients)
        {
          try 
          {
            co_await boost::asio::async_write(*recipient, boost::asio::buffer(data), use_awaitable);
          } 
          catch (const std::exception& e) 
          {
            cerr << "Write error: " << e.what() << endl;
          }
        }
        auto end = std::chrono::high_resolution_clock::now();
        cout << "Broadcast took " << std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() << "us" << endl;
      }
    }
  }
  catch (const std::exception& e) 
  {
    cerr << "Session error: " << e.what() << endl;
  }

  connectedSockets.erase(socket);
  cout << "Client disconnected" << endl;
}

awaitable<void> listener(io_context& ctx, unsigned short port)
{
  tcp::acceptor acceptor(ctx, { tcp::v4(), port });
  cout << "Server listening on port " << port << "..." << endl;
  while (true)
  {
    tcp::socket socket = co_await acceptor.async_accept(use_awaitable);
    auto socketPtr = std::make_shared<tcp::socket>(move(socket));
    co_spawn(ctx, session(socketPtr), detached);
  }
}

int main(int argc, char* argv[])
{
  if (argc < 2)
  {
    cerr << "Usage: " << argv[0] << " <port>" << endl;
    return 1;
  }

  io_context ctx;
  string arg = argv[1];
  size_t pos;
  unsigned short port = stoi(arg, &pos);

  boost::asio::signal_set signals(ctx, SIGINT, SIGTERM);
  signals.async_wait([&](auto, auto) { ctx.stop(); });
  auto listen = listener(ctx, port);
  co_spawn(ctx, move(listen), boost::asio::detached);
  ctx.run();
}