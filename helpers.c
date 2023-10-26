#include "helpers.h"

// parse polls from file
void parsePolls(char *str, int index) {
    char *q = strtok_r(str, ";", &str);
    pollArray[index].question = malloc(strlen(q)+1);
    strcpy(pollArray[index].question, q);
    int numOpt = atoi(strtok_r(str, ";", &str));
    pollNum[index] = numOpt;

    for (int i = 0; i < numOpt; i++) {
        char *opt = strtok_r(str, ";", &str);
        char *txt = strtok_r(opt, ",", &opt);
        int cnt = atoi(opt);
        pollArray[index].options[i].text = malloc(strlen(txt)+1);
        strcpy(pollArray[index].options[i].text, txt);
        pollArray[index].options[i].voteCnt = cnt;
    }
}

// fill in poll array
int fillPollArray(char *pollfile) {
    FILE *polls = fopen(pollfile, "r");
    if (polls == NULL) {fprintf(stderr, "Could not open file.\n"); exit(2);}
    char *buffer = NULL;
    size_t len = 0;
    int index = 0;
    // set all to NULL at first
    for (int i = 0; i < 32; i++) {
        pollArray[i].question = NULL;
        for (int j = 0; j < 4; j++) {
            pollArray[i].options[j].text = NULL;
        }
    }

    
    // fill in polls
    while (getline(&buffer, &len, polls) != EOF){
        parsePolls(buffer, index);
        index++;
    }

	printf("Server initialized with %d polls.\n", index);
    fclose(polls);

    return index;
}

int invalidName(char *name) {
    while (*name) {
        if (*name == ' ') {return 1;}
        name++;
    } 
    return 0;
}

user_t *searchList(user_t *token) {
    node_t *cur = users_list->head;

    while (cur != NULL) {
        user_t *u = cur->data;
        if (!(*(users_list->comparator))(u, token)){
            return u;
        }
        cur = cur->next;
    }
    return NULL;
}

void kill_threads() {
    node_t *cur = users_list->head;
    while (cur != NULL) {
        user_t *user = cur->data;
        if (user->socket_fd != -1) {close(user->socket_fd);}
        pthread_kill(user->tid, 2);
        cur = cur->next;
    }
}

void printPolls(int cnt) {
    for (int i = 0; i < cnt; i++){
        printf("%s;%d", pollArray[i].question, pollNum[i]);
        for (int j = 0; j < pollNum[i]; j++) {
            printf(";%s,%d", pollArray[i].options[j].text, pollArray[i].options[j].voteCnt);
        }
        printf("\n");
    }
}

void printStats() {
    fprintf(stderr, "%d, %d, %d\n", curStats.clientCnt, curStats.threadCnt, curStats.totalVotes);
}

void deleteUsers(void *U) {
	user_t *user = U;
	if (user->username != NULL) {free(user->username);}
	free(user);
}
void printUsers(void *U) {
	user_t *user = U;
    fprintf(stderr, "%s, %d\n", user->username, user->pollVotes);
    //fprintf(stderr, "%s, %d, %ld\n", user->username, user->pollVotes, user->tid);
}
int compareUsers(void *U1, void *U2) {
	const user_t *u1 = U1;
    const user_t *u2 = U2;
    
    return strcmp(u1->username, u2->username);
}
