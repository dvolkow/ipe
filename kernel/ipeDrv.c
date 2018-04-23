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


static int set_vid(const ipe_nlmsg_t *msg);
static int set_eth(const ipe_nlmsg_t *msg);
static int set_name(const ipe_nlmsg_t *msg);
static int set_parent(const ipe_nlmsg_t *msg);

extern int print_list_ndev(const ipe_nlmsg_t *msg);

static int check_eth(const ipe_nlmsg_t *msg);
static int check_vid(const ipe_nlmsg_t *msg);
static int check_src(const ipe_nlmsg_t *msg);
static int check_src_vlan(const ipe_nlmsg_t *msg);
static int dummy(const ipe_nlmsg_t *msg);
//static int check_ifname(const ipe_nlmsg_t *msg);
static int check_everybody(const ipe_nlmsg_t *msg);

static inline ndev_t *vlan_find_dev(ndev_t *real_dev,
					       __be16 vlan_proto, u16 vlan_id);

static struct sock *nl_sk = NULL;


static ipe_tool_t commap[IPE_COMMAND_COUNT] = {
        {set_vid, "set_vid", check_vid},
        {set_eth, "set_eth", check_eth},
        /* debug: */
        #ifdef IPE_DEBUG
                {show_vlan_info, "show_vlan_info", check_src_vlan},
                {print_list_ndev, "print_list_ndev", dummy},
        #endif
        {set_name, "set_name", check_src},
        {set_parent, "set_parent", check_everybody},
};





static int fetch_and_exec(const ipe_nlmsg_t *msg) {
        int command = msg->command;
        int res = 0;

        if (command < 0 || command >= IPE_COMMAND_COUNT) {
                printk(KERN_ERR "%s: bad command #%d!\n",
                                        __FUNCTION__, command);
                return IPE_UNKNOWN_COMMAND;
        }

        res = commap[command].checker(msg);

        return res ? res : commap[command].handler(msg);
}


/*
 * Fetch find case in dependency of type namespace (defaulf/custom)
 * ATTENTION! Here called "dev_hold" function!
 */
ndev_t *get_dev(const ipe_nlmsg_t *msg, const int id) {
        return dev_get_by_index(msg->nsfd[id] == IPE_GLOBAL_NS ? 
                            &init_net : get_net_ns_by_fd(msg->nsfd[id]),
                                                         msg->ifindex[id]);
}


#ifdef IPE_DEBUG
void printk_msg(const ipe_nlmsg_t *msg) {
        printk(KERN_DEBUG "%s: value %d\n", __FUNCTION__, msg->value);
        printk(KERN_DEBUG "%s: command %s\n", __FUNCTION__, commap[msg->command].name);
}
#endif



static int check_vid(const ipe_nlmsg_t *msg) {
        if (msg->value > VLAN_N_VID || msg->value < 0) {
                printk(KERN_WARNING "%s: try set bad VID %d\n", 
                                                __FUNCTION__, msg->value);
                return IPE_BAD_VID;
        }


        if (msg->value == VLAN_N_VID || msg->value == 0) {
                printk(KERN_WARNING "%s: this VID [%d] is reserved!\n", 
                                        __FUNCTION__, msg->value);
                return IPE_BAD_VID;
        }

        return IPE_OK;
}

static int check_eth(const ipe_nlmsg_t *msg) {
        if (vlan_proto_idx(htons(msg->value)) == IPE_BAD_VLAN_PROTO) {
                printk(KERN_WARNING "%s: try set bad VLAN ethertype: %x\n",
                                              __FUNCTION__, htons(msg->value));
                return IPE_BAD_VLAN_PROTO;
        }

        return IPE_OK;
}

static int dummy(const ipe_nlmsg_t *msg) {
        return IPE_OK;
}

static int check_vlan(const ipe_nlmsg_t *msg, const int id) {
        ndev_t *vlan_dev = get_dev(msg, id);

        if (IS_ERR_OR_NULL(vlan_dev)) {
                printk(KERN_WARNING "%s: fail search device #%d info net_namespace [%d]\n",
                       __FUNCTION__, msg->ifindex[id], msg->nsfd[id]);
                return IPE_BAD_PTR;
        }
 
        if (!is_vlan_dev(vlan_dev)) {
                printk(KERN_WARNING "%s: device %s is not vlan type!\n", 
                                                __FUNCTION__, vlan_dev->name);
                dev_put(vlan_dev);
                return IPE_BAD_DEV;
        }

        dev_put(vlan_dev);
        return IPE_OK;
}

