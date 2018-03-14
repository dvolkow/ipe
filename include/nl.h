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
#ifndef __IPE_IPE_H
#define __IPE_IPE_H 1

#define IPE_ARGS_COUNT           3

#define NETLINK_USER            31
#define MAX_PATH_LEN            256
#define NETNS_RUN_DIR           "/var/run/netns"

typedef struct nl_message {
        int     ifindex;     
        int     nsfd;
        int     value;
        char    command;
} nlmsg_t;



typedef struct nl_handler_dict {
        const char command;
        int (*handler)(const nlmsg_t *msg);
} hdict_t;


/* Functions that extend usage netlink */
enum {
        IPE_SET_VID    = 0,
        IPE_SET_ETH    = 1, 
        /* debug: */
        IPE_PRINT_ADDR = 2,
        IPE_PRINT_NS   = 3,
};


const int IPE_GLOBAL_NS  = -1;

#ifdef IPE_DEBUG
        #define IPE_COMMAND_COUNT        4
#else
        #define IPE_COMMAND_COUNT        2
#endif



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
};

#endif // __IPE_IPE_H
