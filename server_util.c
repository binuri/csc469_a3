/*
 * CSC469 Winter 2016
 * Bogdan Simion
 * Date: 19/03/2016
 * 
 * Adapted by demke, bogdan for CSC469 A3 from:
 *
 *   15-213 Spring 2000 L5 (designed based on L5 of Fall 1999) 
 *  
 *      File:      server_util.c
 *      Author:    Jun Gao
 *      Version:   1.0.0
 *      Date:      4/12/2000
 *      History:   4/16/2000 minor bug fix: add bzero() in process_control_msg()
 *   
 *   Please report bugs/comments to bogdan@cs.toronto.edu
 */


#include <sys/un.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <netdb.h>

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>

#include <sys/stat.h>
#include <fcntl.h>

#include "server.h"


/* for message logging purpose */
char *msg_arr[] = {

"dummy",

"REGISTER_REQUEST",
"REGISTER_SUCC",
"REGISTER_FAIL",

"ROOM_LIST_REQUEST",
"ROOM_LIST_SUCC",
"ROOM_LIST_FAIL",

"MEMBER_LIST_REQUEST",
"MEMBER_LIST_SUCC",
"MEMBER_LIST_FAIL",

"SWITCH_ROOM_REQUEST",
"SWITCH_ROOM_SUCC",
"SWITCH_ROOM_FAIL",

"CREATE_ROOM_REQUEST",
"CREATE_ROOM_SUCC",
"CREATE_ROOM_FAIL",

"MEMBER_KEEP_ALIVE",

"QUIT_REQUEST"

};

#ifdef USE_LOCN_SERVER

/* This function announces that the chatserver is ready to accept
 * clients, by copying a file to a CDF webserver.  
 */
static void announce_server_ready(char *local_host_name) {
	char template[40] = "/tmp/chatserver.txt.XXXXXX";
	int fd = mkstemp(template);

	/* space for output: hostname + 5 for each port num, 2 spaces, and \0 */
	int maxlen = MAX_HOST_NAME_LEN + 15; /* 2 extra for good measure */
	char output[MAX_HOST_NAME_LEN + 15]; 

	snprintf(output, maxlen, "%s %hu %hu", local_host_name,
		 server_tcp_port,server_udp_port); 

	write(fd, output, strlen(output));
	close(fd);


	/* Now, send the temp file to web server and remove it */
	bzero(output, maxlen);
	snprintf(output, maxlen, "scp %s cdf.toronto.edu:/u/csc469h/winter/public_html/chatserver.txt",template);

	if (system(output) < 0) {
		unlink(template);
		perror("system");
		exit(1);
	}
	unlink(template);
}
#endif /* USE_LOCN_SERVER */

/* Converts fields in a received control_msghdr into host byte order */
static void ntoh_control_header(struct control_msghdr *cmh) {
	cmh->msg_type = ntohs(cmh->msg_type);
	cmh->member_id = ntohs(cmh->member_id);
	cmh->msg_len = ntohs(cmh->msg_len);
}

int
create_server(int type, u_int16_t server_port) {
	struct sockaddr_in server_addr;
	socklen_t server_addr_len;
	int socket_fd;
	char type_str[4];

	server_addr_len = sizeof(server_addr);

	if( (socket_fd = socket(AF_INET, type, 0)) < 0) {
		perror("socket");
		exit(1);
	}

	bzero(&server_addr, server_addr_len);
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(server_port);
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

	if( bind(socket_fd, (struct sockaddr *)&server_addr,
		 server_addr_len) < 0 ) {
		perror("bind");

		server_addr.sin_port = 0;
		if( bind(socket_fd, (struct sockaddr *)&server_addr,
			 server_addr_len) < 0 ) {
			perror("bind");
			exit(1);
		}	

		if( getsockname(socket_fd, (struct sockaddr *)&server_addr,
				&server_addr_len) < 0 ) {
			perror("getsockname");
			exit(1);
		}
		server_port = ntohs(server_addr.sin_port);
	} 
    

	/* server is created successfully */

	if( type == SOCK_STREAM) {
		if( listen(socket_fd, 5) < 0 ) {
			perror("listen");
			exit(1);
		}
		snprintf(type_str, 4, "TCP");		
		server_tcp_port = server_port;
	} else {
		snprintf(type_str, 4, "UDP");
		server_udp_port = server_port;
	}

	printf("Chat server listening on %s port: %hu\n", type_str, server_port);
	if(log_flag) {
		fprintf(logfp, "Chat server is listening on %s port: %hu\n",
			type_str, server_port);
		fflush(logfp);
	}

	return socket_fd;
};

