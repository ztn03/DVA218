/* File: GBN.c
 * Authors: Kim Svedberg, Zebastian Thorsén
 * Description: File containing all function for the GBN protocol like connection setup & teardown.
 */

#include "GBN.h"


state_t state;

int sender_connection(int sockfd, const struct sockaddr* serverName, socklen_t socklen) {
	char buffer[MAXMSG];
	fd_set activeFdSet;
	srand(time(NULL));
	int nOfBytes = 0;
	int result = 0;

	s_state = CLOSED;

	/* Timeout */
	struct timeval timeout;
	timeout.tv_sec = 5;
	timeout.tv_usec = 0;


	/* Initialize SYN_packet */
	rtp* SYN_packet = malloc(sizeof(*SYN_packet));
	if (SYN_packet == NULL) {
		perror("malloc");
		exit(EXIT_FAILURE);
	}
	SYN_packet->flags = SYN;    //SYN packet                     
	SYN_packet->seq = (rand() % (MAX_SEQ_NUM - MIN_SEQ_NUM + 1)) + MIN_SEQ_NUM;    //Chose a random seq_number between 5 & 99
	SYN_packet->windowsize = windowSize;
	memset(SYN_packet->data, '\0', sizeof(SYN_packet->data));
	SYN_packet->checksum = checksum(SYN_packet);


	/* Initilize SYNACK_packet */
	rtp* SYNACK_packet = malloc(sizeof(*SYNACK_packet));
	if (SYNACK_packet == NULL) {
		perror("malloc");
		exit(EXIT_FAILURE);
	}
	memset(SYNACK_packet->data, '\0', sizeof(SYNACK_packet->data));
	struct sockaddr from;
	socklen_t from_len = sizeof(from);


	/* Initialize ACK_packet */
	rtp* ACK_packet = malloc(sizeof(*ACK_packet));
	if (ACK_packet == NULL) {
		perror("malloc");
		exit(EXIT_FAILURE);
	}
	ACK_packet->flags = ACK;
	memset(ACK_packet->data, '\0', sizeof(ACK_packet->data));


	FD_ZERO(&activeFdSet);
	FD_SET(sockfd, &activeFdSet);

	/* State machine */
	while (1) {
		switch (s_state) {

			/* Start state, begins with a closed connection. Send SYN */
		case CLOSED:
			printf("Sending SYN packet\n");

			/* Send SYN_packet to receiver */
			nOfBytes = sendto(sockfd, SYN_packet, sizeof(*SYN_packet), 0, serverName, socklen);

			/* Failed to send SYN_packet to the receiver */
			if (nOfBytes < 0) {
				perror("Sender connection - Could not send SYN packet\n");
				exit(EXIT_FAILURE);
			}

			/* If all went well, switch to next state */
			s_state = WAIT_SYNACK;
			break;

			/* Wait for an ACK (SYNACK) for the SYN */
		case WAIT_SYNACK:

			timeout.tv_sec = 5;
			timeout.tv_usec = 0;

			result = select(sockfd + 1, &activeFdSet, NULL, NULL, &timeout);

			if (result == -1) {   //Select fails
				perror("WAIT_SYN select failed");
				exit(EXIT_FAILURE);

			}
			else if (result == 0) { //If a timeout occurs that means the packet was lost
				printf("TIMEOUT: SYN packet lost\n");
				s_state = CLOSED;
				break;

			}
			else { /* Receives a packet */
				if (recvfrom(sockfd, SYNACK_packet, sizeof(*SYNACK_packet), 0, &from, &from_len) != -1) {
					printf("New packet arrived!\n");


					/* If the packet is a SYNACK and have a valid checksum*/
					if (SYNACK_packet->flags == SYNACK && SYNACK_packet->checksum == checksum(SYNACK_packet)) {
						printf("Valid SYNACK packet!\n");
						printf("Packet info - Type: %d\tSeq: %d\n", SYNACK_packet->flags, SYNACK_packet->seq);

						/* Finalize the ACK_packet */
						ACK_packet->seq = SYNACK_packet->seq + 1;
						ACK_packet->checksum = checksum(ACK_packet);

						/* Switch to next state */
						s_state = RCVD_SYNACK;

					}
					else { /* If the packet is not a SYNACK or a mismatch of checksum detected */
						printf("Invalid SYNACK packet!\n");
						printf("Retransmitting SYN packet\n");
						s_state = CLOSED;
					}
				}
				else { /* Couldn't read from socket */
					perror("Can't from read socket");
					exit(EXIT_FAILURE);
				}
			}
			break;

			/* Received SYNACK, send an ACK for that*/
		case RCVD_SYNACK:
			nOfBytes = sendto(sockfd, ACK_packet, sizeof(*ACK_packet), 0, serverName, socklen);

			/* Failed to send ACK to the receiver */
			if (nOfBytes < 0) {
				perror("Sender connection - Could not send SYN packet\n");
				exit(EXIT_FAILURE);
			}

			printf("ACK sent!\n");

			/* If successfully sent, switch to next state */
			s_state = ESTABLISHED;
			break;

			/* Sent an ACK for the SYNACK, wait for retransmissions */
		case ESTABLISHED:

			timeout.tv_sec = 5;
			timeout.tv_usec = 0;

			result = select(sockfd + 1, &activeFdSet, NULL, NULL, &timeout);

			if (result == -1) {   //Select fails
				perror("ESTABLISHED select failed");
				exit(EXIT_FAILURE);

			}
			else if (result == 0) { //Timeout occurs, no packet loss
				printf("TIMEOUT occured\n");
				printf("Connection succeccfully established\n\n");

				/* Free all allocated memory */
				free(SYN_packet);
				free(SYNACK_packet);
				free(ACK_packet);
				return 1;   /* Return that connection was made (maybe the seq of client) */

			}
			else { //Receives SYNACK again, ACK packet was lost
				printf("SYNACK arrived again\n");

				/* Swith to the previous state */
				s_state = RCVD_SYNACK;
			}
			break;

		default:
			break;
		}
	}
}


