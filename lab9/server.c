#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/sendfile.h>
#include <fcntl.h>
#include <unistd.h>
#include <netdb.h>

#define MAX_CONNECTION 10

int bind_and_listen(int socket_fd, const struct addrinfo *servinfo) {
    if (bind(socket_fd, servinfo->ai_addr, servinfo->ai_addrlen)) {
        perror("bind error");
        return ERROR;
    }
    // otherwise
    
    if (listen(socket_fd, MAX_CONNECTION) == -1) {
        perror("listen error");
        return ERROR;
    }
    // otherwise

    return SUCCESS;
}

// Return socket_number
int init()
{
	client_state.client_id = "mate";

    struct addrinfo *servinfo;
    const int socket_fd = createSocket(&servinfo, client_state.server_addr, PORT);
    if (socket_fd == FD_ERR)
        return FD_ERR;
    // otherwise

    if (bind_and_listen(socket_fd, servinfo)) {
		freeaddrinfo(servinfo); // free the linked-list
        close(socket_fd);
        return FD_ERR;
    }
    // otherwise

	freeaddrinfo(servinfo); // free the linked-list

    return socket_fd;
}

int handle_hello(const char* args) {
	if (client_state.conn_state != CONNECTING)
		return ERROR;
	// otherwise

	// Send Hello to the client
	char msg[BUF_SIZE];
	sprintf(msg, "hello %s\n", client_state.client_id);
	if (send(client_state.sock_fd, msg, strlen(msg), 0) == -1) {
		perror("send hello error");
		return ERROR;
	}
	// otherwise

	client_state.conn_state = CONNECTED;

	printf("Connected to %s!\n", client_state.client_id);

	return CONUTINUE;
}

int handle_ls(const char* args) {
	if (client_state.conn_state != CONNECTED)
		return ERROR;
	// otherwise

	// Send Hello to the client
	char* dir_ls = list_dir();
	if (!dir_ls) {
		fprintf(stderr, "Error while trying to listing current working directory\n");
		char err_msg[] = "nok filesystem\n";
		if (send(client_state.sock_fd, err_msg, strlen(err_msg), 0) == -1) {
			perror("send \"nok filesystem\" error");
			return ERROR;
		}
	}
	// otherwise

	char prefix_msg[] = "ok\n";
	if (send(client_state.sock_fd, prefix_msg, strlen(prefix_msg), 0) == -1) {
		free(dir_ls);
		perror("send ok error");
		return ERROR;
	}
	// otherwise

	printf("%s\n", dir_ls);

	int result = send(client_state.sock_fd, dir_ls, strlen(dir_ls), 0);
	free(dir_ls);
	if (result == -1) {
		perror("send dir_ls error");
		return ERROR;
	}
	// otherwise

	char postfix_msg[] = "\n";
	if (send(client_state.sock_fd, postfix_msg, strlen(postfix_msg), 0) == -1) {
		perror("send postfix error");
		return ERROR;
	}
	// otherwise

	char cwd[BUF_SIZE];
	getcwd(cwd, BUF_SIZE);
	printf("Listed files at %s!\n", cwd);

	return CONUTINUE;
}

int handle_get(const char* args) {
	if (client_state.conn_state != CONNECTED)
		return ERROR;
	// otherwise

	int size = file_size(args);
	if (size == -1) {
		char msg[] = "nok can't find file\n";
		if (send(client_state.sock_fd, msg, strlen(msg), 0) == -1) {
			perror("send nok file error");
			return ERROR;
		} else
			return CONUTINUE;
	}
	// otherwise

	char ok_msg[BUF_SIZE];
	sprintf(ok_msg, "ok %d\n", size);
	if (send(client_state.sock_fd, ok_msg, strlen(ok_msg), 0) == -1) {
		perror("send ok file error");
		return ERROR;
	}
	// otherwise

	const int file_fd = open(args, O_RDONLY);
	if (file_fd == -1) {
		perror("File open error!");
		char msg[] = "nok file open error\n";
		if (send(client_state.sock_fd, msg, strlen(msg), 0) == -1) {
			perror("send nok file open error");
			return ERROR;
		} else
			return CONUTINUE;
	}
	// otherwise

	if (sendfile(client_state.sock_fd, file_fd, NULL, size) != size) {
		fprintf(stderr, "Send file fail!\n");
		close(file_fd);
		return ERROR;
	}
	// otherwise

	close(file_fd);

	char response[BUF_SIZE];
	if (safe_recv_str(client_state.sock_fd, response, BUF_SIZE, 0, NULL) == -1) {
		fprintf(stderr, "Client {%s} disconnected with error\n", client_state.client_id);
		return ERROR;
	}
	// otherwise
	
	if (strcmp(response, "done") != 0) {
		fprintf(stderr, "Client {%s} response with different message than \"done\"\n", client_state.client_id);
		return ERROR;
	}
	// otherwise

	return CONUTINUE;
}

