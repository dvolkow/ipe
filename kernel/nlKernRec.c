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
#include "../include/nl.h"
#include "../include/vlan.h"

#define IPE_MAX_COMMAND_LEN      IFNAMSIZ

typedef struct net_device ndev_t;


static int set_vid(const nlmsg_t *msg);
static int set_eth(const nlmsg_t *msg);

#ifdef IPE_DEBUG
static int net_namespace_list_print(const nlmsg_t *msg);
static int test_find(const nlmsg_t *msg);
static int show_vlan_info(const nlmsg_t *msg);
#endif

static struct sock *nl_sk = NULL;

static hdict_t dict[IPE_COMMAND_COUNT] = {
        { IPE_SET_ETH, set_eth },
        { IPE_SET_VID, set_vid },
        /* debug: */
#ifdef IPE_DEBUG
        { IPE_PRINT_ADDR, show_vlan_info },
        { IPE_PRINT_NS, net_namespace_list_print }
#endif
};



#ifdef IPE_DEBUG
static void printk_msg(const nlmsg_t *msg) {
        printk(KERN_DEBUG "%s: value %d\n", __FUNCTION__, msg->value);
        printk(KERN_DEBUG "%s: ifindex %d\n", __FUNCTION__, msg->ifindex);
        printk(KERN_DEBUG "%s: nsfd %d\n", __FUNCTION__, msg->nsfd);
        printk(KERN_DEBUG "%s: command %c\n", __FUNCTION__, msg->command);
}
#endif



static int action(const nlmsg_t *msg) {
        int i;
#ifdef IPE_DEBUG
        printk(KERN_DEBUG "entering: %s\n", __FUNCTION__);
#endif
        for (i = 0; i < IPE_COMMAND_COUNT; ++i) {
                if (dict[i].command == msg->command) {
                        dict[i].handler(msg);
                        return IPE_OK;
                }
        }
        return IPE_UNKNOWN_ACTION;
}



static ndev_t *find_device(const nlmsg_t *msg) {
        struct net    *net; // namespace
        ndev_t *dev;

#ifdef IPE_DEBUG
        printk(KERN_DEBUG "%s: entry\n", __FUNCTION__);
#endif
        rtnl_lock();
#ifdef IPE_DEBUG
        printk(KERN_DEBUG "%s: rtnl-lock locked\n", __FUNCTION__);
#endif

        net = get_net_ns_by_fd(msg->nsfd);
        if (IS_ERR(net))
                goto unlock_fail;

        for_each_netdev(net, dev) {
                if (dev->ifindex == msg->ifindex) {
                        rtnl_unlock();
                        return dev;
                }
        }

unlock_fail:
        rtnl_unlock();
#ifdef IPE_DEBUG
        printk(KERN_ERR "%s: fail\n", __FUNCTION__);
#endif
        return NULL;
}




static ndev_t *get_dev(const nlmsg_t *msg) {
        if (msg->nsfd == IPE_GLOBAL_NS) {
#ifdef IPE_DEBUG
                printk(KERN_DEBUG "%s: global namespace search...\n", __FUNCTION__);
#endif
                return dev_get_by_index(&init_net, msg->ifindex);
        } else {
#ifdef IPE_DEBUG
                printk(KERN_DEBUG "%s: %d namespace search...\n", __FUNCTION__, msg->nsfd);
#endif
                return find_device(msg);
        }

        printk(KERN_ERR "%s: failure of search by index %d\n", __FUNCTION__, msg->ifindex);
        return NULL;
}