receiver_connection(int sockfd, const struct sockaddr* client, socklen_t* socklen) {
	fd_set activeFdSet;
	int nOfBytes = 0;
	int result = 0;
	r_state = LISTENING;

	/* Timeout */
	struct timeval timeout;
	timeout.tv_sec = 5;
	timeout.tv_usec = 0;


	/* Initilize SYN packet*/
	rtp* SYN_packet = malloc(sizeof(*SYN_packet));
	if (SYN_packet == NULL) {
		perror("malloc");
		exit(EXIT_FAILURE);
	}
	memset(SYN_packet->data, '\0', sizeof(SYN_packet->data));


	/* Initilize SYNACK packet */
	rtp* SYNACK_packet = malloc(sizeof(*SYNACK_packet));
	if (SYNACK_packet == NULL) {
		perror("malloc");
		exit(EXIT_FAILURE);
	}
	SYNACK_packet->flags = SYNACK;
	memset(SYNACK_packet->data, '\0', sizeof(SYNACK_packet->data));


	/* Initilize ACK packet */
	rtp* ACK_packet = malloc(sizeof(*ACK_packet));
	if (ACK_packet == NULL) {
		perror("malloc");
		exit(EXIT_FAILURE);
	}


	FD_ZERO(&activeFdSet);
	FD_SET(sockfd, &activeFdSet);

	while (1) {
		switch (r_state) {

			/* Listen for a vaild SYN packet */
		case LISTENING:
			printf("Current state: LISTENING\n");

			/* Check if a SYN packet has arrived */
			if (recvfrom(sockfd, SYN_packet, sizeof(*SYN_packet), 0, client, socklen) != -1) {
				printf("New packet arrived!\n");

				/* Check if packet is vaild*/
				if (SYN_packet->flags == SYN && SYN_packet->checksum == checksum(SYN_packet)) {
					printf("Valid SYN packet!\n");
					printf("Packet info - Type: %d\tseq: %d\tWindowSize: %d\n", SYN_packet->flags, SYN_packet->seq, SYN_packet->windowsize);

					/* Finilize SYNACK packet */
					SYNACK_packet->seq = SYN_packet->seq + 1;
					SYNACK_packet->windowsize = SYN_packet->windowsize;
					SYNACK_packet->checksum = checksum(SYNACK_packet);

					/* Switch to next state */
					r_state = RCVD_SYN;

				}
				else { /* Packet is not vaild, not SYN or wrong checksum */
					printf("Invalid SYN packet!\n");

					/* Continue to listen for SYN*/
					r_state = LISTENING;
				}
			}
			else {
				perror("Can't receive SYN packet");
				exit(EXIT_FAILURE);
			}
			break;

			/* Send a SYNACK after a vaild SYN packet*/
		case RCVD_SYN:

			/* Send back a SYNACK to sender */
			nOfBytes = sendto(sockfd, SYNACK_packet, sizeof(*SYNACK_packet), 0, client, *socklen);

			/* Failed to send SYNACK to sender */
			if (nOfBytes < 0) {
				perror("Can't send SYNACK packet");
				exit(EXIT_FAILURE);

			}
			else { /* Successfully sent SYNACK */
				printf("SYNACK sent!\n");
				r_state = WAIT_ACK;
			}
			break;

		case WAIT_ACK:

			timeout.tv_sec = 5;
			timeout.tv_usec = 0;

			result = select(sockfd + 1, &activeFdSet, NULL, NULL, &timeout);

			if (result == -1) {   /* Select failed */
				perror("Select failed");
				exit(EXIT_FAILURE);

			}
			else if (result == 0) { /* timeout occurs, SYNACK packet lost */
				printf("TIMEOUT: SYNACK packet lost\n");
				r_state = RCVD_SYN;

			}
			else { /* Receives a packet */
				if (recvfrom(sockfd, ACK_packet, sizeof(*ACK_packet), 0, client, socklen) != -1) {
					printf("New packet arrived!\n");


					/* If the packet is a ACK and have a valid checksum */
					if (ACK_packet->flags == ACK && ACK_packet->checksum == checksum(ACK_packet)) {
						printf("Valid ACK packet!\n");
						printf("Packet info - Type: %d\tseq: %d\n", ACK_packet->flags, ACK_packet->seq);

						/* Switch to next state */
						r_state = ESTABLISHED;

					}
					else { /* Invalid packet */
						printf("Invalid ACK packet\n");

						/* Switch to previous state */
						r_state = RCVD_SYN;
					}
				}
				else {
					perror("Can't read from socket");
					exit(EXIT_FAILURE);
				}
			}
			break;

		case ESTABLISHED:
			printf("Connection successfully established!\n\n");

			/* Free allocated memory */
			free(SYN_packet);
			free(SYNACK_packet);
			free(ACK_packet);

			return sockfd; /* Retrun that the connecton was made*/
			break;

		default:
			break;
		}
	}
	return -1;
}


