/*
 *Copyright (C) 2002, Simon Nieuviarts
 */

/*------------ Copyright (C) 2020, Alaa Ben Fatma &Wassim Ayari ----------- */
#include <stdio.h>
#include <stdlib.h>
#include "shell.h"
#define STDIN 0
#define STDOUT 1
// GLOBAL definitions
static struct cmdline * l;
pid_t child_pid, aux_pid;
pid_t wpid;
int status = 0;
volatile sig_atomic_t suspendedJobs = 0;

/*-------------------------- BEGIN PILE DEFINITION ------------------------- */
/*--------------------- PILE = stack of suspended jobs --------------------- */

int MAXSIZE = 500;
int stack[500];
int top = -1;

int isempty()
{

	if (top == -1)
	{
		suspendedJobs = 0;
		return 1;
	}
	else
		return 0;
}
void print()
{
	for (int i = 0; i < top; ++i)
	{
		printf("%d\t", stack[i]);
	}
	printf("\n");
}
int isfull()
{

	if (top == MAXSIZE)
		return 1;
	else
		return 0;
}
int contains(int x)
{
	if (isempty())
		return -1;
	for (size_t i = 0; i < top; i++)
	{
		if (stack[i] == x)
			return i;
	}
	return -1;

}

int pop()
{
	int data;

	if (isempty() == 0)
	{
		data = stack[top];
		top = top - 1;
		return data;
	}
	else
	{
		printf("Could not retrieve process, Stack is empty.\n");
		return -127;
	}
}

int push(int data)
{

	if (isfull() == 0)
	{
		top = top + 1;
		stack[top] = data;
		return 0;
	}
	else
	{
		printf("Could not insert job, Stack is full.\n");
		return -128;
	}
}

/*-------------------------- END PILE DEFINITION ------------------------- */

/*------------------ Count the number of commmands we have ----------------- */
int count(struct cmdline cmd)
{
	int i = 0;
	while (cmd.seq[i] != NULL)
		i++;
	return i;
}

/*----------- Run a sequence of commands. cmd = sequence of cmds. ---------- */

/*-------------------------- Example : ls -l | wc -------------------------- */
void runPipes(struct cmdline cmd)
{

	/*------------------------------- Definitions ------------------------------ */
	char ***commands = cmd.seq;
	int pipes[2];
	int fd_input = 0;	// a special fd that is being used to chain the execution of the commands

	/*------------------------ Traverse all the commands ----------------------- */
	while (*commands != NULL)
	{

		/*------------------------------ Define pipes ------------------------------ */
		pipe(pipes);

		switch (fork())
		{
			case -1:
				/*---------------------- FORK() has failed ---------------------- */
				exit(1);
				break;
			case 0:
				/*------------------------------ FORKED CHILD PROCESS ----------------------------- */
				dup2(fd_input, STDIN);	//input pipe < STDIN
				if (*(commands + 1) != NULL)
					dup2(pipes[1], STDOUT);	//Redirect the output of the current command (to be executed) if it is not the last one.
				close(pipes[0]);
				if (execvp((*commands)[0], *commands) == -1)
				{
					printf("%s : command not found.\n", (*commands)[0]);
				}
				exit(1);
				break;
			default:
				/*--------------------------------- Parent --------------------------------- */
				if (cmd.in_background == 0)
				{
					wait(NULL);
				}

				close(pipes[1]);
				fd_input = pipes[0];	//UPDATE : input pipe uses the previous one in order to keep a relevant sequence of executions
				commands++;
				break;
		}
	}
}

/*----------------------------- Handle SIGCHLD ---------------------------- */
void handle_sigchld(int sig)
{
	while ((wpid = waitpid(-1, &status, WNOHANG | WUNTRACED)) > 0);
}

/*------------------------------ Handle SIGINT ----------------------------- */
void handle_sigint(int sig)
{
	//nothing
}

/*----------------------------- Handle SIGTSTP ----------------------------- */
void handle_stp(int sig)
{
	waitpid(-1, &status, WNOHANG);
	if (((child_pid != 0) && (kill(child_pid, 0) == 0)))
	{
		Kill(child_pid, SIGTSTP);
		push(child_pid);
		suspendedJobs = 1;
	}
}

/*------------------- Gets the list of all suspended jobs ------------------ */
void get_jobs()
{

	int access = 0;
	while (access <= top)
	{
		printf("[%i] Arrêté \t %i\n", access + 1, stack[access]);
		access++;
	}
}

