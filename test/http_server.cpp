/* ReaPack: Package manager for REAPER
 * Copyright (C) 2015-2025  Christian Fillion
 * Copyright (C) 2026       Michael DeFreitas
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "http_server.hpp"

#include <stdexcept>
#include <thread>
#include <utility>

#ifdef _WIN32
#include <winsock2.h>
using TestSocket = SOCKET;
using SocketLength = int;
static constexpr TestSocket INVALID_TEST_SOCKET = INVALID_SOCKET;
static constexpr int SHUTDOWN_BOTH = SD_BOTH;
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
using TestSocket = int;
using SocketLength = socklen_t;
static constexpr TestSocket INVALID_TEST_SOCKET = -1;
static constexpr int SHUTDOWN_BOTH = SHUT_RDWR;
#endif

static void closeSocket(const TestSocket socket)
{
#ifdef _WIN32
  closesocket(socket);
#else
  close(socket);
#endif
}

class SocketRuntime {
public:
  SocketRuntime()
  {
#ifdef _WIN32
    WSADATA data;
    if(WSAStartup(MAKEWORD(2, 2), &data) != 0)
      throw std::runtime_error("could not initialize test HTTP sockets");
#endif
  }

  ~SocketRuntime()
  {
#ifdef _WIN32
    WSACleanup();
#endif
  }
};

struct HttpServer::Impl {
  explicit Impl(std::string response)
  {
    listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(listener == INVALID_TEST_SOCKET)
      throw std::runtime_error("could not create test HTTP socket");

    const int enabled = 1;
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR,
      reinterpret_cast<const char *>(&enabled), sizeof(enabled));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if(bind(listener, reinterpret_cast<sockaddr *>(&address),
        sizeof(address)) != 0 || listen(listener, 1) != 0) {
      closeSocket(listener);
      listener = INVALID_TEST_SOCKET;
      throw std::runtime_error("could not listen on test HTTP socket");
    }

    SocketLength addressSize = sizeof(address);
    if(getsockname(listener, reinterpret_cast<sockaddr *>(&address),
        &addressSize) != 0) {
      closeSocket(listener);
      listener = INVALID_TEST_SOCKET;
      throw std::runtime_error("could not inspect test HTTP socket");
    }
    port = ntohs(address.sin_port);

    thread = std::thread([this, response = std::move(response)] {
      const TestSocket client = accept(listener, nullptr, nullptr);
      if(client == INVALID_TEST_SOCKET)
        return;

      char request[2048];
      recv(client, request, sizeof(request), 0);

      size_t sent = 0;
      while(sent < response.size()) {
        const int size = send(client, response.data() + sent,
          static_cast<int>(response.size() - sent), 0);
        if(size <= 0)
          break;
        sent += static_cast<size_t>(size);
      }

      shutdown(client, SHUTDOWN_BOTH);
      closeSocket(client);
    });
  }

  ~Impl()
  {
    if(listener != INVALID_TEST_SOCKET)
      closeSocket(listener);
    if(thread.joinable())
      thread.join();
  }

  SocketRuntime socketRuntime;
  TestSocket listener{INVALID_TEST_SOCKET};
  unsigned short port{};
  std::thread thread;
};

HttpServer::HttpServer(std::string response)
  : m_impl(std::make_unique<Impl>(std::move(response)))
{
}

HttpServer::~HttpServer() = default;

std::string HttpServer::url(const std::string &path) const
{
  return "http://127.0.0.1:" + std::to_string(m_impl->port) + path;
}

std::string httpResponse(const std::string &status,
  const std::string &body, const std::string &headers)
{
  return "HTTP/1.1 " + status + "\r\n" + headers +
    "Content-Length: " + std::to_string(body.size()) + "\r\n"
    "Connection: close\r\n\r\n" + body;
}
