/******************************************************************************
*
*                       GNU GENERAL PUBLIC LICENSE
*       Copyright © 2018 Free Software Foundation, Inc. <https://fsf.org/>
*
* Everyone is permitted to copy and distribute verbatim copies of this license 
* document, but changing it is not allowed.
*
*
* Functions:
* 
* 
*
* Author:
*   March, 2018        Daniel Wolkow            
*
* Description:
* Implementation of 
*
******************************************************************************/
#ifndef __NL_NL_H
#define __NL_NL_H 1

#define NL_ARGS_COUNT           3

#define NL_BE_CONV(data)  (*(__be16 *)(&(data)))

#define NETLINK_USER            31
#define MAX_PATH_LEN            256
#define NETNS_RUN_DIR           "/var/run/netns"

typedef struct nl_message {
        int        ifindex;     
        int        nsfd;
        int        value;
        const char command;
} nlmsg_t;



typedef struct nl_handler_dict {
        const char command;
        int (*handler)(const nlmsg_t *msg);
} hdict_t;


/* Functions that extend usage netlink */
enum {
        NL_SET_VID    = 0,
        NL_SET_ETH    = 1, 
        /* debug: */
        NL_PRINT_ADDR = 2,
        NL_PRINT_NS   = 3,
};


#ifdef EMZ_DEBUG
        #define NL_COMMAND_COUNT        4
#else
        #define NL_COMMAND_COUNT        2
#endif



/* Error's code: */
enum {
        NL_OK             = 0,
        NL_BAD_ARG        = 1,
        NL_BAD_VID        = 2,
        NL_BAD_IF_IDX     = 3,
        NL_UNKNOWN_ACTION = 4,
        NL_FAIL_NS        = 5,
        NL_FAIL_CR_SOC    = 6,
};

#endif // __NL_NL_H
