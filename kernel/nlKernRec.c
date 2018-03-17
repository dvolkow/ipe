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
#include "../include/nl.h"
#include "../include/vlan.h"

#define IPE_MAX_COMMAND_LEN      IFNAMSIZ

typedef struct net_device ndev_t;


static int set_vid(const nlmsg_t *msg);
static int set_eth(const nlmsg_t *msg);

#ifdef IPE_DEBUG
static int show_vlan_info(const nlmsg_t *msg);
#endif

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
static ndev_t *get_dev(const nlmsg_t *msg) {
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
        if (!vlan_dev)
                goto set_fail;

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
        int old_vlan_id  = vlan->vlan_id;
        ndev_t *real_dev = vlan->real_dev;

        if (vlan_vid_add(real_dev, vlan->vlan_proto, msg->value))
                goto set_rtnl_unlock;

#ifdef IPE_DEBUG
        printk(KERN_DEBUG "%s: current vid #%d\n", __FUNCTION__, old_vlan_id);
#endif
        vlan->vlan_id = msg->value;
#ifdef IPE_DEBUG
        printk(KERN_DEBUG "%s: new vid #%d\n", __FUNCTION__, vlan->vlan_id);
#endif
#ifdef IPE_DEBUG
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
        return IPE_NULLPTR;
}





/*
 * Tmp impl
 */
static int set_eth(const nlmsg_t *msg) {
#ifdef IPE_DEBUG
        printk(KERN_DEBUG "%s has been called\n", __FUNCTION__);
#endif

        ndev_t *vlan_dev = get_dev(msg);
        if (!vlan_dev)
                goto set_fail;

        if (!is_vlan_dev(vlan_dev)) {
                printk(KERN_ERR "%s: device %s is not vlan type!\n", 
                                        __FUNCTION__, vlan_dev->name);
                goto set_fail;
        }

        struct vlan_dev_priv *vlan = vlan_dev_priv(vlan_dev);
        if (!vlan) 
                goto set_fail;

        __be16 old_vlan_proto = vlan->vlan_proto;
#ifdef IPE_DEBUG
        printk(KERN_DEBUG "%s: current proto #%x\n", 
                                        __FUNCTION__, old_vlan_proto);
#endif
        vlan->vlan_proto = htons(msg->value);
#ifdef IPE_DEBUG
        printk(KERN_DEBUG "%s: new proto #%x\n", 
                                        __FUNCTION__, vlan->vlan_proto);
#endif
        ndev_t *real_dev = vlan->real_dev;

        rtnl_lock();
#ifdef IPE_DEBUG
        LOG_RTNL_LOCK();
#endif
        struct vlan_info *vlan_info = rcu_dereference_rtnl(real_dev->vlan_info);
#ifdef IPE_DEBUG
        LOG_RTNL_UNLOCK();
#endif
        rtnl_unlock();

        if (!vlan_info) 
                goto set_fail;

        vlan_group_del_device(&vlan_info->grp, old_vlan_proto, vlan->vlan_id);

        vlan_group_set_device(&vlan_info->grp, vlan->vlan_proto, 
                                                        vlan->vlan_id, vlan_dev);

        return IPE_OK;

set_fail:
        printk(KERN_ERR "%s: fail!\n", __FUNCTION__);
        return IPE_BAD_IF_IDX;
}


/* 
 * TODO: cut into debug module 
 * --------DEBUG UTILS:--------
 */
#ifdef IPE_DEBUG
static void printk_vlan_group(const struct vlan_group *vlan_group) {
        printk(KERN_DEBUG "%s: nr_vlan_devs: %u\n", 
                           __FUNCTION__, vlan_group->nr_vlan_devs);
        u16 i, j;

        for (i = 0; i < VLAN_PROTO_NUM; ++i) {
                for (j = 0; j < VLAN_GROUP_ARRAY_SPLIT_PARTS; ++j) {
                        ndev_t **dev = vlan_group->vlan_devices_arrays[i][j];
                        printk(KERN_DEBUG "%s: vlan_devices_arrays[%u][%u]: %p %s\n", 
                                        __FUNCTION__, i, j,  dev,
                                        dev ? (*dev)->name : "null") ;
                }
        }
}

