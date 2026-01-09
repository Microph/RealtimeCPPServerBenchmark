#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <boost/asio.hpp>
#include <boost/asio/io_context.hpp>
#include <set>
#include <chrono>

using boost::asio::awaitable;
using boost::asio::co_spawn;
using boost::asio::detached;
using boost::asio::ip::udp;
using boost::asio::use_awaitable;
using boost::asio::io_context;
using namespace std;

static set<udp::endpoint> connectedEndpoints;

awaitable<void> listener(io_context& ctx, unsigned short port)
{
  udp::socket socket(ctx, { udp::v4(), port });
  cout << "Server listening on port " << port << "..." << endl;
  char data[1024];
  while (true)
  {
    udp::endpoint sender_endpoint;
    size_t length = co_await socket.async_receive_from(boost::asio::buffer(data), sender_endpoint, use_awaitable);

    if (connectedEndpoints.find(sender_endpoint) == connectedEndpoints.end())
    {
      connectedEndpoints.insert(sender_endpoint);
      cout << "Client connected: " << sender_endpoint << '\n';
    }

    if (length > 0)
    {
      string msg(data, length);
      auto start = std::chrono::high_resolution_clock::now();
      for (auto& recipient : connectedEndpoints)
      {
        try
        {
          co_await socket.async_send_to(boost::asio::buffer(msg), recipient, use_awaitable);
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