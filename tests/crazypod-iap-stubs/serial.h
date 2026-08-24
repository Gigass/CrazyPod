#ifndef TEST_CRAZYPOD_IAP_SERIAL_H
#define TEST_CRAZYPOD_IAP_SERIAL_H

int tx_rdy(void);
void tx_writec(unsigned char value);
void serial_bitrate(int rate);
int remote_control_rx(void);

#endif