static void printk_vlan_info(const struct vlan_info *vlan_info) {
        printk(KERN_DEBUG "%s: net_device: %p\n", 
                          __FUNCTION__, vlan_info->real_dev);
        printk(KERN_DEBUG "%s: nr_vids: %u\n", 
                          __FUNCTION__, vlan_info->nr_vids);
        printk(KERN_DEBUG "%s: grp: %p\n", 
                          __FUNCTION__, &vlan_info->grp);
        printk_vlan_group(&vlan_info->grp);
}

static int show_vlan_info(const nlmsg_t *msg) {
        ndev_t *real_dev = NULL;

        printk(KERN_DEBUG "%s has been called\n", __FUNCTION__);

        ndev_t *vlan_dev = get_dev(msg);

        struct vlan_dev_priv *vlan = vlan_dev_priv(vlan_dev);
        real_dev = vlan->real_dev;

        printk(KERN_DEBUG "%s: find parent: %s by addr %p\n", 
                                __FUNCTION__, real_dev->name, real_dev);

        rtnl_lock();
        struct vlan_info *vlan_info = rcu_dereference_rtnl(real_dev->vlan_info);
        rtnl_unlock();

        printk_vlan_info(vlan_info);

        return IPE_OK;
}

static void printk_msg(const nlmsg_t *msg) {
        printk(KERN_DEBUG "%s: value %d\n", __FUNCTION__, msg->value);
        printk(KERN_DEBUG "%s: ifindex %d\n", __FUNCTION__, msg->ifindex);
        printk(KERN_DEBUG "%s: nsfd %d\n", __FUNCTION__, msg->nsfd);
        printk(KERN_DEBUG "%s: command %c\n", __FUNCTION__, msg->command);
}
#endif



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
        printk(KERN_INFO "%s: init module %s\n", __FUNCTION__, THIS_MODULE->name);
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
        printk(KERN_INFO "%s: exiting nl module...\n", __FUNCTION__);
#endif
        netlink_kernel_release(nl_sk);
}


module_init(hello_init); 
module_exit(hello_exit);

MODULE_LICENSE("GPL");



/* Some temporary utils than will be used for sample: */
#if 0
static int printk_addr_by_idx(const nlmsg_t *msg) {
        printk(KERN_DEBUG "%s has been called\n", __FUNCTION__);
        ndev_t *dev = dev_get_by_index(&init_net, msg->ifindex);
        if (!dev) {
                printk(KERN_DEBUG "%s: failure of search by index %d\n", 
                                                __FUNCTION__, msg->ifindex);
                return IPE_BAD_IF_IDX;
        }

        printk(KERN_DEBUG "%s: device by index %d: %p\n", 
                                                __FUNCTION__, msg->ifindex, dev);
        return IPE_OK;
}



static int print_list_ndev(const nlmsg_t *msg) {
        printk(KERN_DEBUG "%s has been called\n", __FUNCTION__);
        
        struct net_device *dev;
        read_lock(&dev_base_lock);
        dev = first_net_device(&init_net);
        while (dev) {
                printk(KERN_INFO "%s: found [%s]:%p\n", 
                                                __FUNCTION__, dev->name, dev);
                dev = next_net_device(dev);
        }
        read_unlock(&dev_base_lock);
        return IPE_OK;
}



