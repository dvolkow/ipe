/******************************************************************************
*
*                       GNU GENERAL PUBLIC LICENSE
*       Copyright © 2018 Free Software Foundation, Inc. <https://fsf.org/>
*
* Everyone is permitted to copy and distribute verbatim copies of this license 
* document, but changing it is not allowed.
*
*
* 
*
* Author:
*   March, 2018        Daniel Wolkow            
*
*
* Description:
*     This extends the capabilities of network utilities (such as iproute2). 
* The initial motivation for this was the possibility of changing the vlan_id 
* and vlan_proto "on the spot," without having to remove the network interfaces 
*
******************************************************************************/

#define _GNU_SOURCE

#include <sys/syscall.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <linux/netlink.h>
#include <linux/if.h> // IFNAMSIZ

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "ipe.h"

#define MAX_PAYLOAD 1024  /* maximum payload size*/

#define NEXT_ARG(args, argv) (argv++, args--)
#define CHECK_ARGS(args)     (args - 1 > 0)


typedef struct nlmsghdr nmsgh_t;



/* Parser's structure for create Netlink message */
typedef struct {
        char *net    [IPE_DEV_COUNT];
        int   ifindex[IPE_DEV_COUNT];
        char ifname  [IFNAMSIZ];
        char *ctype;
        int   value;
} ipe_arg_t;


static ipe_arg_t g_arg;


int sock_fd;
struct sockaddr_nl src_addr, dest_addr;
struct msghdr msg;
nmsgh_t *nlh; 


//#ifdef IPE_DEBUG
static void print_msg(const ipe_nlmsg_t *msg) {
        int i;
        for (i = 0; i < IPE_DEV_COUNT; ++i)
                printf("ifid #%d: %d\n", i, msg->ifindex[i]);
        for (i = 0; i < IPE_DEV_COUNT; ++i)
                printf("nsfd #%d: %d\n", i, msg->nsfd[i]);

        printf("ifname: %s\n", msg->ifname);
        printf("value: %d\n", msg->value);
        printf("command: %d\n", msg->command);
}
//#endif

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



static void create_msg() {
        #ifdef IPE_DEBUG
                printf("%s: entry\n", __FUNCTION__);
        #endif
        int i;

        ipe_nlmsg_t msgs = {
                .value   = g_arg.value,
        };

        for (i = 0; i < IPE_DEV_COUNT; ++i) {
                msgs.ifindex[i] = g_arg.ifindex[i];     
                msgs.nsfd[i]    = g_arg.net[i] ? get_netns_fd(g_arg.net[i]) : -1;
        }

        strcpy(msgs.ifname, g_arg.ifname);

        if (!strcmp(g_arg.ctype, "id"))
                msgs.command = IPE_SET_VID;
        else if (!strcmp(g_arg.ctype, "eth"))
                msgs.command = IPE_SET_ETH;
        else if (!strcmp(g_arg.ctype, "name"))
                msgs.command = IPE_SET_NAME;
        else if (!strcmp(g_arg.ctype, "prev"))
                msgs.command = IPE_SET_PARENT;

        #ifdef IPE_DEBUG
                else if (!strcmp(g_arg.ctype, "parent"))
                        msgs.command = IPE_PRINT_ADDR;
                else if (!strcmp(g_arg.ctype, "list"))
                        msgs.command = IPE_LIST;

                printf("%s: memcpy %lu to %s\n", 
                                __FUNCTION__, sizeof(msgs), (char *)nlh);
        #endif

        printf("ok\n");
        print_msg(&msgs);
        memcpy(NLMSG_DATA(nlh), &msgs, sizeof(msgs));

        #ifdef IPE_DEBUG
                printf("%s: ret\n", __FUNCTION__);
        #endif
}



