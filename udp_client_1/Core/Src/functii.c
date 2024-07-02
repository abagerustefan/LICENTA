#include <string.h>
#include "functii.h"
#include "stdint.h"
#include "stm32f7xx_hal_rtc.h"
#include "def.h"
#include "main.h"

#define NTP_UNIX_OFFSET 2208988800U
#define MAX_VALUE_FOR_NTP_FRACTION 4294967295U
#define FRACTIUNE_DE_SECUNDA_RTC 2480

struct udp_pcb *udp_client_pcb;
ntp_timestamp_t ntp1_for_alarm={0};
ntp_timestamp_t ntp2_for_alarm={0};

struct tm timeinfo_for_alarm1;
struct tm timeinfo_for_alarm2;
bool isRequested = false;
bool isUpdated = false;

void convert_rtc_to_ntp(ntp_timestamp_t * ntp_time)
{
	RTC_TimeTypeDef sTime;
	RTC_DateTypeDef sDate;
	struct tm timeinfo;
	time_t unix_time;

	// Preia data si ora curente din RTC
	HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
	HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN);

	// Completeaza structura tm
	timeinfo.tm_hour = sTime.Hours;
	timeinfo.tm_min = sTime.Minutes;
	timeinfo.tm_sec = sTime.Seconds;
	timeinfo.tm_mday = sDate.Date;
	timeinfo.tm_mon = sDate.Month - 1; // struct tm are lunile de la 0 la 11
	timeinfo.tm_year = sDate.Year + 2000 - 1900; // struct tm are anii de la 1900
	timeinfo.tm_isdst = 0;

	// Convertire structura tm in timp UNIX
	unix_time = mktime(&timeinfo);

	// Adaugare offset NTP pentru a obtine timpul in format NTP
	ntp_time->seconds = unix_time + NTP_UNIX_OFFSET;

	// Calculare fractiuni de secunda pentru RTC
	double fractiune = 0 ;
	fractiune = sTime.SubSeconds*1.0 / sTime.SecondFraction;
	ntp_time->fraction = MAX_VALUE_FOR_NTP_FRACTION * fractiune;
}


void convert_ntp_to_rtc(ntp_timestamp_t ntp_time)
{
    RTC_TimeTypeDef sTime;
    RTC_DateTypeDef sDate;
    struct tm timeinfo;
    time_t unix_time;

    // Convertire timp NTP in timp UNIX
    unix_time = ntp_time.seconds - NTP_UNIX_OFFSET;

    // Convertire timpul UNIX intr-o structura tm
    gmtime_r(&unix_time, &timeinfo);

    // Completare structurile RTC_TimeTypeDef si RTC_DateTypeDef
    sTime.Hours = timeinfo.tm_hour;
    sTime.Minutes = timeinfo.tm_min;
    sTime.Seconds = timeinfo.tm_sec;

    sDate.WeekDay = 0;
    sDate.Date = timeinfo.tm_mday;
    sDate.Month = timeinfo.tm_mon + 1; // struct tm are lunile de la 0 la 11
    sDate.Year = timeinfo.tm_year + 1900 - 2000; // struct tm are anii de la 1900

    // Seteaza data si ora in RTC
    HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
    HAL_RTC_SetDate(&hrtc, &sDate, RTC_FORMAT_BIN);

//    isUpdated = true;

    HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
    HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN);
    uint32_t rtc_subseconds = (ntp_time.fraction * 1.0 * FRACTIUNE_DE_SECUNDA_RTC) / MAX_VALUE_FOR_NTP_FRACTION;
    if(sTime.SubSeconds>rtc_subseconds)
    {
        while(sTime.SubSeconds>rtc_subseconds)
        {
            HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
            HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN);
        }
        isUpdated = true;
    }
}

// Functie care converteste din format NTP in format UNIX
struct tm convert_ntp_to_tm(ntp_timestamp_t ntp_time)
{
	time_t unix_time;
	struct tm timeinfo;
	unix_time = ntp_time.seconds - NTP_UNIX_OFFSET;

	gmtime_r(&unix_time, &timeinfo);

	return timeinfo;
}


// Functie care seteaza intrerupere pentru alarma A
void set_alarm_from_serv(ntp_timestamp_t ntp_time)
{
	RTC_AlarmTypeDef sAlarm, getsAlarm;
    struct tm timeinfo;

    // Convertire in format compatibil cu RTC
    timeinfo = convert_ntp_to_tm(ntp_time);

    // Setare parametrii pentru alarma A
	sAlarm.AlarmTime.Hours = timeinfo.tm_hour;
	sAlarm.AlarmTime.Minutes = timeinfo.tm_min;
	sAlarm.AlarmTime.Seconds = timeinfo.tm_sec;
	sAlarm.AlarmTime.SubSeconds = ntp_time.fraction;
	sAlarm.AlarmTime.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
	sAlarm.AlarmTime.StoreOperation = RTC_STOREOPERATION_RESET;
	sAlarm.AlarmMask = RTC_ALARMMASK_DATEWEEKDAY;
	sAlarm.AlarmSubSecondMask = RTC_ALARMSUBSECONDMASK_SS14;
	sAlarm.AlarmDateWeekDaySel = RTC_ALARMDATEWEEKDAYSEL_DATE;
	sAlarm.AlarmDateWeekDay = timeinfo.tm_mday;
	sAlarm.Alarm = RTC_ALARM_A;

	while(HAL_RTC_SetAlarm_IT(&hrtc, &sAlarm, FORMAT_BIN)!=HAL_OK){}

	HAL_RTC_GetAlarm(&hrtc,&getsAlarm,RTC_ALARM_A,FORMAT_BIN);
}


