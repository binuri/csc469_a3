/*
 *   CSC469 Winter 2016 A3
 *   Instructor: Bogdan Simion
 *   Date:       19/03/2016
 *  
 *      File:      client_recv.c 
 *      Author:    Angela Demke Brown
 *      Version:   1.0.0
 *      Date:      17/11/2010
 *   
 * Please report bugs/comments to bogdan@cs.toronto.edu
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>

#include "client.h"

static char *option_string = "f:";

/* For communication with chat client control process */
int ctrl2rcvr_qid;
char ctrl2rcvr_fname[MAX_FILE_NAME_LEN];

int udp_socket_fd;

void usage(char **argv) {
	printf("usage:\n");
	printf("%s -f <msg queue file name>\n",argv[0]);
	exit(1);
}


void open_client_channel(int *qid) {

	/* Get messsage channel */
	key_t key = ftok(ctrl2rcvr_fname, 42);

	if ((*qid = msgget(key, 0400)) < 0) {
		perror("open_channel - msgget failed");
		fprintf(stderr,"for message channel ./msg_channel\n");

		/* No way to tell parent about our troubles, unless/until it 
		 * wait's for us.  Quit now.
		 */
		exit(1);
	}

	return;
}

void send_error(int qid, u_int16_t code)
{
	/* Send an error result over the message channel to client control process */
	msg_t msg;

	msg.mtype = CTRL_TYPE;
	msg.body.status = RECV_NOTREADY;
	msg.body.value = code;

	if (msgsnd(qid, &msg, sizeof(struct body_s), 0) < 0) {
		perror("send_error msgsnd");
	}
							 
}

void send_ok(int qid, u_int16_t port)
{
	/* Send "success" result over the message channel to client control process */
	msg_t msg;

	msg.mtype = CTRL_TYPE;
	msg.body.status = RECV_READY;
	msg.body.value = port;

	if (msgsnd(qid, &msg, sizeof(struct body_s), 0) < 0) {
		perror("send_ok msgsnd");
	} 

}

void init_receiver()
{

	/* 1. Make sure we can talk to parent (client control process) */
	printf("Trying to open client channel\n");

	open_client_channel(&ctrl2rcvr_qid);

	/**** YOUR CODE TO FILL IMPLEMENT STEPS 2 AND 3 ****/
    // NOTE: Used http://www.lowtek.com/sockets/select.html as an example of select with timeout

    int udp_socket_fd;
	/* 2. Initialize UDP socket for receiving chat messages. */

    // Obtain a file descriptor
    if ((udp_socket_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0){
        printf("ERROR: Error in UDP Socket initialization in receiver\n");
        send_error(ctrl2rcvr_qid, SOCKET_FAILED);
        return;
    }    

    
    struct sockaddr_in udp_server_addr;
    memset(&udp_server_addr, 0, sizeof(struct sockaddr_in));
    
    udp_server_addr.sin_family = AF_INET;
    udp_server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    udp_server_addr.sin_port = 0;
    
    if (bind(udp_socket_fd, (struct sockaddr *)&udp_server_addr, sizeof(struct sockaddr_in)) < 0){
        printf("ERROR: Error in UDP Socket binding in receiver\n");
        send_error(ctrl2rcvr_qid, BIND_FAILED);
        return;
    }

    int socket_addr_len = sizeof(struct sockaddr_in);
    if (getsockname(udp_socket_fd, (struct sockaddr *)&udp_server_addr, (socklen_t *)&socket_addr_len) < 0) {
        printf("ERROR: Error in getting UDP Socket name in receiver\n");
        send_error(ctrl2rcvr_qid, NAME_FAILED);
    }


    /* 3. Tell parent the port number if successful, or failure code if not. 
     *    Use the send_error and send_ok functions
     */
    send_ok(ctrl2rcvr_qid, ntohs(udp_server_addr.sin_port));
}




/* Function to deal with a single message from the chat server */

void handle_received_msg(char *buf)
{
	/**** YOUR CODE HERE ****/
    memset(buf, 0, MAX_MSG_LEN);
    int ret = recvfrom(udp_socket_fd,  buf, MAX_MSG_LEN, 0, NULL, NULL);
    
    if (ret < 0){
        printf ("ERROR: Error in recieving messages from chat server\n");
        return;
    }

    struct chat_msghdr *chmh = (struct chat_msghdr *) buf;
    
    printf("%s ::: %s", chmh->sender.member_name, (char *)chmh->msgdata);

}



/* Main function to receive and deal with messages from chat server
 * and client control process.  
 *
 * You may wish to refer to server_main.c for an example of the main 
 * server loop that receives messages, but remember that the client 
 * receiver will be receiving (1) connection-less UDP messages from the 
 * chat server and (2) IPC messages on the from the client control process
 * which cannot be handled with the same select()/FD_ISSET strategy used 
 * for file or socket fd's.
 */
void receive_msgs()
{
	char *buf = (char *)malloc(MAX_MSG_LEN);
  
	if (buf == 0) {
		printf("Could not malloc memory for message buffer\n");
		exit(1);
	}


	/**** YOUR CODE HERE ****/
    
    fd_set listen_fds;
    fd_set ready_fds;
    
    FD_ZERO(&listen_fds);
    FD_SET(udp_socket_fd, &listen_fds);
    
    struct timeval timeout;  /* Timeout for select */

	while(TRUE) {

        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        ready_fds = listen_fds;
    
        if ((select(udp_socket_fd + 1, &ready_fds, NULL, NULL, &timeout)) < 0){
             printf("Error when listning to ready sockets\n");
            exit(1);
        }

        if (FD_ISSET(udp_socket_fd, &ready_fds)){
            handle_received_msg(buf);
        }

        int res;
        msg_t msg;
        res = msgrcv(ctrl2rcvr_qid, &msg, sizeof(struct body_s), RECV_TYPE, IPC_NOWAIT);

       
        if (res == -1 && errno == ENOMSG) {
            continue;
        } else if (res > 0){
            if (msg.body.status == CHAT_QUIT){
                close(udp_socket_fd);
                exit(0);
            } else {
                printf ("ERROR: Unknown control code : %d\n", msg.body.status);
            }
        } else {
            printf("ERROR: Unknown message recieved\n");
            exit(1);
        }

    }

	/* Cleanup */
	free(buf);
	return;
}


int main(int argc, char **argv) {
	char option;

	printf("RECEIVER alive: parsing options! (argc = %d\n",argc);

	while((option = getopt(argc, argv, option_string)) != -1) {
		switch(option) {
		case 'f':
			strncpy(ctrl2rcvr_fname, optarg, MAX_FILE_NAME_LEN);
			break;
		default:
			printf("invalid option %c\n",option);
			usage(argv);
			break;
		}
	}

	if(strlen(ctrl2rcvr_fname) == 0) {
		usage(argv);
	}

	printf("Receiver options ok... initializing\n");

	init_receiver();

	receive_msgs();

	return 0;
}
