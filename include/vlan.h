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

#ifndef __IPE_VLAN_H
#define __IPE_VLAN_H

#include <linux/if_vlan.h>
#include <linux/u64_stats_sync.h>
#include <linux/list.h>

/* if this changes, algorithm will have to be reworked because this
 * depends on completely exhausting the VLAN identifier space.  Thus
 * it gives constant time look-up, but in many cases it wastes memory.
 */
#define VLAN_GROUP_ARRAY_SPLIT_PARTS  8
#define VLAN_GROUP_ARRAY_PART_LEN     (VLAN_N_VID/VLAN_GROUP_ARRAY_SPLIT_PARTS)

enum vlan_protos {
	VLAN_PROTO_8021Q	= 0,
	VLAN_PROTO_8021AD,
	VLAN_PROTO_NUM,
};

struct vlan_group {
	unsigned int		nr_vlan_devs;
	struct hlist_node	hlist;	/* linked list */
	struct net_device **vlan_devices_arrays[VLAN_PROTO_NUM]
					       [VLAN_GROUP_ARRAY_SPLIT_PARTS];
};

struct vlan_info {
	struct net_device	*real_dev; /* The ethernet(like) device
					    * the vlan is attached to.
					    */
	struct vlan_group	grp;
	struct list_head	vid_list;
	unsigned int		nr_vids;
	struct rcu_head		rcu;
};

static inline unsigned int vlan_proto_idx(__be16 proto)
{
        switch (proto) {
        case htons(ETH_P_8021Q):
                return VLAN_PROTO_8021Q;
        case htons(ETH_P_8021AD):
                return VLAN_PROTO_8021AD;
        default:
#ifdef IPE_DEBUG
                printk(KERN_ERR "%s: incorrect vlan protocol %x!\n", 
                                                 __FUNCTION__, proto);
#endif
//		BUG();
                return 0;
        }
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

static inline void vlan_group_del_device(struct vlan_group *vg,
					 __be16 vlan_proto, u16 vlan_id)
{
        vlan_group_set_device(vg, vlan_proto, vlan_id, NULL);
}


static int vlan_group_prealloc_vid(struct vlan_group *vg,
                                        __be16 vlan_proto, u16 vlan_id)
{
        struct net_device **array;
        unsigned int pidx, vidx;
        unsigned int size;

        ASSERT_RTNL();

        pidx  = vlan_proto_idx(vlan_proto);
        vidx  = vlan_id / VLAN_GROUP_ARRAY_PART_LEN;
        array = vg->vlan_devices_arrays[pidx][vidx];
        if (array != NULL)
                return 0;

        size = sizeof(struct net_device *) * VLAN_GROUP_ARRAY_PART_LEN;
        array = kzalloc(size, GFP_KERNEL);
        if (array == NULL)
                return -ENOBUFS;

        vg->vlan_devices_arrays[pidx][vidx] = array;
        return 0;
}

#endif  //__IPE_VLAN_H
