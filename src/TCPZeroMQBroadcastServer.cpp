#include <zmq.hpp>
#include <string>
#include <iostream>
#include <thread>
#include <vector>
#include <unordered_set>
#include <boost/asio.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/posix/stream_descriptor.hpp>
#include <chrono>

using boost::asio::awaitable;
using boost::asio::co_spawn;
using boost::asio::detached;
using boost::asio::use_awaitable;
using boost::asio::io_context;

static std::unordered_set<std::string> connectedClients;

// RAII guard to ensure we release the FD from stream_descriptor
// so Asio doesn't close it (it's owned by ZMQ).
struct StreamDescriptorDetacher
{
  boost::asio::posix::stream_descriptor& sd;
  
  ~StreamDescriptorDetacher()
  { 
    sd.release(); 
  }
};

void broadcastMessage(zmq::socket_t& router, const std::string& message)
{
  for (const auto& clientId : connectedClients)
  {
    router.send(zmq::buffer(clientId), zmq::send_flags::sndmore);
    router.send(zmq::buffer(message), zmq::send_flags::none);
  }
}

awaitable<void> async_zmq_recv(zmq::socket_t& socket, boost::asio::posix::stream_descriptor& stream_desc, zmq::message_t& msg)
{
  // ZMQ_FD is edge-triggered, so we must loop:
  // 1. Check ZMQ_EVENTS for ZMQ_POLLIN.
  // 2. If readable, try recv.
  // 3. If not readable or recv returns EAGAIN, wait on the FD.
  while (true)
  {
    auto events = socket.get(zmq::sockopt::events);
    if (events & ZMQ_POLLIN) 
    {
      try 
      {
        auto res = socket.recv(msg, zmq::recv_flags::dontwait);
        if (res) 
        {
          co_return;
        }
      }
      catch (const zmq::error_t&)
      {
      }
    }

    co_await stream_desc.async_wait(boost::asio::posix::stream_descriptor::wait_read, use_awaitable);
  }
}

awaitable<void> messageLoop(io_context& asioCtx, zmq::socket_t& router)
{
  // Get the ZMQ file descriptor and wrap it in an asio stream_descriptor
  int fd = router.get(zmq::sockopt::fd);
  boost::asio::posix::stream_descriptor stream_desc(asioCtx, fd);

  // RAII guard to ensure we release the FD from stream_descriptor
  // so Asio doesn't close it (it's owned by ZMQ).
  StreamDescriptorDetacher guard{stream_desc};

  std::cout << "Server message loop started..." << std::endl;
  while (true)
  {
    zmq::message_t clientId;
    co_await async_zmq_recv(router, stream_desc, clientId);

    std::string id(static_cast<char*>(clientId.data()), clientId.size());
    if (connectedClients.insert(id).second)
    {
      std::cout << "Client connected: " << id << std::endl;
    }
    
    zmq::message_t message;
    co_await async_zmq_recv(router, stream_desc, message);

    if (message.size() == 0)
    {
      continue;
    }

    std::string msg(static_cast<char*>(message.data()), message.size());
    std::cout << "Received from " << id << ": " << msg << std::endl;

    auto start = std::chrono::high_resolution_clock::now();
    broadcastMessage(router, msg);
    auto end = std::chrono::high_resolution_clock::now();
    std::cout << "Broadcast took " << std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() << "us" << std::endl;
  }
}

int main(int argc, char* argv[])
{
  if (argc < 2)
  {
    std::cerr << "Usage: " << argv[0] << " <port>" << std::endl;
    return 1;
  }

  io_context ctx;
  std::string arg = argv[1];
  size_t pos;
  unsigned short port = static_cast<unsigned short>(std::stoi(arg, &pos));

  boost::asio::signal_set signals(ctx, SIGINT, SIGTERM);
  signals.async_wait([&](auto, auto) { ctx.stop(); });

  unsigned short port_copy = port;
  std::string endpoint = "tcp://*:" + std::to_string(port_copy);

  // prepare zmq router
  zmq::context_t zmqCtx(1);
  zmq::socket_t router(zmqCtx, zmq::socket_type::router);
  router.bind(endpoint);
  std::cout << "Server listening on port " << port << "..." << std::endl;

  co_spawn(ctx, messageLoop(ctx, router), detached);

  ctx.run();

  return 0;
}