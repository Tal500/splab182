#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>

#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <dirent.h>
#include <ctype.h>

int debug_mode = 0;

ClientState client_state;

const char* splitStr(const char* str, char* before) {
	while (*str != '\0' && *str != ' ') {
		*before = *str;
		++before;
		++str;
	}
	*before = '\0';
	
	while (*str != '\0' && *str == ' ')
		++str;
	
	return str;
}

int exec(const CommandDesc* commands, CommandHandler err_handler, const char* full_cmd) {
	char cmd_prefix[CMD_BUF_SIZE];
	const char* args = splitStr(full_cmd, cmd_prefix);

	while (commands->cmd != NULL) {
		if (strcmp(commands->cmd, cmd_prefix) == 0)
			return commands->handler(args);
		// otherwise

		++commands;
	}
	// otherwise

	return err_handler(full_cmd);
}

int createSocket(struct addrinfo **servinfo, const char* addr, int port) {
	int status;
	struct addrinfo hints;

	memset(&hints, 0, sizeof(hints)); // make sure the struct is empty
	hints.ai_family = AF_UNSPEC;     // don't care IPv4 or IPv6
	hints.ai_socktype = SOCK_STREAM; // TCP stream sockets
	hints.ai_flags = AI_PASSIVE;     // fill in my IP for me

	char port_str[32];
	sprintf(port_str, "%d", port);

	if ((status = getaddrinfo(addr, port_str, &hints, servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
		return -1;
	}
	// otherwise

	// servinfo now points to a linked list of 1 or more struct addrinfos

	int socket_number = socket((*servinfo)->ai_family, (*servinfo)->ai_socktype, (*servinfo)->ai_protocol);
    if (socket_number == FD_ERR) {
		perror("socket error");
		freeaddrinfo(*servinfo); // free the linked-list
		return -1;
	}
	else
		return socket_number;
}

char pending_buf[HUGE_BUF_SIZE];
size_t pending_bytes = 0;

int find_chr(const char* pending_buf, size_t pending_bytes, char chr) {
	for (int i = 0; i < pending_bytes; ++i)
		if (pending_buf[i] == chr)
			return i;
	// otherwise
	return -1;
}

int default_segment_finder(const char* pending_buf, size_t pending_bytes, size_t* skip_last) {
	/*int start = 0;
	// Skip first breaks (from last time and others)
	while (start < *pending_bytes && pending_buf[start] == '\n')
		++start;
	
	memmove(pending_buf, pending_buf + start, *pending_bytes - start);
	*pending_bytes -= start;*/

	*skip_last = 1;

	return find_chr(pending_buf, pending_bytes, '\n');
}

size_t min(size_t a, size_t b) {
	if (a < b)
		return a;
	else
		return b;
}

int safe_recv_str(int __fd, char*__buf, size_t __n, int __flags, SegmentFinder segment_finder) {
	if (!segment_finder)
		segment_finder = default_segment_finder;
	
	size_t skip_last;

	int loc = segment_finder(pending_buf, pending_bytes, &skip_last);
	if (loc < 0) {
		const size_t max_read = min(__n - 1, HUGE_BUF_SIZE - pending_bytes);
		if (max_read <= 0) {
			fprintf(stderr, "Receive buffer overflow!\n");
			return -1;
		}
		// otherwise

		int bytes = recv(__fd, pending_buf + pending_bytes, max_read, __flags);
		if (bytes <= 0) {
			if (bytes == -1)
				perror("recv error");
			
			return -1;
		}

		pending_bytes += bytes;

		loc = segment_finder(pending_buf, pending_bytes, &skip_last);
		if (loc < 0)
			return 0;// Just wait for the next time
	}
	// otherwise
	//printf("loc: %d\n", loc);

	memcpy(__buf, pending_buf, loc);
	__buf[loc] = '\0';

	if (debug_mode)
		printf("%s|Log: %s\n", client_state.server_addr, __buf);
	
	const int total = loc + skip_last;

	//printf("Total: %d\n", total);
	//printf("pending_bytes: %ld\n", pending_bytes);

	memmove(pending_buf, pending_buf + total, pending_bytes - total);

	pending_bytes -= total;

	return total;
}

int safe_recv_str_immediate(int __fd, char* __buf, size_t __n, int __flags, SegmentFinder segment_finder) {
	int bytes = 0;
	while (bytes == 0)
		bytes = safe_recv_str(__fd, __buf, __n, __flags, segment_finder);
	
	return bytes;
}

void parseArgs(int argc, char** argv) {
	if (argc >= 2 && (strcmp(argv[1], "-d") == 0))
		debug_mode = 1;
}

static char* _list_dir(DIR * dir, int len) {
	struct dirent * ent = readdir(dir);
	struct stat info;

	if (ent == NULL) {
		char * listing = malloc((len + 1) * sizeof(char));
		listing[len] = '\0';
		return listing;
	}
	if (stat(ent->d_name, &info) < 0) {
		perror("stat");
		return NULL;
	}
	
	int ent_len = 0;
	if(S_ISREG(info.st_mode)){
		ent_len = strlen(ent->d_name);
		ent_len++; /* Account for the \n */
	}
	
	char * listing = _list_dir(dir, len + ent_len);	
	
	if(S_ISREG(info.st_mode)){	
		strcpy(&listing[len], ent->d_name);
		listing[len + ent_len - 1] = '\n';
	}
	return listing;
}


int file_size(const char * filename){
	long filesize = -1;
	FILE * file;
	file = fopen (filename, "r");
	if (file == NULL){
		perror("fopen");
		goto err;
	}
	
	if(fseek (file, 0, SEEK_END) != 0){
		perror("fseek");
		goto err;
	}
	if((filesize = ftell(file)) < 0){
		perror("ftell");
		goto err;
	}		
	
	fclose(file);
	
	return filesize;
	
	err:
	if (file) {
		fclose(file);
	}
	return -1;
}

//@return must be freed by the caller
char* list_dir(){
	DIR *dir = opendir("."); 
	if (dir == NULL){
		perror("opendir");
		return NULL;
	}
	
	char * listing = _list_dir(dir, 0);
	closedir(dir);
	return listing;
}