#ifndef SNULL_DEVICE_H
#define SNULL_DEVICE_H

#define SNULL_RX_INTR 0x0001
#define SNULL_TX_INTR 0x0002

int snull_add_device(void);
void snull_remove_device(void);

#endif