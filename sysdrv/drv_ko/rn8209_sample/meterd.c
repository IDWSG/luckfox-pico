/**
 * @brief 用户空间守护进程, 监听sample驱动, 打开/dev/ttyS3串口, 发送数据到串口,
 * 并接收串口返回的数据
 *
 */
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>

#define SAMPLE_IOC_MAGIC        'S'
#define SAMPLE_IOC_GET_CMD      _IOWR(SAMPLE_IOC_MAGIC, 1, struct sample_buf)
#define SAMPLE_IOC_PUT_RESULT   _IOW(SAMPLE_IOC_MAGIC, 2, struct sample_buf)
#define SAMPLE_IOC_SET_NONBLOCK _IO(SAMPLE_IOC_MAGIC, 3)

#define SAMPLE_CMD_MAX     64
#define SAMPLE_RESULT_MAX  128

struct sample_buf {
    unsigned int len;
    char         data[SAMPLE_CMD_MAX];
};

#define SAMPLE_DEV "/dev/sample"
#define SERIAL_DEV "/dev/ttyS3"

static volatile int running = 1;
static int sample_fd = -1;
static int serial_fd = -1;

static int serial_open(const char *dev) {
  int fd = open(dev, O_RDWR | O_NOCTTY | O_NONBLOCK);
  if (fd < 0) {
    fprintf(stderr, "Failed to open %s: %s\n", dev, strerror(errno));
    return -1;
  }

  struct termios options;
  tcgetattr(fd, &options);
  cfsetispeed(&options, B9600);
  cfsetospeed(&options, B9600);
  options.c_cflag |= (CLOCAL | CREAD);
  options.c_cflag &= ~PARENB;
  options.c_cflag &= ~CSTOPB;
  options.c_cflag &= ~CSIZE;
  options.c_cflag |= CS8;
  tcsetattr(fd, TCSANOW, &options);

  return fd;
}

static char *rn8209_query(const char *cmd){
  unsigned char tx[16];
  unsigned char rx[32];
  int tx_len = 0, n, i;
  char *result = NULL;

  if (strncmp(cmd, "voltage", 7) == 0) {
  } else if (strncmp(cmd, "current", 7) == 0) {
  } else if (strncmp(cmd, "power", 5) == 0) {
  } else if (strncmp(cmd, "energy", 6) == 0) {
  } else {
    fprintf(stderr, "Unknown command: %s\n", cmd);
    return NULL;
  }

  // 发送命令
  n = write(serial_fd, tx, tx_len);
  if (n != tx_len) {
    fprintf(stderr, "Failed to write to serial: %s\n", strerror(errno));
    return NULL;
  }

  // 等回复
  fd_set readfds;
  struct timeval timeout = {.tv_sec = 0, .tv_usec = 200000}; // 200ms
  FD_ZERO(&readfds);
  FD_SET(serial_fd, &readfds);

  n = select(serial_fd + 1, &readfds, NULL, NULL, &timeout);
  if (n < 0) {
    return NULL;
  }
}



int main(void) {
  struct sample_buf kbuf;
  char cmd[SAMPLE_CMD_MAX];

  sample_fd = open(SAMPLE_DEV, O_RDWR);
  if (sample_fd < 0) {
    fprintf(stderr, "Failed to open %s: %s\n", SAMPLE_DEV, strerror(errno));
    return 1;
  }
  ioctl(sample_fd, SAMPLE_IOC_SET_NONBLOCK, &kbuf);

  // 打开串口
  serial_fd = serial_open(SERIAL_DEV);
  if (serial_fd < 0) {
    fprintf(stderr, "Failed to open %s: %s\n", SERIAL_DEV, strerror(errno));
    close(sample_fd);
    return 1;
  }

  while (running) {
    // 从sample驱动获取命令
    memset(&kbuf, 0, sizeof(kbuf));
    kbuf.len = sizeof(kbuf.data);
    if (ioctl(sample_fd, SAMPLE_IOC_GET_CMD, &kbuf) < 0) {
      if (errno == EAGAIN) {
        usleep(100000); // 没有命令，稍等一下
        continue;
      } else {
        fprintf(stderr, "Failed to get command: %s\n", strerror(errno));
        break;
      }
    }

    // 将命令发送到sample驱动
    memcpy(cmd, kbuf.data, kbuf.len);
    cmd[kbuf.len] = '\0';
    cmd[strcspn(cmd, "\r\n")] = 0; // 去掉尾部换行

    char *result = rn8209_query(cmd);
    if (!result) {
      fprintf(stderr, "Failed to query RN8209: %s\n", cmd);
    }

    // 将结果发送回sample驱动
    memset(&kbuf, 0, sizeof(kbuf));
    kbuf.len = strlen(result);
    if (kbuf.len >= sizeof(kbuf.data)) {
      kbuf.len = sizeof(kbuf.data) - 1;
    }
    memcpy(kbuf.data, result, kbuf.len);

    if (ioctl(sample_fd, SAMPLE_IOC_PUT_RESULT, &kbuf) < 0) {
      fprintf(stderr, "Failed to put result: %s\n", strerror(errno));
      break;
    }
  }


  close(sample_fd);
  close(serial_fd);
  return 0;
}