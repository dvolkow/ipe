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

#ifndef __IPE_IPE_H
#define __IPE_IPE_H 1

#define IPE_ARGS_COUNT           3

#define NETLINK_USER            31
#define MAX_PATH_LEN            256
#define NETNS_RUN_DIR           "/var/run/netns"


/* This structure for Netlink message into ipe */
typedef struct nl_message {
        int     ifindex;     
        int     nsfd;
        int     value;
        char    command;
} nlmsg_t;


/* For map handlers */
typedef struct {
        const char command;
        int (*handler)(const nlmsg_t *msg);
} hdict_t;


/* Functions that extend usage netlink */
enum {
        IPE_SET_VID    = 0,
        IPE_SET_ETH    = 1, 
        /* debug: */
        IPE_PRINT_ADDR = 2,
};


#ifdef IPE_DEBUG
        #define IPE_COMMAND_COUNT        3 // one debug func extended
#else
        #define IPE_COMMAND_COUNT        2
#endif


#ifdef IPE_DEBUG
        #define LOG_RTNL_LOCK()    printk(KERN_ERR "%s: rtnl_lock (%d)\n", \
                                                __FUNCTION__, __LINE__)
        #define LOG_RTNL_UNLOCK()  printk(KERN_ERR "%s: rtnl_unlock (%d)\n", \
                                                __FUNCTION__, __LINE__)
#endif


/* Invalid descriptor for case global netns */
const int IPE_GLOBAL_NS  = -1;


/* Error's code: */
enum {
        IPE_OK             = 0,
        IPE_BAD_ARG        = 1,
        IPE_BAD_VID        = 2,
        IPE_BAD_IF_IDX     = 3,
        IPE_UNKNOWN_ACTION = 4,
        IPE_FAIL_NS        = 5,
        IPE_FAIL_CR_SOC    = 6,
        IPE_FEW_ARG        = 7,
        IPE_NULLPTR        = 8,
};

#endif // __IPE_IPE_H