static int check_dev(const ipe_nlmsg_t *msg, const int id) {
        ndev_t *vlan_dev = get_dev(msg, id);
        if (IS_ERR_OR_NULL(vlan_dev)) {
                printk(KERN_WARNING "%s: fail search device #%d info net_namespace [%d]\n",
                       __FUNCTION__, msg->ifindex[id], msg->nsfd[id]);
                return IPE_BAD_PTR;
        }
        
        dev_put(vlan_dev);
        return IPE_OK;
}


static int check_src(const ipe_nlmsg_t *msg) {
        return check_dev(msg, IPE_SRC);
}

static int check_src_vlan(const ipe_nlmsg_t *msg) {
        return check_vlan(msg, IPE_SRC);
}

static int check_everybody(const ipe_nlmsg_t *msg) {
        int res;
        int i;
        for (i = 0; i < IPE_DEV_COUNT; ++i) {
                res = check_vlan(msg, i);
                if (res)
                        return res;
        }

        #ifdef IPE_DEBUG
                printk(KERN_DEBUG "%s; res %d\n", __FUNCTION__, res);
        #endif

        return res ? res : IPE_OK;
}


/* Must be called under rtnl lock */
static ndev_t *unsafe_get_real_dev(ndev_t *dev) {
        struct vlan_dev_priv *vlan = vlan_dev_priv(dev);
        BUG_ON(!vlan);
        return vlan->real_dev;
}

static int unsafe_change_name(ndev_t *dev, const char *name) {
        strcpy(dev->name, name);
        return IPE_OK;
}

static int set_name(const ipe_nlmsg_t *msg) {
        #ifdef IPE_DEBUG
                printk(KERN_DEBUG "%s has been called\n", __FUNCTION__);
        #endif
        int res = IPE_OK;
        ndev_t *vlan_dev = get_dev(msg, IPE_SRC);
        rtnl_lock();
        res = unsafe_change_name(vlan_dev, msg->ifname);
        rtnl_unlock();

        dev_put(vlan_dev);
        return res < 0 ? res : IPE_OK;
}



static int set_vid(const ipe_nlmsg_t *msg) {
        #ifdef IPE_DEBUG
                printk(KERN_DEBUG "%s has been called\n", __FUNCTION__);
        #endif

        ndev_t *vlan_dev = get_dev(msg, IPE_SRC);
        ndev_t *real_dev = unsafe_get_real_dev(vlan_dev);

        struct vlan_dev_priv *vlan = vlan_dev_priv(vlan_dev);
        BUG_ON(!vlan);

        rtnl_lock();

        int old_vlan_id  = vlan->vlan_id;

        /*
        if (vlan_vid_add(real_dev, vlan->vlan_proto, msg->value))
                goto set_rtnl_unlock;
        */

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

        vlan_group_del_device(&vlan_info->grp,
                               vlan->vlan_proto, old_vlan_id);

        vlan_group_set_device(&vlan_info->grp, vlan->vlan_proto, 
                                                       vlan->vlan_id, vlan_dev);
        // here will be: grp->nr_vlan_devs++;
        dev_put(vlan_dev);

        rtnl_unlock();
        return IPE_OK;

set_rtnl_unlock:
        vlan_vid_del(real_dev, vlan->vlan_proto, vlan->vlan_id);
        dev_put(vlan_dev);

        rtnl_unlock();
        return IPE_DEFAULT_FAIL;
}




int vlan_check_real_dev(struct net_device *real_dev,
			__be16 protocol, u16 vlan_id)
{
	const char *name = real_dev->name;

	if (real_dev->features & NETIF_F_VLAN_CHALLENGED) {
		pr_info("VLANs not supported on %s\n", name);
		return -EOPNOTSUPP;
	}

	if (vlan_find_dev(real_dev, protocol, vlan_id) != NULL)
		return -EEXIST;

	return 0;
}

static inline ndev_t *__vlan_group_get_device(struct vlan_group *vg,
							 unsigned int pidx,
							 u16 vlan_id)
{
	struct net_device **array;

	array = vg->vlan_devices_arrays[pidx]
				       [vlan_id / VLAN_GROUP_ARRAY_PART_LEN];
	return array ? array[vlan_id % VLAN_GROUP_ARRAY_PART_LEN] : NULL;
}

static inline ndev_t *vlan_group_get_device(struct vlan_group *vg,
						       __be16 vlan_proto,
						       u16 vlan_id)
{
	return __vlan_group_get_device(vg, vlan_proto_idx(vlan_proto), vlan_id);
}

