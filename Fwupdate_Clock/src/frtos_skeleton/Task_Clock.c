/***********************************************************************************************************************
* DISCLAIMER
* This software is supplied by Renesas Electronics Corporation and is only intended for use with Renesas products. No
* other uses are authorized. This software is owned by Renesas Electronics Corporation and is protected under all
* applicable laws, including copyright laws.
* THIS SOFTWARE IS PROVIDED "AS IS" AND RENESAS MAKES NO WARRANTIES REGARDING
* THIS SOFTWARE, WHETHER EXPRESS, IMPLIED OR STATUTORY, INCLUDING BUT NOT LIMITED TO WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. ALL SUCH WARRANTIES ARE EXPRESSLY DISCLAIMED. TO THE MAXIMUM
* EXTENT PERMITTED NOT PROHIBITED BY LAW, NEITHER RENESAS ELECTRONICS CORPORATION NOR ANY OF ITS AFFILIATED COMPANIES
* SHALL BE LIABLE FOR ANY DIRECT, INDIRECT, SPECIAL, INCIDENTAL OR CONSEQUENTIAL DAMAGES FOR ANY REASON RELATED TO THIS
* SOFTWARE, EVEN IF RENESAS OR ITS AFFILIATES HAVE BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.
* Renesas reserves the right, without notice, to make changes to this software and to discontinue the availability of
* this software. By using this software, you agree to the additional terms and conditions found by accessing the
* following link:
* http://www.renesas.com/disclaimer
*
* Copyright (C) 2019 Renesas Electronics Corporation. All rights reserved.
***********************************************************************************************************************/
/***********************************************************************************************************************
Includes   <System Includes> , "Project Includes"
***********************************************************************************************************************/
#include "task_function.h"
/* Start user code for import. Do not edit comment generated here */
#include "platform.h"
#include "r_rtc_rx_if.h"
#include "message_buffer.h"
#include "ACM1602.h"
#include "stdio.h"
#include "string.h"

MessageBufferHandle_t			g_ClockCallbackMessageBufferHandle = NULL;
const char* Week[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };

//***********************************************************************
// Clockコールバック
//***********************************************************************
void RTC_Callback(void *pArgs)
{
	BaseType_t 			bHigherPriorityTaskWoken = pdFALSE;
	rtc_cb_evt_t		eRtcEvent = RTC_EVT_PERIODIC;


	eRtcEvent = *(rtc_cb_evt_t *)pArgs;

	xMessageBufferSendFromISR(g_ClockCallbackMessageBufferHandle, &eRtcEvent,sizeof(eRtcEvent), &bHigherPriorityTaskWoken);
	portYIELD_FROM_ISR(bHigherPriorityTaskWoken);
}
/* End user code. Do not edit comment generated here */

void Task_Clock(void * pvParameters)
{
/* Start user code for function. Do not edit comment generated here */
	rtc_err_t			eRtcResult = RTC_SUCCESS;
	rtc_init_t			tRtcInit;
	rtc_cb_evt_t		eRtcEvent = RTC_EVT_PERIODIC;
	tm_t				tDateTime;
	char				szBuff[16 + 1] = { 0x00 };
	int					Backup_tm_yday = 999;


	ACM1602_Initalize();

	g_ClockCallbackMessageBufferHandle = xMessageBufferCreate(sizeof(rtc_cb_evt_t) * 5);
	if (g_ClockCallbackMessageBufferHandle == NULL)
	{
		printf("xMessageBufferCreate Error!\n");
		while(1);
	}

	// ACM1602
	ACM1602_ClearDisplay();
	ACM1602_SetFunction(1,1,0);
	ACM1602_DisplayControl(1,0,0);
	ACM1602_SetEntryMode(1,0);

	// RTCオープン
	tRtcInit.p_callback = RTC_Callback;				// コールバック関数
	tRtcInit.output_freq = RTC_OUTPUT_OFF;			// クロック出力周波数
	tRtcInit.periodic_freq = RTC_PERIODIC_1_HZ;		// 周期割込みの周期(2Hz:500ms)
	tRtcInit.periodic_priority = 10;				// 周期割込みの優先レベル(0 - 15 : 0=割込み禁止)
	tRtcInit.set_time = true;						// RTC初期化および日時設定を実行 or スキップ（true：実行, false：スキップ）

	// 日時を「2020/10/03-23:59:00 (土)」に設定
	tDateTime.tm_year = 120;						// 年(100 - 199, 100=2000年)
	tDateTime.tm_mon = 9;							// 月(0 - 11, 0=1月)
	tDateTime.tm_mday = 3;							// 日(1 - 31)
	tDateTime.tm_hour = 23;							// 時(0 - 23)
	tDateTime.tm_min = 59;							// 分(0 - 59)
	tDateTime.tm_sec = 0;							// 秒(0 - 59)
	tDateTime.tm_wday = 6;							// 曜日(0 - 6, 0 = 日曜日)
	tDateTime.tm_yday = 0;							// 日にち
	tDateTime.tm_isdst = 0;							// 夏時間
	eRtcResult = R_RTC_Open(&tRtcInit, &tDateTime);
	if (eRtcResult != RTC_SUCCESS)
	{
		printf("R_RTC_Open Error! [eRtcResult:%d]\n", eRtcResult);
		while(1);
	}

	while(1)
	{
		// RTCコールバック待ち
		xMessageBufferReceive(g_ClockCallbackMessageBufferHandle, &eRtcEvent, sizeof(eRtcEvent), portMAX_DELAY);
		if (eRtcEvent == RTC_EVT_PERIODIC)
		{
			// 現在日時を取得
			eRtcResult = R_RTC_Read(&tDateTime, NULL);
			if (eRtcResult == RTC_SUCCESS)
			{
#if 0
				sprintf(szBuff,"%04d/%02d/%02d %02d:%02d:%02d",
						(tDateTime.tm_year +1900), (tDateTime.tm_mon + 1), tDateTime.tm_mday,
						tDateTime.tm_hour, tDateTime.tm_min, tDateTime.tm_sec );
				printf("%s\n",szBuff);
#endif
				if (tDateTime.tm_yday != Backup_tm_yday)
				{
					Backup_tm_yday = tDateTime.tm_yday;

					sprintf(szBuff,"%04d/%02d/%02d [%s]", (tDateTime.tm_year +1900), (tDateTime.tm_mon + 1), tDateTime.tm_mday, Week[tDateTime.tm_wday]);
					ACM1602_SetLocate(0, 0);
					ACM1602_Print((const uint8_t*)szBuff, strlen(szBuff));
				}

				sprintf(szBuff,"%02d:%02d:%02d",tDateTime.tm_hour, tDateTime.tm_min, tDateTime.tm_sec);
				ACM1602_SetLocate(0, 1);
				ACM1602_Print((const uint8_t*)szBuff, strlen(szBuff));
			}
		}
	}
/* End user code. Do not edit comment generated here */
}
/* Start user code for other. Do not edit comment generated here */
/* End user code. Do not edit comment generated here */
