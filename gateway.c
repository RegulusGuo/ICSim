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
#include <openssl/evp.h>
#define BLOCK_SIZE 16
#define DEFAULT_DIFFICULTY 1
#define DEFAULT_DOOR_ID 411
#define DEFAULT_DOOR_POS 2
#define DEFAULT_DOOR_DIAG_ID 412
#define DEFAULT_SIGNAL_ID 392
#define DEFAULT_SIGNAL_POS 0
#define DEFAULT_SPEED_ID 580
#define DEFAULT_SPEED_POS 3
#define DEFAULT_SPEED_DIAG_ID 581
#define CAN_DOOR1_LOCK 1
#define CAN_DOOR2_LOCK 2 
#define CAN_DOOR3_LOCK 4
#define CAN_DOOR4_LOCK 8
#define CAN_LEFT_SIGNAL 1
#define CAN_RIGHT_SIGNAL 2
#define ON 1
#define OFF 0
#define DOOR_LOCKED 0
#define DOOR_UNLOCKED 1

void handleErrors(void);
static void aes_encrypt(unsigned char *in, unsigned char *out, unsigned char *key, unsigned char *iv);
static void aes_decrypt(unsigned char *in, unsigned char *out, unsigned char *key, unsigned char *iv);
int server_sock, client_sock;
int door_pos = DEFAULT_DOOR_POS;
int signal_pos = DEFAULT_SIGNAL_POS;
int speed_pos = DEFAULT_SPEED_POS;
int door_len = DEFAULT_DOOR_POS + 1;
int signal_len = DEFAULT_DOOR_POS + 1;
int speed_len = DEFAULT_SPEED_POS + 2;
int difficulty = DEFAULT_DIFFICULTY;
int door_id = DEFAULT_DOOR_ID;
int door_diag_id = DEFAULT_DOOR_DIAG_ID;
int signal_id = DEFAULT_SIGNAL_ID;
int speed_id = DEFAULT_SPEED_ID;
int speed_diag_id = DEFAULT_SPEED_DIAG_ID;
unsigned char *key = (unsigned char *)"AAAAAAAAAAAAAAAA";
unsigned char *iv = (unsigned char *)"BBBBBBBBBBBBBBBB";

void randomize_pkt(char* data,int start, int stop) {
	if (difficulty < 2) return;
	int i = start;
	for(;i < stop;i++) {
		if(rand() % 3 < 1) data[i] = rand() % 255;
	}
}

