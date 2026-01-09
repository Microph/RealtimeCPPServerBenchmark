# CMakeList.txt : CMake project for RealtimeCPPGameServer, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

# Clones vcpkg repo
# Checkouts revision specified in baseline of default registry or builtin baseline
# (Optionally) Applies patches to that revision
# Triggers bootstrap of the executable
include("${CMAKE_SOURCE_DIR}/bootstrap_vcpkg.cmake")
bootstrap_vcpkg()

project ("RealtimeCPPGameServer")

# Use static links as much as possible so that we don't have to apk add them in Dockerfile later
set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_STATIC_RUNTIME ON)
find_package(Boost REQUIRED COMPONENTS asio)
find_package(cppzmq CONFIG REQUIRED)

# Add source to this project's executable.
add_executable (TCPZeroMQBroadcastServer "src/TCPZeroMQBroadcastServer.cpp")
add_executable (TCPZeroMQLoadTest "src/TCPZeroMQLoadTest.cpp")
add_executable (TCPSimpleBroadcastAsyncServer "src/TCPSimpleBroadcastAsyncServer.cpp")
add_executable (TCPSimpleBroadcastThreadPerClientServer "src/TCPSimpleBroadcastThreadPerClientServer.cpp")
add_executable (TCPSimpleBroadcastLoadTest "src/TCPSimpleBroadcastLoadTest.cpp")
add_executable (UDPSimpleBroadcastAsyncServer "src/UDPSimpleBroadcastAsyncServer.cpp")
add_executable (UDPSimpleBroadcastLoadTest "src/UDPSimpleBroadcastLoadTest.cpp")
add_executable (UDPSimpleBroadcastSO_REUSEPORTServer "src/UDPSimpleBroadcastSO_REUSEPORTServer.cpp")
add_executable (UDPSimpleMulticastLoadTest "src/UDPSimpleMulticastLoadTest.cpp")
add_executable (UDPSimpleMulticastServer "src/UDPSimpleMulticastServer.cpp")

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET TCPZeroMQBroadcastServer PROPERTY CXX_STANDARD 20)
  set_property(TARGET TCPZeroMQLoadTest PROPERTY CXX_STANDARD 20)
  set_property(TARGET TCPSimpleBroadcastAsyncServer PROPERTY CXX_STANDARD 20)
  set_property(TARGET TCPSimpleBroadcastThreadPerClientServer PROPERTY CXX_STANDARD 20)
  set_property(TARGET TCPSimpleBroadcastLoadTest PROPERTY CXX_STANDARD 20)
  set_property(TARGET UDPSimpleBroadcastAsyncServer PROPERTY CXX_STANDARD 20)
  set_property(TARGET UDPSimpleBroadcastLoadTest PROPERTY CXX_STANDARD 20)
  set_property(TARGET UDPSimpleBroadcastSO_REUSEPORTServer PROPERTY CXX_STANDARD 20)
  set_property(TARGET UDPSimpleMulticastLoadTest PROPERTY CXX_STANDARD 20)
  set_property(TARGET UDPSimpleMulticastServer PROPERTY CXX_STANDARD 20)
endif()

target_link_libraries(TCPZeroMQBroadcastServer PRIVATE Boost::asio)
target_link_libraries(TCPZeroMQBroadcastServer PRIVATE cppzmq cppzmq-static)
target_link_libraries(TCPZeroMQLoadTest PRIVATE Boost::asio)
target_link_libraries(TCPZeroMQLoadTest PRIVATE cppzmq cppzmq-static)
target_link_libraries(TCPSimpleBroadcastAsyncServer PRIVATE Boost::asio)
target_link_libraries(TCPSimpleBroadcastAsyncServer PRIVATE cppzmq cppzmq-static)
target_link_libraries(TCPSimpleBroadcastThreadPerClientServer PRIVATE Boost::asio)
target_link_libraries(TCPSimpleBroadcastThreadPerClientServer PRIVATE cppzmq cppzmq-static)
target_link_libraries(TCPSimpleBroadcastLoadTest PRIVATE Boost::asio)
target_link_libraries(TCPSimpleBroadcastLoadTest PRIVATE cppzmq cppzmq-static)
target_link_libraries(UDPSimpleBroadcastAsyncServer PRIVATE Boost::asio)
target_link_libraries(UDPSimpleBroadcastAsyncServer PRIVATE cppzmq cppzmq-static)
target_link_libraries(UDPSimpleBroadcastLoadTest PRIVATE Boost::asio)
target_link_libraries(UDPSimpleBroadcastLoadTest PRIVATE cppzmq cppzmq-static)
target_link_libraries(UDPSimpleBroadcastSO_REUSEPORTServer PRIVATE Boost::asio)
target_link_libraries(UDPSimpleBroadcastSO_REUSEPORTServer PRIVATE cppzmq cppzmq-static)
target_link_libraries(UDPSimpleMulticastLoadTest PRIVATE Boost::asio)
target_link_libraries(UDPSimpleMulticastLoadTest PRIVATE cppzmq cppzmq-static)
target_link_libraries(UDPSimpleMulticastServer PRIVATE Boost::asio)
target_link_libraries(UDPSimpleMulticastServer PRIVATE cppzmq cppzmq-static)