#include "server.hpp"
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <csignal>
#include <algorithm>

namespace tr
{

    static constexpr std::size_t MAX_LINE = 1024 * 1024;

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

    static bool write_simple(int fd, std::string_view s)
    {
        std::string out;
        out.reserve(1 + s.size() + 2);
        out.push_back('+');
        out.append(s);
        out.append("\r\n");
        return write_all(fd, out);
    }

    static bool write_error(int fd, std::string_view s)
    {
        std::string out;
        out.reserve(1 + 4 + s.size() + 2);
        out.push_back('-');
        out.append("ERR ");
        out.append(s);
        out.append("\r\n");
        return write_all(fd, out);
    }

    static bool write_integer(int fd, long long n)
    {
        std::string out = ":" + std::to_string(n) + "\r\n";
        return write_all(fd, out);
    }

    static bool write_bulk(int fd, std::string_view s)
    {
        std::string out;
        out.reserve(1 + 20 + 2 + s.size() + 2);
        out.push_back('$');
        out.append(std::to_string(s.size()));
        out.append("\r\n");
        out.append(s);
        out.append("\r\n");
        return write_all(fd, out);
    }

    static bool write_null_bulk(int fd)
    {
        return write_all(fd, std::string("$-1\r\n"));
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
                    if (inbuf.empty())
                        break;

                    if (inbuf[0] == '*')
                    {
                        std::size_t consumed = 0;
                        std::vector<std::string> args;
                        auto st = tr::parse_resp_array(inbuf, consumed, args);

                        if (st == tr::RespParseStatus::NeedMore)
                        {
                            break; // wait for more bytes from ::read
                        }
                        if (st == tr::RespParseStatus::Error)
                        {
                            ::close(client_fd);
                            return;
                        }

                        // Ok
                        inbuf.erase(0, consumed);
                        if (args.empty())
                            continue;

                        // lowercase the command like you already do elsewhere
                        std::string cmd = args[0];
                        std::transform(cmd.begin(), cmd.end(), cmd.begin(),
                                       [](unsigned char c)
                                       { return static_cast<char>(std::tolower(c)); });

                        // 1) PING -> +PONG\r\n
                        if (cmd == "ping")
                        {
                            if (args.size() == 1)
                            {
                                if (!write_simple(client_fd, "PONG"))
                                {
                                    ::close(client_fd);
                                    return;
                                }
                            }
                            else
                            {
                                if (!write_error(client_fd, "ERR wrong number of arguments for 'ping'"))
                                {
                                    ::close(client_fd);
                                    return;
                                }
                            }
                            continue; // parse next frame if any
                        }

                        // 2) GET key -> $len\r\nvalue\r\n  or  $-1\r\n
                        if (cmd == "get")
                        {
                            if (args.size() != 2)
                            {
                                if (!write_error(client_fd, "ERR wrong number of arguments for 'get'"))
                                {
                                    ::close(client_fd);
                                    return;
                                }
                                continue;
                            }
                            auto v = db.get(args[1]);
                            if (v)
                            {
                                if (!write_bulk(client_fd, *v))
                                {
                                    ::close(client_fd);
                                    return;
                                }
                            }
                            else
                            {
                                if (!write_null_bulk(client_fd))
                                {
                                    ::close(client_fd);
                                    return;
                                }
                            }
                            continue;
                        }

                        // 3) SET key value -> +OK\r\n
                        if (cmd == "set")
                        {
                            if (args.size() != 3)
                            {
                                if (!write_error(client_fd, "wrong number of arguments for 'set'"))
                                {
                                    ::close(client_fd);
                                    return;
                                }
                                continue;
                            }
                            db.set(args[1], args[2]);
                            if (!write_simple(client_fd, "OK"))
                            {
                                ::close(client_fd);
                                return;
                            }
                            continue;
                        }

                        // 4) DEL key -> :1\r\n or :0\r\n
                        if (cmd == "del")
                        {
                            if (args.size() != 2)
                            {
                                if (!write_error(client_fd, "wrong number of arguments for 'del'"))
                                {
                                    ::close(client_fd);
                                    return;
                                }
                                continue;
                            }
                            bool deleted = db.del(args[1]);
                            if (!write_integer(client_fd, deleted ? 1 : 0))
                            {
                                ::close(client_fd);
                                return;
                            }
                            continue;
                        }

                        // 5) EXPIRE key seconds -> :1\r\n or :0\r\n
                        if (cmd == "expire")
                        {
                            if (args.size() != 3)
                            {
                                if (!write_error(client_fd, "wrong number of arguments for 'expire'"))
                                {
                                    ::close(client_fd);
                                    return;
                                }
                                continue;
                            }
                            try
                            {
                                long long seconds = std::stoll(args[2]);
                                bool result = db.expire(args[1], seconds);
                                if (!write_integer(client_fd, result ? 1 : 0))
                                {
                                    ::close(client_fd);
                                    return;
                                }
                            }
                            catch (const std::exception &)
                            {
                                if (!write_error(client_fd, "value is not an integer or out of range"))
                                {
                                    ::close(client_fd);
                                    return;
                                }
                            }
                            continue;
                        }

                        // 6) TTL key -> :-1\r\n or :-2\r\n or :seconds\r\n
                        if (cmd == "ttl")
                        {
                            if (args.size() != 2)
                            {
                                if (!write_error(client_fd, "wrong number of arguments for 'ttl'"))
                                {
                                    ::close(client_fd);
                                    return;
                                }
                                continue;
                            }
                            long long ttl = db.ttl(args[1]);
                            if (!write_integer(client_fd, ttl))
                            {
                                ::close(client_fd);
                                return;
                            }
                            continue;
                        }

                        // 7) INCRBY key increment -> :newvalue\r\n
                        if (cmd == "incrby")
                        {
                            if (args.size() != 3)
                            {
                                if (!write_error(client_fd, "wrong number of arguments for 'incrby'"))
                                {
                                    ::close(client_fd);
                                    return;
                                }
                                continue;
                            }
                            try
                            {
                                long long delta = std::stoll(args[2]);
                                auto result = db.incrby(args[1], delta);
                                if (result)
                                {
                                    if (!write_integer(client_fd, *result))
                                    {
                                        ::close(client_fd);
                                        return;
                                    }
                                }
                                else
                                {
                                    if (!write_error(client_fd, "value is not an integer or out of range"))
                                    {
                                        ::close(client_fd);
                                        return;
                                    }
                                }
                            }
                            catch (const std::exception &)
                            {
                                if (!write_error(client_fd, "value is not an integer or out of range"))
                                {
                                    ::close(client_fd);
                                    return;
                                }
                            }
                            continue;
                        }

                        // ...add SET/EXPIRE/TTL/INCR/etc. similarly, using write_simple / write_integer / write_error...
                        // Unknown command:
                        if (!write_error(client_fd, std::string("ERR unknown command '") + args[0] + "'"))
                        {
                            ::close(client_fd);
                            return;
                        }
                        continue;
                    }

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
                if (inbuf.size() > MAX_LINE)
                {
                    ::close(client_fd);
                    return;
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
        ::signal(SIGPIPE, SIG_IGN);
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