int sender_teardown(int sockfd, const struct sockaddr* serverName, socklen_t socklen) {
	struct sockaddr_in from;
	socklen_t fromlen = sizeof(from);
	fd_set activeFdSet;
	int nOfBytes;
	int result;

	/* Timeout */
	struct timeval timeout;
	timeout.tv_sec = 5;
	timeout.tv_usec = 0;

	/* Initilize FIN packet */
	rtp* FIN_packet = malloc(sizeof(*FIN_packet));
	FIN_packet->flags = FIN;
	memset(FIN_packet->data, '\0', sizeof(FIN_packet->data));
	FIN_packet->checksum = checksum(FIN_packet);

	/* Initilize FINACK packet */
	rtp* FINACK_packet = malloc(sizeof(*FINACK_packet));
	memset(FINACK_packet->data, '\0', sizeof(FINACK_packet));

	/* Initilize ACK packet */
	rtp* ACK_packet = malloc(sizeof(*ACK_packet));
	ACK_packet->flags = ACK;
	memset(ACK_packet->data, '\0', sizeof(ACK_packet));

	FD_ZERO(&activeFdSet);
	FD_SET(sockfd, &activeFdSet);

	/* State machine */
	while (1) {
		switch (s_state) {

			/* Start state. The connection is established, send FIN */
		case ESTABLISHED:
			printf("Sending FIN packet\n");
			nOfBytes = sendto(sockfd, FIN_packet, sizeof(*FIN_packet), 0, serverName, socklen);

			/* Failed to send FIN_packet to receiver */
			if (nOfBytes < 0) {
				perror("Send FIN packet fail\n");
				exit(EXIT_FAILURE);
			}

			/* Successfully sent FIN_packet, switch to next state */
			s_state = WAIT_FINACK;
			break;

			/* Wait for an ACK (FINACK) for the FIN*/
		case WAIT_FINACK:
			/* Look if a packet has arrived */
			result = select(sockfd + 1, &activeFdSet, NULL, NULL, &timeout);

			if (result == -1) {   /* Select failed */
				perror("select failed");
				exit(EXIT_FAILURE);

			}
			else if (result == 0) { /* Timeout occurs, packet was lost */
				printf("TIMEOUT - FIN packet loss\n");
				s_state = ESTABLISHED;

			}
			else { /* Receives a packet */
				/* Can read from socket */
				if (recvfrom(sockfd, FINACK_packet, sizeof(*FINACK_packet), 0, &from, &fromlen) != -1) {
					printf("New packet arrived!\n");

					/* Correct packet type and checksum */
					if (FINACK_packet->flags == FINACK && FINACK_packet->checksum == checksum(FINACK_packet)) {

						printf("Valid FINACK packet!\n");
						printf("Packet info - Type: %d\tSeq: %d\n", FINACK_packet->flags, FINACK_packet->seq);

						/* Finalize the ACK_packet */
						ACK_packet->seq = FINACK_packet->seq + 1;
						ACK_packet->checksum = checksum(ACK_packet);

						/* Switch to next state */
						s_state = RCVD_FINACK;

					}
					else { /* Invalid packet type or checksum */
						printf("Invalid FINACK packet\n");
						printf("Retransmitting FIN packet\n");
						/* Switch to previous state */
						s_state = ESTABLISHED;
					}

				}
				else { /* Can't read from socket */
					perror("Can't read socket");
					exit(EXIT_FAILURE);
				}
			}
			break;

		case RCVD_FINACK:
			result = sendto(sockfd, ACK_packet, sizeof(*ACK_packet), 0, serverName, socklen);

			/* Failed to send ACK */
			if (result < 0) {
				perror("Send ACK-packet failed");
				exit(EXIT_FAILURE);
			}

			/* Sent ACK, switch to next state */
			printf("ACK sent!\n");
			s_state = WAIT_TIME;
			break;

		case WAIT_TIME:
			/* Look if a packet has arrived */
			result = select(sockfd + 1, &activeFdSet, NULL, NULL, &timeout);

			if (result == -1) {   /* Select failed */
				perror("select failed");
				exit(EXIT_FAILURE);

			}
			else if (result == 0) { /* Timeout occurs, packet arrived */
				printf("TIMEOUT occurred\n");

				/* Switch to next state */
				s_state = CLOSED;
				break;

			}
			else { /* Receives a new packet, ACK was lost */
				printf("Packet arrived again!\n");

				if (recvfrom(sockfd, FINACK_packet, sizeof(*FINACK_packet), 0, &from, &fromlen) != -1) {
					printf("New packet arrived!\n");

					/* Correct packet type and checksum */
					if (FINACK_packet->flags == FINACK && FINACK_packet->checksum == checksum(FINACK_packet)) {

						printf("Valid FINACK arrived!\n");
						printf("Packet info - Type: %d\tSeq: %d\n", FINACK_packet->flags, FINACK_packet->seq);

						/* Finalize ACK_packet */
						ACK_packet->seq = FINACK_packet->seq + 1;
						ACK_packet->checksum = checksum(ACK_packet);

						/* Switch to previous state */
						s_state = RCVD_FINACK;

					}
					else { /* Invalid packet type or checksum */
						printf("Invalid FINACK packet\n");
						printf("Retransmitting FIN\n");

						/* Switch to begining state */
						s_state = ESTABLISHED;
					}

				}
				else { /* Can't read from socket */
					perror("Can't read socket");
					exit(EXIT_FAILURE);
				}
			}
			break;

			/* Timeout occurred, close connection */
		case CLOSED:

			printf("Connection successfully closed!\n");

			/* Free allocated memory */
			free(FIN_packet);
			free(FINACK_packet);
			free(ACK_packet);

			return 1;   /* Return that connection was closed */
			break;

		default:
			break;
		}
	}
}