int create_room(char *room_name) {
	struct room_type *rt;
	struct room_type *tmp_rptr;

	/* first check the length of room_name, discard if too long */
	if(strlen(room_name) > MAX_ROOM_NAME_LEN) {
		return 1;
	}

	rt = (struct room_type *)malloc(sizeof(struct room_type));
	if(rt == NULL) {
		printf("Memory used up when trying to create room\n");
		exit(1);	
	}

	bzero(rt, sizeof(struct room_type));

	strcpy(rt->room_name, room_name);

	/* make sure we are not exceeding maximum allowable number of rooms */

	if(total_num_of_rooms == MAX_NUM_OF_ROOMS) {
		return 2;
	}

	/* 
	 * go through room list and see whether a room with same name exists 
	 */
    
	if(room_list_head == NULL) {

		/* no rooms yet, create it */
		room_list_head = rt;
		room_list_tail = room_list_head;
	} else {
		for(tmp_rptr=room_list_head; tmp_rptr != NULL; tmp_rptr=tmp_rptr->next_room){
			if(!strcmp(rt->room_name, tmp_rptr->room_name)) {
				/* room exists */
				return 3;
			}
		}

		/* add the new room to the tail */
		room_list_tail->next_room = rt;
		room_list_tail = rt;
	}

	total_num_of_rooms ++;

	if(log_flag){
		fprintf(logfp, "Room [%s] is created.\n", room_name);
		fprintf(logfp, "Total number of rooms:%d\n", total_num_of_rooms);
		fflush(logfp);
	}
	return 0;
};

void 
init_server(){
	int i;

	char local_host_name[MAX_HOST_NAME_LEN];
	struct hostent *hp;
	char err_str[80];

	now = time(NULL);

	/* get the host name that server is running on */
	if (gethostname(local_host_name, MAX_HOST_NAME_LEN) < 0) {
		perror("Bailing - Failed gethostname");
		exit(1);
	}
	  
	/* get the full name */
	hp = gethostbyname(local_host_name);

	if (hp == NULL) {
		fprintf(stderr,"Bailing - Failed gethostbyname for %s:",local_host_name);
		switch (h_errno) {
		case HOST_NOT_FOUND:
			snprintf(err_str, 80, "Host not found.\n");
			break;
		case NO_ADDRESS:
			snprintf(err_str,80,"Requested name is valid but does not have an IP address.\n");
			break;
		case NO_RECOVERY:
			snprintf(err_str,80,"Unrecoverable name server error.\n");
			break;
		case TRY_AGAIN:
			snprintf(err_str,80,"Temporary error on authoritative name server. Try again later.\n");
			break;
		default:
			snprintf(err_str,80,"Unexpected error code.\n");
			break;
		}
		fprintf(stderr,"%s",err_str);
		if (log_flag) {
			fprintf(logfp, "%s",err_str);
			fflush(logfp);
		}
		exit(1);
	}

	if(log_flag) {	
		fprintf(logfp, "%sChat server starts on host: %s\n", 
			ctime(&now), hp->h_name); 
		fflush(logfp);
	}

	/* create master tcp and udp servers */
	tcp_socket_fd = create_server(SOCK_STREAM, server_tcp_port);
	udp_socket_fd = create_server(SOCK_DGRAM, server_udp_port);

	/* initialize the fd_table: -1 means not used */
    
	for( i = 0; i < MAX_CONTROL_SESSIONS; i++) {
		fd_table[i] = -1;
	}

	max_fd_idx = -1;

	/* member, room initialization */

	mem_list_head = NULL;
	mem_list_tail = mem_list_head;
	total_num_of_members = 0;

	room_list_head = NULL;
	room_list_tail = room_list_head;
	total_num_of_rooms = 0;

	if(room_file_name[0] != 0) {
		/* open room file and create some rooms */
		if( (rfp=fopen(room_file_name, "r")) == NULL) {
			perror("fopen");
		} else {
			char line[MAX_LINE_LEN];
			while(!feof(rfp)) {
				fgets(line, MAX_LINE_LEN, rfp);
				if(!feof(rfp)) {
					char *str[MAX_NUM_OF_ROOMS];
					int i;
		
					if(line[strlen(line)-1] == '\n')
						line[strlen(line)-1] = '\0';
					/* parse line to get names */
					str[0] = NULL;
					str[0] = strtok(line, " ");
					if(str[0] != NULL)
						create_room(str[0]);
					for(i=1; i < MAX_NUM_OF_ROOMS; i++) {
						str[i] = NULL;
						if((str[i] = strtok(NULL, " ")) != NULL){
							if(str[i] != NULL)
								create_room(str[i]);
						}
						else 
							break;		
					}
				}
			}
		}
	}

#ifdef USE_LOCN_SERVER
	announce_server_ready(hp->h_name);
#endif

	return;
}