#ifdef IPE_DEBUG
static int net_namespace_list_print(const nlmsg_t *msg) {
        struct net *net;
        struct net_device *dev;

        printk(KERN_DEBUG "%s has been called\n", __FUNCTION__);
        rtnl_lock();

        printk(KERN_DEBUG "%s: locked\n", __FUNCTION__);
        for_each_net(net) {
                for_each_netdev(net, dev) {
                        printk(KERN_DEBUG "%s: dev %s : i = %d, %p, ns %p\n", 
                                __FUNCTION__, dev->name, dev->ifindex, dev, net);
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

        printk(KERN_DEBUG "%s: find dev %p, name %s, ifindex %d\n", 
                                                __FUNCTION__, dev, dev->name, dev->ifindex);
        return IPE_OK;
}
#endif


/* Modified version of unregister_vlan_dev from kernel net/8021q/vlan.c
 *
 * temporary solution
 * @dev: vlan interface that will be changed
 */
static int pseudo_unregister_vlan_dev(ndev_t *dev) {
        struct vlan_dev_priv *vlan = vlan_dev_priv(dev);
        ndev_t *real_dev           = vlan->real_dev;
        struct vlan_info  *vlan_info;
        struct vlan_group *grp;
        u16 vlan_id = vlan->vlan_id;

#ifdef IPE_DEBUG
        printk(KERN_DEBUG "%s: entry, dev %p\n", __FUNCTION__, dev);
        printk(KERN_DEBUG "%s: vlan_id %u\n", __FUNCTION__, vlan_id);
#endif
        ASSERT_RTNL();

        vlan_info = rtnl_dereference(real_dev->vlan_info);
        BUG_ON(!vlan_info);

        grp = &vlan_info->grp;

        vlan_group_del_device(grp, vlan->vlan_proto, vlan_id);

        return IPE_OK;
}


static int pseudo_register_vlan_dev(ndev_t *dev) {
        struct vlan_dev_priv *vlan = vlan_dev_priv(dev);
        ndev_t *real_dev           = vlan->real_dev;
        u16 vlan_id                = vlan->vlan_id;
        struct vlan_info  *vlan_info;
        struct vlan_group *grp;
        int err;

#ifdef IPE_DEBUG
        printk(KERN_DEBUG "%s: entry, dev %p\n", __FUNCTION__, dev);
        printk(KERN_DEBUG "%s: vlan_id %u\n", __FUNCTION__, vlan_id);
#endif

        err = vlan_vid_add(real_dev, vlan->vlan_proto, vlan_id);
        if (err)
                return err;

        ASSERT_RTNL();

        vlan_info = rtnl_dereference(real_dev->vlan_info);
        /* vlan_info should be there now. vlan_vid_add took care of it */
        BUG_ON(!vlan_info);

        grp = &vlan_info->grp;

        err = vlan_group_prealloc_vid(grp, vlan->vlan_proto, vlan_id);	
        if (err < 0)
                goto fail_reg;

        return IPE_OK;

fail_reg:
        printk(KERN_ERR "%s: fail\n", __FUNCTION__);
        vlan_vid_del(real_dev, vlan->vlan_proto, vlan_id);
        return err;
}


static int replace_vid_on_dev(const ndev_t *dev, const nlmsg_t *msg) {
        struct vlan_dev_priv *vlan = vlan_dev_priv(dev);
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
        return IPE_OK;
}

static int old_set_vid(const nlmsg_t *msg) {
#ifdef IPE_DEBUG
        printk(KERN_DEBUG "%s has been called\n", __FUNCTION__);
#endif


        ndev_t *vlan_dev = get_dev(msg);
        if (!vlan_dev)
                goto set_fail;

        if (!is_vlan_dev(vlan_dev)) {
                printk(KERN_ERR "%s: device %s is not vlan type!\n", 
                                        __FUNCTION__, vlan_dev->name);
                goto set_fail_put;
        }

        rtnl_lock();
                pseudo_unregister_vlan_dev(vlan_dev);
                replace_vid_on_dev(vlan_dev, msg);
                pseudo_register_vlan_dev(vlan_dev);
        rtnl_unlock();

        dev_put(vlan_dev);
        return IPE_OK;

set_fail_put:
        dev_put(vlan_dev);
set_fail:
        printk(KERN_ERR "%s: fail\n", __FUNCTION__);
        return IPE_NULLPTR;
}
#endif
