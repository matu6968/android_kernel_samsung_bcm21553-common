/*
 * ICS userspace control for the BCM21553 Android gadget.
 * Copyright (C) 2026 matu6968
 * Licensed under GPL version 2.
 *
 * Keep the vendor functions and controller. Stage a composition while the
 * pullup is down, then publish it atomically on enable, as ICS init expects.
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/etherdevice.h>
#include <linux/usb/composite.h>
#include <linux/usb/f_accessory.h>
#include "f_accessory.h"
#include "f_adb.h"
#include "f_acm.h"
#include "f_mass_storage.h"

extern void obex_function_enable(int enable);
extern void rndis_update_host_addr(const u8 *addr);
extern void gether_update_host_addr(const u8 *addr);

#define ICS_ADB 1
#define ICS_MASS_STORAGE 2
#define ICS_RNDIS 4
#define ICS_ACCESSORY 8

static DEFINE_MUTEX(ics_lock);
static struct {
	struct usb_composite_dev *cdev;
	struct class *class;
	struct device *dev, *mass, *rndis;
	struct usb_device_descriptor desc;
	unsigned functions;
	char function_names[64];
	u8 ethaddr[ETH_ALEN];
	int controlled, enabled, last_state;
} ics;

int android_ics_controlled(void)
{
	return ics.controlled;
}

int android_ics_enabled(void)
{
	return ics.enabled;
}

/* Called by composite switch_work, never from an interrupt. */
void android_usb_state_changed(struct usb_composite_dev *cdev)
{
	char *env[] = { NULL, NULL };
	int state;

	mutex_lock(&ics_lock);
	if (!ics.dev || !ics.controlled)
		goto out;
	state = !cdev->connected ? 0 : cdev->config ? 2 : 1;
	if (state == ics.last_state)
		goto out;
	ics.last_state = state;
	env[0] = state == 2 ? "USB_STATE=CONFIGURED" :
		state == 1 ? "USB_STATE=CONNECTED" : "USB_STATE=DISCONNECTED";
	kobject_uevent_env(&ics.dev->kobj, KOBJ_CHANGE, env);
out:
	mutex_unlock(&ics_lock);
}

static ssize_t state_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct usb_composite_dev *cdev = ics.cdev;
	return sprintf(buf, "%s\n", !cdev->connected ? "DISCONNECTED" :
		cdev->config ? "CONFIGURED" : "CONNECTED");
}

static ssize_t functions_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t ret;
	mutex_lock(&ics_lock);
	ret = sprintf(buf, "%s\n", ics.function_names);
	mutex_unlock(&ics_lock);
	return ret;
}

static ssize_t functions_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	char copy[64], names[64], *p, *name;
	unsigned functions = 0, bit;
	ssize_t ret = -EINVAL;

	if (!count || count >= sizeof(copy))
		return -EINVAL;
	memcpy(copy, buf, count);
	copy[count] = 0;
	strlcpy(names, strim(copy), sizeof(names));
	p = strim(copy);
	while ((name = strsep(&p, ","))) {
		if (!strcmp(name, "adb")) bit = ICS_ADB;
		else if (!strcmp(name, "mass_storage")) bit = ICS_MASS_STORAGE;
		else if (!strcmp(name, "rndis")) bit = ICS_RNDIS;
		else if (!strcmp(name, "accessory")) bit = ICS_ACCESSORY;
		else return -EINVAL;
		if (functions & bit)
			return -EINVAL;
		functions |= bit;
	}
	if ((functions & ICS_ACCESSORY) &&
	    (functions & (ICS_RNDIS | ICS_MASS_STORAGE)))
		return -EINVAL;
	if ((functions & ICS_RNDIS) && (functions & ICS_MASS_STORAGE))
		return -EINVAL;
	mutex_lock(&ics_lock);
	if (ics.enabled && ics.controlled)
		ret = -EBUSY;
	else {
		ics.functions = functions;
		strlcpy(ics.function_names, names, sizeof(ics.function_names));
		ret = count;
	}
	mutex_unlock(&ics_lock);
	return ret;
}

static ssize_t enable_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	return sprintf(buf, "%d\n", ics.enabled);
}

