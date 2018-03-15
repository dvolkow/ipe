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

#define NEXT_ARG(args, argv) (argv++, args--)
#define CHECK_ARGS(args) (args - 1 > 0)


typedef struct nlmsghdr nmsgh_t;



/* Parser's structure for create Netlink message */
typedef struct {
        char *net;
        char *ctype;
        int value;
        int ifindex;
} ipe_arg_t;


static ipe_arg_t g_arg = {
        .net     = NULL,
        .ctype   = NULL,
        .value   = 0,
        .ifindex = 0
};


int sock_fd;
struct sockaddr_nl src_addr, dest_addr;
struct msghdr msg;



static void prepare(void) {
#ifdef IPE_DEBUG
        printf("%s: entry\n", __FUNCTION__);
#endif
        memset(&src_addr, 0, sizeof(src_addr));
        src_addr.nl_family = AF_NETLINK;

        bind(sock_fd, (struct sockaddr*)&src_addr, sizeof(src_addr));
        memset(&dest_addr, 0, sizeof(dest_addr));

        dest_addr.nl_family = AF_NETLINK;
        dest_addr.nl_pid    = 0; /* For Linux Kernel */
        dest_addr.nl_groups = 0; /* unicast */
}



/* This function is paste from iproute2 */
static int get_netns_fd(const char *name)
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



static void create_msg(nmsgh_t *nlh) {
#ifdef IPE_DEBUG
        printf("%s: entry\n", __FUNCTION__);
#endif

        nlmsg_t msgs = {
                .value = g_arg.value,
                .ifindex = g_arg.ifindex,     
                .nsfd = g_arg.net ? get_netns_fd(g_arg.net) : -1,
        };

        if (!strcmp(g_arg.ctype, "id"))
                msgs.command = IPE_SET_VID;
        else if (!strcmp(g_arg.ctype, "eth"))
                msgs.command = IPE_SET_ETH;
        else if (!strcmp(g_arg.ctype, "parent"))
                msgs.command = IPE_PRINT_ADDR;

#ifdef IPE_DEBUG
        printf("%s: memcpy %lu to %s\n", __FUNCTION__, sizeof(msgs), (char *)nlh);
#endif
        memcpy(NLMSG_DATA(nlh), &msgs, sizeof(msgs));
#ifdef IPE_DEBUG
        printf("%s: ret\n", __FUNCTION__);
#endif
}



static void sending(nmsgh_t *nlh, struct msghdr *msgh) {
#ifdef IPE_DEBUG
        printf("%s: entry\n", __FUNCTION__);
#endif
        struct iovec iov;
        nlh = (struct nlmsghdr *)malloc(NLMSG_SPACE(MAX_PAYLOAD));
        memset(nlh, 0, NLMSG_SPACE(MAX_PAYLOAD));

        nlh->nlmsg_len   = NLMSG_SPACE(MAX_PAYLOAD);
        nlh->nlmsg_flags = 0;

        create_msg(nlh);

        iov.iov_base = (void *)nlh;
        iov.iov_len  = nlh->nlmsg_len;

        msg.msg_name    = (void *)&dest_addr;
        msg.msg_namelen = sizeof(dest_addr);
        msg.msg_iov     = &iov;
        msg.msg_iovlen  = 1;

#ifdef IPE_DEBUG
        printf("Sending message to kernel\n");
#endif
        sendmsg(sock_fd,&msg,0);
}



static void show_usage(void) {
        printf("Usage: ipe dev IFINDEX [ netns NETNS ] id  [ VID ]\n");
        printf("                                       eth [ ETH_TYPE ]\n");
}



static int parse_arg(int args, char **argv) {
        
        inline int matches(const char *arg) {
                return !strcmp(*argv, arg);
        }

        if (!CHECK_ARGS(args)) {
                goto usage_ret;
        }

        while (CHECK_ARGS(args)) {
                NEXT_ARG(args, argv);
                if (matches("dev")) {
                        if (CHECK_ARGS(args)) {
                                NEXT_ARG(args, argv);
                                g_arg.ifindex = atoi(*argv);
#ifdef IPE_DEBUG
                                printf("%s: get index %d\n", __FUNCTION__, g_arg.ifindex);
#endif
                        } else {
                                goto usage_ret;
                        }
                } else if (!strcmp(*argv, "netns")) {
                        if (CHECK_ARGS(args)) {
                                NEXT_ARG(args, argv);
                                g_arg.net = *argv;
#ifdef IPE_DEBUG
                                printf("%s: get net %s\n", __FUNCTION__, g_arg.net);
#endif
                        } else {
                                goto usage_ret;
                        }
                } else if (matches("id")) {
                        g_arg.ctype = *argv;
                        if (CHECK_ARGS(args)) {
                                NEXT_ARG(args, argv);
                                g_arg.value = atoi(*argv);
#ifdef IPE_DEBUG
                                printf("%s: get command set vid %d\n", __FUNCTION__, g_arg.value);
#endif
                                goto ret_ok;
                        } else {
                                goto usage_ret;
                        }
                } else if (matches("eth")) {
                        g_arg.ctype = *argv;
                        if (CHECK_ARGS(args)) {
                                NEXT_ARG(args, argv);
                                g_arg.value = atoi(*argv);
#ifdef IPE_DEBUG
                                printf("%s: get command set eth_type %d\n", __FUNCTION__, g_arg.value);
#endif
                                goto ret_ok;
                        } else {
                                goto usage_ret;
                        }
                } else if (matches("parent")) {
                        g_arg.ctype = *argv;
                        goto ret_ok;
                } else {
                        printf("%s: arg \"%s\" not matches\n", __FUNCTION__, *argv);
                        goto usage_ret;
                }
        }

usage_ret:
        show_usage();
        return IPE_FEW_ARG;
ret_ok:
        return IPE_OK;

}



int main(int args, char **argv)
{
        int res = parse_arg(args, argv);
        if (res) {
                return res;
        }

        struct nlmsghdr *nlh;

#ifdef IPE_DEBUG
        printf("%s: start, name = %s\n", __FUNCTION__, name);
#endif

        sock_fd = socket(PF_NETLINK, SOCK_RAW, NETLINK_USER);

        if (sock_fd < 0)
                return -1;

        prepare();
        sending(nlh, &msg);
        close(sock_fd);
}
