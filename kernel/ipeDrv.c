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

// Linix Includes:
#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/netlink.h>
#include <linux/netdevice.h>

#include <linux/if.h> // IFNAMSIZ
#include <linux/if_vlan.h> 

#include <linux/err.h>
#include <net/sock.h>
#include <net/net_namespace.h>

// Local Includes:
#include "../include/ipe.h"
#include "../include/vlan.h"
#include "../include/ipeDebug.h"

#define IPE_MAX_COMMAND_LEN      IFNAMSIZ

typedef struct net_device ndev_t;


static int set_vid(const nlmsg_t *msg);
static int set_eth(const nlmsg_t *msg);


static struct sock *nl_sk = NULL;

static hdict_t dict[IPE_COMMAND_COUNT] = {
        { IPE_SET_VID, set_vid },
        { IPE_SET_ETH, set_eth },
        /* debug: */
        #ifdef IPE_DEBUG
                { IPE_PRINT_ADDR, show_vlan_info },
        #endif
};





/*
 * Fetch needed ops
 */
static int action(const nlmsg_t *msg) {
        int i;
        for (i = 0; i < IPE_COMMAND_COUNT; ++i) {
                if (dict[i].command == msg->command) {
                        dict[i].handler(msg);

                        return IPE_OK;
                }
        }

        return IPE_UNKNOWN_ACTION;
}




/*
 * Find device into custom namespace
 */
static ndev_t *find_device(const nlmsg_t *msg) {
        struct net    *net; // namespace
        ndev_t *dev;

        rtnl_lock();
        #ifdef IPE_DEBUG
                LOG_RTNL_LOCK();
        #endif

        net = get_net_ns_by_fd(msg->nsfd);
        if (IS_ERR(net))
                goto unlock_fail;

        for_each_netdev(net, dev) {
                if (dev->ifindex == msg->ifindex) {
                        dev_hold(dev);
                        #ifdef IPE_DEBUG
                                LOG_RTNL_UNLOCK();
                        #endif
                        rtnl_unlock();

                        return dev;
                }
        }

unlock_fail:
        #ifdef IPE_DEBUG
                LOG_RTNL_UNLOCK();
        #endif
        rtnl_unlock();
        printk(KERN_ERR "%s: fail!\n", __FUNCTION__);

        return NULL;
}




/*
 * Fetch find case in dependency of type namespace (defaulf/custom)
 */
ndev_t *get_dev(const nlmsg_t *msg) {
        if (msg->nsfd == IPE_GLOBAL_NS) {
                #ifdef IPE_DEBUG
                        printk(KERN_DEBUG "%s: global namespace search...\n",
                                                        __FUNCTION__);
                #endif

                return dev_get_by_index(&init_net, msg->ifindex);
        } else {
                #ifdef IPE_DEBUG
                        printk(KERN_DEBUG "%s: %d namespace search...\n", 
                                             __FUNCTION__, msg->nsfd);
                #endif

                return find_device(msg);
        }

        printk(KERN_ERR "%s: failure of search by index %d\n", 
                                          __FUNCTION__, msg->ifindex);

        return NULL;
}






