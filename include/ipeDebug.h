#ifndef __IPE_DEBUG_H
#define __IPE_DEBUG_H   1

#ifdef IPE_DEBUG
        int show_vlan_info (const nlmsg_t *msg);
        void printk_msg    (const nlmsg_t *msg);
#endif


#endif // __IPE_DEBUG_H