/* 
 * Returns a string with a timestamp, the hostname and the port number of 
 * the peer connected to the socket "fd".
 * 
 * The port number is converted from network byte order to host byte order
 * before printing it into the string.
 */
void get_peer_info(int fd, char *str) {
	struct sockaddr_in peer_addr;
	socklen_t peer_addr_len;
	struct hostent *host_entry;
	char *tp;

	peer_addr_len = sizeof(peer_addr);
	if(getpeername(fd, (struct sockaddr *)&peer_addr, &peer_addr_len) < 0 ) {
		perror("getpeername");
		return;
	}
	host_entry = gethostbyaddr((char *)&peer_addr.sin_addr,
				   sizeof(peer_addr.sin_addr), AF_INET);

	now = time(NULL);
	tp = ctime(&now);
	tp[strlen(tp)-1] = '\0'; /* Chop off newline in string from ctime */

	sprintf(str, "%s %s:%hu", tp, host_entry->h_name, ntohs(peer_addr.sin_port));
	return;
}

/* Assumes member_id is in host byte order. */
struct member_type *find_member_with_id(u_int16_t member_id) {
	struct member_type *mt;

	for(mt = mem_list_head; mt != NULL; mt=mt->next_member) {
		if(mt->member_id == member_id)
			break;
	}
	return mt; 

}

void remove_member(struct member_type *mt){

	if(mt->current_room != NULL) {

		/* remove the member from its current room */
		if(mt->prev_room_member == NULL) {
			/* this member is the first member in the room */
			mt->current_room->member_list_head = mt->next_room_member;
			if(mt->current_room->member_list_head == NULL)
				mt->current_room->member_list_tail = 
					mt->current_room->member_list_head;
			else 
				mt->current_room->member_list_head->prev_room_member = NULL;
		} else {

			/* not the first member */
			mt->prev_room_member->next_room_member = mt->next_room_member;
			if( mt->next_room_member != NULL ) 
				mt->next_room_member->prev_room_member = mt->prev_room_member;
			else 
				mt->current_room->member_list_tail = mt->prev_room_member;
		
		}
		     
		mt->current_room->num_of_members --;
	}
	/* remove the member from the member list */

	if(mt->prev_member == NULL) {
		mem_list_head = mt->next_member;
		if(mem_list_head == NULL)
			mem_list_tail = mem_list_head;
		else
			mem_list_head->prev_member = NULL;
	} else {
		mt->prev_member->next_member = mt->next_member;
		if(mt->next_member != NULL ) 
			mt->next_member->prev_member = mt->prev_member;
		else 
			mem_list_tail = mt->prev_member;
	}

	total_num_of_members --;

	/* NOTE: we let the caller free the memory */

	return;
}

