#ifndef __COMMON_H__
#define __COMMON_H__


#define ERROR -1
#define SUCCESS 0
#define SHUTDOWN 4552

#define PORT 2018
#define CONUTINUE 1234

#define FD_ERR -1

#define HUGE_BUF_SIZE 1024*5
#define BUF_SIZE 1024
#define CMD_BUF_SIZE BUF_SIZE

#define DEF_COMMAND(command) { #command, handle_##command }

#include <memory.h>

struct addrinfo;

typedef int (*CommandHandler)(const char* args);// Return CONUTINUE iff should continue. otherwise, return exit code

struct {
	const char* cmd;
	CommandHandler handler;
} typedef CommandDesc;

typedef enum {
	IDLE,
	CONNECTING,
	CONNECTED,
	DOWNLOADING,
} CState;
	
typedef struct {
	char* server_addr;	// Address of the server as given in the [connect] command. "nil" if not connected to any server
	CState conn_state;	// Current state of the client. Initially set to IDLE
	const char* client_id;	// Client identification given by the server. NULL if not connected to a server.
	int sock_fd;		// The file descriptor used in send/recv
} ClientState;

extern ClientState client_state;

const char* splitStr(const char* str, char* before);
int exec(const CommandDesc* commands, CommandHandler err_handler, const char* full_cmd);

int createSocket(struct addrinfo **servinfo, const char* addr, int port);

int find_chr(const char* pending_buf, size_t pending_bytes, char chr);

typedef int (*SegmentFinder)(const char* pending_buf, size_t pending_bytes, size_t* skip_last);
// Pass null to segment_finder in order to do it for line-break.
int safe_recv_str(int __fd, char* __buf, size_t __n, int __flags, SegmentFinder segment_finder);
int safe_recv_str_immediate(int __fd, char* __buf, size_t __n, int __flags, SegmentFinder segment_finder);

void parseArgs(int argc, char** argv);


// Listing dir utils

int file_size(const char * filename);

//@return must be freed by the caller
char* list_dir();


#endif// __COMMON_H__