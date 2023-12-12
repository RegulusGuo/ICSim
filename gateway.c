#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <getopt.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <net/if.h>
#include <pthread.h>
#include <linux/can.h>
#include <linux/can/raw.h>

int server_sock, client_sock;

void *serverToClient(void *arg) {
    struct canfd_frame frame;
    ssize_t nbytes;
    while (1) {
        nbytes = read(server_sock, &frame, sizeof(struct canfd_frame));
        if (nbytes < 0) {
            perror("Error in server read");
            exit(EXIT_FAILURE);
        }
        if (write(client_sock, &frame, nbytes) != nbytes) {
            perror("Error in client write");
            exit(EXIT_FAILURE);
        }
    }
}

void *clientToServer(void *arg) {
    struct canfd_frame frame;
    ssize_t nbytes;
    while (1) {
        nbytes = read(client_sock, &frame, sizeof(struct canfd_frame));
        if (nbytes < 0) {
            perror("Error in client read");
            exit(EXIT_FAILURE);
        }
        if (write(server_sock, &frame, nbytes) != nbytes) {
            perror("Error in server write");
            exit(EXIT_FAILURE);
        }
    }
}

int main(int argc, char *argv[]) {
    pthread_t server_thread, client_thread;
    struct sockaddr_can server_addr, client_addr;
    struct ifreq ifr;
    
    // 创建服务端socket
    server_sock = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (server_sock < 0) {
        perror("Error in server socket");
        exit(EXIT_FAILURE);
    }
    
    // 设置接口名
    memset(&ifr.ifr_name, 0, sizeof(ifr.ifr_name));
    strncpy(ifr.ifr_name, argv[1], strlen(argv[1]));
    ioctl(server_sock, SIOCGIFINDEX, &ifr);
    
    // 绑定服务端socket到CAN接口
    server_addr.can_family = AF_CAN;
    server_addr.can_ifindex = ifr.ifr_ifindex;
    if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Error in server bind");
        exit(EXIT_FAILURE);
    }
    
    // 启用CAN FD模式
    int enable_canfd = 1;
    if (setsockopt(server_sock, SOL_CAN_RAW, CAN_RAW_FD_FRAMES, &enable_canfd, sizeof(enable_canfd)) < 0) {
        perror("Error in enabling CAN FD mode");
        exit(EXIT_FAILURE);
    }
    
    // 创建客户端socket
    client_sock = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (client_sock < 0) {
        perror("Error in client socket");
        exit(EXIT_FAILURE);
    }
    
    // 设置接口名
    memset(&ifr.ifr_name, 0, sizeof(ifr.ifr_name));
    strncpy(ifr.ifr_name, argv[2], strlen(argv[2]));
    ioctl(client_sock, SIOCGIFINDEX, &ifr);
    
    // 绑定客户端socket到CAN接口
    client_addr.can_family = AF_CAN;
    client_addr.can_ifindex = ifr.ifr_ifindex;
    if (bind(client_sock, (struct sockaddr *)&client_addr, sizeof(client_addr)) < 0) {
        perror("Error in client bind");
        exit(EXIT_FAILURE);
    }
    
    // 启用CAN FD模式
    if (setsockopt(client_sock, SOL_CAN_RAW, CAN_RAW_FD_FRAMES, &enable_canfd, sizeof(enable_canfd)) < 0) {
        perror("Error in enabling CAN FD mode");
        exit(EXIT_FAILURE);
    }
    
    // 创建线程并启动
    if (pthread_create(&server_thread, NULL, serverToClient, NULL) != 0) {
        perror("Error in creating server thread");
        exit(EXIT_FAILURE);
    }
    
    if (pthread_create(&client_thread, NULL, clientToServer, NULL) != 0) {
        perror("Error in creating client thread");
        exit(EXIT_FAILURE);
    }
    
    // 等待线程结束
    pthread_join(server_thread, NULL);
    pthread_join(client_thread, NULL);
    
    close(server_sock);
    close(client_sock);
    return 0;
}