int receiver_teardown(int sockfd, const struct sockaddr* client, socklen_t socklen) {
	fd_set activeFdSet;
	int nOfBytes;
	int result;

	r_state = ESTABLISHED;

	/* Timeout */
	struct timeval timeout;
	timeout.tv_sec = 5;
	timeout.tv_usec = 0;

	/* Initilize FIN packet */
	rtp* FIN_packet = malloc(sizeof(*FIN_packet));
	memset(FIN_packet->data, '\0', sizeof(FIN_packet->data));

	/* Initilize FINACK packet */
	rtp* FINACK_packet = malloc(sizeof(*FINACK_packet));
	FINACK_packet->flags = FINACK;
	memset(FINACK_packet->data, '\0', sizeof(FINACK_packet));

	/* Initilize ACK packet */
	rtp* ACK_packet = malloc(sizeof(*ACK_packet));
	memset(ACK_packet->data, '\0', sizeof(ACK_packet));

	FD_ZERO(&activeFdSet);
	FD_SET(sockfd, &activeFdSet);

	while (1) {
		switch (r_state) {

			/* FIN received */
		case ESTABLISHED:
			if (recvfrom(sockfd, FIN_packet, sizeof(*FIN_packet), 0, client, &socklen) != -1) {

				printf("New packet arrived!\n");

				/* Check if packet is FIN and have a valid checksum */
				if (FIN_packet->flags == FIN && FIN_packet->checksum == checksum(FIN_packet)) {
					printf("Valid FIN packet!\n");
					printf("Packet info - Type: %d\tseq: %d\n", FIN_packet->flags, FIN_packet->seq);

					/* Finilize FINACK packet */
					FINACK_packet->seq = FIN_packet->seq + 1;
					FINACK_packet->checksum = checksum(FINACK_packet);

					/* Switch to next state */
					r_state = RCVD_FIN;

				}
				else {
					printf("Invalid FIN packet!\n");

					/* Stay in current state */
					r_state = ESTABLISHED;
				}

			}
			else {
				perror("Can't read from socket");
				exit(EXIT_FAILURE);
			}
			break;

			/* Received FIN, send FINACK */
		case RCVD_FIN:
			nOfBytes = sendto(sockfd, FINACK_packet, sizeof(*FINACK_packet), 0, client, socklen);

			/* Failed to send FINACK */
			if (nOfBytes < 0) {
				perror("Send FINACK failed");
				exit(EXIT_FAILURE);
			}

			printf("FINACK sent!\n");

			/* Switch to next state */
			r_state = WAIT_TIME;
			break;

			/* Sent FINACK, waiting if another packet arrives */
		case WAIT_TIME:

			timeout.tv_sec = 5;
			timeout.tv_usec = 0;

			result = select(sockfd + 1, &activeFdSet, NULL, NULL, &timeout);

			if (result == -1) {
				perror("Select failed");
				exit(EXIT_FAILURE);

			}
			else if (result == 0) { /* Timeout occurs, FINACK was lost */
				printf("TIMEOUT occurred\n");

				/* Retransmit FINACK, switch to previous state */
				r_state = RCVD_FIN;
			}
			else {
				if (recvfrom(sockfd, ACK_packet, sizeof(*ACK_packet), 0, client, &socklen) != -1) {
					printf("New packet arrived!\n");

					/* valid ACK */
					if (ACK_packet->flags == ACK && ACK_packet->checksum == checksum(ACK_packet)) {
						printf("Valid ACK packet!\n");
						printf("Packet info - Type: %d\tseq: %d\n", ACK_packet->flags, ACK_packet->seq);

						/* Switch to next state */
						r_state = CLOSED;

					}
					else {
						printf("Invalid ACK packet!\n");

						/* Switch to previous state */
						r_state = RCVD_FIN;
					}
				}
				else {
					perror("Can't read from socket");
					exit(EXIT_FAILURE);
				}
			}
			break;

			/* Received ACK, close connection */
		case CLOSED:

			printf("Connection successfully closed!\n");

			/* Free allocated memory */
			free(FIN_packet);
			free(FINACK_packet);
			free(ACK_packet);

			return 1; /* Return that connection was closed */
			break;

		default:
			break;
		}
	}
}


