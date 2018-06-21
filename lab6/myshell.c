#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <linux/limits.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>

#include "line_parser.h"
#include "job_control.h"

#define STDIN 0
#define STDOUT 1

#define BUFFER_SIZE 2048

struct termios shell_term;

void signal_handler(int sig)
{
	printf("Signal %s was received and ignored\n", strsignal(sig));
}

void empty_handler(int sig)
{
}

int countList(const cmd_line *line)
{
	int i = 0;

	while (line) {
		++i;
		line = line->next;
	}

	return i;
}

int isBlocking(const cmd_line *line)
{
	int blocking = 0;

	while (line) {
		blocking = line->blocking;
		line = line->next;
	}

	return blocking;
}

int redirect_fd(const char* target_file, int old_fd, int flags, mode_t mode)
{
	if (target_file)
	{
		int fd = open(target_file, flags, mode);
		return dup2(fd, old_fd);
	}
	else
		return -1;
}

job* execute(job **jobs, const cmd_line *line, const char *cmd)
{
	int cmdCount = countList(line);

	int in_fd_for_child = STDIN;

	const int blocking = isBlocking(line);

	pid_t firstId = 0;

	for (int i = 0; i < cmdCount; ++i, line = line->next)
	{
		int pipefd[2];
		if (line->next)
		{
			if (pipe(pipefd) == -1)
				exit(-1);
		}

		const pid_t id = fork();
		if (id == 0) {
			if (in_fd_for_child != STDIN)
			{
				dup2(in_fd_for_child, STDIN);
				close(in_fd_for_child);
			}

			redirect_fd(line->input_redirect, STDIN, O_RDONLY, S_IRUSR | S_IWUSR);
			redirect_fd(line->output_redirect, STDOUT, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
			
			if (line->next)
			{
				close(pipefd[0]);

				dup2(pipefd[1], STDOUT);

				close(pipefd[1]);
			}

			execvp(line->arguments[0], line->arguments);

			perror("Process failed to run");
			fflush(stdout);

			return NULL;
		}

		if (in_fd_for_child != STDIN)
			close(in_fd_for_child);

		if (i == 0)
		{
			firstId = id;
			if (setpgid(id, 0) != 0)
				perror("Failed to set master group id");;
		} else
			setpgid(id, firstId);

		if (line->next)
		{
			close(pipefd[1]);
			in_fd_for_child = pipefd[0];
		}
	}

	job* j = add_job(jobs, cmd);
	j->pgid = firstId;
	j->status = RUNNING;

	tcsetpgrp(STDIN_FILENO, j->pgid);
	tcgetattr(STDIN_FILENO, j->tmodes);
	tcsetpgrp(STDIN_FILENO, getpid());

	if (blocking)
		run_job_in_foreground(jobs, j, 1, &shell_term, getpid());

	return j;
}

int main(int argc, char *argv[])
{
	//signal(SIGTSTP, signal_handler);
	signal(SIGQUIT, signal_handler);
	//signal(SIGCHLD, signal_handler);// This line is bad!!

	signal(SIGTTIN, SIG_DFL);
	signal(SIGTSTP, SIG_DFL);

	// Ignore those signals so they could get to the current foreground process
	signal(SIGTTOU, SIG_IGN);

	signal(SIGCHLD, SIG_IGN);
	//signal(SIGSTOP, empty_handler);

	char cwd[PATH_MAX];
	getcwd(cwd, PATH_MAX);

	job* jobs = NULL;

	tcgetattr(STDIN_FILENO, &shell_term);

	int to_continue = 1;

	while (to_continue)
	{
		printf("%s$ ", cwd);
		fflush(stdout);

		char buffer[BUFFER_SIZE];

		const char* read = fgets(buffer, BUFFER_SIZE, stdin);

		if (read != NULL && strcmp(buffer, "\n") != 0) {
			cmd_line *line = parse_cmd_lines(buffer);
			
			if (line->arg_count > 0)
			{
				if (strcmp(line->arguments[0], "quit") == 0)
					to_continue = 0;
				else if (strcmp(line->arguments[0], "jobs") == 0)
					print_jobs(&jobs);
				else if (strcmp(line->arguments[0], "fg") == 0) {
					if (line->arg_count != 2)
						fprintf(stderr, "fg arguments are bad\n");
					else {
						job *j = find_job_by_index(jobs, atoi(line->arguments[1]));
						if (j)
							run_job_in_foreground(&jobs, j, 1, &shell_term, getpid());
					}
				} else if (strcmp(line->arguments[0], "bg") == 0) {
					if (line->arg_count != 2)
						fprintf(stderr, "bg arguments are bad\n");
					else {
						job *j = find_job_by_index(jobs, atoi(line->arguments[1]));
						if (j)
							run_job_in_background(j, 1);
					}
				} else {
					if (!execute(&jobs, line, buffer))
						to_continue = 0;
				}
			}

			fflush(stdout);

			// Cleanup
			free_cmd_lines(line);
		}
	}

	free_job_list(&jobs);

	return 0;
}
