#include <pthread.h>
#include <signal.h>
#include <semaphore.h>

#include "server.h"
#include "protocol.h"

#include "helpers.h"

#define P(X) sem_wait(X)
#define V(X) sem_post(X)

pthread_mutex_t buffer_lock;
pthread_mutex_t stats_lock;
int usercnt = 0; 
sem_t user_lock, user_write; // reader/writer
sem_t poll_locks[32]; // producer/consumer
pthread_mutex_t log_lock; 

FILE *logs;
int listen_fd;
int numPolls;
volatile sig_atomic_t flag = 0;

void sigint_handler(int sig)
{
    flag = 1;
}

// format plist response
void format_poll(char **str, uint32_t answeredPolls) {
    size_t strsize = 0;
    for (int i = 0; i < numPolls; i++) {
        P(&poll_locks[i]);
        strsize += strlen("Poll ") + snprintf(NULL, 0, "%d", i) + strlen(" - ") + strlen(pollArray[i].question);
        if (!(answeredPolls & (1 << i))) {
            strsize += strlen(" - 0:")  + strlen(pollArray[i].options[0].text);
            for (int j = 1; j < pollNum[i]; j++) {
                strsize += strlen(", ") + snprintf(NULL, 0, "%d", j) + strlen(":") + strlen(pollArray[i].options[j].text);
            }
        }
        strsize += strlen("\n");
        V(&poll_locks[i]);
    }

    *str = malloc(strsize+1);
    (*str)[0] = '\0';

    for (int i = 0; i < numPolls; i++) {
        P(&poll_locks[i]);
        char pollIndex[3];
        snprintf(pollIndex, sizeof(pollIndex), "%d", i);

        strcat(*str, "Poll ");
        strcat(*str, pollIndex);
        strcat(*str, " - ");
        strcat(*str, pollArray[i].question);
        if (!(answeredPolls & (1 << i))) {
            strcat(*str, " - 0:");
            strcat(*str, pollArray[i].options[0].text);
            for (int j = 1; j < pollNum[i]; j++) {
                char qIndex[3];
                snprintf(qIndex, sizeof(qIndex), "%d", j);
                strcat(*str, ", ");
                strcat(*str, qIndex);
                strcat(*str, ":");
                strcat(*str, pollArray[i].options[j].text);
            }
        }
        strcat(*str, "\n");
        V(&poll_locks[i]);
    }

}

