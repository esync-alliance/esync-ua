#include <INTEGRITY.h>
#include <stdlib.h>
#include <stdio.h>
#include <bsp.h>
#include <time.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string.h>
#include <netinet/in.h>

#define TIME_SYNC_MAGIC_ID 0x12345

struct nw_time_recv {
    int sfd;
};

struct nw_time_recv *recv_ctx = NULL;
// Forward decleration
void nw_time_recv_deinit(void);

int nw_time_recv_init(uint16_t srv_port)
{
    int err = -1;

    if (recv_ctx) return 0;
    recv_ctx = (struct nw_time_recv *)malloc(sizeof(*recv_ctx));
    memset(recv_ctx, 0, sizeof(*recv_ctx));
    recv_ctx->sfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (recv_ctx->sfd == -1)
        goto error;
    struct sockaddr_in udp_server;
    udp_server.sin_family = AF_INET;
    udp_server.sin_addr.s_addr = INADDR_ANY;
    udp_server.sin_port = htons(srv_port);

    err = bind(recv_ctx->sfd, (struct sockaddr *)&udp_server, sizeof(udp_server));
    if (err)
        goto error;

    return 0;
error:
    nw_time_recv_deinit();

    return err;
}

void nw_time_recv_deinit(void)
{
    if (recv_ctx->sfd)
        close(recv_ctx->sfd);
    free(recv_ctx);
    recv_ctx = NULL;
}

int nw_time_recv(uint8_t *buffer, int size)
{
    struct sockaddr_in udp_client;
    int addr_len = sizeof(udp_client);
    return recvfrom(recv_ctx->sfd, (uint8_t *)buffer, size, 0,
                    (struct sockaddr *)&udp_client,
                    &addr_len);
}
#define deserialize(val, size, data) do { \
    int i; \
    for (i=size-1; i >=0; --i) {\
    val |= (data[i]<<(i*8));  \
    }                         \
} while(0)

int main(void)
{
    int err;
    uint16_t port = 12345;
    
    if (nw_time_recv_init(port)) {
        printf("nw_time_recv_init() failed \n");
        return 0;
    }
    uint8_t buffer[32];
    int len;
    
    while(1) {
	len = nw_time_recv(buffer, sizeof(buffer));
        if (len > 0) {
            int32_t *magic_id = (int32_t *)buffer;
            uint8_t *buf;
            if (*magic_id == TIME_SYNC_MAGIC_ID) {
                struct timespec tm;
                struct timespec now;

                memset(&tm, 0, sizeof(tm));
                buf = buffer+sizeof(int32_t);
                deserialize(tm.tv_sec, 8, buf);
                buf += 8;
                deserialize(tm.tv_nsec, 8, buf);
                clock_gettime(CLOCK_REALTIME, &now);
                if (tm.tv_sec >= now.tv_sec+10) {
                    clock_settime(CLOCK_REALTIME, &tm);
                    printf("============Apply system clock: tv_sec: %ld, tv_nsec: %ld \n",
                           tm.tv_sec, tm.tv_nsec);
                }
            }
        }
    }
    nw_time_recv_deinit();
    Exit(0);
}