int handle_bye(const char* args) {
	if (client_state.conn_state != CONNECTED)
		return ERROR;
	// otherwise

	printf("Disconnected from %s!\n", client_state.client_id);

	return 0;
}

int handle_shutserver(const char* args) {
	if (client_state.conn_state != CONNECTED)
		return ERROR;
	// otherwise

	printf("Shutserver request from %s!\n", client_state.client_id);

	return SHUTDOWN;
}

int handle_nok(const char* args) {
	fprintf(stderr, "Server Error: %s\n", args);

	return ERROR;
}

int err_handler(const char* full_cmd) {
	char msg[BUF_SIZE];
    sprintf(msg, "Bad command! {%s}\n", full_cmd);
	if (send(client_state.sock_fd, msg, strlen(msg), 0) == -1)
		perror("send error error");
	
    fprintf(stderr, "%s\n", msg);
	return ERROR;
}

int serv_client() {
	const CommandDesc commands[] = {
		DEF_COMMAND(hello),
		DEF_COMMAND(ls),
		DEF_COMMAND(get),
		DEF_COMMAND(bye),
		DEF_COMMAND(shutserver),
		DEF_COMMAND(nok),
		{ NULL, NULL }
	};

	int result;
	do {
		char cmd[BUF_SIZE];
		// TODO: Perform it by line break
		int bytes = safe_recv_str(client_state.sock_fd, cmd, BUF_SIZE, 0, NULL);
		if (bytes < 0) {
			fprintf(stderr, "Client {%s} disconnected with error\n", client_state.client_id);
			return 0;
		}
		else if (bytes == 0)
			continue;
		// otherwise
		
		printf("Received %s\n", cmd);

		result = exec(commands, err_handler, cmd);
	} while (result == CONUTINUE);

	return result;
}

int main(int argc, char** argv) {
	parseArgs(argc, argv);

	/*char host_name[BUF_SIZE];
	if (gethostname(host_name, BUF_SIZE)) {
        perror("gethostname error");
        return FD_ERR;
	}
    // otherwise
	
	client_state.server_addr = host_name;*/
	client_state.server_addr = "0.0.0.0";
	//client_state.server_addr = "127.0.0.1";

	const int server_fd  = init();
    if (server_fd == FD_ERR)
        return -1;
    // otherwise

	int exit_code = SUCCESS;
	client_state.conn_state = IDLE;
	client_state.sock_fd = FD_ERR;

	printf("Initialization complete!\n");

    while (1) {
        struct sockaddr_storage their_addr;
        socklen_t addr_size;
		memset(&addr_size, 0, sizeof(addr_size));// Only for making valgrind silent
        client_state.sock_fd = accept(server_fd, (struct sockaddr *)&their_addr, &addr_size);
		if (client_state.sock_fd == FD_ERR) {
        	perror("accept error");
			break;
		}
		// otherwise

		client_state.conn_state = CONNECTING;

		exit_code = serv_client();

		client_state.conn_state = IDLE;

		close(client_state.sock_fd);

		if (exit_code)
			break;
    }

    close(server_fd);

	printf("Shutdown server!\n");

	if (exit_code == SHUTDOWN)
		return 0;
	else
    	return exit_code;
}