void remove_room(struct room_type *rt){
	struct room_type *trt;

	/* this room has no members */

	for(trt = room_list_head; trt != NULL; trt = trt->next_room) {
		if(trt == rt) {
			/* head */
			room_list_head = trt->next_room;
			if(room_list_head == NULL)
				room_list_tail = NULL;
			break;
		} else if(trt->next_room == rt){
			trt->next_room = rt->next_room;
			if(trt->next_room == NULL)
				room_list_tail = NULL;
			break;
		}
	}

	/* NOTE: we let the caller to free the memory */
	return;
}


/* Assumes input msg is in network host byte order.
 */
void dump_control_msg(int fd, char *msg, int type){
	struct control_msghdr *cmh;
	
	get_peer_info(fd, info_str);
	if(log_flag) {
		if(type == 1)
			fprintf(logfp, "RECEIVE %s control message\n", info_str);
		else
			fprintf(logfp, "SEND    %s control message\n", info_str);

		fflush(logfp);	
	}

	cmh = (struct control_msghdr *)msg;

	if(cmh->msg_type == REGISTER_REQUEST) {
		if(log_flag){
			struct register_msgdata *rdata;
			fprintf(logfp, "msg_type:%s\tmsg_len:%d\n",
				msg_arr[cmh->msg_type], cmh->msg_len);
			fprintf(logfp, "msg_data:\n");
			rdata =(struct register_msgdata *)cmh->msgdata;
			fprintf(logfp, "udp_port:%hu\n", ntohs(rdata->udp_port));
			fprintf(logfp, "member_name:%s\n", (char *)rdata->member_name);
			fflush(logfp);
		}
	} else if( cmh->msg_type >=REGISTER_SUCC && cmh->msg_type <= QUIT_REQUEST) {
		if(log_flag){
			fprintf(logfp, 
				"msg_type:%s\tmsg_len:%d\tmember_id:%d\n",
				msg_arr[cmh->msg_type],
				cmh->msg_len, cmh->member_id);
			if(cmh->msg_len > sizeof(struct control_msghdr)) {
				fprintf(logfp, "msg_data:");
				fprintf(logfp, "%s\n", (char *)cmh->msgdata);
			}
			fflush(logfp);
		}
	} else {
		if(log_flag) {
			fprintf(logfp, "Unrecognized message type! %d\n",cmh->msg_type);
			fflush(logfp);
		}
	}

}

void
process_chat_msg(int udp_socket_fd) {
	int n;
	char buf[MAX_MSG_LEN];
	struct chat_msghdr *cmh;
	struct member_type *mt;
	struct member_type *tmp_mptr;

	bzero(buf, MAX_MSG_LEN);

	n = recvfrom(udp_socket_fd, buf, MAX_MSG_LEN, 0, NULL, 0);
	if(n<0) {
		perror("recvfrom");
		return;
	} 

	cmh = (struct chat_msghdr *)buf;

	/* now distribute to all the members in the group */

	/* find the member first */
	if( (mt =find_member_with_id(ntohs(cmh->sender.member_id))) == NULL) {
		/* no match, ignore: invalid id*/
		if(log_flag) {
			fprintf(logfp, 
				"Chat message is discarded because the sender's member id is invalid!\n");
			fflush(logfp);
		}
		return;

	}

	mt->quiet_flag = 0; 

	strcpy(cmh->sender.member_name, mt->member_name);

	/* update certain things of the member */
	mt->num_chat_msgs ++;
	mt->num_bytes_rcved += n;

	if(log_flag) {
		char *tp;

		now = time(NULL);
		tp = ctime(&now);
		tp[strlen(tp)-1] = '\0'; /* Chop off newline */

		fprintf(logfp, "Chat message from [%s %d](%s)::\n", 
			mt->member_name, mt->member_id, tp);
		fprintf(logfp, "\"%s\"\n", (char *)cmh->msgdata); 
		fprintf(logfp, "Received %d chat messages(%d bytes) from this member.\n",
			mt->num_chat_msgs, mt->num_bytes_rcved);
		fflush(logfp);
	}
    

	/* find which room this member is in */
	if(mt->current_room == NULL) {
		if(log_flag) {
			fprintf(logfp, 
				"Chat message is discarded because the sender is not in any room!\n");
			fflush(logfp);
		}
		return;
	}

	for(tmp_mptr = mt->current_room->member_list_head; tmp_mptr != NULL;
	    tmp_mptr = tmp_mptr->next_room_member) {
		/* send messages one by one, iteratively */
		n = sendto(udp_socket_fd, cmh, n, 0,
			   (struct sockaddr *)&tmp_mptr->member_udp_addr, 
			   sizeof(struct sockaddr_in));
		if(n<0) {
			perror("send to");
			return;
		}

	}
	if(log_flag) {
		fprintf(logfp, "Chat message is broadcast to room [%s(%d)].\n",
			mt->current_room->room_name, mt->current_room->num_of_members);             
		fflush(logfp);
	}
     
	return;
}

