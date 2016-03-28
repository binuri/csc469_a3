/*
 *   CSC469 Winter 2016 A3
 *   Instructor: Bogdan Simion
 *   Date:       19/03/2016
 *  
 *      File:      client_util.c 
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

/* Currently, all functions in this file are only used if the client is 
 * retrieving chatserver parameters from the location server.
 */

#ifdef USE_LOCN_SERVER
static void build_req(char *buf)
{
	/* Write an HTTP GET request for the chatserver.txt file into buf */

	int nextpos;

	sprintf(buf,"GET /~csc469h/winter/chatserver.txt HTTP/1.0\r\n");

	nextpos = strlen(buf);
	sprintf(&buf[nextpos],"\r\n");
}
#endif /* USE_LOCN_SERVER */

#ifdef USE_LOCN_SERVER
static char *skip_http_headers(char *buf)
{
	/* Given a pointer to a buffer which contains an HTTP reply,
	 * skip lines until we find a blank, and then return a pointer
	 * to the start of the next line, which is the reply body.
	 * 
	 * DO NOT call this function if buf does not contain an HTTP
	 * reply message.  The termination condition on the while loop 
	 * is ill-defined for arbitrary character arrays, and may lead 
	 * to bad things(TM). 
	 *
	 * Feel free to improve on this.
	 */

	char *curpos;
	int n;
	char line[256];

	curpos = buf;

	while ( sscanf(curpos,"%256[^\n]%n",line,&n) > 0) {
		if (strlen(line) == 1) { /* Just the \r was consumed */
			/* Found our blank */
			curpos += n+1; /* skip line and \n at end */
			break;
		}
		curpos += n+1;
	}

	return curpos;
}
#endif /* USE_LOCN_SERVER */

#ifdef USE_LOCN_SERVER 
int retrieve_chatserver_info(char *chatserver_name, u_int16_t *tcp_port, u_int16_t *udp_port)
{
	int locn_socket_fd;
	char *buf;
	int buflen;
	int code;
	int  n;

	/* Initialize locnserver_addr. 
	 * We use a text file at a web server for location info
	 * so this is just contacting the CDF web server 
	 */

	/* 
	 * 1. Set up TCP connection to web server "www.cdf.toronto.edu", 
	 *    port 80 
	 */

	/**** YOUR CODE HERE ****/

	/* The code you write should initialize locn_socket_fd so that
	 * it is valid for the write() in the next step.
	 */

	/* 2. write HTTP GET request to socket */

	buf = (char *)malloc(MAX_MSG_LEN);
	bzero(buf, MAX_MSG_LEN);
	build_req(buf);
	buflen = strlen(buf);

	write(locn_socket_fd, buf, buflen);

	/* 3. Read reply from web server */

	read(locn_socket_fd, buf, MAX_MSG_LEN);

	/* 
	 * 4. Check if request succeeded.  If so, skip headers and initialize
	 *    server parameters with body of message.  If not, print the 
	 *    STATUS-CODE and STATUS-TEXT and return -1.
	 */

	/* Ignore version, read STATUS-CODE into variable 'code' , and record
	 * the number of characters scanned from buf into variable 'n'
	 */
	sscanf(buf, "%*s %d%n", &code, &n);


	/**** YOUR CODE HERE ****/


	/* 5. Clean up after ourselves and return. */

	free(buf);
	return 0;

}

#endif /* USE_LOCN_SERVER */
