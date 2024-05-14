/* File: GBN.h
 * Authors: Kim Svedberg, Zebastian Thorsén
 * Description: File containing protocol varibles and states.
 */

#ifndef gbn_h
#define gbn_h

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/time.h>


 /* Protocal parameters */
#define hostNameLength 50   /* The lenght of host name*/
#define windowSize 1        /* Sliding window size */
#define MAXMSG 1024         /* Maximun data to be sent once*/
#define LOSS_PROB 1e-2      /* Packet loss probability */
#define CORR_PROB 1e-3      /* Packet corrution probability */
#define MAX_SEQ_NUM 100     /* The maximum random sequence number */
#define MIN_SEQ_NUM 5       /* The minimum random sequence number */

/* Packet flags */
#define SYN 0                  
#define SYNACK 1
#define DATA 2
#define ACK 3
#define FIN 4
#define FINACK 5

int s_state;            /* Sender state */
int r_state;            /* Receiver state */

/* All possible states */
enum states {
    CLOSED,
    WAIT_SYNACK,
    RCVD_SYNACK,
    ESTABLISHED,
    WAIT,
    SEND_DATA,
    PACKET_LOSS,
    RCVD_ACK,
    WAIT_FINACK,
    RCVD_FINACK,
    WAIT_TIME,
    LISTENING,
    RCVD_SYN,
    WAIT_ACK,
    READ_DATA,
    DEFAULT,
    RCVD_FIN,
};

/* Transport protocol header */
typedef struct rtp_struct {
    uint8_t flags;
    //int id; If there are more then one connections
    uint8_t seq;
    int windowsize; /////////////////////////////////
    uint16_t checksum;
    uint8_t  data[MAXMSG];
} rtp;

/* State information (Maybe not needed)*/
typedef struct states_t {
    int state;
    int seqnum;
    int window_size;
} state_t;


/* All function for the protocol */
int sender_connection(int sockfd, const struct sockaddr* serverName, socklen_t socklen);
int receiver_connection(int sockfd, const struct sockaddr* client, socklen_t* socklen);

ssize_t sender_gbn(int sockfd, const void* buf, size_t len, int flags);
ssize_t receiver_gbn(int sockfd, void* buf, size_t len, int flags);
ssize_t maybe_sendto(int sockfd, const void* buf, size_t len, int flags,
    const struct sockaddr* to, socklen_t tolen);

int sender_teardown(int sockfd, const struct sockaddr* serverName, socklen_t socklen);
int receiver_teardown(int sockfd, const struct sockaddr* client, socklen_t socklen);

uint16_t checksum(rtp* packet);

#endif