static ssize_t enable_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct usb_configuration *c;
	unsigned flags;
	unsigned long irqflags;
	long enabled;
	int ret = 0;

	if (strict_strtol(buf, 0, &enabled) || (enabled != 0 && enabled != 1))
		return -EINVAL;
	mutex_lock(&ics_lock);
	ics.controlled = 1;
	if (!enabled) {
		ics.enabled = 0;
		ics.last_state = -1;
		usb_gadget_disconnect(ics.cdev->gadget);
		android_usb_reset_config(ics.cdev);
		goto out;
	}
	if (ics.enabled)
		goto out;
	flags = ics.functions;
	if (!flags) {
		ret = -EINVAL;
		goto out;
	}
	/* The legacy RNDIS backend intentionally has no simultaneous ADB. */
	set_current_usb_config(flags & ICS_RNDIS ? 0 : 1);
	acc_function_enable(!!(flags & ICS_ACCESSORY));
	mass_storage_function_enable(!!(flags & ICS_MASS_STORAGE));
	acm_function_enable((flags & ICS_MASS_STORAGE) && (flags & ICS_ADB));
	obex_function_enable(0);
	adb_function_enable((flags & ICS_ADB) && !(flags & ICS_RNDIS));
	spin_lock_irqsave(&ics.cdev->lock, irqflags);
	list_for_each_entry(c, &ics.cdev->configs, list)
		android_usb_rebuild_interfaces(c);
	ics.cdev->desc = ics.desc;
	if (flags & ICS_ACCESSORY) {
		ics.cdev->desc.idVendor = cpu_to_le16(USB_ACCESSORY_VENDOR_ID);
		ics.cdev->desc.idProduct = cpu_to_le16(flags & ICS_ADB ?
			USB_ACCESSORY_ADB_PRODUCT_ID : USB_ACCESSORY_PRODUCT_ID);
		ics.cdev->desc.bDeviceClass = USB_CLASS_PER_INTERFACE;
	} else if (flags & ICS_RNDIS)
		ics.cdev->desc.bDeviceClass = USB_CLASS_COMM;
	else if ((flags & ICS_MASS_STORAGE) && !(flags & ICS_ADB))
		ics.cdev->desc.bDeviceClass = USB_CLASS_MASS_STORAGE;
	spin_unlock_irqrestore(&ics.cdev->lock, irqflags);
	ics.enabled = 1;
	ret = usb_gadget_connect(ics.cdev->gadget);
	if (ret)
		ics.enabled = 0;
out:
	mutex_unlock(&ics_lock);
	return ret ? ret : count;
}

/* USB descriptor values in this ABI are hexadecimal. */
#define ICS_DESCRIPTOR_ATTR(field, bits) \
static ssize_t field##_show(struct device *dev, \
		struct device_attribute *attr, char *buf) \
{ \
	return sprintf(buf, "%04x\n", bits == 16 ? \
		le16_to_cpu(ics.desc.field) : ics.desc.field); \
} \
static ssize_t field##_store(struct device *dev, \
		struct device_attribute *attr, const char *buf, size_t count) \
{ \
	unsigned long value; \
	int ret = strict_strtoul(buf, 16, &value); \
	if (ret || value >= (1U << bits)) return -EINVAL; \
	mutex_lock(&ics_lock); \
	if (ics.controlled && ics.enabled) ret = -EBUSY; \
	else ics.desc.field = bits == 16 ? cpu_to_le16(value) : value; \
	mutex_unlock(&ics_lock); \
	return ret ? ret : count; \
} \
static DEVICE_ATTR(field, 0644, field##_show, field##_store)

ICS_DESCRIPTOR_ATTR(idVendor, 16);
ICS_DESCRIPTOR_ATTR(idProduct, 16);
ICS_DESCRIPTOR_ATTR(bcdDevice, 16);
ICS_DESCRIPTOR_ATTR(bDeviceClass, 8);
ICS_DESCRIPTOR_ATTR(bDeviceSubClass, 8);
ICS_DESCRIPTOR_ATTR(bDeviceProtocol, 8);
static DEVICE_ATTR(functions, 0644, functions_show, functions_store);
static DEVICE_ATTR(enable, 0644, enable_show, enable_store);
static DEVICE_ATTR(state, 0444, state_show, NULL);

static struct attribute *ics_attrs[] = {
	&dev_attr_idVendor.attr, &dev_attr_idProduct.attr,
	&dev_attr_bcdDevice.attr, &dev_attr_bDeviceClass.attr,
	&dev_attr_bDeviceSubClass.attr, &dev_attr_bDeviceProtocol.attr,
	&dev_attr_functions.attr, &dev_attr_enable.attr, &dev_attr_state.attr, NULL
};
static const struct attribute_group ics_attr_group = { .attrs = ics_attrs };

static ssize_t ethaddr_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	return sprintf(buf, "%pM\n", ics.ethaddr);
}

