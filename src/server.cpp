#include "server.hpp"
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>

namespace tr
{
    bool write_all(int fd, const std::string &s)
    {
        size_t length = s.length();
        size_t sent = 0;

        while (sent < length)
        {
            ssize_t n = ::write(fd, s.data() + sent, length - sent);
            if (n > 0)
            {
                sent = sent + n;
                continue;
            }
            else if (n == 0)
            {
                return false;
            }
            else if (n < 0)
            {
                if (errno == EINTR)
                {
                    continue;
                }
                else if (errno == EPIPE || errno == ECONNRESET)
                {
                    return false;
                }
                else if (errno == EAGAIN || errno == EWOULDBLOCK)
                {
                    return false;
                }
                return false;
            }
        }
        return true;
    }

    void handle_client(int client_fd, KVStore &db)
    {
        std::string inbuf;
        char buf[4096];

        for (;;)
        {
            ssize_t n = ::read(client_fd, buf, sizeof(buf));
            if (n > 0)
            {
                inbuf.append(buf, n);
                for (;;)
                {
                    std::size_t lf = inbuf.find('\n');
                    if (lf == std::string::npos)
                        break;

                    std::string line = inbuf.substr(0, lf);
                    if (!line.empty() && (line.back() == '\r'))
                    {
                        line.pop_back();
                        if (line.empty())
                        {
                            inbuf.erase(0, lf + 1);
                            continue;
                        }
                    }

                    inbuf.erase(0, lf + 1);
                    std::vector<std::string> cmd = tr::parse_line(line);
                    if (cmd.empty())
                        continue;
                    if (cmd[0] == "EXIT" || cmd[0] == "exit")
                    {
                        ::close(client_fd);
                        return;
                    }
                    std::string result = tr::eval_command(db, cmd);
                    if (!result.empty())
                    {
                        std::string reply = result + "\n";
                        bool success = write_all(client_fd, reply);
                        if (!success)
                        {
                            ::close(client_fd);
                            return;
                        }
                    }
                }
            }
            else if (n == 0)
            {
                ::close(client_fd);
                break;
            }
            else
            {
                if (errno == EINTR)
                {
                    continue;
                }
                else
                {
                    std::cout << std::strerror(errno) << "\n";
                    ::close(client_fd);
                    break;
                }
            }
        }
    }

    int run_server(const uint16_t port)
    {
        int listen_fd = ::socket(AF_INET, SOCK_STREAM, 0); // Creates a new socket for IPv4
        int yes = 1;
        if (listen_fd < 0)
        {
            std::cerr << "socket() failed: " << std::strerror(errno) << "\n";
            return 1;
        }
        std::cout << "socket created" << "\n";
        int res = setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)); // Reuse the same socket after restarting
        if (res < 0)
        {
            std::cout << std::strerror(errno) << "\n";
            return 1;
        }
        std::cout << "reuseaddr set" << "\n";
        sockaddr_in addr{};
        addr.sin_family = AF_INET;                                                     // Use IPv4
        addr.sin_port = htons(port);                                                   // Make sure to be using big endian
        int ok = ::inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);                    // Parse and convert IP into binary form for OS
        int rc = ::bind(listen_fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)); // Claims the IP and port for the socket
        if (rc < 0)
        {
            std::cout << "bind() failed: " << std::strerror(errno) << "\n";
            ::close(listen_fd);
            return 1;
        }
        int lis = listen(listen_fd, 16); // Turn into listening socket and have up to 16 pending callers in queue
        if (lis < 0)
        {
            std::cout << "listen() failed: " << std::strerror(errno) << "\n";
            ::close(listen_fd);
            return 1;
        }
        tr::KVStore db;
        while (true)
        {
            int client_fd = ::accept(listen_fd, nullptr, nullptr);
            if (client_fd < 0)
            {
                if (errno == EINTR)
                {
                    continue;
                }
                else
                {
                    std::cout << "accept() failed: ...\n";
                    continue;
                }
            }
            else
            {
                handle_client(client_fd, db);
                // std::cout << "Client Connected \n";
                continue;
            }

            return 0;
        }

        return 0;
    }
}