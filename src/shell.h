#ifndef __RUNNER_H__
#define __RUNNER_H__

#include "csapp.h"
#include "readcmd.h"
#define MAXJOBS 100
int contains(int x);
int count(struct cmdline cmd);
void fg();
void get_jobs();
void handle_sigchld(int sig);
void handle_sigint(int sig);
void handle_stp(int sig);
int isempty();
int isfull();
int main();
int peek();
int pop();
void print();
int push(int data);
void quit();
void runPipes(struct cmdline cmd);
#endif /* __RUNNER_H__ */