static ssize_t ethaddr_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned a[6], i;
	u8 addr[ETH_ALEN];
	if (sscanf(buf, "%x:%x:%x:%x:%x:%x", &a[0], &a[1], &a[2],
		&a[3], &a[4], &a[5]) != 6)
		return -EINVAL;
	for (i = 0; i < ETH_ALEN; i++) {
		if (a[i] > 255) return -EINVAL;
		addr[i] = a[i];
	}
	if (!is_valid_ether_addr(addr))
		return -EINVAL;
	mutex_lock(&ics_lock);
	if (ics.controlled && ics.enabled && (ics.functions & ICS_RNDIS)) {
		mutex_unlock(&ics_lock);
		return -EBUSY;
	}
	memcpy(ics.ethaddr, addr, ETH_ALEN);
	gether_update_host_addr(addr);
	rndis_update_host_addr(addr);
	mutex_unlock(&ics_lock);
	return count;
}
static DEVICE_ATTR(ethaddr, 0644, ethaddr_show, ethaddr_store);

int android_ics_init(struct usb_composite_dev *cdev)
{
	struct kobject *lun = mass_storage_lun_kobj();
	int ret;

	ics.cdev = cdev;
	ics.desc = cdev->desc;
	ics.functions = ICS_ADB | ICS_MASS_STORAGE;
	strlcpy(ics.function_names, "adb,mass_storage", sizeof(ics.function_names));
	ics.enabled = 1;
	ics.last_state = -1;
	ics.class = class_create(THIS_MODULE, "android_usb");
	if (IS_ERR(ics.class)) {
		ret = PTR_ERR(ics.class);
		ics.class = NULL;
		return ret;
	}
	ics.dev = device_create(ics.class, NULL, MKDEV(0, 0), NULL, "android0");
	if (IS_ERR(ics.dev)) {
		ret = PTR_ERR(ics.dev); ics.dev = NULL; goto fail;
	}
	ret = sysfs_create_group(&ics.dev->kobj, &ics_attr_group);
	if (ret) goto fail;
	ics.mass = device_create(ics.class, ics.dev, MKDEV(0, 1), NULL, "f_mass_storage");
	if (IS_ERR(ics.mass)) {
		ret = PTR_ERR(ics.mass); ics.mass = NULL; goto fail;
	}
	ret = lun ? sysfs_create_link(&ics.mass->kobj, lun, "lun") : -ENODEV;
	if (ret) goto fail;
	ics.rndis = device_create(ics.class, ics.dev, MKDEV(0, 2), NULL, "f_rndis");
	if (IS_ERR(ics.rndis)) {
		ret = PTR_ERR(ics.rndis); ics.rndis = NULL; goto fail;
	}
	ret = device_create_file(ics.rndis, &dev_attr_ethaddr);
	if (ret) goto fail;
	return 0;
fail:
	android_ics_cleanup();
	return ret;
}

void android_ics_cleanup(void)
{
	mutex_lock(&ics_lock);
	ics.controlled = 0;
	if (ics.rndis) {
		device_remove_file(ics.rndis, &dev_attr_ethaddr);
		device_unregister(ics.rndis);
		ics.rndis = NULL;
	}
	if (ics.mass) {
		sysfs_remove_link(&ics.mass->kobj, "lun");
		device_unregister(ics.mass);
		ics.mass = NULL;
	}
	if (ics.dev) {
		sysfs_remove_group(&ics.dev->kobj, &ics_attr_group);
		device_unregister(ics.dev);
		ics.dev = NULL;
	}
	if (ics.class) {
		class_destroy(ics.class);
		ics.class = NULL;
	}
	mutex_unlock(&ics_lock);
}
