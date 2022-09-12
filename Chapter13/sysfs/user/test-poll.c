#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/stat.h>
#include <unistd.h>

#define POLL_PATH "/sys/kernel/hello/poll_test/poll_group/poll_attr"

int main(int argc, char *argv[])
{
    int epfd = epoll_create1(EPOLL_CLOEXEC);
    if (epfd < 0) {
        printf("Failed to create epoll fd\n");
        return -1;
    }

    unsigned char buffer[16] = {0};
    int file_fd = open(POLL_PATH, O_RDWR);
    if (file_fd < 0) {
        printf("Failed to open polling file\n");
        return -1;
    }
    /* first read to the end of file */
    read(file_fd, buffer, sizeof(buffer));

    struct epoll_event event = { 0 };
    event.data.fd = file_fd;
    event.events = EPOLLERR | EPOLLPRI;

    if (epoll_ctl(epfd, EPOLL_CTL_ADD, file_fd, &event)) {
        printf("Failed to add poll fd\n");
        goto err;
    }

    memset(buffer, 0, sizeof(buffer));

    struct epoll_event revent;
    if (1 == epoll_wait(epfd, &revent, 1, -1)) {
        printf("Get notified: %d\n", revent.data.fd);
        int ret = read(revent.data.fd, buffer, sizeof(buffer));
        if (ret > 0) {
            printf("%d, %s\n", ret, buffer);
        } else {
            printf("Cannot read from poll file: %d\n", ret);
        }
    } else {
        printf("Failed to wait epoll\n");
        goto err;
    }

    return 0;

err:
    close(epfd);
    close(file_fd);
    return -1;
}
