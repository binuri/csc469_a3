/*
 *   CSC469 Winter 2016 A3
 *   Instructor: Bogdan Simion
 *   Date:       19/03/2016
 *
 *      File:      client_main.c
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

#include <netinet/in.h>
#include <netdb.h>

#include "client.h"
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/*************** GLOBAL VARIABLES ******************/

char info[150];
FILE *logfp = NULL;

static char *option_string = "h:t:u:n:";

/* For communication with chat server */
/* These variables provide some extra clues about what you will need to
 * implement.
 */
char server_host_name[MAX_HOST_NAME_LEN];

/* For control messages */
u_int16_t server_tcp_port;
struct sockaddr_in server_tcp_addr;

/* For chat messages */
u_int16_t server_udp_port;
struct sockaddr_in server_udp_addr;
int udp_socket_fd;

/* Needed for REGISTER_REQUEST */
char member_name[MAX_MEMBER_NAME_LEN];
u_int16_t client_udp_port;

/* Initialize with value returned in REGISTER_SUCC response */
u_int16_t member_id = 0;

/* For communication with receiver process */
pid_t receiver_pid;
char ctrl2rcvr_fname[MAX_FILE_NAME_LEN];
int ctrl2rcvr_qid;

/* MAX_MSG_LEN is maximum size of a message, including header+body.
 * We define the maximum size of the msgdata field based on this.
 */
#define MAX_MSGDATA (MAX_MSG_LEN - sizeof(struct chat_msghdr))


/************* FUNCTION DEFINITIONS ***********/

static void usage(char **argv) {

    printf("usage:\n");

#ifdef USE_LOCN_SERVER
    printf("%s -n <client member name>\n",argv[0]);
#else
    printf("%s -h <server host name> -t <server tcp port> -u <server udp port> -n <client member name> [-f <client_log\n",argv[0]);
#endif /* USE_LOCN_SERVER */

    exit(1);
}


void log_info(char *msg){
    if (DEBUG){
        fputs(msg, logfp);
    }
    fflush(logfp);
}


void shutdown_clean() {
    /* Function to clean up after ourselves on exit, freeing any
     * used resources
     */

    /* Add to this function to clean up any additional resources that you
     * might allocate.
     */

    log_info("[shutdown_clean] Safely terminating client and receiver.\n");

    msg_t msg;

    /* 1. Send message to receiver to quit */
    msg.mtype = RECV_TYPE;
    msg.body.status = CHAT_QUIT;
    msgsnd(ctrl2rcvr_qid, &msg, sizeof(struct body_s), 0);

    /* 2. Close open fd's */
    close(udp_socket_fd);

    /* 3. Wait for receiver to exit */
    waitpid(receiver_pid, 0, 0);

    /* 4. Destroy message channel */
    unlink(ctrl2rcvr_fname);
    if (msgctl(ctrl2rcvr_qid, IPC_RMID, NULL)) {
        perror("cleanup - msgctl removal failed");
    }

    exit(0);
}



int initialize_client_only_channel(int *qid)
{
    /* Create IPC message queue for communication with receiver process */

    int msg_fd;
    int msg_key;

    /* 1. Create file for message channels */

    snprintf(ctrl2rcvr_fname,MAX_FILE_NAME_LEN,"/tmp/ctrl2rcvr_channel.XXXXXX");
    msg_fd = mkstemp(ctrl2rcvr_fname);

    if (msg_fd  < 0) {
        perror("Could not create file for communication channel");
        return -1;
    }

    close(msg_fd);

    /* 2. Create message channel... if it already exists, delete it and try again */

    msg_key = ftok(ctrl2rcvr_fname, 42);

    if ( (*qid = msgget(msg_key, IPC_CREAT|IPC_EXCL|S_IREAD|S_IWRITE)) < 0) {
        if (errno == EEXIST) {
            if ( (*qid = msgget(msg_key, S_IREAD|S_IWRITE)) < 0) {
                perror("First try said queue existed. Second try can't get it");
                unlink(ctrl2rcvr_fname);
                return -1;
            }
            if (msgctl(*qid, IPC_RMID, NULL)) {
                perror("msgctl removal failed. Giving up");
                unlink(ctrl2rcvr_fname);
                return -1;
            } else {
                /* Removed... try opening again */
                if ( (*qid = msgget(msg_key, IPC_CREAT|IPC_EXCL|S_IREAD|S_IWRITE)) < 0) {
                    perror("Removed queue, but create still fails. Giving up");
                    unlink(ctrl2rcvr_fname);
                    return -1;
                }
            }

        } else {
            perror("Could not create message queue for client control <--> receiver");
            unlink(ctrl2rcvr_fname);
            return -1;
        }

    }

    return 0;
}