static inline ndev_t *vlan_find_dev(ndev_t *real_dev,
					       __be16 vlan_proto, u16 vlan_id)
{
	struct vlan_info *vlan_info = rcu_dereference_rtnl(real_dev->vlan_info);

	if (vlan_info)
		return vlan_group_get_device(&vlan_info->grp,
					     vlan_proto, vlan_id);

	return NULL;
}

static struct vlan_info *vlan_info_alloc(ndev_t *dev)
{
	struct vlan_info *vlan_info;

	vlan_info = kzalloc(sizeof(struct vlan_info), GFP_KERNEL);
	if (!vlan_info)
		return NULL;

	vlan_info->real_dev = dev;
	INIT_LIST_HEAD(&vlan_info->vid_list);

	return vlan_info;
}



static int check_loop_case(ndev_t *ldev, ndev_t *updev) {
        /* updev must be not in uppers in ldev for IPE_OK */ 
        if (netdev_has_upper_dev(ldev, updev)) 
                return IPE_BAD_DEV;

        return IPE_OK;
}


/*
 * TODO: This functions are very similary, should be think about 
 * refactoring. Moreover, they is very long
 */
static int set_parent(const ipe_nlmsg_t *msg) {
        int err;
        __be16 vlan_proto;
        u16    vlan_id;
        ndev_t *new_real_dev;

        rtnl_lock();

        ndev_t *vlan_dev = get_dev(msg, IPE_SRC);
        ndev_t *real_dev = unsafe_get_real_dev(vlan_dev);
        if (!is_vlan_dev(real_dev)) {
                printk(KERN_ERR "%s: device %s bounded with phy interface %s!\n",
                                __FUNCTION__, vlan_dev->name, real_dev->name);
                goto ret_err;
        }

        new_real_dev = get_dev(msg, IPE_DST);
        if (vlan_dev == new_real_dev) {
                printk(KERN_ERR "%s: u try set self as parent!\n", 
                                __FUNCTION__);
                goto ret_err;
        }

        if (check_loop_case(vlan_dev, new_real_dev)) {
                printk(KERN_ERR "%s: device %s has %s as upper neighbour!\n",
                                __FUNCTION__, vlan_dev->name, new_real_dev->name);
                goto ret_err;
        }


        struct vlan_dev_priv *vlan = vlan_dev_priv(vlan_dev);
        BUG_ON(!vlan);

        vlan_proto = vlan->vlan_proto;
        vlan_id    = vlan->vlan_id;

        err = vlan_check_real_dev(new_real_dev, vlan_proto, vlan_id);
        if (err < 0) 
                goto set_rtnl_unlock;

        if (vlan_vid_add(real_dev, vlan_proto, vlan_id))
                goto set_rtnl_unlock;

        if (vlan_vid_add(new_real_dev, vlan_proto, vlan_id))
                goto set_rtnl_unlock;

        struct vlan_info *vlan_info = rcu_dereference_rtnl(real_dev->vlan_info);
        /* vlan_info should be there now. vlan_vid_add took care of it */
        BUG_ON(!vlan_info);
        struct vlan_info *dst_info = rcu_dereference_rtnl(new_real_dev->vlan_info);
        BUG_ON(!dst_info);
        
        //struct vlan_info *dst_info = vlan_info_alloc(new_real_dev);

        vlan->real_dev = new_real_dev;

        struct vlan_group *grp = &dst_info->grp;
        if (vlan_group_prealloc_vid(grp, vlan_proto, vlan_id) < 0) {
                printk(KERN_ERR "%s: fail alloc memory for vlan group %p!\n", 
                                                           __FUNCTION__, grp);
                goto set_rtnl_unlock;
        }

        vlan_group_del_device(&vlan_info->grp, vlan->vlan_proto, vlan->vlan_id);

        vlan_group_set_device(&dst_info->grp, vlan->vlan_proto, 
                                                       vlan->vlan_id, vlan_dev);

        dev_put(vlan_dev);
        dev_put(new_real_dev);

        rtnl_unlock();
        return IPE_OK;

set_rtnl_unlock:
        vlan_vid_del(real_dev, vlan->vlan_proto, vlan->vlan_id);

ret_err:
        dev_put(vlan_dev);
        dev_put(new_real_dev);
        rtnl_unlock();

        return IPE_DEFAULT_FAIL;
}





/*
 * TODO: This functions are very similary, should be think about 
 * refactoring. Moreover, they is very long
 */
