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
//		BUG();
		return 0;
	}
}

static inline void vlan_group_del_device(struct vlan_group *vg,
					 __be16 vlan_proto, u16 vlan_id);

static inline void vlan_group_set_device(struct vlan_group *vg,
					 __be16 vlan_proto, u16 vlan_id,
					 struct net_device *dev);
#endif  //__IPE_VLAN_H
