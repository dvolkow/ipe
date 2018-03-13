#define _GNU_SOURCE

#include <sys/syscall.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <linux/netlink.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "../include/nl.h"


#define MAX_PAYLOAD 1024 /* maximum payload size*/

int sock_fd;
struct sockaddr_nl src_addr, dest_addr;
struct msghdr msg;



static void prepare(void) {
        printf("%s: entry\n", __FUNCTION__);
        memset(&src_addr, 0, sizeof(src_addr));
        src_addr.nl_family = AF_NETLINK;

        bind(sock_fd, (struct sockaddr*)&src_addr, sizeof(src_addr));

        memset(&dest_addr, 0, sizeof(dest_addr));
        dest_addr.nl_family = AF_NETLINK;
        dest_addr.nl_pid = 0; /* For Linux Kernel */
        dest_addr.nl_groups = 0; /* unicast */
}


int get_netns_fd(const char *name)
{
	char pathbuf[MAX_PATH_LEN];
	const char *path, *ptr;

	path = name;
	ptr = strchr(name, '/');
	if (!ptr) {
		snprintf(pathbuf, sizeof(pathbuf), "%s/%s",
			NETNS_RUN_DIR, name );
		path = pathbuf;
	}
	return open(path, O_RDONLY);
}


static void create_msg(struct nlmsghdr *nlh, const char *vrf) {
        printf("%s: entry\n", __FUNCTION__);

        nlmsg_t msgs = {
                .value = 10,
                .ifindex = 2,     
                .nsfd = vrf ? get_netns_fd(vrf) : 0,
                .command = NL_PRINT_ADDR
        };

        printf("%s: memcpy %lu to %s\n", __FUNCTION__, sizeof(msgs), (char *)nlh);
        memcpy(NLMSG_DATA(nlh), &msgs, sizeof(msgs));
        printf("%s: ret\n", __FUNCTION__);
}



static void sending(struct nlmsghdr *nlh, struct msghdr *msgh, const char *vrf) {
        printf("%s: entry\n", __FUNCTION__);
        struct iovec iov;
        nlh = (struct nlmsghdr *)malloc(NLMSG_SPACE(MAX_PAYLOAD));
        memset(nlh, 0, NLMSG_SPACE(MAX_PAYLOAD));
        nlh->nlmsg_len = NLMSG_SPACE(MAX_PAYLOAD);
        nlh->nlmsg_flags = 0;

        create_msg(nlh, vrf);

        iov.iov_base = (void *)nlh;
        iov.iov_len = nlh->nlmsg_len;
        msg.msg_name = (void *)&dest_addr;
        msg.msg_namelen = sizeof(dest_addr);
        msg.msg_iov = &iov;
        msg.msg_iovlen = 1;

        printf("Sending message to kernel\n");
        sendmsg(sock_fd,&msg,0);
}


int main(int args, char** argv)
{
        const char *name = argv[1];
        struct nlmsghdr *nlh;

        printf("%s: start, name = %s\n", __FUNCTION__, name);

        sock_fd = socket(PF_NETLINK, SOCK_RAW, NETLINK_USER);

        if (sock_fd < 0)
                return -1;

        prepare();
        sending(nlh, &msg, name);
        close(sock_fd);
}