int create_receiver()
{
    /* Create the receiver process using fork/exec and get the port number
     * that it is receiving chat messages on.
     */

    int retries = 20;
    int numtries = 0;

    /* 1. Set up message channel for use by control and receiver processes */
    if (initialize_client_only_channel(&ctrl2rcvr_qid) < 0) {
        return -1;
    }

    /* 2. fork/exec xterm */
    receiver_pid = fork();

    if (receiver_pid < 0) {
        fprintf(stderr,"Could not fork child for receiver\n");
        return -1;
    }
    if ( receiver_pid == 0) {
        /* this is the child. Exec receiver */
        char *argv[] = {"xterm",
                "-e",
                "./receiver",
                "-f",
                ctrl2rcvr_fname,
                0
        };

        execvp("xterm", argv);
        printf("Child: exec returned. that can't be good.\n");
        exit(1);
    }

    /* This is the parent */

    /* 3. Read message queue and find out what port client receiver is using */
    while ( numtries < retries ) {
        int result;
        msg_t msg;
        result = msgrcv(ctrl2rcvr_qid, &msg, sizeof(struct body_s), CTRL_TYPE, IPC_NOWAIT);
        if (result == -1 && errno == ENOMSG) {
            sleep(1);
            numtries++;
        } else if (result > 0) {
            if (msg.body.status == RECV_READY) {
                printf("Start of receiver successful, port %u\n", msg.body.value);
                client_udp_port = msg.body.value;
            } else {
                printf("start of receiver failed with code %u\n",msg.body.value);
                return -1;
            }
            break;
        } else {
            perror("msgrcv");
        }
    }
    if (numtries == retries) {
        /* give up.  wait for receiver to exit so we get an exit code at least */
        int exitcode;
        printf("Gave up waiting for msg.  Waiting for receiver to exit now\n");
        waitpid(receiver_pid, &exitcode, 0);
        printf("start of receiver failed, exited with code %d\n",exitcode);
    }

    return 0;
}


