#include "lab4_util.h"

#define SYS_READ 0
#define SYS_WRITE 1
#define SYS_OPEN 2
#define SYS_CLOSE 3
#define SYS_LSEEK 8
#define SYS_GETDENTS 78
#define SYS_OPENAT 257

#define O_RDONLY 0
#define O_WRONLY 1
#define O_RDWR   2
#define	O_NONBLOCK	0x0004		/* no delay */
/*#define O_CLOEXEC 0		// close-on-exec flag */


#define AT_FDCWD -100/* Special value used to indicate openat should use the current working directory. */

#define DT_DIR 4

#define STDOUT 1

#define FAIL_IF_M1(val) if (val == -1) { system_call(SYS_WRITE, STDOUT, "Error!\n", sizeof("Error!\n")); return 0x55; }
#define M1_IF_M1(val) if (val == -1) return -1;

#define BUF_SIZE 1024

struct linux_dirent {
   unsigned long  d_ino;     /* Inode number */
   unsigned long  d_off;     /* Offset to next linux_dirent */
   unsigned short d_reclen;  /* Length of this linux_dirent */
   char           d_name[];  /* Filename (null-terminated) */
                     /* length is actually (d_reclen - 2 -
                        offsetof(struct linux_dirent, d_name)) */
   /*
   char           pad;       // Zero padding byte
   char           d_type;    // File type (only since Linux
                             // 2.6.4); offset is (d_reclen - 1)
   */
};

extern int system_call();

int my_strcpy(char *dst, const char *src)
{
	int i = 0;
	for (; src[i] != '\0'; ++i)
		dst[i] = src[i];

	dst[i] = '\0';

	return i;
}

/* Make sure all pointers are disjoint! */
int concat(char* result, const char *s1, const char *s2)
{
	const int s1_len = my_strcpy(result, s1);
	const int s2_len = my_strcpy(result + s1_len, s2);

	return s1_len + s2_len;
}

/* Make sure all pointers are disjoint! */
int concat_3(char* result, const char *s1, const char *s2, const char* s3)
{
	int length = concat(result, s1, s2);
	length += my_strcpy(result + length, s3);

	return length;
}

/* Make sure all pointers are disjoint! */
int concat_dir_and_file(char* result, const char *dirpath, const char *filename)
{
	return concat_3(result, dirpath, "/", filename);
}

int print_dir(const char* path, const char* filter, const char* command);

int print_dir(const char* path, const char* filter, const char* command)
{
	const int dir_fd = system_call(SYS_OPENAT, AT_FDCWD, path, O_RDONLY|O_NONBLOCK/*|O_DIRECTORY|O_CLOEXEC*/);
	M1_IF_M1(dir_fd);

	char buf[BUF_SIZE];
	const int nread = system_call(SYS_GETDENTS, dir_fd, buf, BUF_SIZE);

	M1_IF_M1(nread);
	
	M1_IF_M1(system_call(SYS_CLOSE, dir_fd));

	int bpos;
	for (bpos = 0; bpos < nread;) {
		const struct linux_dirent *d = (struct linux_dirent *) (buf + bpos);
		const char d_type = *(buf + bpos + d->d_reclen - 1);
		
		char file_path[BUF_SIZE];
		const int file_path_length = concat_dir_and_file(file_path, path, d->d_name);

		if (simple_strcmp(d->d_name, ".") && simple_strcmp(d->d_name, ".."))
		{
			if (filter == 0 || !simple_strcmp(d->d_name, filter))
			{
				if (command == 0)
				{
					M1_IF_M1(system_call(SYS_WRITE, STDOUT, file_path, file_path_length));
					M1_IF_M1(system_call(SYS_WRITE, STDOUT, "\n", sizeof("\n")));
				} else {
					char cmd_line[1024];
					concat_3(cmd_line, command, " ", file_path);
					simple_system(cmd_line);
					return 1;
				}
			}

			if (d_type == DT_DIR)
			{
				int result = print_dir(file_path, filter, command);
				M1_IF_M1(result);
				if (result == 1)
					return 1;
			}
		}

		bpos += d->d_reclen;
	}

	return 0;
}

int main (int argc , char* argv[], char* envp[])
{
	const char* filter = 0;
	const char* command = 0;
	if (argc >= 3 && !simple_strcmp(argv[1], "-n"))
		filter = argv[2];
	else if (argc >= 4 && !simple_strcmp(argv[1], "-e"))
	{
		filter = argv[2];
		command = argv[3];
	}

	if (filter == 0)
		FAIL_IF_M1(system_call(SYS_WRITE, STDOUT, ".\n", sizeof(".\n")));

	int result = print_dir(".", filter, command);
	FAIL_IF_M1(result);

	if (command != 0 && result != 1)
	{
		char msg[1024];
		const int msg_length = concat_3(msg, "The file '", filter, "' Does not exist.\n");
		M1_IF_M1(system_call(SYS_WRITE, STDOUT, msg, msg_length));
	}

	return 0;
}
