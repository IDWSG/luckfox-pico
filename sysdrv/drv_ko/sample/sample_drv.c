/**
 * 内核字符设备驱动, 注册/dev/sample
 * 
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include <linux/wait.h>
#include <linux/slab.h>

#include "sample_ioctl.h"

#define DEVICE_NAME "sample"
#define CLASS_NAME "sample_class"

static dev_t sample_dev;
static struct cdev sample_cdev;
static struct class *sample_class;
static struct device *sample_device;

struct sample_state {
  struct mutex lock;
  wait_queue_head_t cmd_wq;
  wait_queue_head_t result_wq;  // cat 用来等待结果
  
  char cmd_buf[SAMPLE_CMD_MAX]; // echo写进来的命令
  size_t cmd_len;
  bool cmd_ready;

  char result_buf[SAMPLE_RESULT_MAX];
  size_t result_len;
  bool result_ready;

  bool meterd_nonblock; // meterd 是否设置了非阻塞模式
};

static struct sample_state *st;

static int sample_open(struct inode *inode, struct file *file) {
  return 0;
}
static int sample_release(struct inode *inode, struct file *file) {
  return 0;
}

static ssize_t sample_read(struct file *file, char __user *buf, size_t count, loff_t *ppos) {
  ssize_t ret = 0;

  if (count > SAMPLE_RESULT_MAX) {
    count = SAMPLE_RESULT_MAX;
  }

  mutex_lock(&st->lock);
  while (!st->result_ready) {
    mutex_unlock(&st->lock);
    if (file->f_flags & O_NONBLOCK || st->meterd_nonblock) {
      return -EAGAIN;
    }
    if (wait_event_interruptible(st->result_wq, st->result_ready)) {
      return -ERESTARTSYS;
    }
    mutex_lock(&st->lock);
  }

  if (copy_to_user(buf, st->result_buf, st->result_len)) {
    ret = -EFAULT;
  } else {
    ret = st->result_len;
    st->result_ready = false; // 已经读取了结果
  }
  mutex_unlock(&st->lock);
  pr_info("sample_read: returned %zd bytes\n", ret);
  return ret;
}

static ssize_t sample_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos) {
  ssize_t ret = 0;

  if (count > SAMPLE_CMD_MAX) {
    count = SAMPLE_CMD_MAX;
  }

  mutex_lock(&st->lock);
  if (copy_from_user(st->cmd_buf, buf, count)) {
    ret = -EFAULT;
  } else {
    st->cmd_len = count;
    st->cmd_ready = true;
    ret = count;
    wake_up_interruptible(&st->cmd_wq); // 唤醒等待命令的 meterd
  }
  mutex_unlock(&st->lock);
  pr_info("sample_write: received %zd bytes\n", ret);
  return ret;
}

static long sample_ioctl(struct file *file, unsigned int cmd, unsigned long arg) {
  long ret = 0;
  struct sample_buf kbuf;

  switch (cmd) {
    case SAMPLE_IOC_GET_CMD:
      mutex_lock(&st->lock);
      while (!st->cmd_ready) {
        mutex_unlock(&st->lock);
        if (file->f_flags & O_NONBLOCK || st->meterd_nonblock) {
          return -EAGAIN;
        }
        if (wait_event_interruptible(st->cmd_wq, st->cmd_ready)) {
          return -ERESTARTSYS;
        }
        mutex_lock(&st->lock);
      }

      kbuf.len = st->cmd_len;
      memcpy(kbuf.data, st->cmd_buf, st->cmd_len);
      st->cmd_ready = false; // 已经取走了命令
      mutex_unlock(&st->lock);

      if (copy_to_user((struct sample_buf __user *)arg, &kbuf, sizeof(kbuf))) {
        ret = -EFAULT;
      }
      break;

    case SAMPLE_IOC_PUT_RESULT:
      if (copy_from_user(&kbuf, (struct sample_buf __user *)arg, sizeof(kbuf))) {
        ret = -EFAULT;
        break;
      }

      mutex_lock(&st->lock);
      st->result_len = kbuf.len;
      memcpy(st->result_buf, kbuf.data, kbuf.len);
      st->result_ready = true;
      wake_up_interruptible(&st->result_wq); // 唤醒等待结果的 cat
      mutex_unlock(&st->lock);
      break;

    case SAMPLE_IOC_SET_NONBLOCK:
      mutex_lock(&st->lock);
      st->meterd_nonblock = true;
      mutex_unlock(&st->lock);
      break;

    default:
      ret = -EINVAL;
  }

  return ret;
}

static struct file_operations sample_fops = {
  .owner = THIS_MODULE,
  .open = sample_open,
  .release = sample_release,
  .read = sample_read,
  .write = sample_write,
  .unlocked_ioctl = sample_ioctl,
};

static int __init sample_init(void) {
  int ret;

  st = kzalloc(sizeof(*st), GFP_KERNEL);
  if (!st) {
    return -ENOMEM;
  }

  mutex_init(&st->lock);
  init_waitqueue_head(&st->cmd_wq);
  init_waitqueue_head(&st->result_wq);

  ret = alloc_chrdev_region(&sample_dev, 0, 1, DEVICE_NAME);
  if (ret < 0) {
    kfree(st);
    return ret;
  }

  cdev_init(&sample_cdev, &sample_fops);
  ret = cdev_add(&sample_cdev, sample_dev, 1);
  if (ret < 0) {
    unregister_chrdev_region(sample_dev, 1);
    kfree(st);
    return ret;
  }

  sample_class = class_create(THIS_MODULE, CLASS_NAME);
  if (IS_ERR(sample_class)) {
    cdev_del(&sample_cdev);
    unregister_chrdev_region(sample_dev, 1);
    kfree(st);
    return PTR_ERR(sample_class);
  }

  sample_device = device_create(sample_class, NULL, sample_dev, NULL, DEVICE_NAME);
  if (IS_ERR(sample_device)) {
    class_destroy(sample_class);
    cdev_del(&sample_cdev);
    unregister_chrdev_region(sample_dev, 1);
    kfree(st);
    return PTR_ERR(sample_device);
  }

  pr_info("Sample driver initialized\n");
  return 0;
}

static void __exit sample_exit(void) {
  device_destroy(sample_class, sample_dev);
  class_destroy(sample_class);
  cdev_del(&sample_cdev);
  unregister_chrdev_region(sample_dev, 1);
  kfree(st);
  pr_info("Sample driver exited\n");
}

module_init(sample_init);
module_exit(sample_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("TJR");
MODULE_DESCRIPTION("Sample character device driver for RN8209");