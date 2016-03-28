/*
 * CSC469 Winter 2016
 * Bogdan Simion
 * Date: 19/03/2016
 * 
 * Based on:
 *
 *   15-213 Spring 2000 L5 (designed based on L5 of Fall 1999) 
 *
 *      File: 	   server_main.c
 *      Author:    Jun Gao
 *      Version:   1.0.1
 *      Date:      4/12/2000
 *      History:   19/11/2006 minor bug fix to initialize client_addr_len
 *                            prior to calling accept() (Linux-specific)
 *                 4/17/2000 minor bug fix in the timer used by select();
 *           
 *   
 *   Please report bugs/comments to bogdan@cs.toronto.edu
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <netinet/in.h>
#include <netdb.h>

#include "server.h"

char optstr[]="t:u:f:s:r:";

void 
usage(char **argv) {
	printf("usage:\n");
	printf("%s -t <tcp port> -u <udp port> [-f <log file name> -s <sweep interval(mins) -r <room file name>]\n", argv[0]);
	exit(1);
}

int 
main(int argc, char **argv) {
	int i;
	char c;

	int maxfd;
	fd_set rset;
	fd_set allset;
	int num_ready_fds; 

	struct timeval *time_out;
	struct timeval tv;

	bzero(&log_file_name, MAX_FILE_NAME_LEN);
	log_flag = 0;

	bzero(&room_file_name, MAX_FILE_NAME_LEN);

	time_out = (struct timeval *)NULL;
	sweep_int = 0;

	/* process arguments */
	while((c = getopt(argc, argv, optstr)) != -1){
		switch(c) {
		case 't':
			server_tcp_port = atoi(optarg);
			break;
		case 'u':
			server_udp_port = atoi(optarg);
			break;
		case 'f':
			strncpy(log_file_name, optarg, MAX_FILE_NAME_LEN);
			break;
		case 's':
			sweep_int = atoi(optarg);

			sweep_int *= 60;       /* convert to seconds */
			tv.tv_sec = sweep_int;  
			tv.tv_usec = 0;
			if(sweep_int != 0) 
				time_out = &tv;
			break;
		case 'r':
			strncpy(room_file_name, optarg, MAX_FILE_NAME_LEN);
			break;
		default:
			printf("invalid option\n");
			break;
		}
	}

	if(server_tcp_port == 0 || server_udp_port == 0) {
		usage(argv);
	}

	if(log_file_name[0] != 0 ) {
		log_flag = 1; 
		if( (logfp = fopen(log_file_name, "a+")) == NULL) {
			perror("fopen");;
			exit(1);
		}
	}


	/* initialize tcp and udp server; create rooms if config file present */
	init_server();

	/* usual preparation stuff for select() */
	FD_ZERO(&allset);
	FD_SET(tcp_socket_fd, &allset);
	FD_SET(udp_socket_fd, &allset);

	maxfd = ((udp_socket_fd > tcp_socket_fd) ? udp_socket_fd : tcp_socket_fd);

	/*
	 * server sits in an infinite loop waiting for events
	 *
	 * three things can happen: 
	 *  1. chat client connects to server through tcp and sends 
	 *     control messages;
	 *  2. chat client sends chat messages through udp
	 *  3. server times out periodically to remove dormant/crashed 
	 *     clients and rooms that do not have a member
	 * if time_out == NULL, server will not time out, so 3. won't happen
	 */

	for( ; ; ) {

		rset = allset;

		if((num_ready_fds = select(maxfd+1, &rset, NULL, NULL, time_out)) < 0) {
			perror("select");
			exit(1);
		}

		if(num_ready_fds <=0 ) {
			/* due to time out */

			struct member_type *mt;
			struct room_type *rt;

			/* go through the member list and sweep members that are there for
			   more than 1 sweep interval without messages */

			mt = mem_list_head;
			while(mt != NULL) {
				struct member_type *tmp_mt;

				tmp_mt=mt->next_member;

				if(mt->quiet_flag == 0) {
					/* active in the last time interval */
					mt->quiet_flag ++ ; 
				} else {
					/* remove this member */
					remove_member(mt);
					if(log_flag) {
						char *tp;
						now = time(NULL);
						tp = ctime(&now);
						tp[strlen(tp)-1] = '\0';

						fprintf(logfp, 
							"%s member [%s] is removed from the session\n", 
							tp, mt->member_name);
						fprintf(logfp, "Total number of members:%d\n", 
							total_num_of_members);
						fflush(logfp);
					}
					free(mt);

				}
				mt = tmp_mt; 
			}

			/* go through room list */
			rt = room_list_head;
			while(rt != NULL) {
				struct room_type *tmp_rt;
				tmp_rt = rt->next_room;

				if(rt->num_of_members == 0) {
					if(rt->empty_flag == 0 ) {
						rt->empty_flag ++;
					} else {
						/* remove this room */
						remove_room(rt);
						total_num_of_rooms --;

						/* need to log this info */
						if(log_flag) {
							char *tp;
							now = time(NULL);
							tp = ctime(&now);
							tp[strlen(tp)-1] = '\0';

							fprintf(logfp, 
								"%s room [%s] is removed from the session\n", 
								tp, rt->room_name);
							fprintf(logfp, "Total number of rooms:%d\n", 
								total_num_of_rooms);
							fflush(logfp);
						}
	
						free(rt);

					}
				}

				rt = tmp_rt;
			}

			/* reset the timer here */
			if(sweep_int != 0) {
				tv.tv_sec = sweep_int;  
				tv.tv_usec = 0;
				time_out = &tv;
			}
			continue;
		}

		if(FD_ISSET(udp_socket_fd, &rset)) {

			/*
			 * message arrives at the udp server port 
			 * --> chat message 
			 */

			process_chat_msg(udp_socket_fd);

			/* no more descriptors are ready, we go back to wait */
			if( --num_ready_fds <= 0)
				continue;
		}

		if(FD_ISSET(tcp_socket_fd, &rset)) {

			/* 
			 * a request to set up tcp connection for control messages 
			 */

			struct sockaddr_in client_addr;
			socklen_t client_addr_len = sizeof(struct sockaddr_in);
			int connect_fd; 

			if( (connect_fd = accept(tcp_socket_fd, 
						 (struct sockaddr *)&client_addr, &client_addr_len)) < 0 ) {

				perror("accept");

			} else {
				/* we accepted a new connection */

				if(log_flag) {
					get_peer_info(connect_fd, info_str);
					fprintf(logfp, "%s connects successfully\n", info_str);
					fflush(logfp);
				}

				/* find a place in fd_table[] to store the accepted fd */

				for(i=0; i< MAX_CONTROL_SESSIONS; i++) {
					if(fd_table[i] == -1){
						fd_table[i] = connect_fd;
						break;
					}
				}

				if(i == MAX_CONTROL_SESSIONS) {
					if(log_flag) {
						fprintf(logfp, "too many connections\n");
						fflush(logfp);
					}
					close(connect_fd);
				}

				if(i > max_fd_idx)
					max_fd_idx = i;

				if(connect_fd > maxfd)
					maxfd = connect_fd;

				FD_SET(connect_fd, &allset);

			}

			if( --num_ready_fds <= 0)
				continue;
		}

		/* 
		 * check which descriptor has data to read, and process 
		 * the control message 
		 */

		/* tmp_fd_idx is used to denote the second highest fd number:
		 * it is useful in the case that the max_fd_idx is removed.
		 */ 
		int tmp_fd_idx = -1;   
		
		for(i=0; i<= max_fd_idx; i++) {
			if(fd_table[i] == -1) 
				continue;
			
			if(FD_ISSET(fd_table[i], &rset)) {
				process_control_msg(fd_table[i]);
				/*
				 * close connection after processing since
				 * the semantics are that a connection is only
				 * good for one control message
				 */
				close(fd_table[i]);
				FD_CLR(fd_table[i], &allset);
				fd_table[i] = -1;

				if( --num_ready_fds <=0 )
					break;
			} else {
				tmp_fd_idx = i;
			}	
		}

		if(i == max_fd_idx) 
			max_fd_idx = tmp_fd_idx;

	}
	
	return 0;
}
