#include "lab4_util.h"

#define SYS_READ 0
#define SYS_WRITE 1
#define SYS_OPEN 2
#define SYS_CLOSE 3
#define SYS_LSEEK 8

#define O_RDONLY 0
#define O_WRONLY 1
#define O_RDWR   2

#define SEEK_SET 0

#define STDOUT 1


#define FAIL_IF_M1(val) if (val == -1) return 0x55;

extern int system_call();

int main (int argc , char* argv[], char* envp[])
{
	const int fd = system_call(SYS_OPEN, argv[1], O_WRONLY, 0777);
	FAIL_IF_M1(fd);

	const char* xname = argv[2];
	const size_t xname_len = simple_strlen(xname);

	const size_t start = 4117;

	FAIL_IF_M1(system_call(SYS_LSEEK, fd, start, SEEK_SET));

	FAIL_IF_M1(system_call(SYS_WRITE, fd, xname, xname_len));
	FAIL_IF_M1(system_call(SYS_WRITE, fd, ".\n", sizeof(".\n")));/* writes .\n\0 */

	FAIL_IF_M1(system_call(SYS_CLOSE, fd));

	return 0;
}