void 
process_control_msg(int fd) {
	struct control_msghdr *cmh;
	struct member_type *mt;
	char buf[MAX_MSG_LEN];

	bzero(buf, MAX_MSG_LEN);

	/* NOTE: In this version, do one read */
	if (read(fd, buf, MAX_MSG_LEN) < 0) {
		if(log_flag) {
			fprintf(logfp, "process_control_msg read error: %s\n",
				strerror(errno));
			fflush(logfp);
		}
		/* if read failed, just return */
		return;
	}

	cmh = (struct control_msghdr *)buf;
	
	/* Convert message header to host order and log it */
	ntoh_control_header(cmh);
	dump_control_msg(fd, buf, 1);

	/* make sure the sender has a valid id */

	if( cmh->msg_type >= ROOM_LIST_REQUEST && cmh->msg_type <= QUIT_REQUEST ) {
		if((mt=find_member_with_id(cmh->member_id)) == NULL) {

			/* no match, send fail message : invalid id*/
			strcpy(err_str, "Member id invalid!");

			/* hack! not valid for message type 19 and 20! */
			send_control_msg_reply(fd, cmh->msg_type+2, 0, err_str);

			return;
		} else {
			/* member id valid */
			mt->quiet_flag = 0;
		}
	}


	switch(cmh->msg_type) {

	case REGISTER_REQUEST:
		process_register_request(fd, buf);
		break;

	case ROOM_LIST_REQUEST: 
		process_room_list_request(fd, mt, buf);
		break;

	case MEMBER_LIST_REQUEST:
		process_member_list_request(fd, mt, buf);
		break;

	case SWITCH_ROOM_REQUEST:
		process_switch_room_request(fd, mt, buf);
		break;

	case CREATE_ROOM_REQUEST: 
		process_create_room_request(fd, mt, buf);
		break;

	case MEMBER_KEEP_ALIVE:
		break;

	case QUIT_REQUEST: 
		process_quit_request(fd, mt, buf);
		break;

	default:
		if(log_flag) {
			fprintf(logfp, "Unrecognized message type!\n");
			fflush(logfp);
		}
		break;
	}

	return;
}


/* Input parameters "type" and "id" should be in host byte order */
void send_control_msg_reply(int fd, 
			    u_int16_t type, u_int16_t id, char *data){
	struct control_msghdr *cmh;
	int len;

	bzero(msg_buf, MAX_MSG_LEN);
	cmh = (struct control_msghdr *)msg_buf;


	len = sizeof(struct control_msghdr);
	if(data != NULL) {
		strcpy((char *)cmh->msgdata, data);
		len += strlen(data);
	}

	cmh->msg_type = htons(type);
	cmh->member_id = htons(id);
	cmh->msg_len = htons(len);

	write(fd, msg_buf, len);

	/* Convert header to host byte order and log it */
	ntoh_control_header(cmh);
	dump_control_msg(fd, msg_buf, 0);

	return;
}


