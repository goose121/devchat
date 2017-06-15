#include <sys/ioctl.h>
#include <fcntl.h>
#include <dev/devchat.h>
#include <errno.h>
#include <stdio.h>

int main(int argc, char *argv[]) {
	int fd = open("/dev/chat", O_RDWR);
	if (ioctl(fd, DEVCHATCLR) == 0)
		return 0;
	perror(NULL);
	return errno;
}