void getPoll(char **str, uint32_t answeredPolls, int ind) {
    size_t strsize = 0;
    if (ind != -1) { //specific
        P(&poll_locks[ind]);
        strsize += strlen("Poll ") + snprintf(NULL, 0, "%d", ind) + strlen(" - ");
        strsize += strlen(pollArray[ind].options[0].text) + strlen(":") + snprintf(NULL, 0, "%d", pollArray[ind].options[0].voteCnt);
        for (int j = 1; j < pollNum[ind]; j++) {
            strsize += strlen(",") + strlen(pollArray[ind].options[j].text) + strlen(":") + snprintf(NULL, 0, "%d", pollArray[ind].options[j].voteCnt);
        }
        strsize += strlen("\n");

        *str = malloc(strsize+1);
        (*str)[0] = '\0';

        char pollIndex[3];
        snprintf(pollIndex, sizeof(pollIndex), "%d", ind);

        strcat(*str, "Poll ");
        strcat(*str, pollIndex);
        strcat(*str, " - ");
        strcat(*str, pollArray[ind].options[0].text);
        strcat(*str, ":");
        char qCnt[10];
        snprintf(qCnt, sizeof(qCnt), "%d", pollArray[ind].options[0].voteCnt);
        strcat(*str, qCnt);
        for (int j = 1; j < pollNum[ind]; j++) {
            snprintf(qCnt, sizeof(qCnt), "%d", pollArray[ind].options[j].voteCnt);
            strcat(*str, ",");
            strcat(*str, pollArray[ind].options[j].text);
            strcat(*str, ":");
            strcat(*str, qCnt);
        }
        strcat(*str, "\n");
        V(&poll_locks[ind]);

    } else { //all

        for (int i = 0; i < numPolls; i++) {
            if (answeredPolls & (1 << i)) {
                P(&poll_locks[i]);
                strsize += strlen("Poll ") + snprintf(NULL, 0, "%d", i) + strlen(" - ");
                strsize += strlen(pollArray[i].options[0].text) + strlen(":") + snprintf(NULL, 0, "%d", pollArray[i].options[0].voteCnt);
                for (int j = 1; j < pollNum[i]; j++) {
                    strsize += strlen(",") + strlen(pollArray[i].options[j].text) + strlen(":") + snprintf(NULL, 0, "%d", pollArray[i].options[j].voteCnt);
                }
                strsize += strlen("\n");
                V(&poll_locks[i]);
            }
        }

        *str = malloc(strsize+1);
        (*str)[0] = '\0';

        for (int i = 0; i < numPolls; i++) {
            if (answeredPolls & (1 << i)) {
                P(&poll_locks[i]);
                char pollIndex[3];
                snprintf(pollIndex, sizeof(pollIndex), "%d", i);
                strcat(*str, "Poll ");
                strcat(*str, pollIndex);
                strcat(*str, " - ");
                strcat(*str, pollArray[i].options[0].text);
                strcat(*str, ":");
                char qCnt[10];
                snprintf(qCnt, sizeof(qCnt), "%d", pollArray[i].options[0].voteCnt);
                strcat(*str, qCnt);
                for (int j = 1; j < pollNum[i]; j++) {
                    snprintf(qCnt, sizeof(qCnt), "%d", pollArray[i].options[j].voteCnt);
                    strcat(*str, ",");
                    strcat(*str, pollArray[i].options[j].text);
                    strcat(*str, ":");
                    strcat(*str, qCnt);
                }
                strcat(*str, "\n");
                V(&poll_locks[i]);
            }
        }
    }

}

// intialize server
int server_init(int server_port, char *pollfile, char *logfile) {
    int sockfd;
    struct sockaddr_in servaddr;

    // socket create and verification
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        printf("socket creation failed...\n");
        exit(EXIT_FAILURE);
    }
    bzero(&servaddr, sizeof(servaddr));

    // assign IP, PORT
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(server_port);

    int opt = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, (char *)&opt, sizeof(opt))<0) {
	    perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    // Binding newly created socket to given IP and verification
    if ((bind(sockfd, (SA*)&servaddr, sizeof(servaddr))) != 0) {
        printf("socket bind failed\n");
        exit(EXIT_FAILURE);
    }

    curStats = (stats_t) {0, 0, 0}; // Intialize server stats to 0
    numPolls = fillPollArray(pollfile); // Fill in pollArray and announce number of polls
    logs = fopen(logfile, "w+"); // Open log file
    if (logs == NULL) {fprintf(stderr, "Could not open file.\n"); exit(2);}

    users_list = CreateList(&compareUsers, &printUsers, &deleteUsers); // Create users list

    sem_init(&user_lock, 0, 1); // Initialize semaphores to 1
    sem_init(&user_write, 0, 1); 
    for (int i = 0; i < 32; i++)  // Initialize all locks for pollArray
        sem_init(&poll_locks[i], 0, 1);
    

    struct sigaction myaction = {{0}}; // Installing sigint handler
    myaction.sa_handler = sigint_handler;

    if (sigaction(SIGINT, &myaction, NULL) == -1) {
        printf("signal handler failed to install\n");
    }

    // Now server is ready to listen and verification
    if ((listen(sockfd, 1)) != 0) {
        printf("Listen failed\n");
        exit(EXIT_FAILURE);
    }
        
    printf("Currently listening on port %d.\n", server_port);

    return sockfd;
}

