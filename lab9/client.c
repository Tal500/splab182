#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>

char server_addr_buf[BUF_SIZE];

void set_idle()
{
	client_state.server_addr = server_addr_buf;
	strcpy(client_state.server_addr, "nil");
	client_state.conn_state = IDLE;
	client_state.client_id = NULL;
	client_state.sock_fd = FD_ERR;
}

int handle_ok_status(int fd, char* args) {
	char prefix_msg[BUF_SIZE];
	if (safe_recv_str_immediate(client_state.sock_fd, prefix_msg, BUF_SIZE, 0, NULL) == -1) {
		fprintf(stderr, "Error: Didn't received properly ok_status result!\n");
		return 0;
	}
	// otherwise

	const char ok[] = "ok";
	const char nok[] = "nok";

	char prefix[BUF_SIZE];

	strcpy(args, splitStr(prefix_msg, prefix));

	if (strcmp(prefix, ok) == 0)
		return 1;
	else if (strcmp(prefix, nok) == 0) {
		fprintf(stderr, "Server Error: nok %s\n", args);
		return 0;
	}
	else {
		fprintf(stderr, "Bad response from server: %s\n", prefix_msg);
		return 0;
	}
}

int handle_quit(const char* args) {
	return 0;
}

int handle_conn(const char* args) {
	if (client_state.conn_state != IDLE) {
		fprintf(stderr, "Error: Client is already connected to: %s\n", client_state.server_addr == NULL ? "(no name)" : client_state.server_addr);
		return CONUTINUE;
	}
	// otherwise

	client_state.conn_state = CONNECTING;
	strcpy(client_state.server_addr, args);

	printf("connect to: %s:%d\n", args, PORT);

    struct addrinfo *servinfo;
    client_state.sock_fd = createSocket(&servinfo, client_state.server_addr, PORT);
    if (client_state.sock_fd == FD_ERR)
		return -1;
    // otherwise

	// Connect
	if (connect(client_state.sock_fd, servinfo->ai_addr, servinfo->ai_addrlen) != 0) {
		perror("connection error");
		freeaddrinfo(servinfo); // free the linked-list
		return -1;
	}
	// otherwise

	freeaddrinfo(servinfo); // free the linked-list

	const char msg[] = "hello\n";
	if (send(client_state.sock_fd, msg, strlen(msg), 0) == -1) {
		perror("send hello error");
		return -1;
	}
	// otherwise

	char result_msg[BUF_SIZE];
	if (safe_recv_str(client_state.sock_fd, result_msg, BUF_SIZE - 1, 0, NULL) == -1)
		return -1;
	// otherwise

	char result_msg_before[BUF_SIZE];
	const char* client_id = splitStr(result_msg, result_msg_before);
	const char expected_msg[] = "hello";

	if (strcmp(result_msg_before, expected_msg) != 0) {
		fprintf(stderr, "server returned unexpected msg prefix. expected {%s}, recieved prefix {%s}\n", expected_msg, result_msg_before);
		return -1;
	}
	// otherwise

	client_state.client_id = client_id;

	client_state.conn_state = CONNECTED;

	return CONUTINUE;
}

int ls_segment_finder(const char* pending_buf, size_t pending_bytes, size_t* skip_last) {
	*skip_last = 1;

	//printf("pending_bytes: %ld\n", pending_bytes);

	int i = 0;
	while (i + 1 < pending_bytes && !(pending_buf[i] == '\n' && pending_buf[i+1] == '\n'))
		++i;

	if (i + 1 < pending_bytes)
		return i + 1;
	else
		return -1;
}

char dir_ls[HUGE_BUF_SIZE];
int handle_ls(const char* args) {
	if (client_state.conn_state != CONNECTED) {
		fprintf(stderr, "Error: Client is not connected!\n");
		return CONUTINUE;
	}
	// otherwise

	const char msg[] = "ls\n";
	if (send(client_state.sock_fd, msg, strlen(msg), 0) == -1) {
		perror("send ls error");
		return ERROR;
	}
	// otherwise

	char prefix_args[BUF_SIZE];
	if (handle_ok_status(client_state.sock_fd, prefix_args)) {
		//printf("Received OK response!\n");

		if (safe_recv_str_immediate(client_state.sock_fd, dir_ls, HUGE_BUF_SIZE, 0, ls_segment_finder) == -1) {
			fprintf(stderr, "Error: Didn't received properly ls result!\n");
			return ERROR;
		}

		printf("%s", dir_ls);
		return CONUTINUE;
	}
	else
		return ERROR;
}

