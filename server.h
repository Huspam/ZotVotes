#ifndef SERVER_H
#define SERVER_H

#include <arpa/inet.h>
#include <getopt.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>


#include "linkedlist.h"

#define BUFFER_SIZE 1024
#define SA struct sockaddr

#define USAGE_MSG "./bin/zoteVote_server [-h] PORT_NUMBER POLL_FILENAME LOG_FILENAME"\
                  "\n  -h                 Displays this help menu and returns EXIT_SUCCESS."\
                  "\n  PORT_NUMBER        Port number to listen on."\
                  "\n  POLL_FILENAME      File to read poll information from at the start of the server"\
                  "\n  LOG_FILENAME       File to output server actions into. Create/overwrite, if exists\n"

typedef struct {
    char* username;	// Pointer to dynamically allocated string
    int socket_fd;		// >= 0 if connection active on server, set to -1 if not active
    pthread_t tid;		// Current thread id, only if active on server
    uint32_t pollVotes;	// Bit vector storage for polls that user has voted on
				// Only updated when the user logout/disconnect from server
} user_t;

typedef struct {
    int clientCnt;  // # of attempted logins (successful and unsuccessful) 
    int threadCnt;  // # of threads created (successful login)
    int totalVotes; // # of votes placed on any poll - updated by all client threads
} stats_t;   // Stats collected since server start-up

typedef struct {
    char* text;	// Pointer to dynamically allocated string
    int voteCnt;  // Count for the choice
} choice_t;

typedef struct {
    char* question;         // Pointer to dynamically allocated string
    choice_t options[4];    // At most 4 options per poll. Stored low to high index.
                            // choice_t text pointer set to NULL if the option is not used. 
} poll_t; 

poll_t pollArray[32]; // Global variable
                      // One poll per index, stored lowest to highest index.  
                      // Set question pointer to NULL if poll does not exist
                      // Maximum of 32 polls on the server.

int pollNum[32];
list_t *users_list;

stats_t curStats;  // Global variable



// INSERT FUNCTIONS HERE
void format_poll(char **str, uint32_t answeredPolls);
void getPoll(char **str, uint32_t answeredPolls, int ind);
int server_init(int server_port, char *pollfile, char *logfile);
void *process_client(void* clientfd_ptr);
void run_server(int server_port, char *pollfile, char *logfile);

#endif