// function running in thread
void *process_client(void* usr_ptr) {
    user_t *client = usr_ptr;
    int received_size;
    uint32_t userVotes = client->pollVotes;
    fd_set read_fds;
    client->tid = pthread_self();

    P(&user_lock);
    usercnt--;
    if (usercnt == 0) { // Last out
        V(&user_write); 
    }
    V(&user_lock);

    int retval;
    while(1){
        // Selects
        FD_ZERO(&read_fds);
        FD_SET(client->socket_fd, &read_fds);
        retval = select(client->socket_fd + 1, &read_fds, NULL, NULL, NULL);
        if (retval!=1 && !FD_ISSET(client->socket_fd, &read_fds)){
            printf("Error with select() function\n");
            break;
        }

        
        petrV_header *user = malloc(sizeof(petrV_header));
        if (rd_msgheader(client->socket_fd, user) < 0){
            close(client->socket_fd);
            free(user);
            pthread_detach(pthread_self());
            break;  
        }

        pthread_mutex_lock(&buffer_lock);
        P(&user_write);
        if (user->msg_type == LOGOUT) { // Logging out
            petrV_header *msgOK = calloc(1, sizeof(petrV_header));
            msgOK->msg_len = 0;
            msgOK->msg_type = OK;
            wr_msg(client->socket_fd, msgOK, 0);
            free(msgOK);
            pthread_mutex_unlock(&buffer_lock);
            client->pollVotes = userVotes;
            close(client->socket_fd);
            client->socket_fd = -1;
            V(&user_write);
            pthread_mutex_lock(&log_lock);
            fprintf(logs, "%s LOGOUT\n", client->username);
            fflush(logs);
            pthread_mutex_unlock(&log_lock);
            free(user);
            pthread_detach(pthread_self());
            break;     
        }

        else if (user->msg_type == PLIST) { // Return poll list
            char *poll_response = NULL;
            format_poll(&poll_response, userVotes);
            petrV_header *msgPLIST = calloc(1, sizeof(petrV_header));
            msgPLIST->msg_len = strlen(poll_response);
            msgPLIST->msg_type = PLIST;
            wr_msg(client->socket_fd, msgPLIST, poll_response);
            free(msgPLIST);
            pthread_mutex_unlock(&buffer_lock);
            V(&user_write);
            
            pthread_mutex_lock(&log_lock);
            fprintf(logs, "%s PLIST\n", client->username);
            fflush(logs);
            pthread_mutex_unlock(&log_lock);
            free(poll_response);
        }

        else if (user->msg_type == VOTE) { // Vote on poll; update everything
            char *voting = malloc(user->msg_len+1);
            read(client->socket_fd, voting, user->msg_len);
            int pIndex = atoi(strtok_r(voting, " ", &voting));
            int optid = atoi(voting);
        
            
            if (pIndex > 31 || pIndex < 0) { // poll index OOB
                petrV_header *msgERR = calloc(1, sizeof(petrV_header));
                msgERR->msg_len = 0;
                msgERR->msg_type = EPNOTFOUND;
                wr_msg(client->socket_fd, msgERR, 0);
                free(msgERR);
                pthread_mutex_unlock(&buffer_lock);
                V(&user_write);
                continue;
            }
            P(&poll_locks[pIndex]);
            if (pollArray[pIndex].question == NULL) { //poll N/A
                petrV_header *msgERR = calloc(1, sizeof(petrV_header));
                msgERR->msg_len = 0;
                msgERR->msg_type = EPNOTFOUND;
                wr_msg(client->socket_fd, msgERR, 0);
                free(msgERR);
            }
            else if (optid > 3 || optid < 0 || pollArray[pIndex].options[optid].text == NULL) { // option N/A
                petrV_header *msgERR = calloc(1, sizeof(petrV_header));
                msgERR->msg_len = 0;
                msgERR->msg_type = ECNOTFOUND;
                wr_msg(client->socket_fd, msgERR, 0);
                free(msgERR);
            }
            else if (userVotes & (1 << pIndex)) { // already voted
                petrV_header *msgERR = calloc(1, sizeof(petrV_header));
                msgERR->msg_len = 0;
                msgERR->msg_type = EPDENIED;
                wr_msg(client->socket_fd, msgERR, 0);
                free(msgERR);
            }
            else {
                petrV_header *msgOK = calloc(1, sizeof(petrV_header));
                msgOK->msg_len = 0;
                msgOK->msg_type = OK;
                wr_msg(client->socket_fd, msgOK, 0);
                free(msgOK);

                //update votecnt
                pthread_mutex_lock(&stats_lock);
                curStats.totalVotes++;
                pthread_mutex_unlock(&stats_lock);

                userVotes = (1 << pIndex) | userVotes; // set the pIndex'th bit of user pollVotes
                pollArray[pIndex].options[optid].voteCnt++; // update pollArray

                pthread_mutex_lock(&log_lock);
                fprintf(logs, "%s VOTE %d %d %d\n", client->username, pIndex, optid, userVotes);
                fflush(logs);
                pthread_mutex_unlock(&log_lock);
            }
            V(&user_write);
            pthread_mutex_unlock(&buffer_lock);
            V(&poll_locks[pIndex]);

        }

        else if (user->msg_type == STATS) { // send back poll stats
            char *statsreq = malloc(user->msg_len+1);
            read(client->socket_fd, statsreq, user->msg_len);
            int pIndex = atoi(statsreq);
            
            // error checking
            if ((pIndex == -1 && userVotes == 0) || (pIndex != -1 && !(userVotes & (1 << pIndex)))) {
                petrV_header *msgERR = calloc(1, sizeof(petrV_header));
                msgERR->msg_len = 0;
                msgERR->msg_type = EPDENIED;
                wr_msg(client->socket_fd, msgERR, 0);
                free(msgERR);
                pthread_mutex_unlock(&buffer_lock);
                V(&user_write);
                continue;
            } 

            char *stats_r = NULL;
            getPoll(&stats_r, userVotes, pIndex);
            petrV_header *msgSTATS = calloc(1, sizeof(petrV_header));
            msgSTATS->msg_len = strlen(stats_r);
            msgSTATS->msg_type = STATS;
            wr_msg(client->socket_fd, msgSTATS, stats_r);
            free(msgSTATS);
            pthread_mutex_unlock(&buffer_lock);
            V(&user_write);
            
            pthread_mutex_lock(&log_lock);
            fprintf(logs, "%s STATS %d\n", client->username, userVotes);
            fflush(logs);
            pthread_mutex_unlock(&log_lock);
        } 
    }
    // Close the socket at the end
    // printf("Close current client connection\n");

    return usr_ptr;
}