static int set_vid(const nlmsg_t *msg) {
        #ifdef IPE_DEBUG
                printk(KERN_DEBUG "%s has been called\n", __FUNCTION__);
        #endif

        ndev_t *vlan_dev = get_dev(msg);
        if (!vlan_dev) {
                printk(KERN_WARNING "%s: fail search device #%d info net_namespace [%d]\n",
                                                __FUNCTION__, msg->ifindex, msg->nsfd);
                goto set_fail;
        }

        if (msg->value > VLAN_N_VID || msg->value < 0) {
                printk(KERN_WARNING "%s: try set bad VID %d\n", __FUNCTION__, msg->value);
                goto set_fail;
        }

        if (!is_vlan_dev(vlan_dev)) {
                printk(KERN_WARNING "%s: device %s is not vlan type!\n", 
                                                __FUNCTION__, vlan_dev->name);
                goto set_fail_put;
        }

        struct vlan_dev_priv *vlan = vlan_dev_priv(vlan_dev);
        BUG_ON(!vlan);

        rtnl_lock();
        #ifdef IPE_DEBUG
                LOG_RTNL_LOCK();
        #endif

        int old_vlan_id  = vlan->vlan_id;
        ndev_t *real_dev = vlan->real_dev;

        if (vlan_vid_add(real_dev, vlan->vlan_proto, msg->value))
                goto set_rtnl_unlock;

        #ifdef IPE_DEBUG
                printk(KERN_DEBUG "%s: current vid #%d\n", 
                                        __FUNCTION__, old_vlan_id);
        #endif
        vlan->vlan_id = msg->value;
        #ifdef IPE_DEBUG
                printk(KERN_DEBUG "%s: new vid #%d\n", 
                                        __FUNCTION__, vlan->vlan_id);
                printk(KERN_DEBUG "%s: find parent: %s by addr %p\n", 
                                        __FUNCTION__, real_dev->name, real_dev);
        #endif

        struct vlan_info *vlan_info = rcu_dereference_rtnl(real_dev->vlan_info);
        /* vlan_info should be there now. vlan_vid_add took care of it */
        BUG_ON(!vlan_info);


        struct vlan_group *grp = &vlan_info->grp;
        if (vlan_group_prealloc_vid(grp, vlan->vlan_proto, vlan->vlan_id) < 0) {
                printk(KERN_ERR "%s: fail alloc memory for vlan group %p!\n", 
                                                           __FUNCTION__, grp);
                goto set_rtnl_unlock;
        }
        #ifdef IPE_DEBUG
                LOG_RTNL_UNLOCK();
        #endif
        rtnl_unlock();

        vlan_group_del_device(&vlan_info->grp,
                               vlan->vlan_proto, old_vlan_id);

        vlan_group_set_device(&vlan_info->grp, vlan->vlan_proto, 
                                                       vlan->vlan_id, vlan_dev);
        // here will be: grp->nr_vlan_devs++;
        dev_put(vlan_dev);

        return IPE_OK;

set_rtnl_unlock:
        #ifdef IPE_DEBUG
                LOG_RTNL_UNLOCK();
        #endif
        rtnl_unlock();
        vlan_vid_del(real_dev, vlan->vlan_proto, vlan->vlan_id);

set_fail_put:
        dev_put(vlan_dev);

set_fail:
        printk(KERN_ERR "%s: fail\n", __FUNCTION__);

        return IPE_DEFAULT_FAIL;
}





/*
 * Tmp impl
 */
