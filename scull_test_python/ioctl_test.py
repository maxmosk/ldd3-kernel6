import os
from fcntl import ioctl

fd = open("/dev/sculldev", "w")

def _IO(n):
    return (((0) << (((0+8)+8)+14)) | ((ord('k')) << (0+8)) | (((n)) << 0) | ((0) << ((0+8)+8)))

SCULL_IOCRESET = _IO(0)
SCULL_IOCTQUANTUM = _IO(3)
SCULL_IOCQQUANTUM = _IO(7)

ioctl(fd, SCULL_IOCRESET, 0)
ioctl(fd, SCULL_IOCTQUANTUM, 500)
ioctl(fd, SCULL_IOCQQUANTUM, 0)
ioctl(fd, SCULL_IOCRESET, 0)
ioctl(fd, SCULL_IOCQQUANTUM, 0)