static int set_eth(const ipe_nlmsg_t *msg) {

        __be16 old_vlan_proto;
        ndev_t *vlan_dev = get_dev(msg, IPE_SRC);
        ndev_t *real_dev = unsafe_get_real_dev(vlan_dev);

        struct vlan_dev_priv *vlan = vlan_dev_priv(vlan_dev);
        BUG_ON(!vlan);

        rtnl_lock();

        old_vlan_proto = vlan->vlan_proto;

        /*
        if (vlan_vid_add(real_dev, htons(msg->value), vlan->vlan_id))
                goto set_rtnl_unlock;
        */

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

        vlan_group_del_device(&vlan_info->grp, old_vlan_proto, vlan->vlan_id);
        vlan_group_set_device(&vlan_info->grp, vlan->vlan_proto, 
                                                       vlan->vlan_id, vlan_dev);
        dev_put(vlan_dev);

        rtnl_unlock();
        return IPE_OK;

set_rtnl_unlock:
        vlan_vid_del(real_dev, vlan->vlan_proto, vlan->vlan_id);
        dev_put(vlan_dev);

        rtnl_unlock();
        return IPE_DEFAULT_FAIL;
}




static int send_reply(struct nlmsghdr *nlh, const ipe_nlmsg_t *msg, 
                                                ipe_reply_t *reply) 
{
        struct sk_buff *skb;
        int pid;
        int msg_size;
        int res;

#ifdef IPE_DEBUG
        printk(KERN_DEBUG "%s: entry nlh %p, msg %p\n", 
                        __FUNCTION__, nlh, msg);
#endif
        msg_size = sizeof(ipe_reply_t);
        pid = nlh->nlmsg_pid; /* pid of sending process */
#ifdef IPE_DEBUG
        printk(KERN_DEBUG "%s: process to send message: [%d]\n", 
                                __FUNCTION__, pid);
#endif
        skb = nlmsg_new(msg_size, 0);
        if (!skb) {
                printk(KERN_ERR "%s: failed to allocate new skb!\n", 
                                                __FUNCTION__);
                return IPE_BAD_ALLOC;
        }

        nlh = nlmsg_put(skb, 0, 0, NLMSG_DONE, msg_size, 0);
        NETLINK_CB(skb).dst_group = 0; /* not in mcast group */
        memcpy(nlmsg_data(nlh), reply, msg_size);

#ifdef IPE_DEBUG
        printk(KERN_DEBUG "%s: payload -- %d\n", 
                        __FUNCTION__, ((ipe_reply_t *)nlmsg_data(nlh))->retcode);
        printk(KERN_DEBUG "%s: retcode %d\n", __FUNCTION__, reply->retcode);
#endif

        res = nlmsg_unicast(nl_sk, skb, pid);

        if (res < 0) {
                printk(KERN_ERR "%s: error while sending back to user %d\n",
                                __FUNCTION__, pid);
                return IPE_DEFAULT_FAIL;
        }

        return res;
}



static void init_reply(ipe_reply_t *reply, const int retcode,
                                            const ipe_nlmsg_t *msg) 
{
        reply->retcode  = retcode;
        if (retcode) {
                snprintf(reply->report, IPE_BUFF_SIZE, 
                        "%s(%d) return with exit code 0x%x\n",
                         commap[(int)(msg->command)].name, msg->value, retcode);
        } else {
                snprintf(reply->report, IPE_BUFF_SIZE, 
                        "%s(%d) success!\n",
                         commap[(int)(msg->command)].name, msg->value);
        }
}


/*
 * Call hadler for required ops
 */
static void vlan_ext_handler(struct sk_buff *skb) {
        struct  nlmsghdr *nlh;
        ipe_nlmsg_t *msg;
        ipe_reply_t reply;
        int res;

        nlh = (struct nlmsghdr*)skb->data;
        msg = (ipe_nlmsg_t *)nlmsg_data(nlh);

        #ifdef IPE_DEBUG
                printk_msg(msg);
        #endif

        init_reply(&reply, fetch_and_exec(msg), msg);

        res = send_reply(nlh, msg, &reply);
        #ifdef IPE_DEBUG
        if (res) {
                printk(KERN_ERR "%s: send_reply return with exit code %d!\n",
                                                           __FUNCTION__, res);
        }
        #endif
}



static int __init ipe_init(void) {

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




static void __exit ipe_exit(void) {
        #ifdef IPE_DEBUG
                printk(KERN_INFO "%s: exiting %s\n", 
                                               __FUNCTION__, THIS_MODULE->name);
        #endif
        netlink_kernel_release(nl_sk);
}


module_init(ipe_init); 
module_exit(ipe_exit);

MODULE_LICENSE( "GPL" );
MODULE_VERSION( "0.1" );
MODULE_AUTHOR( "Daniel Wolkow <volkov12@rambler.ru>" );
MODULE_DESCRIPTION( "Netlink extending driver for ipe" );