static int set_eth(const nlmsg_t *msg) {
        #ifdef IPE_DEBUG
                printk(KERN_DEBUG "%s has been called\n", __FUNCTION__);
        #endif

        ndev_t *vlan_dev = get_dev(msg);
        if (!vlan_dev) {
                printk(KERN_WARNING "%s: fail search device #%d into net_namespace [%d]\n",
                                                __FUNCTION__, msg->ifindex, msg->nsfd);
                goto set_fail;
        }

        if (vlan_proto_idx(htons(msg->value)) == IPE_BAD_VLAN_PROTO) {
                printk(KERN_WARNING "%s: try set bad VLAN ethertype: %x\n",
                                                __FUNCTION__, htons(msg->value));
                goto set_fail;
        }

        if (!is_vlan_dev(vlan_dev)) {
                printk(KERN_ERR "%s: device %s is not vlan type!\n", 
                                        __FUNCTION__, vlan_dev->name);
                goto set_fail_put;
        }

        struct vlan_dev_priv *vlan = vlan_dev_priv(vlan_dev);
        BUG_ON(!vlan);

        rtnl_lock();
        #ifdef IPE_DEBUG
                LOG_RTNL_LOCK();
        #endif

        __be16 old_vlan_proto = vlan->vlan_proto;
        ndev_t *real_dev = vlan->real_dev;

        if (vlan_vid_add(real_dev, htons(msg->value), vlan->vlan_id))
                goto set_rtnl_unlock;

        #ifdef IPE_DEBUG
                printk(KERN_DEBUG "%s: current proto #%x\n", 
                                        __FUNCTION__, old_vlan_proto);
        #endif
        vlan->vlan_proto = htons(msg->value);
        #ifdef IPE_DEBUG
                printk(KERN_DEBUG "%s: new proto #%x\n", 
                                        __FUNCTION__, vlan->vlan_proto);
                printk(KERN_DEBUG "%s: find parent: %s by addr %p\n", 
                                        __FUNCTION__, real_dev->name, real_dev);
        #endif

        struct vlan_info *vlan_info = rcu_dereference_rtnl(real_dev->vlan_info);
        /* vlan_info should be there now. vlan_vid_add took care of it */
        BUG_ON(!vlan_info);
        

        struct vlan_group *grp = &vlan_info->grp;
        if (vlan_group_prealloc_vid(grp, vlan->vlan_proto, vlan->vlan_id) < 0) {
                printk(KERN_ERR "%s: fail alloc memory for vlan group %p!\n", 
                                                           __FUNCTION__, grp);
                goto set_rtnl_unlock;
        }
        #ifdef IPE_DEBUG
                LOG_RTNL_UNLOCK();
        #endif
        rtnl_unlock();

        vlan_group_del_device(&vlan_info->grp, old_vlan_proto, vlan->vlan_id);
        vlan_group_set_device(&vlan_info->grp, vlan->vlan_proto, 
                                                       vlan->vlan_id, vlan_dev);

        dev_put(vlan_dev);
        return IPE_OK;

set_rtnl_unlock:
        #ifdef IPE_DEBUG
                LOG_RTNL_UNLOCK();
        #endif
        rtnl_unlock();
        vlan_vid_del(real_dev, vlan->vlan_proto, vlan->vlan_id);

set_fail_put:
        dev_put(vlan_dev);


set_fail:
        printk(KERN_ERR "%s: fail!\n", __FUNCTION__);

        return IPE_DEFAULT_FAIL;
}




/*
 * Call hadler for required ops
 */
static void vlan_ext_handler(struct sk_buff *skb) {
        struct  nlmsghdr *nlh;
        nlmsg_t *msg;

        nlh = (struct nlmsghdr*)skb->data;
        msg = (nlmsg_t *)nlmsg_data(nlh);

        #ifdef IPE_DEBUG
                printk_msg(msg);
        #endif

        if (action(msg)) 
                printk(KERN_WARNING "%s: unknown action\n", __FUNCTION__);
}



static int __init hello_init(void) {

        //This is for 3.6 kernels and above.
        struct netlink_kernel_cfg cfg = {
                .input = vlan_ext_handler,
        };

        #ifdef IPE_DEBUG
                printk(KERN_INFO "%s: init module %s\n", 
                                               __FUNCTION__, THIS_MODULE->name);
        #endif
        nl_sk = netlink_kernel_create(&init_net, NETLINK_USER, &cfg);
        if (!nl_sk) {
                printk(KERN_ALERT "%s: error creating socket.\n", __FUNCTION__);

                return IPE_FAIL_CR_SOC;
        }

        return IPE_OK;
}




static void __exit hello_exit(void) {
        #ifdef IPE_DEBUG
                printk(KERN_INFO "%s: exiting %s\n", 
                                               __FUNCTION__, THIS_MODULE->name);
        #endif
        netlink_kernel_release(nl_sk);
}


module_init(hello_init); 
module_exit(hello_exit);

MODULE_LICENSE( "GPL" );
MODULE_VERSION( "0.1" );
MODULE_AUTHOR( "Daniel Wolkow <volkov12@rambler.ru>" );
MODULE_DESCRIPTION( "Netlink extending driver for ipe" );