// run the server
void run_server(int server_port, char *pollfile, char *logfile) {
    listen_fd = server_init(server_port, pollfile, logfile); // Initiate server and start listening on specified port
    
    int client_fd;
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    pthread_t tid;

    while(1){
        // Wait and Accept the connection from client
        // printf("Wait for new client connection\n");
        int* client_fd = malloc(sizeof(int));
        *client_fd = accept(listen_fd, (SA*)&client_addr, &client_addr_len);
        if (*client_fd < 0) {
            if (flag) { // SIGINT
                close(listen_fd);
                fclose(logs);
                kill_threads();
                printPolls(numPolls);
                PrintLinkedList(users_list);
                printStats();
                DeleteList(&users_list);
                exit(1);
            }
        } else {
            // printf("Client connection accepted\n");
            
            // Get username
            petrV_header *user = malloc(sizeof(petrV_header));
            pthread_mutex_lock(&buffer_lock);
            if (rd_msgheader(*client_fd, user) < 0) {free(user); exit(2);}
            char *username = malloc(user->msg_len+1);
            if (user->msg_type == LOGIN){
                read(*client_fd, username, user->msg_len);
            } else {
                free(username);
                close(*client_fd);
                continue;
            }
            

            // Verify username
            if (invalidName(username)) {
                petrV_header *msgBAD = calloc(1, sizeof(petrV_header));
                msgBAD->msg_len = 0;
                msgBAD->msg_type = ESERV;
                wr_msg(*client_fd, msgBAD, 0);
                free(msgBAD);
                free(username);
                close(*client_fd);
                continue;
            }
            user_t *newLogin = malloc(sizeof(user_t));
            user_t *found;
            newLogin->username = username;
            pthread_mutex_unlock(&buffer_lock);

        
            if ((found = searchList(newLogin)) != NULL && found->socket_fd != -1){
                // Exists and in use
                petrV_header *msgBAD = calloc(1, sizeof(petrV_header));
                msgBAD->msg_len = 0;
                msgBAD->msg_type = EUSRLGDIN;
                wr_msg(*client_fd, msgBAD, 0);
                free(msgBAD);
                pthread_mutex_lock(&log_lock);
                fprintf(logs, "REJECT %s\n", username);
                fflush(logs);
                pthread_mutex_unlock(&log_lock);
                free(username);
                free(newLogin);
                close(*client_fd);
            } else {
                P(&user_lock);
                usercnt++;
                if (usercnt == 1) { // First in
                    P(&user_write); 
                }
                V(&user_lock);
                // Login user (successful)
                if (found == NULL) { // new user
                    newLogin->pollVotes = 0;
                    InsertAtHead(users_list, newLogin);
                    pthread_mutex_lock(&log_lock);
                    fprintf(logs, "CONNECTED %s\n", username);
                    fflush(logs);
                    pthread_mutex_unlock(&log_lock);                
                } else { // relogin
                    free(newLogin);
                    newLogin = found;
                    pthread_mutex_lock(&log_lock);
                    fprintf(logs, "RECONNECTED %s\n", username);
                    fflush(logs);
                    pthread_mutex_unlock(&log_lock);
                }
                newLogin->socket_fd = *client_fd;
                
                // send the OK
                petrV_header *msgOK = calloc(1, sizeof(petrV_header));
                msgOK->msg_len = 0;
                msgOK->msg_type = OK;
                wr_msg(*client_fd, msgOK, 0);
                free(msgOK);
                
                

                pthread_create(&tid, NULL, process_client, (void *)newLogin); // create thread
                

                pthread_mutex_lock(&stats_lock);
                curStats.threadCnt++;
                pthread_mutex_unlock(&stats_lock);
                if (flag) { // SIGINT
                    close(listen_fd);
                    fclose(logs);
                    kill_threads();
                    printPolls(numPolls);
                    PrintLinkedList(users_list);
                    printStats();
                    DeleteList(&users_list);
                    exit(1);
                }
            }
            

            pthread_mutex_lock(&stats_lock);
            curStats.clientCnt++;
            pthread_mutex_unlock(&stats_lock);
        }
    }
    //close(listen_fd);
    // fclose(logs);
    return;
}


int main(int argc, char *argv[]) {
    int opt;
    while ((opt = getopt(argc, argv, "h")) != -1) {
        switch (opt) {
            case 'h':
                fprintf(stderr, USAGE_MSG);
                exit(EXIT_FAILURE);
        }
    }

    // 3 positional arguments necessary
    if (argc != 4) {
        fprintf(stderr, USAGE_MSG);
        exit(EXIT_FAILURE);
    }
    unsigned int port_number = atoi(argv[1]);
    char *poll_filename = argv[2];
    char *log_filename = argv[3];

    if (port_number == 0) {exit(EXIT_FAILURE);}

    run_server(port_number, poll_filename, log_filename);
    
    return 0;
}
