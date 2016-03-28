#
#  CSC469 Winter 2016
#  Date: 19/03/2016
#
#  Based on:
#  Makefile for Lab Assigment 5 (15-213, Spring 2000)

#  Attention: 
#     Please uncomment and modify this file properly so that you 
#     can use this file to make your chatclient.
#


#   Don't forget to add your cdf user-ID here!

NAME=
VERSION=1


###########################################################################

CC = gcc
CFLAGS = -Wall -g #-DUSE_LOCN_SERVER
SERVER_BIN = chatserver 
SERVER_OBJS = server_util.o server_main.o


CLIENT_BIN = chatclient receiver
CLIENT_OBJS = client_main.o client_util.o 
RECVR_OBJS = client_recv.o client_util.o

all: $(SERVER_BIN) $(CLIENT_BIN)

chatserver: $(SERVER_OBJS) 
	$(CC) $(CFLAGS) $(SERVER_OBJS) -o chatserver

server_util.o: server_util.c server.h defs.h
server_main.o: server_main.c defs.h server.h

chatclient: $(CLIENT_OBJS) 
	$(CC) $(CFLAGS) $(CLIENT_OBJS) -o chatclient 

client_util.o: client_util.c client.h defs.h
client_main.o: client_main.c client.h defs.h 

receiver: $(RECVR_OBJS)
	$(CC) $(CFLAGS) $(RECVR_OBJS) -o receiver

clean:
	rm -f *.o $(SERVER_BIN) $(CLIENT_BIN) core *~


###########################################################################

