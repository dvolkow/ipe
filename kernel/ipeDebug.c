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

/* Some temporary utils than will be used for sample: */
#include <linux/netdevice.h>
#include "../include/ipe.h"
#include "../include/vlan.h"

typedef struct net_device ndev_t;

extern ndev_t *get_dev(const ipe_nlmsg_t *msg);

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

int show_vlan_info(const ipe_nlmsg_t *msg) {
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

void printk_msg(const ipe_nlmsg_t *msg) {
        printk(KERN_DEBUG "%s: value %d\n", __FUNCTION__, msg->value);
        printk(KERN_DEBUG "%s: ifindex %d\n", __FUNCTION__, msg->ifindex[IPE_SRC]);
        printk(KERN_DEBUG "%s: nsfd %d\n", __FUNCTION__, msg->nsfd[IPE_SRC]);
        printk(KERN_DEBUG "%s: command %c\n", __FUNCTION__, msg->command);
}
#endif // IPE_DEBUG


/*
 * Another functions than used for debug, but they still useless now:
 */
#ifdef IPE_EXT_DEBUG
static int printk_addr_by_idx(const ipe_nlmsg_t *msg) {
        printk(KERN_DEBUG "%s has been called\n", __FUNCTION__);
        ndev_t *dev = dev_get_by_index(&init_net, msg->ifindex[IPE_SRC]);
        if (!dev) {
                printk(KERN_DEBUG "%s: failure of search by index %d\n", 
                                                __FUNCTION__, msg->ifindex[IPE_SRC]);
                return IPE_BAD_IF_IDX;
        }

        printk(KERN_DEBUG "%s: device by index %d: %p\n", 
                                           __FUNCTION__, msg->ifindex[IPE_SRC], dev);
        return IPE_OK;
}



static int print_list_ndev(const ipe_nlmsg_t *msg) {
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



static int net_namespace_list_print(const ipe_nlmsg_t *msg) {
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



static int test_find(const ipe_nlmsg_t *msg) {
        ndev_t *dev = find_device(msg);
        if (!dev) {
                printk(KERN_ERR "%s: fail search dev %d!\n", 
                                        __FUNCTION__, msg->ifindex[IPE_SRC]);
                return IPE_BAD_ARG;
        }

        printk(KERN_DEBUG "%s: find dev %p, name %s, ifindex %d\n", 
                                   __FUNCTION__, dev, dev->name, dev->ifindex);
        return IPE_OK;
}


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

        printk(KERN_DEBUG "%s: entry, dev %p\n", __FUNCTION__, dev);
        printk(KERN_DEBUG "%s: vlan_id %u\n", __FUNCTION__, vlan_id);

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

        printk(KERN_DEBUG "%s: entry, dev %p\n", __FUNCTION__, dev);
        printk(KERN_DEBUG "%s: vlan_id %u\n", __FUNCTION__, vlan_id);

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


static int replace_vid_on_dev(const ndev_t *dev, const ipe_nlmsg_t *msg) {
        struct vlan_dev_priv *vlan = vlan_dev_priv(dev);
        int old_vlan_id = vlan->vlan_id;

        printk(KERN_DEBUG "%s: current vid #%d\n", __FUNCTION__, old_vlan_id);

        vlan->vlan_id = msg->value;

        printk(KERN_DEBUG "%s: new vid #%d\n", __FUNCTION__, vlan->vlan_id);

        return IPE_OK;
}

static int old_set_vid(const ipe_nlmsg_t *msg) {

        printk(KERN_DEBUG "%s has been called\n", __FUNCTION__);

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

#endif // IPE_EXT_DEBUG