static int set_vid(const nlmsg_t *msg) {
#ifdef IPE_DEBUG
        printk(KERN_DEBUG "%s has been called\n", __FUNCTION__);
#endif


        ndev_t *vlan_dev = get_dev(msg);
        if (!vlan_dev)
                goto set_fail;

        struct vlan_dev_priv *vlan = vlan_dev_priv(vlan_dev);
        if (!vlan) {
                printk(KERN_ERR "%s: failure of search by index %d\n", __FUNCTION__, msg->ifindex);
                return IPE_BAD_IF_IDX;
        }

        int old_vlan_id = vlan->vlan_id;
#ifdef IPE_DEBUG
        printk(KERN_DEBUG "%s: current vid #%d\n", __FUNCTION__, old_vlan_id);
#endif
        vlan->vlan_id = msg->value;
#ifdef IPE_DEBUG
        printk(KERN_DEBUG "%s: new vid #%d\n", __FUNCTION__, vlan->vlan_id);
#endif

#ifdef IPE_DEBUG
        printk(KERN_DEBUG "%s: bottom half...\n", __FUNCTION__);
#endif
        ndev_t *real_dev = vlan->real_dev;
#ifdef IPE_DEBUG
        printk(KERN_DEBUG "%s: find parent: %s by addr %p\n", __FUNCTION__, real_dev->name, real_dev);
#endif
        rtnl_lock();
        struct vlan_info *vlan_info = rcu_dereference_rtnl(real_dev->vlan_info);
        rtnl_unlock();

        if (!vlan_info) 
                goto set_fail;


#ifdef IPE_DEBUG
        printk(KERN_DEBUG "%s: delete old dev\n", __FUNCTION__);
#endif
        vlan_group_del_device(&vlan_info->grp,
                               vlan->vlan_proto, old_vlan_id);
#ifdef IPE_DEBUG
        printk(KERN_DEBUG "%s: set new dev\n", __FUNCTION__);
#endif
        vlan_group_set_device(&vlan_info->grp,
                               vlan->vlan_proto, vlan->vlan_id, vlan_dev);

        return IPE_OK;

set_fail:
        printk(KERN_ERR "%s: fail\n", __FUNCTION__);
        return IPE_NULLPTR;
}




static int set_eth(const nlmsg_t *msg) {
#ifdef IPE_DEBUG
        printk(KERN_DEBUG "%s has been called\n", __FUNCTION__);
#endif

        ndev_t *vlan_dev = get_dev(msg);
        if (!vlan_dev)
                goto set_fail;

        struct vlan_dev_priv *vlan = vlan_dev_priv(vlan_dev);
        if (!vlan) {
set_fail:
                printk(KERN_ERR "%s: failure of search by index %d\n", __FUNCTION__, msg->ifindex);
                return IPE_BAD_IF_IDX;
        }

        __be16 old_vlan_proto = vlan->vlan_proto;
#ifdef IPE_DEBUG
        printk(KERN_DEBUG "%s: current proto #%x\n", __FUNCTION__, old_vlan_proto);
#endif
        vlan->vlan_proto = htons(msg->value);
#ifdef IPE_DEBUG
        printk(KERN_DEBUG "%s: new proto #%x\n", __FUNCTION__, vlan->vlan_proto);
#endif

#ifdef IPE_DEBUG
        printk(KERN_DEBUG "%s: bottom half...\n", __FUNCTION__);
#endif
        ndev_t *real_dev = vlan->real_dev;
#ifdef IPE_DEBUG
        printk(KERN_DEBUG "%s: find parent: %s by addr %p\n", __FUNCTION__, real_dev->name, real_dev);
#endif
        rtnl_lock();
        struct vlan_info *vlan_info = rcu_dereference_rtnl(real_dev->vlan_info);
        rtnl_unlock();

        if (!vlan_info) 
                goto set_fail;

#ifdef IPE_DEBUG
        printk(KERN_DEBUG "%s: delete old dev\n", __FUNCTION__);
#endif
        vlan_group_del_device(&vlan_info->grp,
                               old_vlan_proto, vlan->vlan_id);
#ifdef IPE_DEBUG
        printk(KERN_DEBUG "%s: set new dev\n", __FUNCTION__);
#endif
        vlan_group_set_device(&vlan_info->grp,
                               vlan->vlan_proto, vlan->vlan_id, vlan_dev);

        return IPE_OK;
}




static void vlan_ext_handler(struct sk_buff *skb) {
        struct  nlmsghdr *nlh;
        nlmsg_t *msg;

#ifdef IPE_DEBUG
        printk(KERN_DEBUG "Entering: %s\n", __FUNCTION__);
#endif
        nlh = (struct nlmsghdr*)skb->data;
        msg = (nlmsg_t *)nlmsg_data(nlh);

#ifdef IPE_DEBUG
        printk_msg(msg);
#endif

        if (action(msg)) {
                printk(KERN_ERR "%s: unknown action\n", __FUNCTION__);
        }
}




