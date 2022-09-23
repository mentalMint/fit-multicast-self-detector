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
#define BUF_SIZE 16
#define PERIOD_SEC 3
#define INTERFACE "enp0s3"

int socket_fd;
struct sockaddr_in addr{};
struct ip_mreq mreq{};
int addrlen;
char send_buf[BUF_SIZE];

struct StringsComparator {
    bool operator()(const char* a, const char* b) const {
        return strcmp(a, b) < 0;
    }
};

void print_map(const std::map<char*, bool, StringsComparator> &map) {
    for (auto n: map) {
        if (n.second) {
            printf("%s is online\n", n.first);
        } else {
            printf("%s is offline\n", n.first);
        }
    }
}

void print_array(int* array, size_t size) {
    for (size_t i = 0; i < size; i++) {
        printf("%d ", array[i]);
    }
    fflush(stdout);
}

void set_select(fd_set* readfds, struct timeval* tv) {
    if (FD_ISSET(socket_fd, readfds) == 0) {
        FD_ZERO(readfds);
        FD_SET(socket_fd, readfds);
    }
    tv->tv_sec = PERIOD_SEC;
    tv->tv_usec = 0;
}

void check_host_name(int hostname) { //This function returns host name for local computer
    if (hostname == -1) {
        perror("gethostname");
        exit(1);
    }
}

void check_host_entry(struct hostent* hostentry) { //find host info from host name
    if (hostentry == NULL) {
        perror("gethostbyname");
        exit(1);
    }
}

void IP_formatter(const char* IPbuffer) { //convert send_buf string to dotted decimal format
    if (NULL == IPbuffer) {
        perror("inet_ntoa");
        exit(1);
    }
}

void close_all() {
    close(socket_fd);
}

void leave() {
//    if (-1 == sendto(socket_fd, send_buf, BUF_SIZE, 0,
//                     (struct sockaddr*) &(addr), (socklen_t) addrlen)) {
//        perror("sendto");
//        close_all();
//        exit(EXIT_FAILURE);
//    }
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
//    printf("===Sent===\n");
//    printf("==========\n");
//    fflush(stdout);
}

void set_addr() {
    addr.sin_addr.s_addr = mreq.imr_multiaddr.s_addr;
    addrlen = sizeof(addr);
}

void remove_offline_users(std::map<char*, bool, StringsComparator>& users_online) {
//    printf("Check if someone offline\n");
//    fflush(stdout);
    bool some_users_left = false;
    for (auto user = users_online.cbegin(); user != users_online.cend();) {
        if (!user->second) {
//            printf("Erase %s\n", user->first);
            free(user->first);
            user = users_online.erase(user);
            some_users_left = true;
        } else {
            ++user;
        }
    }

//    printf("===Users were online===\n");
//    print_map(users_online);
//    printf("==================\n\n");

    for (std::_Rb_tree_iterator<std::pair<char* const, bool>> user = users_online.begin(); user != users_online.end(); user++) {
        user->second = false;
    }
    if (some_users_left) {
        printf("==Some users left==\n");
        printf("===Users online===\n");
        print_map(users_online);
        printf("==================\n\n");
        fflush(stdout);
    } else {
//        printf("==No changes==\n");
//        printf("===Users online===\n");
//        print_map(users_online);
//        printf("==================\n\n");
//        fflush(stdout);
    }
}

int main() {
    //SEND_SOCKET
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
        return EXIT_FAILURE;
    }

    char ip_addr[] = "224.0.0.1";
    if (0 == inet_aton(ip_addr, &(mreq.imr_multiaddr))) {
        fprintf(stderr, "Invalid address\n");
        return EXIT_FAILURE;
    }
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);

    if (setsockopt(socket_fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) != 0) {
        perror("setsockopt. Ip add membership");
        return EXIT_FAILURE;
    }

    memset(send_buf, 0, BUF_SIZE);

    if (SIG_ERR == signal(SIGINT, (__sighandler_t) leave)) {
        perror("signal");
        close_all();
        return EXIT_FAILURE;
    }

    //SELECT
    fd_set readfds;
    struct timeval tv{};
    int retval;

    char recv_buf[BUF_SIZE];
    memset(recv_buf, 0, BUF_SIZE);

    set_addr();

    std::map<char*, bool, StringsComparator> users_online;
    send_datagram();

    struct timespec timer1_start{}, timer1_end{};
    struct timespec timer2_start{}, timer2_end{};

    clock_gettime(CLOCK_MONOTONIC_RAW, &timer1_start);
    clock_gettime(CLOCK_MONOTONIC_RAW, &timer2_start);

    while (true) {
        set_addr();
        set_select(&readfds, &tv);
        retval = select(socket_fd + 1, &readfds, NULL, NULL, &tv);

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
//                if (users_online.find(user_address_tmp) == users_online.end()) {
                bool contains = false;
                for (auto user : users_online) {
                    if (strcmp(user.first, user_address_tmp) == 0) {
                        contains = true;
                        break;
                    }
                }
                if (!contains) {
                    char* user_address = (char*) malloc(20);
                    strcpy(user_address, user_address_tmp);
                    users_online.insert({user_address, true});
                    printf("==New user online==\n");
                    printf("===Users online===\n");
                    print_map(users_online);
                    printf("==================\n\n");
                    fflush(stdout);
                } else {
                    printf("%s confirmed\n", user_address_tmp);
                    fflush(stdout);
                    users_online[user_address_tmp] = true;
                }

                clock_gettime(CLOCK_MONOTONIC_RAW, &timer1_end);
                if (timer1_end.tv_sec - timer1_start.tv_sec >= PERIOD_SEC) {
                    send_datagram();
                    set_addr();
                    clock_gettime(CLOCK_MONOTONIC_RAW, &timer1_start);
                }

                clock_gettime(CLOCK_MONOTONIC_RAW, &timer2_end);
                if (timer2_end.tv_sec - timer2_start.tv_sec >= PERIOD_SEC * 2) {
                    remove_offline_users(users_online);
                    clock_gettime(CLOCK_MONOTONIC_RAW, &timer2_start);
                }
            }
//            printf("===Received===\n");
//            printf("Sender IP: %s\n", user_address_tmp);
//            printf("==============\n");
        } else {
            clock_gettime(CLOCK_MONOTONIC_RAW, &timer2_end);
            if (timer2_end.tv_sec - timer2_start.tv_sec >= PERIOD_SEC * 2) {
//                if (!users_online.empty()) {
//                    for (auto user: users_online) {
//                        free(user.first);
//                    }
//                    users_online.clear();
//                    printf("==All users left==\n");
//                    printf("===Users online===\n");
//                    printf("==================\n\n");
//                    fflush(stdout);
//                }
                remove_offline_users(users_online);
                clock_gettime(CLOCK_MONOTONIC_RAW, &timer2_start);
            }
            send_datagram();
        }
    }

    close_all();
    return EXIT_FAILURE;
}
