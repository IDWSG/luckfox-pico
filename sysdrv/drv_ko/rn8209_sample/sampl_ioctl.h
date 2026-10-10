#ifndef SAMPL_IOCTL_H_
#define SAMPL_IOCTL_H_

#include <linux/ioctl.h>

#define SAMPLE_CMD_MAX   64
#define SAMPLE_RESULT_MAX 128

#define SAMPLE_IOC_MAGIC        'S'

/* meterd 用：放回一条结果 */
#define SAMPLE_IOC_PUT_RESULT   _IOW(SAMPLE_IOC_MAGIC, 2, struct sample_buf)

#define SAMPLE_IOC_GET_CMD      _IOWR(SAMPLE_IOC_MAGIC, 1, struct sample_buf)

/* meterd 用：非阻塞模式开关 */
#define SAMPLE_IOC_SET_NONBLOCK _IO(SAMPLE_IOC_MAGIC, 3)

struct sample_buf {
  unsigned int len;
  char data[SAMPLE_CMD_MAX];
};

#endif // SAMPL_IOCTL_H_