void udp_client_connect(void)
{
    ip_addr_t server_ip;
    err_t err;

    // Creare un nou block UDP
    udp_client_pcb = udp_new();
	ip_addr_t myIPaddr;
	IP_ADDR4(&myIPaddr, 192, 168, 137, 110);
	udp_bind(udp_client_pcb, &myIPaddr, 1234);
    if (udp_client_pcb == NULL) {
        return;
    }

    // Setare IP-ului serverului
    IP4_ADDR(&server_ip, 192, 168, 137, 1);

    // Conectare la server
    err = udp_connect(udp_client_pcb, &server_ip, 1234); // Server port
    if (err != ERR_OK) {
        printf("Cannot connect to server. Error: %d\n", err);
        udp_remove(udp_client_pcb);
        udp_client_pcb = NULL;
    }

    // Setare functie de callback
    udp_recv(udp_client_pcb, udp_receive_callback, NULL);
}

void udp_client_send(const char* message)
{
    struct pbuf *p;

    // Alocare pbuf
    p = pbuf_alloc(PBUF_TRANSPORT, strlen(message), PBUF_RAM);
    if (p == NULL) {
        return;
    }

    // Copiaza mesajul parametru in pbuf
    memcpy(p->payload, message, strlen(message));

    // Trimitere pbuf
    if (udp_send(udp_client_pcb, p) != ERR_OK) {
    	return;
    }

    /* Free pbuf */
    pbuf_free(p);
}

void udp_receive_callback(void *arg, struct udp_pcb *pcb, struct pbuf *p, const ip_addr_t *addr, u16_t port)
{
	// Verificare pbuf daca este gol
    if (p != NULL)
    {
    	HAL_GPIO_TogglePin(LD2_GPIO_Port, LD2_Pin);
    	HAL_Delay(2);
    	HAL_GPIO_TogglePin(LD2_GPIO_Port, LD2_Pin);
    	// Verificare daca lungimea este de 16 bytes
        if (p->len == 16)
        {
            uint64_t ntp_time1, ntp_time2;
            uint32_t sec, frac;

            // Parsare date in doua structuri ntp
            memcpy(&ntp_time1, p->payload, 8);
            memcpy(&ntp_time2, (uint8_t*)p->payload + 8, 8);

            frac = (uint32_t)(ntp_time1 >> 32);
            sec = (uint32_t)ntp_time1;

            // Convertire din network byte order to host byte order
            sec = lwip_ntohl(sec);
            frac =  lwip_ntohl(frac);

            // Setare parametrii
            ntp1_for_alarm.seconds = sec;
            ntp1_for_alarm.fraction = (frac* 1.0 * FRACTIUNE_DE_SECUNDA_RTC) / MAX_VALUE_FOR_NTP_FRACTION;

            // Setare alarma A pentru T0
            set_alarm_from_serv(ntp1_for_alarm);
            ntp1_for_alarm.seconds = 0;
            ntp1_for_alarm.fraction = 0;

            frac = (uint32_t)(ntp_time2 >> 32);
            sec = (uint32_t)ntp_time2;

            sec = lwip_ntohl(sec);
            frac = lwip_ntohl(frac);

            ntp2_for_alarm.seconds = sec;
            ntp2_for_alarm.fraction = (frac* 1.0 * FRACTIUNE_DE_SECUNDA_RTC) / MAX_VALUE_FOR_NTP_FRACTION;
        }
        pbuf_free(p);
    }

}

void HAL_RTC_AlarmAEventCallback(RTC_HandleTypeDef *hrtc)
{
    RTC_TimeTypeDef sTime;
    RTC_DateTypeDef sDate;
    HAL_RTC_GetTime(hrtc, &sTime, RTC_FORMAT_BIN);
    HAL_RTC_GetDate(hrtc, &sDate, RTC_FORMAT_BIN);

	if(ntp2_for_alarm.seconds == 0 && ntp2_for_alarm.fraction == 0)
	{
		isRequested = false;
//		isUpdated = false;
	}
	else
	{
		//Setare alarma A pentru T1
		set_alarm_from_serv(ntp2_for_alarm);
		ntp2_for_alarm.seconds = 0;
		ntp2_for_alarm.fraction = 0;
	}
	HAL_GPIO_TogglePin(LD1_GPIO_Port, LD1_Pin);
	HAL_GPIO_TogglePin(LD3_GPIO_Port, LD3_Pin);
}