/* All process_x_request functions receive as input a message buffer
 * with the header already converted to host byte order.
 * Hence, no further conversion should be performed in these functions 
 * (unless it is back to network order for sending a reply).
 *
 * Currently, these functions do not make use the message header at 
 * all, they just use the size of the header struct to locate the
 * message body.
 */

void process_register_request(int fd, char *msg) {
	struct register_msgdata *rdata;
	struct member_type *mt;


	struct member_type *tmp_ptr;

	struct sockaddr_in peer_addr;
	socklen_t peer_addr_len;


	bzero(msg_buf, MAX_MSG_LEN);
	rdata =(struct register_msgdata *)((struct control_msghdr *)msg)->msgdata;

	/* all right, someone wants to register */

	if(total_num_of_members == MAX_NUM_OF_MEMBERS) {
		/* can't take any more member */
		strcpy(err_str, "Number of members reached maximum!");
		send_control_msg_reply(fd, REGISTER_FAIL, 0, err_str);
		return;
	}

	if( (mt = (struct member_type *)malloc(sizeof(struct member_type))) ==
	    NULL) {
		printf("Memory used up when try to create member!\n");
		exit(1);
	}

	/* set the member object to all zero */
	bzero(mt, sizeof(struct member_type));

	strncpy(mt->member_name, (char *)rdata->member_name, MAX_MEMBER_NAME_LEN);

	mt->member_udp_addr.sin_family = AF_INET;
	/* Leave udp_port contained in message in network byte order */
	mt->member_udp_addr.sin_port = rdata->udp_port;

	peer_addr_len = sizeof(peer_addr);
	if(getpeername(fd, (struct sockaddr *)&peer_addr, &peer_addr_len) < 0 ) {
		perror("getpeername");
		return;
	}

	mt->member_udp_addr.sin_addr = peer_addr.sin_addr;

	/* insert this new member to the member list */

	/* make sure the member name is not used before */

	if(mem_list_head == NULL) {
		/* no member yet */
		mem_list_head = mt;
		mem_list_tail = mt;
		total_num_of_members ++;
	} else {

		tmp_ptr = mem_list_head;

		while(tmp_ptr != NULL) {
			if(!strcmp(tmp_ptr->member_name, mt->member_name)) {
				/* return a reject message */
				strcpy(err_str, "Name has already been used!");
				send_control_msg_reply(fd, REGISTER_FAIL, 0, err_str);

				return;
			} else {
				tmp_ptr = tmp_ptr->next_member;            
			}
		}	
		if(tmp_ptr == NULL){
			/* add to the tail */
			mem_list_tail->next_member = mt;
			mt->prev_member = mem_list_tail;
			mem_list_tail = mem_list_tail->next_member;
	
			total_num_of_members ++;
		}

	}

	/* create an id */
	for(;;) {
		u_int16_t tmp_id;
		tmp_ptr = mem_list_head;
		tmp_id = 1 + (u_int16_t) (65535.0 * rand()/(RAND_MAX+1.0));
		while(tmp_ptr != mem_list_tail) {
			if(tmp_id != tmp_ptr->member_id)
				tmp_ptr = tmp_ptr->next_member;
			else
				break;
		}
		if(tmp_ptr == mem_list_tail) {
			mem_list_tail->member_id = tmp_id;
			break;
		}
	}

    
	/* send accept message */

	send_control_msg_reply(fd, REGISTER_SUCC, mem_list_tail->member_id, NULL);


	if(log_flag) {
		fprintf(logfp, "Total number of members:%d\n", total_num_of_members);
		fflush(logfp);
	}

	return;
}



