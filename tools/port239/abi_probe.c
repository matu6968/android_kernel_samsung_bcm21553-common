#include <stddef.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/ioctl.h>
struct file;
struct inode;
#include <linux/android_pmem.h>
#include <linux/ashmem.h>
#include <linux/usb/f_accessory.h>
#include <linux/videodev2.h>
typedef __kernel_pid_t pid_t;
typedef __kernel_uid32_t uid_t;
typedef __u8 uint8_t;
#include "binder.h"
#include "logger.h"
const unsigned int abi_values[] = {
 sizeof(void *), sizeof(struct pmem_region), offsetof(struct pmem_region, len),
 PMEM_GET_PHYS, PMEM_MAP, PMEM_GET_SIZE, PMEM_UNMAP, PMEM_ALLOCATE,
 PMEM_CONNECT, PMEM_GET_TOTAL_SIZE, PMEM_CACHE_FLUSH,
 sizeof(struct ashmem_pin), ASHMEM_SET_NAME, ASHMEM_GET_NAME, ASHMEM_SET_SIZE,
 ASHMEM_GET_SIZE, ASHMEM_SET_PROT_MASK, ASHMEM_GET_PROT_MASK, ASHMEM_PIN,
 ASHMEM_UNPIN, ASHMEM_GET_PIN_STATUS, ASHMEM_PURGE_ALL_CACHES,
 ACCESSORY_GET_STRING_MANUFACTURER, ACCESSORY_GET_STRING_MODEL,
 ACCESSORY_GET_STRING_DESCRIPTION, ACCESSORY_GET_STRING_VERSION,
 ACCESSORY_GET_STRING_URI, ACCESSORY_GET_STRING_SERIAL,
 ACCESSORY_IS_START_REQUESTED,
 sizeof(struct flat_binder_object), sizeof(struct binder_write_read),
 sizeof(struct binder_transaction_data), BINDER_WRITE_READ, BINDER_VERSION,
 BINDER_CURRENT_PROTOCOL_VERSION, sizeof(struct logger_entry),
 offsetof(struct logger_entry, msg), LOGGER_GET_LOG_BUF_SIZE,
 LOGGER_GET_LOG_LEN, LOGGER_GET_NEXT_ENTRY_LEN, LOGGER_FLUSH_LOG,
 sizeof(struct v4l2_buffer), sizeof(struct v4l2_format),
 sizeof(struct v4l2_requestbuffers), sizeof(struct v4l2_control),
 offsetof(struct v4l2_buffer, timestamp), offsetof(struct v4l2_buffer, m),
 VIDIOC_QUERYCAP, VIDIOC_S_FMT, VIDIOC_G_FMT, VIDIOC_REQBUFS, VIDIOC_QBUF,
 VIDIOC_DQBUF, VIDIOC_STREAMON, VIDIOC_STREAMOFF, VIDIOC_S_CTRL, VIDIOC_G_CTRL,
 V4L2_CID_PRIVATE_BASE, V4L2_CID_CAMERA_CLASS_BASE,
};
