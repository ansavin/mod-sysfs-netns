#include <linux/kobject.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/netdevice.h>
#include <net/netns/generic.h>

#ifdef DEBUG
#define dprintk(X...) printk(KERN_INFO X)
#else
#define dprintk(X...)                                                          \
	do {                                                                   \
	} while (0)
#endif

struct pernet_data {
	struct kobject kobject;
	struct net *net;
	int property;
};

struct pernet_data_net {
	struct pernet_data *data;
};

static struct kset *sysfs_netns_kset;
struct kobject *sysfs_netns_kobj;
unsigned int sysfs_netns_net_id;

static ssize_t property_show(struct kobject *kobj, struct kobj_attribute *attr,
			     char *buf)
{
	struct pernet_data *c = container_of(kobj, struct pernet_data, kobject);
	return sysfs_emit(buf, "%i\n", c->property);
}

static ssize_t property_store(struct kobject *kobj, struct kobj_attribute *attr,
			      const char *buf, size_t count)
{
	int ret;

	struct pernet_data *c = container_of(kobj, struct pernet_data, kobject);

	ret = kstrtoint(buf, 10, &c->property);
	if (ret < 0)
		return ret;

	return count;
}

static struct kobj_attribute property_attribute =
	__ATTR(property, 0664, property_show, property_store);

static struct attribute *attrs[] = {
	&property_attribute.attr,
	NULL, /* need to NULL terminate the list of attributes */
};

static const void *data_kobj_namespace(struct kobject *kobj)
{
	return container_of(kobj, struct pernet_data, kobject)->net;
}

static void data_kobj_release(struct kobject *kobj)
{
	struct pernet_data *c = container_of(kobj, struct pernet_data, kobject);
	dprintk("%s(%px) -> kfree(%px)\n", __func__, kobj, c);
	kfree(c);
}

static struct kobj_type data_kobj_type = {
	.release = data_kobj_release,
	.default_attrs = attrs,
	.sysfs_ops = &kobj_sysfs_ops,
	.namespace = data_kobj_namespace,
};

struct pernet_data *pernet_data_alloc(struct kobject *parent, struct net *net)
{
	struct pernet_data *p;

	p = kzalloc(sizeof(*p), GFP_KERNEL);
	if (p) {
		p->net = net;
		p->kobject.kset = sysfs_netns_kset;
		if (kobject_init_and_add(&p->kobject, &data_kobj_type, parent,
					 "data") == 0)
			return p;
		dprintk("%s: kobject_put(%px)\n", __func__, &p->kobject);
		kobject_put(&p->kobject);
	}
	return NULL;
}

static int netns_setup(struct net *net)
{
	struct pernet_data *data;
	struct pernet_data_net *nn;

	dprintk("%s(%px)\n", __func__, net);

	nn = net_generic(net, sysfs_netns_net_id);

	data = pernet_data_alloc(sysfs_netns_kobj, net);
	if (!data) {
		return -ENOMEM;
	}

	nn->data = data;

	return 0;
}

void netns_destroy(struct net *net)
{
	struct pernet_data *data;
	struct pernet_data_net *nn;

	dprintk("%s(%px)\n", __func__, net);

	nn = net_generic(net, sysfs_netns_net_id);

	data = nn->data;

	dprintk("%s: kobject_put(%px)\n", __func__, &data->kobject);
	kobject_del(&data->kobject);
	kobject_put(&data->kobject);
}

static struct pernet_operations sysfs_netns_net_ops = {
	.init = netns_setup,
	.exit = netns_destroy,
	.id = &sysfs_netns_net_id,
	.size = sizeof(struct pernet_data_net),
};

static void netns_object_release(struct kobject *kobj)
{
	dprintk("%s(%px)\n", __func__, kobj);
	kfree(kobj);
}

static const struct kobj_ns_type_operations *
netns_object_child_ns_type(struct kobject *kobj)
{
	return &net_ns_type_operations;
}

static struct kobj_type sysfs_netns_object_type = {
	.release = netns_object_release,
	.sysfs_ops = &kobj_sysfs_ops,
	.child_ns_type = netns_object_child_ns_type,
};
static struct kobject *netns_object_alloc(const char *name, struct kset *kset,
					  struct kobject *parent)
{
	struct kobject *kobj;

	kobj = kzalloc(sizeof(*kobj), GFP_KERNEL);
	if (kobj) {
		kobj->kset = kset;
		if (kobject_init_and_add(kobj, &sysfs_netns_object_type, parent,
					 "%s", name) == 0)
			return kobj;
		dprintk("%s: kobject_put(%px)\n", __func__, kobj);
		kobject_put(kobj);
	}
	return NULL;
}

static int __init sysfs_netns_init(void)
{
	int err;

	sysfs_netns_kset =
		kset_create_and_add("kset_sysfs_netns", NULL, kernel_kobj);

	if (!sysfs_netns_kset) {
		printk(KERN_WARNING "can't create kset\n");
		return -ENOMEM;
	}

	sysfs_netns_kobj = netns_object_alloc("net", sysfs_netns_kset, NULL);
	if (!sysfs_netns_kobj) {
		printk(KERN_WARNING "can't create kobject\n");
		kset_unregister(sysfs_netns_kset);
		sysfs_netns_kset = NULL;
		return -ENOMEM;
	}

	err = register_pernet_subsys(&sysfs_netns_net_ops);
	if (err) {
		dprintk("%s: kobject_put(%px)\n", __func__, sysfs_netns_kobj);
		kobject_put(sysfs_netns_kobj);
		sysfs_netns_kobj = NULL;
		kset_unregister(sysfs_netns_kset);
		sysfs_netns_kset = NULL;
		return -ENOMEM;
	}

	return 0;
}

static void __exit sysfs_netns_exit(void)
{
	unregister_pernet_subsys(&sysfs_netns_net_ops);
	dprintk("%s: kobject_put(%px)\n", __func__, sysfs_netns_kobj);
	kobject_put(sysfs_netns_kobj);
	sysfs_netns_kobj = NULL;
	kset_unregister(sysfs_netns_kset);
	sysfs_netns_kset = NULL;
}

module_init(sysfs_netns_init);
module_exit(sysfs_netns_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Andrei Savin <andrei.v.savin@gmail.com>");
MODULE_DESCRIPTION("Sysfs with net namespaces example");
MODULE_VERSION("1.0");