int file_unreaden_bytes;
int get_segment_finder(const char* pending_buf, size_t pending_bytes, size_t* skip_last) {
	*skip_last = 0;

	if (file_unreaden_bytes == 0)
		return -1;// Should never get there
	// otherwise

	if (pending_bytes == 0)// If nothing available
		return -1;
	// otherwise

	//printf("file_unreaden_bytes: %d\n", file_unreaden_bytes);
	//printf("pending_bytes: %ld\n", pending_bytes);

	if (pending_bytes >= file_unreaden_bytes) {
		int result = file_unreaden_bytes;
		file_unreaden_bytes = 0;
		return result;
	} else {
		file_unreaden_bytes -= pending_bytes;
		return pending_bytes;
	}
}

int handle_get(const char* args) {
	if (client_state.conn_state != CONNECTED) {
		fprintf(stderr, "Error: Client is not connected!\n");
		return CONUTINUE;
	}
	// otherwise

	char temp_filename[BUF_SIZE];
	sprintf(temp_filename, "%s.tmp", args);

	FILE* file = fopen(temp_filename, "w");
	if (!file) {
		fprintf(stderr, "Error: Can't open file %s for writing!\n", temp_filename);
		return CONUTINUE;
	}
	// otherwise

	int exit_code = CONUTINUE;

	char get_msg[BUF_SIZE];
	sprintf(get_msg, "get %s\n", args);
	if (send(client_state.sock_fd, get_msg, strlen(get_msg), 0) == -1) {
		perror("send get error");
		exit_code = ERROR;
		goto close_file;
	}
	// otherwise

	char prefix_args[BUF_SIZE];
	if (!handle_ok_status(client_state.sock_fd, prefix_args)) {
		exit_code = ERROR;
		goto close_file;
	}
	// otherwise

	int file_size;
	sscanf(prefix_args, "%d", &file_size);
	file_unreaden_bytes = file_size;

	while (file_unreaden_bytes > 0) {
		char content[BUF_SIZE];
		int bytes = safe_recv_str(client_state.sock_fd, content, BUF_SIZE, 0, get_segment_finder);
		if (bytes > 0 && fwrite(content, 1, bytes, file) != bytes) {
			perror("error on fwrite");
			exit_code = ERROR;
			goto close_file;
		}
	}

	close_file:
	fclose(file);

	if (exit_code == CONUTINUE) {
		if (rename(temp_filename, args) == -1) {
			perror("rename temp file error");
			exit_code = ERROR;
		}

		char done[] = "done\n";
		if (send(client_state.sock_fd, done, strlen(done), 0) == -1) {
			perror("send done error");
			exit_code = ERROR;
		}
	} else {
		if (remove(temp_filename) == -1)
			perror("remove temp file error");
	}

	return exit_code;
}

int handle_bye(const char* args) {
	if (client_state.conn_state != CONNECTED) {
		fprintf(stderr, "Error: Client is not connected!\n");
		return CONUTINUE;
	}
	// otherwise

	const char msg[] = "bye\n";
	if (send(client_state.sock_fd, msg, strlen(msg), 0) == -1) {
		perror("send bye error");
		return -1;
	}
	// otherwise

	close(client_state.sock_fd);

	set_idle();

	return CONUTINUE;
}

int handle_shutserver(const char* args) {
	if (client_state.conn_state != CONNECTED) {
		fprintf(stderr, "Error: Client is not connected!\n");
		return CONUTINUE;
	}
	// otherwise

	const char msg[] = "shutserver\n";
	if (send(client_state.sock_fd, msg, strlen(msg), 0) == -1) {
		perror("send shutserver error");
		return -1;
	}
	// otherwise

	close(client_state.sock_fd);

	set_idle();

	return CONUTINUE;
}

int err_handler(const char* full_cmd) {
    fprintf(stderr, "Bad command! {%s}\n", full_cmd);
	return CONUTINUE;
}

int main(int argc, char** argv) {
	parseArgs(argc, argv);

    const CommandDesc commands[] = {
		DEF_COMMAND(quit),
		DEF_COMMAND(conn),
		DEF_COMMAND(ls),
		DEF_COMMAND(get),
		DEF_COMMAND(bye),
		DEF_COMMAND(shutserver),
		{ NULL, NULL }
	};

	set_idle();

	char cmd[CMD_BUF_SIZE];
	int result;
	do {
		printf("server:%s>", client_state.server_addr);
		fgets(cmd, CMD_BUF_SIZE, stdin);
		for (int i = 0; i < CMD_BUF_SIZE; ++i)
			if (cmd[i] == '\n') {
				cmd[i] = '\0';
				break;
			}
		
		result = exec(commands, err_handler, cmd);
	} while (result == CONUTINUE);

	if (client_state.sock_fd != FD_ERR)
		close(client_state.sock_fd);

	return result;
}