static int __init hello_init(void) {

        //This is for 3.6 kernels and above.
        struct netlink_kernel_cfg cfg = {
                .input = vlan_ext_handler,
        };

#ifdef IPE_DEBUG
        printk(KERN_DEBUG "Entering: %s\n", __FUNCTION__);
#endif
        nl_sk = netlink_kernel_create(&init_net, NETLINK_USER, &cfg);
        if (!nl_sk) {
                printk(KERN_ALERT "Error creating socket.\n");
                return IPE_FAIL_CR_SOC;
        }

        return IPE_OK;
}




static void __exit hello_exit(void) {
#ifdef IPE_DEBUG
        printk(KERN_DEBUG "Exiting nl module...\n");
#endif
        netlink_kernel_release(nl_sk);
}


module_init(hello_init); 
module_exit(hello_exit);

MODULE_LICENSE("GPL");


#if 0
static int printk_addr_by_idx(const nlmsg_t *msg) {
        printk(KERN_DEBUG "%s has been called\n", __FUNCTION__);
        ndev_t *dev = dev_get_by_index(&init_net, msg->ifindex);
        if (!dev) {
                printk(KERN_DEBUG "%s: failure of search by index %d\n", __FUNCTION__, msg->ifindex);
                return IPE_BAD_IF_IDX;
        }

        printk(KERN_DEBUG "%s: device by index %d: %p\n", __FUNCTION__, msg->ifindex, dev);
        return IPE_OK;
}



static int print_list_ndev(const nlmsg_t *msg) {
        printk(KERN_DEBUG "%s has been called\n", __FUNCTION__);
        
        struct net_device *dev;
        read_lock(&dev_base_lock);
        dev = first_net_device(&init_net);
        while (dev) {
                printk(KERN_INFO "%s: found [%s]:%p\n", __FUNCTION__, dev->name, dev);
                dev = next_net_device(dev);
        }
        read_unlock(&dev_base_lock);
        return IPE_OK;
}

#endif


#ifdef IPE_DEBUG
static int net_namespace_list_print(const nlmsg_t *msg) {
        struct net *net;
        struct net_device *dev;

        printk(KERN_DEBUG "%s has been called\n", __FUNCTION__);
        rtnl_lock();

        printk(KERN_DEBUG "%s: locked\n", __FUNCTION__);
        for_each_net(net) {
                for_each_netdev(net, dev) {
                        printk(KERN_DEBUG "%s: dev %s : i = %d, %p, ns %p\n", __FUNCTION__, dev->name, dev->ifindex, dev, net);
                }
        }
        rtnl_unlock();
        return IPE_OK;
}



static int test_find(const nlmsg_t *msg) {
        ndev_t *dev = find_device(msg);
        if (!dev) {
                printk(KERN_ERR "%s: fail search dev %d!\n", __FUNCTION__, msg->ifindex);
                return IPE_BAD_ARG;
        }

        printk(KERN_DEBUG "%s: find dev %p, name %s, ifindex %d\n", __FUNCTION__, dev, dev->name, dev->ifindex);
        return IPE_OK;
}


static inline void vlan_group_del_device(struct vlan_group *vg,
					 __be16 vlan_proto, u16 vlan_id)
{
        vlan_group_set_device(vg, vlan_proto, vlan_id, NULL);
}

static inline void vlan_group_set_device(struct vlan_group *vg,
					 __be16 vlan_proto, u16 vlan_id,
					 struct net_device *dev)
{
	struct net_device **array;
	if (!vg)
		return;
	array = vg->vlan_devices_arrays[vlan_proto_idx(vlan_proto)]
				       [vlan_id / VLAN_GROUP_ARRAY_PART_LEN];
	array[vlan_id % VLAN_GROUP_ARRAY_PART_LEN] = dev;
}



static int show_vlan_info(const nlmsg_t *msg) {
        ndev_t *real_dev = NULL;

        printk(KERN_DEBUG "%s has been called\n", __FUNCTION__);

        ndev_t *vlan_dev = get_dev(msg);

        struct vlan_dev_priv *vlan = vlan_dev_priv(vlan_dev);
        real_dev = vlan->real_dev;

        printk(KERN_DEBUG "%s: find parent: %s by addr %p\n", __FUNCTION__, real_dev->name, real_dev);

        rtnl_lock();
        struct vlan_info *vlan_info = rcu_dereference_rtnl(real_dev->vlan_info);
        rtnl_unlock();

        printk(KERN_DEBUG "%s: vlan_info: nr_vids: %u\n", __FUNCTION__, vlan_info->nr_vids);

        return IPE_OK;
}

#endif