void process_create_room_request(int fd, struct member_type *mt, char *msg) {
	int ret;

	/* all right, someone wants to create a room */
	bzero(msg_buf, MAX_MSG_LEN);

	ret = create_room((char *)((struct control_msghdr *)msg)->msgdata);
	if(ret == 1 ) {
		strcpy(err_str, "Room name too long!");
		send_control_msg_reply(fd, CREATE_ROOM_FAIL, mt->member_id, err_str);

		return;
	} else if(ret == 2) {
		strcpy(err_str, "Number of rooms reached maximum!");
		send_control_msg_reply(fd, CREATE_ROOM_FAIL, mt->member_id, err_str);

		return;
	} else if(ret == 3) {
		strcpy(err_str, "Room exists!");
		send_control_msg_reply(fd, CREATE_ROOM_FAIL, mt->member_id, err_str);

		return;
	}


	/* send succ message */

	send_control_msg_reply(fd, CREATE_ROOM_SUCC, mt->member_id, NULL);

	if(log_flag){
		fprintf(logfp, "Total number of rooms:%d\n", total_num_of_rooms);
		fflush(logfp);
	}

	return;
}

void process_room_list_request(int fd, struct member_type *mt, char *msg) {
	struct room_type *tmp_rptr;

	char *loc; 
	char list[MAX_MSG_LEN];

	/* all right, someone wants to list rooms */
	bzero(msg_buf, MAX_MSG_LEN);

	if(total_num_of_rooms == 0 ) {
		strcpy(err_str, "No rooms available!");
		send_control_msg_reply(fd, ROOM_LIST_FAIL, mt->member_id, err_str);
		return;
	}

	/* find all the rooms */
	bzero(list, MAX_MSG_LEN);
	loc = (char *)&list;

	for(tmp_rptr=room_list_head; tmp_rptr != NULL; tmp_rptr=tmp_rptr->next_room) {

		sprintf(loc, "[%s (%d)]", tmp_rptr->room_name, tmp_rptr->num_of_members);

		if(tmp_rptr != room_list_tail) {
			char *next_loc;
			next_loc = loc + strlen(loc)+1;
			loc[strlen(loc)] =' ';
			loc = next_loc; 
		} 

	}
	send_control_msg_reply(fd, ROOM_LIST_SUCC, mt->member_id, list);

	return;
}

void process_switch_room_request(int fd, struct member_type *mt, char *msg) {
	struct room_type *tmp_rptr;
	char *to_room;

	/* all right, someone wants to switch */
	bzero(msg_buf, MAX_MSG_LEN);


	to_room = (char *)((struct control_msghdr *)msg)->msgdata;

	/* go through the room list and try to find the room */

	if(room_list_head == NULL) {
		/* no rooms yet, can't switch, send fail message */

		strcpy(err_str, "No room available yet!");
		send_control_msg_reply(fd, SWITCH_ROOM_FAIL, mt->member_id, err_str);
		return;

	} else {
		for(tmp_rptr=room_list_head; 
		    tmp_rptr != NULL; tmp_rptr=tmp_rptr->next_room){
			if(!strcmp(to_room, tmp_rptr->room_name)) {
				/* room found */
				/* make sure the room can still take more member */
				if(tmp_rptr->num_of_members  ==
				   MAX_NUM_OF_MEMBERS_PER_ROOM) {
					/* send reject message */
					strcpy(err_str, "Room is full!");
					send_control_msg_reply(fd, SWITCH_ROOM_FAIL, 
							       mt->member_id, err_str);
					return;

				}

				/* make sure the member is not already in this room */

				if(mt->current_room == tmp_rptr) {
					/* send reject message */
					strcpy(err_str, "Already in this room!");
					send_control_msg_reply(fd, SWITCH_ROOM_FAIL, 
							       mt->member_id, err_str);
					return;
				}

				if(mt->current_room != NULL) {

					/* remove the member from its current room */
					if(mt->prev_room_member == NULL) {
						/* this member is the first member in the room */
						mt->current_room->member_list_head =
							mt->next_room_member;
						if(mt->current_room->member_list_head == NULL)
							mt->current_room->member_list_tail =
								mt->current_room->member_list_head;
						else 
							mt->current_room->member_list_head->prev_room_member =
								NULL;
					} else {

						/* not the first member */
						mt->prev_room_member->next_room_member = 
							mt->next_room_member;
						if( mt->next_room_member != NULL ) {
							mt->next_room_member->prev_room_member = 
								mt->prev_room_member;
						} else {
							mt->current_room->member_list_tail = 
								mt->prev_room_member;
						}
					}
		     
					mt->current_room->num_of_members --;
	    
				}
		
				mt->next_room_member = NULL;
				mt->prev_room_member = NULL;
				mt->current_room = tmp_rptr;

				/* put the member in the new room */
				if(tmp_rptr->member_list_head == NULL) {
					tmp_rptr->member_list_head = mt;
					tmp_rptr->member_list_tail = tmp_rptr->member_list_head;
				} else {
					tmp_rptr->member_list_tail->next_room_member = mt;
					mt->prev_room_member = tmp_rptr->member_list_tail;
					tmp_rptr->member_list_tail= 
						tmp_rptr->member_list_tail->next_room_member;
				}

				tmp_rptr->num_of_members ++;
				tmp_rptr->empty_flag = 0; 

				send_control_msg_reply(fd, SWITCH_ROOM_SUCC, mt->member_id, NULL);
				return;
		
			}
		}
		if(tmp_rptr == NULL) {
			/* send fail mesage */

			strcpy(err_str, "Room not found!");
			send_control_msg_reply(fd, SWITCH_ROOM_FAIL, mt->member_id, err_str);

			return;

		}
	}
	return;
}

