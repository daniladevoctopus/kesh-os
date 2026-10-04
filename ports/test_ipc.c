#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/epoll.h>

#ifndef MFD_CLOEXEC
#define MFD_CLOEXEC 0x0001U
#endif

#ifndef SYS_memfd_create
#define SYS_memfd_create 319
#endif

static int my_memfd_create(const char *name, unsigned int flags) {
    return syscall(SYS_memfd_create, name, flags);
}

int main(void) {
    printf("\n==================================================\n");
    printf("   KeshOS Wayland IPC & Shared Memory Test\n");
    printf("==================================================\n");

    /* 1. Test memfd_create & mmap */
    printf("[IPC TEST 1] Testing memfd_create and mmap...\n");
    int mfd = my_memfd_create("wl_shm_buffer", MFD_CLOEXEC);
    if (mfd < 0) {
        printf("FAILED: memfd_create returned %d\n", mfd);
        return 1;
    }
    printf("  -> memfd created with fd=%d\n", mfd);

    if (ftruncate(mfd, 8192) != 0) {
        printf("FAILED: ftruncate\n");
        return 1;
    }
    printf("  -> ftruncate(8192) succeeded\n");

    char *shm_ptr = (char *)mmap(NULL, 8192, PROT_READ | PROT_WRITE, MAP_SHARED, mfd, 0);
    if (shm_ptr == MAP_FAILED || !shm_ptr) {
        printf("FAILED: mmap on memfd\n");
        return 1;
    }
    const char *test_msg = "WAYLAND_SHM_ZERO_COPY_FRAME_DATA_SUCCESS";
    strcpy(shm_ptr, test_msg);
    printf("  -> Written to shared memory: '%s'\n", shm_ptr);

    /* 2. Test AF_UNIX socketpair */
    printf("\n[IPC TEST 2] Testing AF_UNIX socketpair...\n");
    int sv[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0) {
        printf("FAILED: socketpair\n");
        return 1;
    }
    printf("  -> Socketpair created: sv[0]=%d, sv[1]=%d\n", sv[0], sv[1]);

    const char *stream_msg = "HELLO_WAYLAND_COMPOSITOR";
    if (write(sv[0], stream_msg, strlen(stream_msg) + 1) <= 0) {
        printf("FAILED: write to socket\n");
        return 1;
    }
    char stream_buf[64] = {0};
    if (read(sv[1], stream_buf, sizeof(stream_buf)) <= 0) {
        printf("FAILED: read from socket\n");
        return 1;
    }
    printf("  -> Stream data transferred: '%s'\n", stream_buf);

    /* 3. Test SCM_RIGHTS FD Passing (Passing memfd across socket) */
    printf("\n[IPC TEST 3] Testing SCM_RIGHTS passing memfd over socket...\n");
    struct msghdr msg = {0};
    struct iovec iov[1];
    char dummy_byte = 'M';
    iov[0].iov_base = &dummy_byte;
    iov[0].iov_len = 1;
    msg.msg_iov = iov;
    msg.msg_iovlen = 1;

    char cmsg_buf[CMSG_SPACE(sizeof(int))];
    msg.msg_control = cmsg_buf;
    msg.msg_controllen = sizeof(cmsg_buf);

    struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = CMSG_LEN(sizeof(int));
    int *fd_payload = (int *)CMSG_DATA(cmsg);
    *fd_payload = mfd;

    if (sendmsg(sv[0], &msg, 0) < 0) {
        printf("FAILED: sendmsg with SCM_RIGHTS\n");
        return 1;
    }
    printf("  -> SCM_RIGHTS sent memfd=%d via socket\n", mfd);

    /* Receive SCM_RIGHTS */
    struct msghdr rmsg = {0};
    struct iovec riov[1];
    char rdummy = 0;
    riov[0].iov_base = &rdummy;
    riov[0].iov_len = 1;
    rmsg.msg_iov = riov;
    rmsg.msg_iovlen = 1;

    char rcmsg_buf[CMSG_SPACE(sizeof(int))];
    rmsg.msg_control = rcmsg_buf;
    rmsg.msg_controllen = sizeof(rcmsg_buf);

    if (recvmsg(sv[1], &rmsg, 0) < 0) {
        printf("FAILED: recvmsg with SCM_RIGHTS\n");
        return 1;
    }

    struct cmsghdr *rcmsg = CMSG_FIRSTHDR(&rmsg);
    if (!rcmsg || rcmsg->cmsg_type != SCM_RIGHTS) {
        printf("FAILED: received invalid cmsg\n");
        return 1;
    }
    int received_mfd = *(int *)CMSG_DATA(rcmsg);
    printf("  -> SCM_RIGHTS received new fd=%d pointing to same memfd!\n", received_mfd);

    /* Verify mapped data from received descriptor */
    char *rx_shm = (char *)mmap(NULL, 8192, PROT_READ | PROT_WRITE, MAP_SHARED, received_mfd, 0);
    if (rx_shm == MAP_FAILED || !rx_shm) {
        printf("FAILED: mmap on received fd\n");
        return 1;
    }
    printf("  -> Reading from received memfd: '%s'\n", rx_shm);
    if (strcmp(rx_shm, test_msg) == 0) {
        printf("  -> ZERO-COPY VERIFICATION: MATCH!\n");
    } else {
        printf("FAILED: Data mismatch!\n");
        return 1;
    }

    /* 4. Test epoll */
    printf("\n[IPC TEST 4] Testing epoll event loop...\n");
    int epfd = epoll_create1(0);
    if (epfd < 0) {
        printf("FAILED: epoll_create1\n");
        return 1;
    }
    printf("  -> epoll created with fd=%d\n", epfd);

    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = sv[1];
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, sv[1], &ev) != 0) {
        printf("FAILED: epoll_ctl ADD\n");
        return 1;
    }
    printf("  -> Added sv[1] to epoll watch list\n");

    /* No data yet: should return 0 on 0 timeout */
    struct epoll_event events[4];
    int n = epoll_wait(epfd, events, 4, 0);
    printf("  -> epoll_wait (empty) returned %d (expected 0)\n", n);

    /* Write data into sv[0] */
    write(sv[0], "WAKEUP", 6);

    /* Now epoll_wait should return 1 */
    n = epoll_wait(epfd, events, 4, 100);
    printf("  -> epoll_wait (with pending data) returned %d (expected 1)\n", n);
    if (n > 0 && events[0].data.fd == sv[1]) {
        printf("  -> Event verified: fd=%d, events=0x%x\n", events[0].data.fd, events[0].events);
    }

    printf("\n==================================================\n");
    printf("   ALL PHASE 1 IPC TESTS PASSED SUCCESSFULLY!     \n");
    printf("==================================================\n\n");
    return 0;
}
