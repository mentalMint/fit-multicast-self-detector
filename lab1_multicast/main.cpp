#include <iostream>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <cstring>
#include <netdb.h>
#include <sys/types.h>
#include <csignal>
#include <algorithm>
#include <map>
#include <net/if.h>
#include <sys/ioctl.h>

#define PORT 12000
#define BUF_SIZE 1
#define PERIOD_SEC 3
#define CONNECTION_TIME_MICROSEC 100000
#define INTERFACE "eth0"
#define STATUS_DEFAULT 0
#define STATUS_CONNECT 1
#define STATUS_EXIT 2
#define USAGE "Usage: execute IPADDR\nWhere: IPADDR { IPv4 multicast group address | IPv6 multicast group address }\n"

int socket_fd;
struct sockaddr_in socket_address{};
struct sockaddr_in6 socket_address6{};
struct ip_mreq group{};
struct ipv6_mreq group6{};
int addrlen;
int send_buf[BUF_SIZE];

struct StringsComparator {
    bool operator()(const char* a, const char* b) const {
        return strcmp(a, b) < 0;
    }
};

void print_map(const std::map<char*, bool, StringsComparator> &map) {
    for (auto n: map) {
        printf("%s\n", n.first);
    }
}

void set_select(fd_set* readfds, struct timeval* tv, bool connection) {
    if (FD_ISSET(socket_fd, readfds) == 0) {
        FD_ZERO(readfds);
        FD_SET(socket_fd, readfds);
    }
    if (connection) {
        tv->tv_sec = 0;
        tv->tv_usec = CONNECTION_TIME_MICROSEC;
    } else {
        tv->tv_sec = PERIOD_SEC;
        tv->tv_usec = 0;
    }
}

void close_all() {
    close(socket_fd);
}

void leave() {
    send_buf[0] = STATUS_EXIT;
    if (-1 == sendto(socket_fd, send_buf, BUF_SIZE, 0,
                     (struct sockaddr*) &(socket_address), (socklen_t) addrlen)) {
        perror("sendto");
        close_all();
        exit(EXIT_FAILURE);
    }
    printf("\n===Exit===\n");
    close_all();
    exit(EXIT_SUCCESS);
}

void send_datagram(struct sockaddr* address) {
    if (-1 == sendto(socket_fd, send_buf, BUF_SIZE, 0,
                     address, (socklen_t) sizeof(*address))) {
        perror("sendto");
        close_all();
        exit(EXIT_FAILURE);
    }
}

void set_addr(int ip_version) {
    if (ip_version == AF_INET) {
        socket_address.sin_addr.s_addr = group.imr_multiaddr.s_addr;
        addrlen = sizeof(socket_address);
    } else {
        socket_address6.sin6_addr = group6.ipv6mr_multiaddr;
        addrlen = sizeof(socket_address6);
    }
}

void remove_offline_users(std::map<char*, bool, StringsComparator> &users_online) {
    bool some_users_left = false;
    for (auto user = users_online.cbegin(); user != users_online.cend();) {
        if (!user->second) {
            free(user->first);
            user = users_online.erase(user);
            some_users_left = true;
        } else {
            ++user;
        }
    }

    for (auto &user: users_online) {
        user.second = false;
    }
    if (some_users_left) {
        printf("==Some users left==\n");
        printf("===Users online===\n");
        print_map(users_online);
        printf("==================\n\n");
        fflush(stdout);
    }
}

void check_need_for_send(struct sockaddr* address, struct timespec* timer1_start, struct timespec* timer1_end,
                         const int* recv_buf, int ip_version) {
    clock_gettime(CLOCK_MONOTONIC_RAW, timer1_end);
    if (timer1_end->tv_sec - timer1_start->tv_sec >= PERIOD_SEC || recv_buf[0] == STATUS_CONNECT) {
        send_datagram(address);
        set_addr(ip_version);
        clock_gettime(CLOCK_MONOTONIC_RAW, timer1_start);
    }
}

void check_remove_timer(struct timespec* timer2_start, struct timespec* timer2_end,
                        std::map<char*, bool, StringsComparator> &users_online) {
    clock_gettime(CLOCK_MONOTONIC_RAW, timer2_end);
    if (timer2_end->tv_sec - timer2_start->tv_sec >= PERIOD_SEC * 3) {
        remove_offline_users(users_online);
        clock_gettime(CLOCK_MONOTONIC_RAW, timer2_start);
    }
}

void print_users_online(const std::map<char*, bool, StringsComparator> &users_online) {
    printf("===Users online===\n");
    print_map(users_online);
    printf("==================\n\n");
    fflush(stdout);
}

