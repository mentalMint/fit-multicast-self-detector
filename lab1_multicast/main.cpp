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

int socket_fd;
struct sockaddr_in addr{};
struct ip_mreq mreq{};
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
                     (struct sockaddr*) &(addr), (socklen_t) addrlen)) {
        perror("sendto");
        close_all();
        exit(EXIT_FAILURE);
    }
    printf("\n===Exit===\n");
    close_all();
    exit(EXIT_SUCCESS);
}

void send_datagram() {
    if (-1 == sendto(socket_fd, send_buf, BUF_SIZE, 0,
                     (struct sockaddr*) &(addr), (socklen_t) addrlen)) {
        perror("sendto");
        close_all();
        exit(EXIT_FAILURE);
    }
}

void set_addr() {
    addr.sin_addr.s_addr = mreq.imr_multiaddr.s_addr;
    addrlen = sizeof(addr);
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

void check_need_for_send(struct timespec* timer1_start, struct timespec* timer1_end, const int* recv_buf) {
    clock_gettime(CLOCK_MONOTONIC_RAW, timer1_end);
    if (timer1_end->tv_sec - timer1_start->tv_sec >= PERIOD_SEC || recv_buf[0] == STATUS_CONNECT) {
        send_datagram();
        set_addr();
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

int main() {
    socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_fd < 0) {
        perror("socket");
        return EXIT_FAILURE;
    }

    struct ifreq ifr{};
    ifr.ifr_addr.sa_family = AF_INET;
    strncpy(ifr.ifr_name, INTERFACE, IFNAMSIZ - 1);
    ioctl(socket_fd, SIOCGIFADDR, &ifr);
    char* user_address_tmp = inet_ntoa(((struct sockaddr_in*) &ifr.ifr_addr)->sin_addr);
    char* host_address = (char*) malloc(20);
    strcpy(host_address, user_address_tmp);
    printf("Your IP: %s\n", host_address);

    const int optval = 1;
    if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) != 0) {
        perror("setsockopt. Reuse address");
        return EXIT_FAILURE;
    }

    bzero(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);

    if (0 != bind(socket_fd, (sockaddr*) &addr, sizeof(addr))) {
        perror("bind");
        close_all();
        return EXIT_FAILURE;
    }

    char ip_addr[] = "224.0.0.1";
    if (0 == inet_aton(ip_addr, &(mreq.imr_multiaddr))) {
        fprintf(stderr, "Invalid address\n");
        close_all();
        return EXIT_FAILURE;
    }
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);

    if (setsockopt(socket_fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) != 0) {
        perror("setsockopt. Ip add membership");
        close_all();
        return EXIT_FAILURE;
    }

    memset(send_buf, 0, BUF_SIZE);
    set_addr();

    if (SIG_ERR == signal(SIGINT, (__sighandler_t) leave)) {
        perror("signal");
        close_all();
        return EXIT_FAILURE;
    }

    set_addr();
    send_buf[0] = STATUS_CONNECT;
    send_datagram();
    send_buf[0] = STATUS_DEFAULT;
    fd_set readfds;
    int recv_buf[BUF_SIZE];
    memset(recv_buf, 0, BUF_SIZE);

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
        set_addr();
        set_select(&readfds, &tv, connection);
        int retval = select(socket_fd + 1, &readfds, nullptr, nullptr, &tv);
        if (retval == -1) {
            perror("select");
            break;
        } else if (retval > 0) {
            ssize_t length = recvfrom(socket_fd, recv_buf, BUF_SIZE, 0,
                                      (struct sockaddr*) &addr, (socklen_t*) (&addrlen));
            if (length < 0) {
                perror("recvfrom");
                break;
            }
            user_address_tmp = inet_ntoa(addr.sin_addr);
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
                        char* user_address = (char*) malloc(20);
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
                check_need_for_send(&timer1_start, &timer1_end, recv_buf);
                check_remove_timer(&timer2_start, &timer2_end, users_online);
            }
        } else {
            if (connection) {
                print_users_online(users_online);
                connection = false;
            }
            check_remove_timer(&timer2_start, &timer2_end, users_online);
            send_datagram();
        }
    }
    close_all();
    return EXIT_FAILURE;
}