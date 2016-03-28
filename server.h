/*
 * CSC469 Winter 2016
 * Bogdan Simion
 * Date: 19/03/2016
 *
 * Adapted by demke, bogdan for CSC469 A3 from:
 *
 *   15-213 Spring 2000 L5 (designed based on L5 of Fall 1999) 
 *  
 *      File:      server.h
 *      Author:    Jun Gao
 *      Version:   1.0.0
 *      Date:      4/12/2000
 *   
 * Please report bugs/comments to bogdan@cs.toronto.edu
 */

#ifndef _SERVER_H
#define _SERVER_H


#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>

#include "defs.h"

/* defines */

/* max number of accepted tcp connections */
#define MAX_CONTROL_SESSIONS    50

#define MAX_ERR_STR_LEN    80

/* max length of a line in room config file */
#define MAX_LINE_LEN    256 

/* data structures */

struct room_type;

struct member_type {
	
	u_int16_t member_id;
	char member_name[MAX_MEMBER_NAME_LEN];
	char member_host_name[MAX_HOST_NAME_LEN];
	
	int quiet_flag;

	/* contains member's ip address and udp port*/
	struct sockaddr_in member_udp_addr;  

	int num_chat_msgs;
	int num_bytes_rcved;
	float bw_usage;
    
	int num_control_msgs;

	struct member_type *next_member;
	struct member_type *prev_member;

	struct member_type *next_room_member;
	struct member_type *prev_room_member;

	struct room_type *current_room;
};

struct room_type {

	char room_name[MAX_ROOM_NAME_LEN];
	struct room_type *next_room;

	int num_of_members;

	int empty_flag;

	/* pointer points to all the members within this room */
	struct member_type *member_list_head;
	struct member_type *member_list_tail;
};


/* global variables */

extern char optstr[];
extern char *optarg;

u_int16_t server_tcp_port;
u_int16_t server_udp_port;

int tcp_socket_fd;
int udp_socket_fd;

int fd_table[MAX_CONTROL_SESSIONS];
int max_fd_idx;

char log_file_name[MAX_FILE_NAME_LEN];
int log_flag;
FILE *logfp;

char room_file_name[MAX_FILE_NAME_LEN];
FILE *rfp;

time_t now;

int sweep_int;

char info_str[MAX_HOST_NAME_LEN + 40];

struct member_type *mem_list_head;
struct member_type *mem_list_tail;

struct room_type *room_list_head;
struct room_type *room_list_tail;

int total_num_of_members;
int total_num_of_rooms;

/* scratch memory used for building messages */
char msg_buf[MAX_MSG_LEN];
int msg_len;

char err_str[MAX_ERR_STR_LEN];

/* function prototypes */

/*
 *  FUNCTION: create_server 
 *
 *  SYNOPSIS: Create a TCP or UDP server 
 *
 *  PASS:     type ==> SOCK_STREAM or SOCK_DGRAM
 *            server_port ==> the port that server binds to 
 *
 *  RETURN:   Returns the socket fd on success
 *
 *  NOTE:     If the specified port has been used, let kernel choose a port;
 *            Information will be logged.
 *           
 */
int create_server(int type, u_int16_t server_port);

/*
 *  FUNCTION: create_room
 *
 *  SYNOPSIS: Create a chat room
 *
 *  PASS:     room_name ==> string that contains the room name
 *
 *  RETURN:   if success, return 0
 *            else return > 0
 *              1: room name too long
 *              2: number of rooms reach maximum
 *              3: room exists
 *
 *  NOTE:     Information will be logged.
 *           
 */
int create_room(char *room_name);

/*
 *  FUNCTION: init_server 
 *
 *  SYNOPSIS: Initialize chat server, do the following:
 *
 *            create tcp and udp server; 
 *            initialize fd_table; 
 *            member, room list
 *            initialization; 
 *            create rooms if room config file is presented. 
 *
 *  PASS:     none 
 *
 *  RETURN:   void
 *
 *  NOTE:     Information will be logged.
 *           
 */
void init_server();

/*
 *  FUNCTION: get_peer_info
 *
 *  SYNOPSIS: Retrieve the peer port number and IP address 
 *
 *  PASS:     fd ==> the conneced fd 
 *            str ==> the return string which contains the time, host:port info
 *
 *  RETURN:   void
 *
 *  NOTE:     
 *           
 */
void get_peer_info(int fd, char *str);

/*
 *  FUNCTION: find_member_with_id
 *
 *  SYNOPSIS: locate the member in the member list based on the input member id
 *
 *  PASS:     member_id ==> search criteria 
 *
 *  RETURN:   if found returns the member pointer 
 *            else return NULL
 *
 *  NOTE:     
 *           
 */
struct member_type *find_member_with_id(u_int16_t member_id);

/*
 *  FUNCTION: remove_member 
 *
 *  SYNOPSIS: remove a member from member list and appropriate room list
 *
 *  PASS:     mt ==> the member needs to be removed 
 *
 *  RETURN:   void 
 *
 *  NOTE:     the caller has to free memory
 *           
 */
void remove_member(struct member_type *mt);

/*
 *  FUNCTION: remove_room 
 *
 *  SYNOPSIS: remove a room from room list
 *
 *  PASS:     rt ==> the member needs to be removed 
 *
 *  RETURN:   void 
 *
 *  NOTE:     the caller has to free memory
 *           
 */
void remove_room(struct room_type *rt);


/*
 *  FUNCTION: dump_control_msg 
 *
 *  SYNOPSIS: log each control message received and sent
 *
 *  PASS:     fd ==> the socket that message is sent or received 
 *            buf ==> the message
 *            type ==> 1: RECEIVE; 0: SEND
 *
 *  RETURN:   void 
 *
 *  NOTE:     
 *           
 */
void dump_control_msg(int fd, char *buf, int type);

/*
 *  FUNCTION: process_chat_msg
 *
 *  SYNOPSIS: receive chat message and distribute to the members in the room 
 *
 *  PASS:     udp_socket_fd ==> the socket that chat message is received.
 *
 *  RETURN:   void
 *
 *  NOTE:
 *
 */
void process_chat_msg(int udp_socket_fd);

/*
 *  FUNCTION: process_control_msg
 *
 *  SYNOPSIS: receive control message and process it by calling 
 *            appropriate functions
 *
 *  PASS:     fd ==> the socket that chat message is received.
 *
 *  RETURN:   void
 *
 *  NOTE:
 *
 */
void process_control_msg(int fd);

/*
 *  FUNCTION: send_control_msg_reply 
 *
 *  SYNOPSIS: send a control msg reply to a client
 *
 *  PASS:     fd ==> the socket to be used for sending 
 *            type ==> message type
 *            id   ==> member id
 *            data ==> message data to be sent
 *
 *  RETURN:   void
 *
 *  NOTE:
 *
 */
void send_control_msg_reply(int fd,
                            u_int16_t type, u_int16_t id, char *data);

/*
 * The following functions process each message request and call the above
 * function to send the client a reply.
 *
 */
void process_register_request(int fd, char *buf);
void process_create_room_request(int fd, struct member_type *mt, char *buf);
void process_room_list_request(int fd, struct member_type *mt, char *buf);
void process_switch_room_request(int fd, struct member_type *mt, char *buf);
void process_member_list_request(int fd, struct member_type *mt, char *buf);
void process_quit_request(int fd, struct member_type *mt, char *buf);

#endif