void process_member_list_request(int fd, struct member_type *mt, char *msg) {
	struct room_type *rt;
	char *room;
	struct member_type *tmp_mptr;
	char list[MAX_MSG_LEN];

	/* all right, someone wants to list members */
	bzero(msg_buf, MAX_MSG_LEN);

	room = (char *)((struct control_msghdr *)msg)->msgdata;

	/* go through the room list and try to find the room */

	if(room_list_head == NULL) {
		strcpy(err_str, "No room available yet!");
		send_control_msg_reply(fd, MEMBER_LIST_FAIL, mt->member_id, err_str);
		return;
	} else {
		for(rt=room_list_head; rt != NULL; rt=rt->next_room){
			if(!strcmp(room, rt->room_name)) {
				/* room found */			       
				char *loc;

				if(rt->member_list_head == NULL) {
					/* no members in this room */
					strcpy(err_str, "No member in this room!");
					send_control_msg_reply(fd, MEMBER_LIST_FAIL, 
							       mt->member_id, err_str);
					return;

				}

				bzero(list, MAX_MSG_LEN);
				loc = (char *)&list;

				for(tmp_mptr = rt->member_list_head; tmp_mptr != NULL; 
				    tmp_mptr = tmp_mptr->next_room_member) {
					sprintf(loc, "(%s)", tmp_mptr->member_name);
					if(tmp_mptr->next_room_member != NULL) {
						char *next_loc;
						next_loc = loc + strlen(loc) + 1;
						loc[strlen(loc)] = ' ';
						loc = next_loc;
					}
				}

				send_control_msg_reply(fd, MEMBER_LIST_SUCC,
						       mt->member_id, list);

				return;
			}
		}
		if(rt == NULL) {
			/* send fail mesage */
			strcpy(err_str, "Room not found!");
			send_control_msg_reply(fd, MEMBER_LIST_FAIL, mt->member_id, err_str);
			return;
		}
	}
	return;
}

void process_quit_request(int fd, struct member_type *mt, char *msg) {

	bzero(msg_buf, MAX_MSG_LEN); /* probably not necessary... */

	/* all right, someone wants to quit */

	remove_member(mt);

	if(log_flag) { 
		char *tp;
		now = time(NULL);
		tp = ctime(&now);
		tp[strlen(tp)-1] = '\0'; /* Chop off newline */
 
		fprintf(logfp, "%s member [%s] left the session\n", tp, mt->member_name);
		fprintf(logfp, "Total number of members:%d\n", total_num_of_members);
		fflush(logfp);
	}

	free(mt);

	return;
}