int send_control_msg(u_int16_t msg_type, char *res){

    log_info("[send_control_msg] Starting to send control message...\n");

    char *buf = (char *)malloc(MAX_MSG_LEN);
    bzero(buf, MAX_MSG_LEN);

    struct control_msghdr *cmh;
    cmh = (struct control_msghdr *)buf;

    cmh->msg_type = htons(msg_type);

    if (msg_type == REGISTER_REQUEST) {
        struct register_msgdata *rdata;
        rdata = (struct register_msgdata *) cmh->msgdata;
        rdata->udp_port = htons(client_udp_port);
        strcpy((char *)rdata->member_name, member_name);

        cmh->msg_len = sizeof(struct control_msghdr) + 
                       sizeof(struct register_msgdata) +
                       strlen(member_name) + 1;
        
    } else {
        cmh->msg_len = sizeof(struct control_msghdr) + 1;
    }

    cmh->member_id = member_id;

    /** Sending control message */
    int tcp_fd = 0;
    if ((tcp_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        printf("ERROR: Failed to create socket for control messages\n");
        log_info("[send_control_msg Failed to create socket for control messages\n");
        free(buf);
        return - 1;
    }

    // Debug log
    snprintf(info, sizeof(info), "[send_control_msg] Socket created for control messages (TCP) => %d\n", tcp_fd);
    log_info(info);

    if (connect(tcp_fd, (struct sockaddr*)&server_tcp_addr, sizeof(server_tcp_addr)) < 0){
        printf("ERROR: Failed to connect to the server's TCP Socket\n");
        log_info("[send_control_msg] Failed to connect to the server's TCP Socket\n");
        free(buf);
        return - 1;
    }

    log_info("[send_control_msg] Successfully connected to the server's TCP socket\n");

    send(tcp_fd, buf, cmh->msg_len, 0);

    log_info("[send_control_msg] Sent control message to server\n");

    memset(res, 0, MAX_MSG_LEN);

    if (recv(tcp_fd, res, MAX_MSG_LEN, 0) < 0){
        printf("ERROR: Failed to recieve control message from server\n");
        log_info("[send_control_msg] Failed to recieve control message from server\n");
        free(buf);
        return - 1;
    }

    log_info("[send_control_msg] Received control message from server\n");

    close(tcp_fd);

    log_info("[send_control_msg] Successfully finished sending control messages...\n");
    free(buf);
    return 0;


}


/*********************************************************************/

/* We define one handle_XXX_req() function for each type of
 * control message request from the chat client to the chat server.
 * These functions should return 0 on success, and a negative number
 * on error.
 */

int handle_register_req()
{

    log_info("[handle_register_req] Starting to register client with the server\n");


    char res[MAX_MSG_LEN];
    memset(res, 0, MAX_MSG_LEN);

    if (send_control_msg(REGISTER_REQUEST, res) < 0){
        return -1;
    }

    struct control_msghdr *res_hdr= (struct control_msghdr *)res;
    if (ntohs(res_hdr->msg_type) == REGISTER_SUCC){
        member_id = res_hdr->member_id;
        snprintf(info, 
            sizeof(info), 
            "[handle_register_req] Successfully finished registering with the server. The member_id of the client is %d\n", 
            member_id);
        log_info(info);
        printf("SUCCESS: Registration with server complete\n");
    } else {
        log_info("[handle_register_req] Failed to register with the server\n");
        printf("ERROR: %s\n", (char *)res_hdr->msgdata);
        return -2;
    }
    
    return 0;
}

int handle_room_list_req()
{

    return 0;
}
int handle_member_list_req(char *room_name)
{

    return 0;
}

int handle_switch_room_req(char *room_name)
{

    return 0;
}

int handle_create_room_req(char *room_name)
{

    return 0;
}


int handle_quit_req()
{

    log_info("[handle_quit_req] Starting to shutdown the client...\n");

    char return_msg[MAX_MSG_LEN];
    if (send_control_msg(QUIT_REQUEST, return_msg) < 0){
        log_info("[handle_quit_req] Failed to remove client from the server.\n");
        return -1;
    }

    shutdown_clean();
    return 0;
}


int init_client()
{

    char info[150];

    log_info("[init_client] Initializing client\n");

    /* Initialize client so that it is ready to start exchanging messages
     * with the chat server.
     *
     * YOUR CODE HERE
     */

#ifdef USE_LOCN_SERVER

    /* 0. Get server host name, port numbers from location server.
     *    See retrieve_chatserver_info() in client_util.c
     */
    if(retrieve_chatserver_info(
                server_host_name, &server_tcp_port, &server_udp_port) < 0){
        printf("ERROR: Could not retrieve chatserver information");
        log_info("[init_client] Could not retrieve chatserver information\n");
        return -1;
    }
#endif

    /* 1. initialization to allow TCP-based control messages to chat server */

    // Will be the list of addrinfo structs
    struct addrinfo *tcp_info;

    // Hints(restrictions) to be used when getting the address info
    struct addrinfo tcp_hints;
    memset(&tcp_hints, 0, sizeof(tcp_hints));
    tcp_hints.ai_family = AF_INET;
    tcp_hints.ai_socktype = SOCK_STREAM;

    // Defines the service
    char tcp_port[MAX_HOST_NAME_LEN];
    sprintf(tcp_port, "%d", server_tcp_port);

    // Gets the tcp address information for the host
    int status = getaddrinfo(server_host_name, tcp_port, &tcp_hints, &tcp_info);
    if (status != 0){
        printf("ERROR: Failed to initialize valid TCP address information from server\n");
        log_info("[init_client] Failed to initialize valid TCP address info from the server\n");
        return -1;
    }

    // Saving the server address info to be used when connecting
    memcpy(&server_tcp_addr, tcp_info->ai_addr, sizeof(struct sockaddr_in));

    // Debug log
    snprintf(info, 
        sizeof(info), 
        "[init_client] Initialized TCP INFORMATION OF CHAT SERVER => (Given TCP port: %s, Server TCP port: %hu)\n", 
        tcp_port, server_tcp_addr.sin_port);
    log_info(info);

    /* 2. initialization to allow UDP-based chat messages to chat server */
    struct addrinfo *udp_info;

    struct addrinfo udp_hints;
    memset(&udp_hints, 0, sizeof(udp_hints));
    udp_hints.ai_family = AF_INET;
    udp_hints.ai_socktype = SOCK_DGRAM;

    char udp_port[MAX_HOST_NAME_LEN];
    sprintf(udp_port, "%d", server_udp_port);

    status = getaddrinfo(server_host_name, udp_port, &udp_hints, &udp_info);
    if (status != 0){
        printf("ERROR: Failed to initialize valid UDP address info for server\n");
        log_info("[init_client] Failed to initialize valid UDP address info for server\n");
        return -1;
    }

    // Saving the server address info to be used when connecting
    memcpy(&server_udp_addr, udp_info->ai_addr, sizeof(struct sockaddr_in));

     // Debug log
    snprintf(info, 
        sizeof(info),
         "[init_client] Initialized UDP INFORMATION OF CHAT SERVER => (Server UDP port: %hu)\n", 
         server_udp_addr.sin_port);
    log_info(info);

    // Create the socket to be used for chat messages
    int udp_socket_fd = socket(udp_hints.ai_family, udp_hints.ai_socktype, 0);
    if (udp_socket_fd < 0){
        printf("ERROR: Failed to create socket for chat messages\n");
        log_info("[init_client] Failed to create socket for chat messages\n");
        return -1;
    }

    // Debug log
    snprintf(info, 
        sizeof(info), 
        "[init_client] Socket file descriptor created for chat messages (UDP) => %d\n", 
        udp_socket_fd);
    log_info(info);

    /* 3. spawn receiver process - see create_receiver() in this file. */

    if ((create_receiver()) != 0){
        printf("ERROR: Failed to create receiver for chat messages\n");
        log_info("[init_client] Failed to create receiver for chat messages\n");
        return -1;
    }

    log_info("[init_client] Created receiver for chat messages\n");

    /* 4. register with chat server */
    if ((status = handle_register_req()) != 0){
        printf("ERROR: Failed to register with chat server\n");
        log_info("[init_client] Failed to register with chat server\n");

        shutdown_clean();
        return -1;
    }

     log_info("[init_client] Successfully registered client with chat server\n");

    return 0;
}

void handle_chatmsg_input(char *inputdata)
{
    /* inputdata is a pointer to the message that the user typed in.
     * This function should package it into the msgdata field of a chat_msghdr
     * struct and send the chat message to the chat server.
     */

    char *buf = (char *)malloc(MAX_MSG_LEN);

    if (buf == 0) {
        printf("Could not malloc memory for message buffer\n");
        shutdown_clean();
        exit(1);
    }

    bzero(buf, MAX_MSG_LEN);


    /**** YOUR CODE HERE ****/


    free(buf);
    return;
}


/* This should be called with the leading "!" stripped off the original
 * input line.
 *
 * You can change this function in any way you like.
 *
 */
void handle_command_input(char *line)
{
    char cmd = line[0]; /* single character identifying which command */
    int len = 0;
    int goodlen = 0;
    int result;

    line++; /* skip cmd char */

    /* 1. Simple format check */

    switch(cmd) {

    case 'r':
    case 'q':
        if (strlen(line) != 0) {
            printf("Error in command format: !%c should not be followed by anything.\n",cmd);
            return;
        }
        break;

    case 'c':
    case 'm':
    case 's':
        {
            int allowed_len = MAX_ROOM_NAME_LEN;

            if (line[0] != ' ') {
                printf("Error in command format: !%c should be followed by a space and a room name.\n",cmd);
                return;
            }
            line++; /* skip space before room name */

            len = strlen(line);
            goodlen = strcspn(line, " \t\n"); /* Any more whitespace in line? */
            if (len != goodlen) {
                printf("Error in command format: line contains extra whitespace (space, tab or carriage return)\n");
                return;
            }
            if (len > allowed_len) {
                printf("Error in command format: name must not exceed %d characters.\n",allowed_len);
                return;
            }
        }
        break;

    default:
        printf("Error: unrecognized command !%c\n",cmd);
        return;
        break;
    }

    /* 2. Passed format checks.  Handle the command */

    switch(cmd) {

    case 'r':
        result = handle_room_list_req();
        break;

    case 'c':
        result = handle_create_room_req(line);
        break;

    case 'm':
        result = handle_member_list_req(line);
        break;

    case 's':
        result = handle_switch_room_req(line);
        break;

    case 'q':
        result = handle_quit_req(); // does not return. Exits.
        break;

    default:
        printf("Error !%c is not a recognized command.\n",cmd);
        break;
    }

    /* Currently, we ignore the result of command handling.
     * You may want to change that.
     */
    (void)result;
    return;
}

void get_user_input()
{
    char *buf = (char *)malloc(MAX_MSGDATA);
    char *result_str;

    while(TRUE) {

        bzero(buf, MAX_MSGDATA);

        printf("\n[%s]>  ",member_name);

        result_str = fgets(buf,MAX_MSGDATA,stdin);

        if (result_str == NULL) {
            printf("Error or EOF while reading user input.  Guess we're done.\n");
            break;
        }

        /* Check if control message or chat message */

        if (buf[0] == '!') {
            /* buf probably ends with newline.  If so, get rid of it. */
            int len = strlen(buf);
            if (buf[len-1] == '\n') {
                buf[len-1] = '\0';
            }
            handle_command_input(&buf[1]);

        } else {
            handle_chatmsg_input(buf);
        }
    }

    free(buf);

}

int main(int argc, char **argv)
{
    char option;

    while((option = getopt(argc, argv, option_string)) != -1) {
        switch(option) {
        case 'h':
            strncpy(server_host_name, optarg, MAX_HOST_NAME_LEN);
            break;
        case 't':
            server_tcp_port = atoi(optarg);
            break;
        case 'u':
            server_udp_port = atoi(optarg);
            break;
        case 'n':
            strncpy(member_name, optarg, MAX_MEMBER_NAME_LEN);
            break;
        default:
            printf("invalid option %c\n",option);
            usage(argv);
            break;
        }
    }

    if (DEBUG){
        logfp = fopen("pclient.log", "w");
        if (logfp == NULL){
            printf("Invalid log file provided for chat client\n");
        } else {
            printf("NOTE: Logging for chat client enabled\n");
            fputs("Starting log for chat client...\n", logfp);
            fflush(logfp);
        }
    }

    //REMOVE ME
    printf("Header sizes:\n");
    printf("control_msghdr: %lu bytes\n",sizeof(struct control_msghdr));
    printf("chat_msghdr: %lu bytes\n",sizeof(struct chat_msghdr));
    printf("register_msgdata: %lu bytes\n",sizeof(struct register_msgdata));
#ifdef USE_LOCN_SERVER

    printf("Using location server to retrieve chatserver information\n");

    if (strlen(member_name) == 0) {
        usage(argv);
    }

#else

    if(server_tcp_port == 0 || server_udp_port == 0 ||
       strlen(server_host_name) == 0 || strlen(member_name) == 0) {
        usage(argv);
    }

#endif /* USE_LOCN_SERVER */

    member_id = 0;
    if (init_client() < 0) {
        printf("Chat client is terminated since the the initialization was unsuccessful\n");
        return -1;
    }

    get_user_input();

    return 0;
}