void sendFunc(struct rtp client)
{
	client.checksum = checkSumCalc(client);	// checksum before sending packet 

	if(client.flags == SENDING)
	{
		if(client.seq == 1)
		{
			//corrupt_header(&client, 2);
			//testcorrupt++;
		}
		if(outoforder == 1 && client.seq == 10)
		{
			printf("Test of out of order\n");
			outoforder++;
			return;	// Return instead of sending packet (thus skipping seq = 10)
		}
	}

	if(client.flags == SYN && testcorrupt == 5)
	{
		strcpy(client.data, "Test of corrupt ACK");
		corrupt_header(&client, 2);
		testcorrupt++;
	}


	int rc = sendto(sockfd, &client, sizeof(struct rtp), 0, (const struct sockaddr*)&serveradress, sizeof(serveradress));
	if(rc < 0)
		perror("Failed to send message");
}

ssize_t sender_gbn(int sockfd, const void* buf, size_t len, int flags) // receives array of strings as buf
{
	int attempts = 0; // attempts made to send packet, if exceeds limit then throw away and go in to closed state

	/* Initialize DATA packet */
	rtp* DATA_packet = malloc(sizeof(*DATA_packet)); // allocate memory for datapacket of rtp type
	DATA_packet->flags = DATA; // indicate that it is data
	memset(DATA_packet->data, '\0', sizeof(DATA_packet->data));

	/* Initialize ACK packet */
	rtp* ACK_packet = malloc(sizeof(*ACK_packet));
	memset(ACK_packet->data, '\0', sizeof(ACK_packet->data));

	struct sockaddr client_sockaddr;
	socklen_t client_socklen = sizeof(client_sockaddr);

	int num_UNACK_packet = 0;
	size_t DATA_offset = 0;


	int base = 0;                   /* Base sequence number of the window */
	int next_seq_num = 0;           /* Next sequence number to be sent */
	size_t total_packets = len;

	const char** data_array = (const char**)buf;



	while (base < total_packets)
	{
		int window_sizei = 0;        /* Initialize window size for each iteration */

		switch (state.state) { //state.state because of state_t struct

		case ESTABLISHED:

			int num_UNACK_packet = 0;
			size_t DATA_offset = 0;

			// Send packets for current window
			while (window_sizei < state.window_size && next_seq_num < total_packets) // as long as windows size is smaller than 
			{
				//int data_packet_count = 0;
				
				// Prepare DATA packet
				DATA_packet->seq = next_seq_num;
				strncpy(DATA_packet->data, data_array[next_seq_num], sizeof(DATA_packet->data) - 1); // Copy data to packet, buf+next_seq_num
				DATA_packet->checksum = checksum(DATA_packet);

				// Send DATA packet
                        if (attempts > MAX_ATTEMPTS) {
                            // If the max attempts are reached
                            printf("ERROR: Max attempts are reached.\n");
                            errno = 0;
                            state.state = CLOSED;
                            break;
                        } else if (maybe_sendto(sockfd, DATA_packet, sizeof(*DATA_packet), 0, &state.address, state.sck_len) == -1) {
                            // If error in sending DATA packet
                            printf("ERROR: Unable to send DATA packet.\n");
                            state.state = CLOSED;
                            break;
                        } else {
                            // If successfully sent a DATA packet
                            printf("SUCCESS: Sent DATA packet (%d)...\n", DATA_packet->seqnum);
                            printf("type: %d\t%dseqnum: %d\tchecksum(received): %d\tchecksum(calculated): \n", DATA_packet->type, DATA_packet->seqnum, DATA_packet->checksum, checksum(DATA_packet));

                            if (window_sizei == 0) {
                                // If first packet, set time out before FIN
				    alarm(5);
                                //timeout using select
                            }
                            UNACKed_packets_counter++;
			    next_seq_num++;
                        }
                    }
			windowsizei++;
                }
                attempts++;
				
		
				//sendto(sockfd, DATA_packet, sizeof(*DATA_packet), 0, (struct sockaddr*)&client_sockaddr, client_socklen); //sizeof




				
// Start timer for the first packet in the window
                   /* if (window_sizei == 0) {
                      //  gettimeofday(&start, NULL);
		    }

				// Increment window variables
				window_sizei++;
				next_seq_num++;
			}*/
	    // Wait for ACKs
                while (base < next_seq_num)
		{
                    fd_set readfds;
                    FD_ZERO(&readfds);
                    FD_SET(sockfd, &readfds);

                 struct timeval timeout;
                 timeout.timeout_sec = 5;
		 timeout.timeout_usec = 0;

		int result = select(sockfd + 1, &readfds, NULL, NULL, &timeout);

                    if (result == -1) {
                        perror("select");
                        free(DATA_packet);
			free(ACK_packet);
                        return -1;
                    } else if (result == 0) {
                        // Timeout occurred, change state to PACKET_LOSS to retransmit packets
                        state.state = PACKET_LOSS;
                        break;
                    } else { //received ACK
			    ssize_t recv_len = recvfrom(sockfd, ACK_packet, sizeof(*ACK_packet), 0, (struct sockaddr*)&client_sockaddr, &client_socklen);
                        if (recv_len > 0 && ACK_packet->flags == DATA && ACK_packet->checksum == checksum(ACK_packet)) {
                            base = ACK_packet->seq + 1;

                           if (base == next_seq_num) {
				  // All packets in the window are acknowledged
                                state.state = ESTABLISHED; ////////state_t ???
                                break;
                            } else {
                                // More packets are in-flight
                                state.state = RCVD_ACK;
                            }
			   } 
		  } 
		} 
			    
			
			break;

		case PACKET_LOSS:
                for (int i = base; i < next_seq_num; i++) {
                    DATA_packet->seq = i;
                    strncpy(DATA_packet->data, data_array[i], sizeof(DATA_packet->data) - 1);
                    DATA_packet->checksum = checksum(DATA_packet);
                    DATA_packet->flags = DATA;

                    //sendto(sockfd, DATA_packet, sizeof(*DATA_packet), 0, (struct sockaddr*)&client_sockaddr, client_socklen);
			if(maybesendto(sockfd, DATA_packet, sizeof(*DATA_packet), 0, &state.address, state.sck_len) == -1)
			{
	            printf("ERROR: Unable to retransmit DATA packet.\n");
                    state.state = CLOSED;
                    break;
			}
			else {
                    printf("SUCCESS: Retransmitted DATA packet (%d)...\n", DATA_packet->seq);
                }	
                }
            // Restart timeout for retransmissions
            fd_set readfds;
            FD_ZERO(&readfds);
            FD_SET(sockfd, &readfds);

            struct timeval timeout;
            timeout.tv_sec = 5;
            timeout.tv_usec = 0;

            int result = select(sockfd + 1, &readfds, NULL, NULL, &timeout);

            if (result == -1) {
                perror("select");
                free(DATA_packet);
                free(ACK_packet);
                return -1;
            } else if (result == 0) {
                // Timeout occurred again, handle appropriately
                printf("ERROR: Timeout occurred during retransmission.\n");
                state.state = PACKET_LOSS;
            } else {
                ssize_t recv_len = recvfrom(sockfd, ACK_packet, sizeof(*ACK_packet), 0, (struct sockaddr*)&client_sockaddr, &client_socklen);
                if (recv_len > 0 && ACK_packet->flags == ACK && ACK_packet->checksum == checksum(ACK_packet)) {
                    base = ACK_packet->seq + 1;

                    if (base == next_seq_num) {
                        // All packets in the window are acknowledged
                        state.state = ESTABLISHED;
                    } else {
                        // More packets are in-flight
                        state.state = RCVD_ACK;
                    }
                }
            }
            break;
		
                //restart timer for the first packet in the window
                //gettimeofday(&start, NULL);
		/*alarm(5);
                state.state = ESTABLISHED;
			break;*/

		case RCVD_ACK: // continue sending packages within window
			state.state = ESTABLISHED;
			break;

		default:
			break;
		}
	}

	/* Free allocated memory */
	free(DATA_packet);
	free(ACK_packet);
	return 1;

}


