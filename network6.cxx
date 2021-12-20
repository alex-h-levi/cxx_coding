/*
   A very simple TCP socket server for v4 or v6
  */
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/epoll.h>
#include <iostream>
#include <errno.h>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <ctime>
#include <iomanip>
#include <regex>
#include <atomic>
#include <thread>
#include <chrono>
#include <netdb.h>
#include <charconv>

#define __FILENAME__ (__builtin_strrchr(__FILE__, '/') ? __builtin_strrchr(__FILE__, '/') + 1 : __FILE__)    // only show filename and not it's path (less clutter)
#define INFO(MSG) \
    time_now = std::time(nullptr);\
    std::clog << std::put_time(std::localtime(&time_now), "%y-%m-%d %OH:%OM:%OS") << " [INFO] " \
    << __FILENAME__ << "(" << __FUNCTION__ << ":" << __LINE__ << ") >> " \
    << MSG << endl
#define ERROR(MSG) \
    time_now = std::time(nullptr); \
    std::cerr << std::put_time(std::localtime(&time_now), "%y-%m-%d %OH:%OM:%OS") << " [ERROR] " \
    << __FILENAME__ << "(" << __FUNCTION__ << ":" << __LINE__ << ") >> " \
    << MSG << endl

static std::time_t time_now = std::time(nullptr);

constexpr int max_concurr_conn = 10;
constexpr int cli_conn_timeout = 10000;

using namespace std;

void log_error(string prefix) {
    string err_msg = strerror(errno);
    cerr << prefix << ": " << err_msg << endl;
}

atomic_bool srv_run{true};

