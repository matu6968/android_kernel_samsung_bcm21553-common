/* SPDX-License-Identifier: GPL-2.0 */
#ifndef BCM_F_ACCESSORY_H
#define BCM_F_ACCESSORY_H

#include <linux/usb/composite.h>

#ifdef CONFIG_USB_ANDROID_ACCESSORY
int acc_setup(void);
void acc_cleanup(void);
int acc_bind_config(struct usb_configuration *c);
void acc_function_enable(int enable);
int acc_ctrlrequest(struct usb_composite_dev *cdev,
		const struct usb_ctrlrequest *ctrl);
void android_usb_state_changed(struct usb_composite_dev *cdev);
void android_usb_reset_config(struct usb_composite_dev *cdev);
void android_usb_rebuild_interfaces(struct usb_configuration *c);
int android_ics_init(struct usb_composite_dev *cdev);
void android_ics_cleanup(void);
int android_ics_controlled(void);
int android_ics_enabled(void);
#else
static inline int acc_setup(void) { return 0; }
static inline void acc_cleanup(void) { }
static inline int acc_bind_config(struct usb_configuration *c) { return 0; }
static inline void acc_function_enable(int enable) { }
static inline int acc_ctrlrequest(struct usb_composite_dev *cdev,
		const struct usb_ctrlrequest *ctrl) { return -EOPNOTSUPP; }
static inline void android_usb_state_changed(struct usb_composite_dev *cdev) { }
static inline void android_usb_rebuild_interfaces(struct usb_configuration *c) { }
static inline int android_ics_init(struct usb_composite_dev *cdev) { return 0; }
static inline void android_ics_cleanup(void) { }
static inline int android_ics_controlled(void) { return 0; }
static inline int android_ics_enabled(void) { return 1; }
#endif
#endif
