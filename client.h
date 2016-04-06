/*
 *   CSC469 Winter 2016
 *   Bogdan Simion
 *   Date: 19/03/2016
 *  
 *      File:      client.h 
 *      Author:    Angela Demke Brown
 *      Version:   1.0.0
 *      Date:      17/11/2010
 *   
 * Please report bugs/comments to bogdan@cs.toronto.edu
 *
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <sys/wait.h>
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "defs.h"

/*** Reqired for logging ***/
#define DEBUG 1

/*** Defines for client control <--> receiver communication ***/

struct body_s {
  u_int16_t status;
  u_int16_t value;
} ;

typedef struct msgbuf {
  long mtype;
  struct body_s body;
} msg_t;


#define CTRL_TYPE 1 /* mtype for messages to control process */
#define RECV_TYPE 2 /* mtype for messages to receiver process */

/* Not many options for status right now. You may add more if you wish. */

/* receiver can tell controller it's ready and supply port number,
 * or not ready and a failure code.  Controller can tell receiver to quit.
 */

#define RECV_READY    1
#define RECV_NOTREADY 2
#define CHAT_QUIT     3

/* Failure codes from receiver. */
#define NO_SERVER     10
#define SOCKET_FAILED 11
#define BIND_FAILED   12
#define NAME_FAILED   13


extern char *optarg; /* For option parsing */

extern int retrieve_chatserver_info(char *chatserver_name, u_int16_t *tcp_port, u_int16_t *udp_port);