void srv_thread(int port) {
    int srvPort = port;
    int rc;

    sockaddr_storage srv_sockaddr, cli_sockaddr;
    // It is safe to set the sockadd storage structures to 0 up to the full length
    memset(&srv_sockaddr, 0, sizeof(sockaddr_storage));
    memset(&cli_sockaddr, 0, sizeof(sockaddr_storage));
    // the sizes will be determined a bit later first trying IPv6 and falling back onto IPv4
    socklen_t srv_sockaddr_size = 0, cli_sockaddr_size = 0;

    int on = 1;
    int family;

    int cli_sock;
    char buf[INET6_ADDRSTRLEN + 128];
    
    int srv_sock = socket(AF_INET6, SOCK_STREAM, 0);
    if (srv_sock < 0) {
        // capture the error message which led to socket call to fail
        // it could be a number of reasons. Let's investigate first
        log_error("socket call with PF_INET6 failed.");
        srv_sock = socket(AF_INET, SOCK_STREAM, 0);
        if (srv_sock < 0)
        {
            log_error("socket call with PF_INET failed");
            cerr << "Give up and exit." << endl;
            exit (EXIT_FAILURE);
        }
        // Confirm running Address family IPv4
        family = AF_INET;
        // adjust the socket address structure match sockaddr_in
        srv_sockaddr_size = sizeof(struct sockaddr_in);
        auto psa = reinterpret_cast<sockaddr_in*>(&srv_sockaddr);
        psa->sin_family = AF_INET;
        psa->sin_addr.s_addr = htonl(INADDR_ANY);
        psa->sin_port = htons(srvPort);
    } else {
        // Confirm running address family IPv6
        family = AF_INET6;
        // adjust the socket address structure match sockaddr_in6
        srv_sockaddr_size = sizeof(struct sockaddr_in6);
        auto psa6 = reinterpret_cast<sockaddr_in6*>(&srv_sockaddr);
        psa6->sin6_family = AF_INET6;
        psa6->sin6_addr = in6addr_any;
        psa6->sin6_port = htons(srvPort);
        srv_sockaddr_size = sizeof(sockaddr_in6);
    }
    int soc_flags = fcntl(srv_sock, F_GETFL, 0);
    fcntl(srv_sock, F_SETFL, soc_flags | O_NONBLOCK);
    clog << "Server socket descriptor " << setw(8) << setfill('0') << srv_sock << endl;
    rc = setsockopt(srv_sock, SOL_SOCKET, SO_REUSEADDR|SO_REUSEPORT, &on, sizeof(on));

    // eh, eh we have sockaddr, sockaddr_in, sockaddr_in6, sockaddr_storage, but we always pass in sockaddr pointer
    rc = bind(srv_sock, (struct sockaddr *)&srv_sockaddr, srv_sockaddr_size);
    if (rc < 0) {
        log_error("socket bind failed");
        exit(1);
    }
    rc = listen(srv_sock, max_concurr_conn);
    if (rc < 0) {
        log_error("socket listen failed");
        exit(1);
    }

    epoll_event events[max_concurr_conn];
    int epoll_fd = epoll_create1(0);
    int conn_count = 0;
    while (srv_run) {
        // I will accept new connections only when the total num of active connections
        // is less than the total number of concurrent connections the kernel can handle
        // per process.
        if (conn_count < max_concurr_conn) {
            static struct epoll_event ev;
            cli_sock = accept(srv_sock,(struct sockaddr *)&cli_sockaddr, &cli_sockaddr_size);
            if (cli_sock == -1) {
                if (errno == EAGAIN) {
                    // Non blocking - wait again...
                    INFO("EAGAIN - will continue to wait for new events from clients...");
                }
            } else {
                // new connection. tell the kernel to add to its watch list.
                conn_count++;
                ev.events |= (EPOLLIN|EPOLLHUP|EPOLLRDHUP|EPOLLERR);
                ev.data.fd = cli_sock;
                int ectl = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, cli_sock, &ev);
                if (cli_sockaddr.ss_family == AF_INET) {
                    auto psa = reinterpret_cast<sockaddr_in*>(&cli_sockaddr);
                    inet_ntop(psa->sin_family, (struct sockaddr *)&psa->sin_addr, buf, sizeof(buf));
                } else {
                    auto psa = reinterpret_cast<sockaddr_in6*>(&cli_sockaddr);
                    inet_ntop(psa->sin6_family, (struct sockaddr *)&psa->sin6_addr, buf, sizeof(buf));
                    if (IN6_IS_ADDR_V4MAPPED(&psa->sin6_addr)) {
                        size_t sz = strlen(buf);
                        const char ipv4mapped[] = "+IN6_IS_ADDR_V4MAPPED";
                        if (sz + sizeof(ipv4mapped) < sizeof(buf)) {
                            strncpy(&buf[sz], ipv4mapped, sizeof(ipv4mapped));
                        }
                    }
                }
                INFO(buf);
            }
        }
        char buffer[1024];
        int num_events = epoll_wait(epoll_fd, events, max_concurr_conn, 0);
        if (num_events > 0) {
            for(int event = 0; event < num_events; event++) {
                if (events[event].events & EPOLLIN) {
                    memset(buffer, 0, sizeof(buffer));
                    int r = read(events[event].data.fd, buffer, sizeof(buffer));
                    if (r > 0) {
                        write(events[event].data.fd, "ACK", 3);
                        // as a rule - a separate message from the client [buybuy] to terminate
                        cmatch cm;
                        std::regex_match ("subject", std::regex("(sub)(.*)") );
                        if(regex_match(buffer, regex(R"(\[[BbUuYy]{6}\])"))) {
                            INFO("client disconnect request is comming next...");
                            struct epoll_event ev;
                            memcpy(&ev, &events[event], sizeof(epoll_event));
                            epoll_ctl(epoll_fd, EPOLL_CTL_DEL, events[event].data.fd, &ev);
                            close(events[event].data.fd);
                            conn_count--;
                        }
                        INFO(buffer);
                    }
                } else if (events[event].events & EPOLLHUP || events[event].events & EPOLLERR) {
                    // a disconnect hit detected - disconnect current client
                    INFO("client disconnect");
                    struct epoll_event ev;
                    memcpy(&ev, &events[event], sizeof(epoll_event));
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, events[event].data.fd, &ev);
                    close(events[event].data.fd);
                    conn_count--;
                }
            }
        } else {
            INFO("epoll_wait - time out [no new events]");
        }
    }
    INFO("server socket shutdown");
    shutdown(srv_sock, SHUT_RDWR);
    close(srv_sock);
    INFO("server thread terminated.");
}

