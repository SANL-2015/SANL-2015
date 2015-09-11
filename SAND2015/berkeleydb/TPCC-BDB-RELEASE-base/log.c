#include "log.h"
#include <sys/types.h>
#include <fcntl.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <stdio.h>

int fd;

int
init_log(int pid)
{
#define BUFSIZE 40
    char log_name[BUFSIZE];
    mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;

    snprintf(log_name, BUFSIZE, "./log%d.txt", getpid());
/** +EDIT */
    //fd = open(log_name, O_WRONLY | O_CREAT | O_EXCL, mode);
    fd = open(log_name, O_WRONLY | O_CREAT, mode);
/** -EDIT */
    if(fd <= 0)
    {
	printf("Problem opening file %s: %s\n", log_name, strerror(errno));
	return -1;
    }
    return 0;
}

void
write_log(const char* format, ...)
{
    char buf[256];
    va_list ap;
    int len;
    
    if(fd <= 0)
    {
	if(init_log(0) != 0)
	    return;
    }

    va_start(ap, format);
    len = vsnprintf(buf, sizeof(buf), format, ap);
    va_end(ap);
    
    buf[255] = '\0';
    lseek(fd, 0, SEEK_END);
    write(fd, buf, strlen(buf));
    //    printf("%s\n", buf);
}
