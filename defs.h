/*
 * CSC469 Winter 2016
 * Bogdan Simion
 * Date: 19/03/2016
 *
 * Adapted by demke, bogdan for CSC469 A3 from:
 *
 *   15-213 Spring 2000 L5 (designed based on L5 of Fall 1999) 
 *  
 *      File:      defs.h 
 *      Author:    Jun Gao
 *      Version:   1.0.0
 *      Date:      4/12/2000
 *   
 * Please report bugs/comments to bogdan@cs.toronto.edu
 *
 * This file must be included in your client implementation!
 * This file must not be modified by students!
 */

#ifndef _DEFS_H
#define _DEFS_H

#define REGISTER_REQUEST 	1
#define REGISTER_SUCC	 	2
#define REGISTER_FAIL	 	3

#define ROOM_LIST_REQUEST	4 
#define ROOM_LIST_SUCC 	        5 
#define ROOM_LIST_FAIL	        6

#define MEMBER_LIST_REQUEST	7
#define MEMBER_LIST_SUCC	8
#define MEMBER_LIST_FAIL        9

#define SWITCH_ROOM_REQUEST	10
#define SWITCH_ROOM_SUCC	11
#define SWITCH_ROOM_FAIL 	12	

#define CREATE_ROOM_REQUEST	13
#define CREATE_ROOM_SUCC	14
#define CREATE_ROOM_FAIL	15

#define MEMBER_KEEP_ALIVE	16

#define QUIT_REQUEST		17	

/* maximum length of a member name */
#define MAX_MEMBER_NAME_LEN     24	

/* maximum length of a host name */
#define MAX_HOST_NAME_LEN	80

/* maximum length of a file name */
#define MAX_FILE_NAME_LEN	80

/* maximum length of a room name */
#define MAX_ROOM_NAME_LEN       24 

/* maximum number of rooms in one session */
#define MAX_NUM_OF_ROOMS	40 

/* maximum number of members in one room */
#define MAX_NUM_OF_MEMBERS_PER_ROOM  30

#define MAX_NUM_OF_MEMBERS    (MAX_NUM_OF_ROOMS) * (MAX_NUM_OF_MEMBERS_PER_ROOM)

/* maximum length of a message */
#define MAX_MSG_LEN		2048	


/* data structures - message header size must be explicitly set to ensure
 * size is the same on 32-bit and 64-bit systems. The "packed" attribute
 * tells gcc to use the minimum size for these structures, so no extra 
 * padding is produced by the compiler for structure alignment. 
 *
 * The last item of each of these structs is a zero-length array, which
 * occupies no space but serves as a pointer to the first byte _after_
 * the header. (See https://gcc.gnu.org/onlinedocs/gcc/Zero-Length.html
 * if you have not encountered these zero-length arrays before.)
 */
 
/* common control message header - 8 bytes */

struct control_msghdr {
    u_int16_t msg_type;
    u_int16_t member_id;
    u_int16_t msg_len;
    u_int16_t reserved;
    caddr_t   msgdata[0];
} __attribute__ ((packed));

/* chat message header  - 26 bytes */

struct chat_msghdr {
    union {
    	char member_name[MAX_MEMBER_NAME_LEN];
	u_int16_t member_id;
    }sender;
    u_int16_t msg_len;
    caddr_t msgdata[0];
} __attribute__ ((packed));

/* REGISTER_REQUEST message data definition - 2 bytes */

struct register_msgdata {
	u_int16_t udp_port;
	caddr_t member_name[0];
} __attribute__ ((packed));

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#endif