void cli_thread(const char *psrv_addr, int srv_port) {
    INFO("client thread started.");
    sockaddr_storage srv_sockaddr;
    socklen_t srv_sockaddr_size = sizeof(sockaddr_storage);
    
    int family = AF_INET6;
    int cli_sock = socket(AF_INET6, SOCK_STREAM, 0);

    if (cli_sock < 0) {
        // capture the error message which led to socket call to fail
        // it could be a number of reasons. Let's investigate first
        log_error("socket call with PF_INET6 failed.");
        cli_sock = socket(AF_INET, SOCK_STREAM, 0);
        if (cli_sock < 0)
        {
            log_error("socket call with PF_INET failed");
            cerr << "Give up and exit." << endl;
            exit (EXIT_FAILURE);
        }
        // Confirm running Address family IPv4
        family = AF_INET6;
        // adjust the socket address structure match sockaddr_in
        srv_sockaddr_size = sizeof(struct sockaddr_in);
        auto psa = reinterpret_cast<sockaddr_in*>(&srv_sockaddr);
        psa->sin_family = AF_INET;
        psa->sin_addr.s_addr = htonl(INADDR_ANY);
        psa->sin_port = htons(srv_port);
    } else {
        // Confirm running address family IPv6
        family = AF_INET6;
        // adjust the socket address structure match sockaddr_in6
        srv_sockaddr_size = sizeof(struct sockaddr_in6);
        auto psa6 = reinterpret_cast<sockaddr_in6*>(&srv_sockaddr);
        psa6->sin6_family = AF_INET6;
        psa6->sin6_addr = in6addr_any;
        psa6->sin6_port = htons(srv_port);
        srv_sockaddr_size = sizeof(sockaddr_in6);
    }

    addrinfo hints;
    addrinfo *result, *rp;
    int sfd, s;
    size_t len;
    ssize_t nread;
    char buf[256];
    /* Obtain address(es) matching host/port. */

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET6;    /* Allow IPv4 or IPv6 */
    hints.ai_socktype = SOCK_STREAM; /* Datagram socket */
    hints.ai_flags = 0;
    hints.ai_protocol = 0;          /* Any protocol */

    char num_char[5 + sizeof(char)];
    to_chars(num_char, &num_char[5], srv_port);
    s = getaddrinfo(psrv_addr, num_char, &hints, &result);
    if (s != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
        exit(EXIT_FAILURE);
    }
   
    /* getaddrinfo() returns a list of address structures.
        Try each address until we successfully connect(2).
        If socket(2) (or connect(2)) fails, we (close the socket
        and) try the next address. */

    for (rp = result; rp != NULL; rp = rp->ai_next) {
        cli_sock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (cli_sock == -1)
            continue;
        if (connect(cli_sock, rp->ai_addr, rp->ai_addrlen) != -1)
            break;                  /* Success */
        close(cli_sock);
    }
    freeaddrinfo(result);       
    char msg_buf[256];
    memset(msg_buf, 0, 256);
    write(cli_sock, "[buybuy]", 8);
    int rb = read(cli_sock, msg_buf, 255);
    INFO(msg_buf);
    shutdown(cli_sock, SHUT_RDWR);
    close(cli_sock);
    INFO("client thread terminated.");
}

int main(int argc,const char **argv) {

    thread srv_thx{srv_thread, 5000};
    char srv_addr[] = "localhost";
    vector<thread> v_cli;
    const int max_cli_thx = 5;
    for (int i = 0; i < max_cli_thx; i++) {
        v_cli.emplace_back(thread{cli_thread, srv_addr, 5000});
    }

    for_each(v_cli.begin(), v_cli.end(), [&](thread& tx){
        if (tx.joinable())
            tx.join();
    });

    srv_run = false;

    srv_thx.join();
    INFO("main terminated.");
    return 0;
}