/*------------------- Stop the most recent suspended job ------------------ */
void to_stop()
{
	if (isempty() != 1)
	{
		Kill(pop(), SIGTERM);
	}
	if (top == -1)
	{
		suspendedJobs = 0;
	}
}

/*------------ Resume the execution of the recent suspended job ------------ */
void fg()
{
	kill(pop(), SIGCONT);
}

/*--------------- Quit the program if we have no hanging jobs -------------- */
void quit()
{
	if (isempty() != 1)
	{
		puts("\n Il y a des processus arretés.");
		usleep(10000);
	}
	else
	{
		exit(0);
	}
}

/*---------------------------------- Main program ---------------------------------- */
int main()
{

	suspendedJobs = 0;

	/*----------------------------- Signal handlers ---------------------------- */
	Signal(SIGCHLD, handle_sigchld);
	Signal(SIGINT, handle_sigint);
	signal(SIGTSTP, handle_stp);

	while (1)
	{
		int i;
		printf("shell > ");
		l = readcmd();
		/*If input stream closed, normal termination */
		if (!l)
		{
			printf("exit\n");
			exit(0);
		}

		if (l->err)
		{ /*Syntax error, read another command */
			printf("error: %s\n", l->err);
			continue;
		}

		/*Visit each command of the pipe */
		for (i = 0; l->seq[i] != 0; i++)
		{
			if (strcmp(*(l->seq[i]), "quit") == 0)
			{
				quit();
				continue;
			}
			if (strcmp(*(l->seq[i]), "fg") == 0)
			{
				fg();
				continue;
			}
			if (strcmp(*(l->seq[i]), "jobs") == 0)
			{
				get_jobs();
				continue;
			}
			if (strcmp(*(l->seq[i]), "stop") == 0)
			{
				to_stop();
				continue;
			}
			char **cmd = l->seq[i];
			mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH | O_WRONLY | O_RDONLY;
			if (i == 0)
			{
				aux_pid = child_pid;
				child_pid = fork();
			}
			if (child_pid == 0)
			{
				int file_desc;
				if (i == 0 && l->in)
				{
					if ((access(l->in, R_OK) == -1))
					{
						printf("Erreur while loading %s\n", l->in);
						exit(1);
					}
					file_desc = open((l->in), O_RDONLY);
					dup2(file_desc, STDIN_FILENO);
				}

				if (l->out)
				{

					/*------------------------------ File exists? ------------------------------ */
					if ((access(l->out, F_OK)) != -1)
					{

						/*--------------------------- Can we write in it? -------------------------- */

						if ((access(l->out, W_OK) == -1))
						{
							printf("%s : Permission denied.\n", l->out);
							exit(1);
						}
						else
						{

							/*-------------------------- File exists. We recreate it with relevant permissions in order to overwrite its contetns. -------------------------- */

							struct stat fileStat;
							stat(l->out, &fileStat);
							remove((l->out));
							creat(l->out, fileStat.st_mode);
							file_desc = open((l->out), O_WRONLY);
						}
					}
					else
					{

						/*--------------------------- File does not exist. -------------------------- */

						creat(l->out, mode);
						file_desc = open((l->out), O_WRONLY);
					}
					dup2(file_desc, STDOUT_FILENO);
				}
				if (l->in_background == 1)
				{

					/*--------------------  Run ps command to see the running children processes. -------------------- */
					printf("Process[%d] is running in the background\n", getpid());
				}

				/*------------------------ Count number of commands ------------------------ */

				if (count(*l) > 1)
				{

					/*------------------ More than 1 command<=> piped command ----------------- */

					runPipes(*l);
				}
				else
				{

					/*--------------------- Only 1 command to be executed. --------------------- */

					if (execvp(cmd[i], cmd) == -1)
					{
						printf("%s : command not found.\n", cmd[i]);
					}
				}

				/*------------- We close our file descriptor and exit the child ------------ */
				close(file_desc);
				exit(1);
			}
			else
			{

				/*--------------------------------- PARENT --------------------------------- */
				if (l->in_background == 0 && count(*l) < 2)
				{

					/*------------ Not a piped command NOR running in the background ----------- */
					pause();
				}
				else {}
				/*--- We stop the program for 0.1 seconds in order to re-maintain the order of the execution. ---*/
				usleep(10000);
			}
		}
	}
}