ssize_t receiver_gbn(int sockfd, void* buf, size_t len, int flags) {

	int expSeq;

	/* Initialize DATA packet */
	rtp* DATA_packet = malloc(sizeof(*DATA_packet));
	memset(DATA_packet->data, '\0', sizeof(DATA_packet->data));

	/* Initilaze ACK packet */
	rtp* ACK_packet = malloc(sizeof(*ACK_packet));
	ACK_packet->flags = ACK;
	memset(ACK_packet->data, '\0', sizeof(ACK_packet->data));

	struct sockaddr client_addr;
	socklen_t client_len = sizeof(client_addr);

	while (r_state == ESTABLISHED) {
		if (recvfrom(sockfd, DATA_packet, sizeof(*DATA_packet), 0, &client_addr, &client_len) != -1) {
			printf("Received a packet!\n");

			/* If the packet is a FIN */
			if (DATA_packet->flags == FIN && DATA_packet->checksum == checksum(DATA_packet)) {
				printf("Received a valid FIN packet!\n");
				return 0;

			}
			else { /* If the packet is not FIN*/
				if (DATA_packet->flags == DATA && DATA_packet->checksum == checksum(DATA_packet)) {
					printf("Received a valid DATA packet!\n");

					/* If the data packet has expected sequence number */
					if (DATA_packet->seq == expSeq) {
						printf("Data packet has expected sequence number!\n");

						/* Finilize ACK packet */
						ACK_packet->seq = DATA_packet->seq + 1;
						ACK_packet->checksum = checksum(ACK_packet);

					}
					else { /* wrong sequence number, resend old ACK*/
						printf("DATA packet has wrong sequence number!\n");

						/* Finialize ACK packet */
						ACK_packet->seq = expSeq;
						ACK_packet->checksum = checksum(ACK_packet);
					}

					/* Regardless of sequence number, send ACK */
					if (maybe_sendto(sockfd, ACK_packet, sizeof(*ACK_packet), 0, &client_addr, &client_len) == -1) {
						perror("maybe_sendto");
						exit(EXIT_FAILURE);

					}
					else {
						printf("Successfully sent ACK!\n");
					}
				}
			}

		}
		else {
			r_state = CLOSED;
			perror("Can't read from socket\n");
			exit(EXIT_FAILURE);
		}
	}

	/* free allocated memory */
	free(DATA_packet);
	free(ACK_packet);
	return 0;
}