void check_connection_timer(bool* connection, struct timespec* timer3_start, struct timespec* timer3_end,
                            const std::map<char*, bool, StringsComparator> &users_online) {
    if (*connection) {
        clock_gettime(CLOCK_MONOTONIC_RAW, timer3_end);
        if (timer3_end->tv_nsec - timer3_start->tv_nsec > CONNECTION_TIME_MICROSEC * 1000) {
            print_users_online(users_online);
            *connection = false;
        }
    }
}

int get_ip_version(const char* src) {
    char buf[16];
    if (inet_pton(AF_INET, src, buf)) {
        return AF_INET;
    } else if (inet_pton(AF_INET6, src, buf)) {
        return AF_INET6;
    }
    return -1;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Wrong number of arguments.\n\n");
        fprintf(stderr, USAGE);
        return EXIT_FAILURE;
    }

    char* group_address = (char*) malloc(INET6_ADDRSTRLEN);
    strcpy(group_address, argv[1]);

    int ip_version = get_ip_version(group_address);
    if (ip_version == -1) {
        fprintf(stderr, "Wrong input.\n\n");
        fprintf(stderr, USAGE);
        return EXIT_FAILURE;
    }
    char* user_address_tmp;
    char* host_address;
    if (ip_version == AF_INET) {
        socket_fd = socket(ip_version, SOCK_DGRAM, 0);
        if (socket_fd < 0) {
            perror("socket");
            return EXIT_FAILURE;
        }

        struct ifreq interface{};
        interface.ifr_addr.sa_family = ip_version;
        strncpy(interface.ifr_name, INTERFACE, IFNAMSIZ - 1);
        if (ioctl(socket_fd, SIOCGIFADDR, &interface) == -1) {
            perror("ioctl");
            close_all();
            return EXIT_FAILURE;
        }
        user_address_tmp = inet_ntoa(((struct sockaddr_in*) &interface.ifr_addr)->sin_addr);
        host_address = (char*) malloc(INET6_ADDRSTRLEN);
        strcpy(host_address, user_address_tmp);
        printf("Your IP-address: %s\n", host_address);

        const int optval = 1;
        if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) != 0) {
            perror("setsockopt. Reuse address");
            return EXIT_FAILURE;
        }

        bzero(&socket_address, sizeof(socket_address));
        socket_address.sin_family = ip_version;
        socket_address.sin_port = htons(PORT);

        if (0 != bind(socket_fd, (sockaddr*) &socket_address, sizeof(socket_address))) {
            perror("bind");
            return EXIT_FAILURE;
        }

        if (0 == inet_aton(group_address, &(group.imr_multiaddr))) {
            fprintf(stderr, "Invalid address\n");
            return EXIT_FAILURE;
        }
        group.imr_interface.s_addr = htonl(INADDR_ANY);

        if (setsockopt(socket_fd, ip_version == AF_INET ? IPPROTO_IP : IPPROTO_IPV6, IP_ADD_MEMBERSHIP,
                       &group, sizeof(group)) != 0) {
            perror("setsockopt. Ip add membership");
            return EXIT_FAILURE;
        }

        memset(send_buf, 0, BUF_SIZE);
        if (SIG_ERR == signal(SIGINT, (__sighandler_t) leave)) {
            perror("signal");
            close_all();
            return EXIT_FAILURE;
        }
        send_buf[0] = STATUS_CONNECT;
        set_addr(ip_version);
        send_datagram((sockaddr*) (&socket_address));
        send_buf[0] = STATUS_DEFAULT;
        int recv_buf[BUF_SIZE];
        memset(recv_buf, 0, BUF_SIZE);
        fd_set read_fds;

        std::map<char*, bool, StringsComparator> users_online;
        bool connection = true;
        struct timeval tv{};
        struct timespec timer1_start{}, timer1_end{};
        struct timespec timer2_start{}, timer2_end{};
        struct timespec timer3_start{}, timer3_end{};
        clock_gettime(CLOCK_MONOTONIC_RAW, &timer1_start);
        clock_gettime(CLOCK_MONOTONIC_RAW, &timer2_start);
        clock_gettime(CLOCK_MONOTONIC_RAW, &timer3_start);

        while (true) {
            set_addr(ip_version);
            set_select(&read_fds, &tv, connection);
            int retval = select(socket_fd + 1, &read_fds, nullptr, nullptr, &tv);
            if (retval == -1) {
                perror("select");
                break;
            } else if (retval > 0) {
                ssize_t length = recvfrom(socket_fd, recv_buf, BUF_SIZE, 0,
                                          (struct sockaddr*) &socket_address, (socklen_t*) (&addrlen));
                if (length < 0) {
                    perror("recvfrom");
                    break;
                }
                user_address_tmp = inet_ntoa(socket_address.sin_addr);
                if (strcmp(user_address_tmp, host_address) != 0) {
                    bool contains = false;
                    for (auto user: users_online) {
                        if (strcmp(user.first, user_address_tmp) == 0) {
                            contains = true;
                            break;
                        }
                    }
                    if (recv_buf[0] != STATUS_EXIT) {
                        if (!contains) {
                            char* user_address = (char*) malloc(INET6_ADDRSTRLEN);
                            strcpy(user_address, user_address_tmp);
                            users_online.insert({user_address, true});
                            if (!connection) {
                                printf("==New user online==\n");
                                print_users_online(users_online);
                            }
                        } else {
                            users_online[user_address_tmp] = true;
                        }
                        check_connection_timer(&connection, &timer3_start, &timer3_end, users_online);
                    } else {
                        users_online.erase(user_address_tmp);
                        printf("==Some users left==\n");
                        print_users_online(users_online);
                    }
                    check_need_for_send((sockaddr*) (&socket_address), &timer1_start, &timer1_end, recv_buf,
                                        ip_version);
                    check_remove_timer(&timer2_start, &timer2_end, users_online);
                }
            } else {
                if (connection) {
                    print_users_online(users_online);
                    connection = false;
                }
                check_remove_timer(&timer2_start, &timer2_end, users_online);
                send_datagram((sockaddr*) (&socket_address));
            }
        }
    } else {
        char* multicastIP;              /* Arg: IP Multicast Address */
        char multicastPort[] = "12000";            /* Arg: Port */
        struct addrinfo* multicastAddr;            /* Multicast Address */
        struct addrinfo* localAddr;                /* Local address to bind to */
        struct addrinfo hints = {0};   /* Hints for name lookup */

        multicastIP = argv[1];      /* First arg:  Multicast IP address */

        /* Resolve the multicast group address */
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = PF_UNSPEC;     /* IPv4 or IPv6 records (don't care). */
        hints.ai_socktype = SOCK_DGRAM;    /* Connectionless communication.      */
        hints.ai_protocol = IPPROTO_UDP;   /* UDP transport layer protocol only. */

        if (getaddrinfo(multicastIP, nullptr, &hints, &multicastAddr) != 0) {
            perror("getaddrinfo() failed");
            return EXIT_FAILURE;
        }

        printf("Using %s\n", multicastAddr->ai_family == PF_INET6 ? "IPv6" : "IPv4");

        /* Get a local address with the same family (IPv4 or IPv6) as our multicast group */
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = multicastAddr->ai_family;
        hints.ai_socktype = SOCK_DGRAM;
        hints.ai_flags = AI_PASSIVE; /* Return an address we can bind to */
        hints.ai_protocol = IPPROTO_UDP;   /* UDP transport layer protocol only. */

        if (getaddrinfo(nullptr, multicastPort, &hints, &localAddr) != 0) {
            perror("getaddrinfo() failed");
            return EXIT_FAILURE;
        }

        char* local_address = (char*) malloc(INET6_ADDRSTRLEN);
        inet_ntop(AF_INET6, &((sockaddr_in6*) localAddr->ai_addr)->sin6_addr,
                  local_address, INET6_ADDRSTRLEN);
        printf("Your IP-address: %s\n", local_address);
        free(local_address);

        socket_fd = socket(ip_version, SOCK_DGRAM, 0);
        if (socket_fd < 0) {
            perror("socket");
            return EXIT_FAILURE;
        }

//        struct ifreq interface{};
//        interface.ifr_addr.sa_family = ip_version;
//        strncpy(interface.ifr_name, INTERFACE, IFNAMSIZ - 1);
//        if (ioctl(socket_fd, SIOCGIFADDR, &interface) == -1) {
//            perror("ioctl");
//            close_all();
//            return EXIT_FAILURE;
//        }
//        char* user_address_tmp = inet_ntoa(((struct sockaddr_in*) &interface.ifr_addr)->sin_addr);
//        char* host_address = (char*) malloc(INET6_ADDRSTRLEN);
//        strcpy(host_address, user_address_tmp);
//        printf("Your IP-address: %s\n", host_address);


        const int optval = 1;
        if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) != 0) {
            perror("setsockopt. Reuse address");
            close_all();
            return EXIT_FAILURE;
        }

        bzero(&socket_address6, sizeof(socket_address6));
        socket_address6.sin6_family = ip_version;
        socket_address6.sin6_port = htons(PORT);
        if (0 != bind(socket_fd, (sockaddr*) &socket_address6, sizeof(socket_address6))) {
            perror("bind");
            close_all();
            return EXIT_FAILURE;
        }

        if (0 == inet_pton(AF_INET6, group_address, &(group6.ipv6mr_multiaddr))) {
            fprintf(stderr, "Invalid address\n");
            close_all();
            return EXIT_FAILURE;
        }

        if (0 == inet_pton(AF_INET6, group_address, &(group6.ipv6mr_multiaddr))) {
            fprintf(stderr, "Invalid address\n");
            close_all();
            return EXIT_FAILURE;
        }

        group6.ipv6mr_interface = if_nametoindex(INTERFACE);
        if (group6.ipv6mr_interface == 0) {
            perror("if_nametoindex");
            close_all();
            return EXIT_FAILURE;
        }
        printf("Interface index %d\n", group6.ipv6mr_interface);
        socket_address6.sin6_scope_id = group6.ipv6mr_interface ;
        socket_address6.sin6_addr = ((sockaddr_in6*) localAddr)->sin6_addr;

        if (setsockopt(socket_fd, IPPROTO_IPV6, IPV6_ADD_MEMBERSHIP,
                       &group6, sizeof(group6)) != 0) {
            perror("setsockopt. Ip add membership");
            close_all();
            return EXIT_FAILURE;
        }

        memset(send_buf, 0, BUF_SIZE);
        set_addr(ip_version);
        if (SIG_ERR == signal(SIGINT, (__sighandler_t) leave)) {
            perror("signal");
            close_all();
            return EXIT_FAILURE;
        }
        send_buf[0] = STATUS_CONNECT;
        set_addr(ip_version);
        printf("Check 1\n");
        send_datagram((sockaddr*) (&socket_address6));

        send_buf[0] = STATUS_DEFAULT;
        int recv_buf[BUF_SIZE];
        memset(recv_buf, 0, BUF_SIZE);
        fd_set read_fds;

        std::map<char*, bool, StringsComparator> users_online;
        bool connection = true;
        struct timeval tv{};
        struct timespec timer1_start{}, timer1_end{};
        struct timespec timer2_start{}, timer2_end{};
        struct timespec timer3_start{}, timer3_end{};
        clock_gettime(CLOCK_MONOTONIC_RAW, &timer1_start);
        clock_gettime(CLOCK_MONOTONIC_RAW, &timer2_start);
        clock_gettime(CLOCK_MONOTONIC_RAW, &timer3_start);

        while (true) {
            set_addr(ip_version);
            set_select(&read_fds, &tv, connection);
            int retval = select(socket_fd + 1, &read_fds, nullptr, nullptr, &tv);
            if (retval == -1) {
                perror("select");
                break;
            } else if (retval > 0) {
                ssize_t length = recvfrom(socket_fd, recv_buf, BUF_SIZE, 0,
                                          (struct sockaddr*) &socket_address, (socklen_t*) (&addrlen));
                if (length < 0) {
                    perror("recvfrom");
                    break;
                }
                user_address_tmp = inet_ntoa(socket_address.sin_addr);
                if (strcmp(user_address_tmp, host_address) != 0) {
                    bool contains = false;
                    for (auto user: users_online) {
                        if (strcmp(user.first, user_address_tmp) == 0) {
                            contains = true;
                            break;
                        }
                    }
                    if (recv_buf[0] != STATUS_EXIT) {
                        if (!contains) {
                            char* user_address = (char*) malloc(INET6_ADDRSTRLEN);
                            strcpy(user_address, user_address_tmp);
                            users_online.insert({user_address, true});
                            if (!connection) {
                                printf("==New user online==\n");
                                print_users_online(users_online);
                            }
                        } else {
                            users_online[user_address_tmp] = true;
                        }
                        check_connection_timer(&connection, &timer3_start, &timer3_end, users_online);
                    } else {
                        users_online.erase(user_address_tmp);
                        printf("==Some users left==\n");
                        print_users_online(users_online);
                    }
                    check_need_for_send((sockaddr*) (&socket_address6), &timer1_start, &timer1_end, recv_buf,
                                        ip_version);
                    check_remove_timer(&timer2_start, &timer2_end, users_online);
                }
            } else {
                if (connection) {
                    print_users_online(users_online);
                    connection = false;
                }
                check_remove_timer(&timer2_start, &timer2_end, users_online);
                send_datagram((sockaddr*) (&socket_address6));
            }
        }
    }
    close_all();
    return EXIT_FAILURE;
}
