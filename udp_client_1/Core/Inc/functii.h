/*
 * functii.h
 *
 *  Created on: Jun 9, 2024
 *      Author: Stefan
 */

#ifndef INC_FUNCTII_H_
#define INC_FUNCTII_H_
#include "stm32f7xx_hal.h"
#include "lwip.h"
#include "lwip/udp.h"
#include "time.h"

typedef enum{
	false=0,
	true
} bool;
typedef struct {
    uint32_t seconds;
    uint32_t fraction;
} ntp_timestamp_t;

extern ntp_timestamp_t ntp1_for_alarm;
extern ntp_timestamp_t ntp2_for_alarm;
extern bool isRequested;
extern bool isUpdated;

extern void convert_rtc_to_ntp(ntp_timestamp_t * ntp_time);
extern void convert_ntp_to_rtc(ntp_timestamp_t ntp_time);
extern struct tm convert_ntp_to_tm(ntp_timestamp_t ntp_time);
extern void udp_client_connect(void);
extern void udp_client_send(const char* message);
extern void udp_receive_callback(void *arg, struct udp_pcb *pcb, struct pbuf *p, const ip_addr_t *addr, u16_t port);


#endif /* INC_FUNCTII_H_ */