/* Checksum calculator */
uint16_t checksum(rtp* packet) {
	/* Calculate the number of 16-bit words in the packet */
	int nwords = (sizeof(packet->flags) + sizeof(packet->seq) + sizeof(packet->data)) / sizeof(uint16_t);

	/* Create an array to hold the data for checksum calculation */
	uint16_t buffer_array[nwords];

	/* Combine seq and flags fields of the packet into the first element of the buffer_array */
	buffer_array[0] = (uint16_t)packet->seq + ((uint16_t)packet->flags << 8);

	/* Loop through each byte of the data field */
	for (int byte_index = 1; byte_index <= sizeof(packet->data); byte_index++) {

		/* Calculate the index of the word where the byte will be stored */
		int word_index = (byte_index + 1) / 2;

		if (byte_index % 2 == 1) {
			/* If byte_index is odd, store the byte in the lower byte of the word */
			buffer_array[word_index] = packet->data[byte_index - 1];
		}
		else {
			/* If byte_index is even, shift the previously stored byte to the higher byte
			 * of the word and add the new byte */
			buffer_array[word_index] = buffer_array[word_index] << 8;
			buffer_array[word_index] += packet->data[byte_index - 1];
		}
	}

	/* create a pointer to buffer_array and initialize a 32-bit sum*/
	uint16_t* buffer = buffer_array;
	uint32_t sum;

	/*Iterate through each 16-bit word in buffer_array and sum them up*/
	for (sum = 0; nwords > 0; nwords--) {
		sum += *buffer++;
	}

	/* Reduce the sum to a 16-bit value by adding the carry from the high
	 * 16 bits to the low 16 bits */
	sum = (sum >> 16) + (sum & 0xffff);

	sum += (sum >> 16);  /* Add any carry from the previous step */
	return ~sum;    /* Return the one's complement of the sum */

}


/* ERROR generator */
ssize_t maybe_sendto(int sockfd, const void* buf, size_t len, int flags, const struct sockaddr* to, socklen_t tolen) {
	char* buffer = malloc(len);
	memcpy(buffer, buf, len);

	/* Packet not lost */
	if (rand() > LOSS_PROB * RAND_MAX) {

		/* Packet corrupted */
		if (rand() < CORR_PROB * RAND_MAX) {

			/* Selecting a random byte inside the packet */
			int index = (int)((len - 1) * rand() / (RAND_MAX + 1.0));

			/* Inverting a bit */
			char c = buffer[index];
			if (c & 0x01) {
				c &= 0xFE;
			}
			else {
				c |= 0x01;
			}
			buffer[index] = c;
		}

		/* Sending the packet */
		int result = sendto(sockfd, buffer, len, flags, to, tolen);
		if (result == -1) {
			perror("maybe_sendto problem");
			exit(EXIT_FAILURE);
		}
		/* Free buffer and return the bytes sent */
		free(buffer);
		return result;

	}
	else { /* Packet lost */
		return(len);
	}
}
