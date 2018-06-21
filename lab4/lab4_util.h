#include <stdio.h>

unsigned int simple_strlen (const char *str);
int simple_strcmp(const char *str1, const char *str2); 
int simple_strncmp(const char* str1, const char* str2, unsigned int n);
char *simple_itoa(int num);
int positive_atoi(char* str);
void simple_system(char* command);

#define PRINT_FD(stdio) (stdio->_fileno)