static void sending(struct msghdr *msgh) {
        #ifdef IPE_DEBUG
                printf("%s: entry\n", __FUNCTION__);
        #endif

        struct iovec iov;
        nlh = (struct nlmsghdr *)malloc(NLMSG_SPACE(MAX_PAYLOAD));
        memset(nlh, 0, NLMSG_SPACE(MAX_PAYLOAD));

        nlh->nlmsg_len   = NLMSG_SPACE(MAX_PAYLOAD);
        nlh->nlmsg_pid   = getpid();
        nlh->nlmsg_flags = 0;

        create_msg();

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
        printf("Usage: ipe dev IFINDEX [ netns NETNS ] id   [ VID ]\n");
        printf("                                       eth  [ ETH_TYPE ]\n");
        printf("                                       name [ IFNAME ]\n");
        printf("                                       dst IFINDEX [ dstns NETNS ] prev\n");
#ifdef IPE_DEBUG
        printf("                                       parent\n");
        printf("           list\n");
#endif
        printf("where ETH_TYPE := { 33024 for 0x8100 aka 802.1Q          |\n");
        printf("                    34984 for 0x88A8 aka 802.1ad         }\n");
        /* TODO: need support into kernelspace */
#if 0
        printf("                    37120 for 0x9100 aka deprecated QinQ |\n");
        printf("                    37376 for 0x9200 aka deprecated QinQ }\n");
#endif
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
                                g_arg.ifindex[IPE_SRC] = atoi(*argv);
                                #ifdef IPE_DEBUG
                                        printf("%s: get index %d\n", 
                                                 __FUNCTION__, g_arg.ifindex[IPE_SRC]);
                                #endif
                        } else {
                                goto usage_ret;
                        }
                } else if (matches("dst")) {
                        if (CHECK_ARGS(args)) {
                                NEXT_ARG(args, argv);
                                g_arg.ifindex[IPE_DST] = atoi(*argv);
                                #ifdef IPE_DEBUG
                                        printf("%s: get index %d\n", 
                                                 __FUNCTION__, g_arg.ifindex[IPE_DST]);
                                #endif
                        } else {
                                goto usage_ret;
                        }
                } else if (matches("netns")) {
                        if (CHECK_ARGS(args)) {
                                NEXT_ARG(args, argv);
                                g_arg.net[IPE_SRC] = *argv;
                                #ifdef IPE_DEBUG
                                        printf("%s: get net %s\n", 
                                                 __FUNCTION__, g_arg.net[IPE_SRC]);
                                #endif
                        } else {
                                goto usage_ret;
                        }
                } else if (matches("dstns")) {
                        if (CHECK_ARGS(args)) {
                                NEXT_ARG(args, argv);
                                g_arg.net[IPE_DST] = *argv;
                                #ifdef IPE_DEBUG
                                        printf("%s: get net %s\n", 
                                                 __FUNCTION__, g_arg.net[IPE_DST]);
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
                                        printf("%s: get command set vid %d\n", 
                                                 __FUNCTION__, g_arg.value);
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
                                        printf("%s: get command set eth_type %d\n", 
                                                 __FUNCTION__, g_arg.value);
                                #endif
                                goto ret_ok;
                        } else {
                                goto usage_ret;
                        }
                } else if (matches("name")) {
                        g_arg.ctype = *argv;
                        if (CHECK_ARGS(args)) {
                                NEXT_ARG(args, argv);
                                strcpy(g_arg.ifname, *argv);
                                #ifdef IPE_DEBUG
                                        printf("%s: get command set ifname %s\n", 
                                                 __FUNCTION__, g_arg.ifname);
                                #endif
                                goto ret_ok;
                        } else {
                                goto usage_ret;
                        }
                } else if (matches("prev")) {
                        g_arg.ctype = *argv;
                        goto ret_ok;
                } else if (matches("parent")) {
                        g_arg.ctype = *argv;
                        goto ret_ok;
                } else if (matches("list")) {
                        g_arg.ctype = *argv;
                        goto ret_ok;
                } else {
                        printf("%s: arg \"%s\" not matches\n", 
                                                        __FUNCTION__, *argv);
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
        ipe_reply_t reply;
        int res = parse_arg(args, argv);
        if (res) {
                printf("res %d\n", res);
                return res;
        }


        sock_fd = socket(PF_NETLINK, SOCK_RAW, NETLINK_USER);

        if (sock_fd < 0) {
                printf("Bad socket: %d\n", sock_fd);
                return IPE_BAD_SOC;
        }

        prepare();
        sending(&msg);

        recvmsg(sock_fd, &msg, 0);
        memcpy(&reply, NLMSG_DATA(nlh), sizeof(ipe_reply_t));

#ifdef IPE_DEBUG
        printf("%s", reply.report);
        printf("return code: %d\n", reply.retcode);
        printf("%s: result is %s\n", __FUNCTION__,
                       reply.retcode < IPE_ERR_COUNT ? errors[reply.retcode].name : "unknown");
#endif

        free(nlh);
        close(sock_fd);

        return reply.retcode;
}