void *serverToClient(void *arg) {
    struct canfd_frame frame;
    ssize_t nbytes;
    while (1) {
        nbytes = read(server_sock, &frame, sizeof(struct canfd_frame));
        if (nbytes < 0) {
            perror("Error in server read");
            exit(EXIT_FAILURE);
        }
        /*unsigned char decrypt_text[64];
        unsigned char ciphertext[32];
        strncpy(ciphertext,frame.data,32);
        aes_decrypt(ciphertext, decrypt_text, key, iv);
        if (strncmp(decrypt_text,"DOOR",4)==0){
            int door1 = (decrypt_text[6] == 'C');
            int door2 = (decrypt_text[9] == 'C') << 1;
            int door3 = (decrypt_text[12] == 'C') << 2;
            int door4 = (decrypt_text[15] == 'C') << 3;
            int door_state = door4 | door3 | door2 | door1;
            memset(&frame, 0, sizeof(frame));
	        frame.can_id = door_id;
	        frame.len = door_len;
	        frame.data[door_pos] = door_state;
	        if (door_pos) randomize_pkt(frame.data, 0, door_pos);
	        if (door_len != door_pos + 1) randomize_pkt(frame.data, door_pos + 1, door_len);
        }else{
            if (strncmp(decrypt_text,"Status of",9)==0){
                if (decrypt_text[15] == 'd'){
                    memset(&frame, 0, sizeof(frame));
	                frame.can_id = speed_diag_id;
	                frame.len = speed_len;
	                frame.data[speed_pos] = 0;
                }else{
                    memset(&frame, 0, sizeof(frame));
	                frame.can_id = door_diag_id;
	                frame.len = door_len;
	                frame.data[door_pos] = 0;
                }
            }else{
                if (strncmp(decrypt_text,"Speed",5)==0){
                    int a,b;
                    char a1[] = {'0','x',decrypt_text[9],decrypt_text[10],0};
                    char b1[] = {'0','x',decrypt_text[11],decrypt_text[12],0};
                    sscanf(a1,"%x",&a);
                    sscanf(b1,"%x",&b);
                    memset(&frame, 0, sizeof(frame));
		            frame.can_id = speed_id;
		            frame.len = speed_len;
		            frame.data[speed_pos+1] = (char)b;
		            frame.data[speed_pos] = (char)a;
		            if (speed_pos) randomize_pkt(frame.data,0, speed_pos);
		            if (speed_len != speed_pos + 2) randomize_pkt(frame.data,speed_pos+2, speed_len);
                }else{
                    if (strncmp(decrypt_text,"Turn",4)==0){
                        memset(&frame, 0, sizeof(frame));
	                    frame.can_id = signal_id;
	                    frame.len = signal_len;
	                    frame.data[signal_pos] = 0;
                        if (decrypt_text[5] == 'L'){
                            frame.data[signal_pos] = frame.data[signal_pos] ^ CAN_LEFT_SIGNAL;
                        }else{
                            frame.data[signal_pos] = frame.data[signal_pos] ^ CAN_RIGHT_SIGNAL;
                        }
	                    if(signal_pos) randomize_pkt(frame.data,0, signal_pos);
	                    if(signal_len != signal_pos + 1) randomize_pkt(frame.data,signal_pos+1, signal_len);
                    }
                }
            }
        }*/
        if (write(client_sock, &frame, CAN_MTU) != CAN_MTU) {
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
void handleErrors(void)
{
    printf("Error: openssl internal error.\n");
    ERR_print_errors_fp(stderr);
    exit(EXIT_FAILURE);
}
static void aes_encrypt(unsigned char *in, unsigned char *out, unsigned char *key, unsigned char *iv)
{
    EVP_CIPHER_CTX *ctx;
    int in_len = strlen((const char *)in);
    int out_len = 0;
    ctx = EVP_CIPHER_CTX_new();
    if (ctx == NULL) {
        printf("Error: EVP_CIPHER_CTX_new() failed.\n");
        exit(EXIT_FAILURE);
    }
    EVP_CIPHER_CTX_init(ctx);
    if (EVP_EncryptInit_ex(ctx, EVP_aes_128_cbc(), NULL, key, iv) != 1) {
        printf("Error: EVP_EncryptInit_ex() failed.\n");
        handleErrors();
    }
    if (EVP_EncryptUpdate(ctx, out, &out_len, in, in_len) != 1) {
        printf("Error: EVP_EncryptUpdate() failed.\n");
        handleErrors();
    }
    int final_len = 0;
    if (EVP_EncryptFinal_ex(ctx, out + out_len, &final_len) != 1) {
        printf("Error: EVP_EncryptFinal_ex() failed.\n");
        handleErrors();
    }
    out_len += final_len;
    EVP_CIPHER_CTX_free(ctx);
}
static void aes_decrypt(unsigned char *in, unsigned char *out, unsigned char *key, unsigned char *iv)
{
    EVP_CIPHER_CTX *ctx;
    int in_len = strlen((const char *)in);
    int out_len = 0;
    ctx = EVP_CIPHER_CTX_new();
    if (ctx == NULL) {
        printf("Error: EVP_CIPHER_CTX_new() failed.\n");
        exit(EXIT_FAILURE);
    }
    EVP_CIPHER_CTX_init(ctx);
    if (EVP_DecryptInit_ex(ctx, EVP_aes_128_cbc(), NULL, key, iv) != 1) {
        printf("Error: EVP_DecryptInit_ex() failed.\n");
        handleErrors();
    }
    if (EVP_DecryptUpdate(ctx, out, &out_len, in, in_len) != 1) {
        printf("Error: EVP_DecryptUpdate() failed.\n");
        handleErrors();
    }
    int final_len = 0;
    if (EVP_DecryptFinal_ex(ctx, out + out_len, &final_len) != 1) {
        printf("Error: EVP_DecryptFinal_ex() failed.\n");
        handleErrors();
    }
    out_len += final_len;
    EVP_CIPHER_CTX_free(ctx);
}
