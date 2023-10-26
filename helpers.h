#ifndef HELPERS_H
#define HELPERS_H

#include "server.h"
#include <pthread.h>
#include <signal.h>

int fillPollArray(char *pollfile);
int invalidName(char *name);
user_t *searchList(user_t *token);
void kill_threads();
void printPolls(int cnt);
void printStats();
void deleteUsers(void *U);
int compareUsers(void *U1, void *U2);
void printUsers(void *U);

#endif