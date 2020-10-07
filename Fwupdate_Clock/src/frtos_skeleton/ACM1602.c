/*
 * ACM1602.c
 *
 *  Created on: 2020/10/03
 *      Author: MIBC
 */
#include "ACM1602.h"
#include "stdio.h"

//#define DEBUG_PRINTF

#define ACM1602_I2C_CH				( 12 )						// I2Cのチャンネル
#define ACM1602_SLAVE_ADDRESS		( 0x50 )					// スレーブアドレス
#define ACM1602_WAIT_TIMEOUT		( 3000 )					// コールバック関数タイムアウト(ms)
#define ACM1602_RETRY_COUNT			( 3 )						// リトライ回数

// ACM1602用グローバル変数
typedef struct
{
	sci_iic_info_t					tSciI2cInfo;				// 簡易I2Cハンドル
	uint8_t							SlaveAddr;					// ACM1602スレーブアドレス
	SemaphoreHandle_t				pCallbackSemaphorHandle;	// コールバック待ち用セマフォ
} ACM1602_GLOBAL_INFO_TABLE;

static ACM1602_GLOBAL_INFO_TABLE 	g_tACM1602;


//------------------------
// プロトタイプ宣言
//------------------------
// ACM1602 コールバック
void ACM1602_Callback(void);
// マスター送信処理
sci_iic_return_t master_send(sci_iic_info_t *p_sci_iic_info);
// DDRAMアドレス設定
sci_iic_return_t ACM1602_SetDdramAddress(uint8_t Addr);
// データ書込み
static sci_iic_return_t ACM1602_WriteDataRam(uint8_t Data);


//========================================================================
// ACM1602 初期化
// ※ACM1602を使用する場合は、処理開始ルーチンで1回だけコールしてください
//========================================================================
sci_iic_return_t ACM1602_Initalize(void)
{
	sci_iic_return_t				eSciI2cResult = SCI_IIC_SUCCESS;


	g_tACM1602.SlaveAddr = ACM1602_SLAVE_ADDRESS;

	// コールバック同期用セマフォ生成
	g_tACM1602.pCallbackSemaphorHandle = xSemaphoreCreateBinary();
	if (g_tACM1602.pCallbackSemaphorHandle == NULL)
	{
#ifdef DEBUG_PRINTF
		printf("xSemaphoreCreateBinary Error.\n");
#endif 	// #ifdef DEBUG_PRINTF
		return SCI_IIC_ERR_OTHER;
	}

	// 簡易I2Cモード初期化
	g_tACM1602.tSciI2cInfo.ch_no = ACM1602_I2C_CH;
	g_tACM1602.tSciI2cInfo.p_slv_adr = &g_tACM1602.SlaveAddr;
	g_tACM1602.tSciI2cInfo.dev_sts = SCI_IIC_NO_INIT;
	g_tACM1602.tSciI2cInfo.p_data1st = NULL;
	g_tACM1602.tSciI2cInfo.cnt1st = 0;
	g_tACM1602.tSciI2cInfo.p_data2nd = NULL;
	g_tACM1602.tSciI2cInfo.cnt2nd = 0;
	g_tACM1602.tSciI2cInfo.callbackfunc = ACM1602_Callback;
	eSciI2cResult = R_SCI_IIC_Open(&g_tACM1602.tSciI2cInfo);
	if (eSciI2cResult != SCI_IIC_SUCCESS)
	{
#ifdef DEBUG_PRINTF
		printf("R_SCI_IIC_Open Error. [eSciI2cResult:%d]\n", eSciI2cResult);
#endif 	// #ifdef DEBUG_PRINTF
		return eSciI2cResult;
	}

	ACM1602_ClearDisplay();
	ACM1602_SetFunction(1,1,0);
	ACM1602_DisplayControl(1,0,0);
	ACM1602_SetEntryMode(1,0);

	return SCI_IIC_SUCCESS;
}


//========================================================================
// ディスプレイクリア
//========================================================================
sci_iic_return_t ACM1602_ClearDisplay(void)
{
	sci_iic_return_t				eSciI2cResult = SCI_IIC_SUCCESS;
	uint8_t							Command[2] = { 0x00, 0x01 };


	// マスター送信
	g_tACM1602.tSciI2cInfo.p_data1st = &Command[0];
	g_tACM1602.tSciI2cInfo.cnt1st = 1;
	g_tACM1602.tSciI2cInfo.p_data2nd = &Command[1];
	g_tACM1602.tSciI2cInfo.cnt2nd = 1;
	eSciI2cResult = master_send(&g_tACM1602.tSciI2cInfo);
	if (eSciI2cResult != SCI_IIC_SUCCESS)
	{
#ifdef DEBUG_PRINTF
		printf("master_send Error. [eSciI2cResult:%d]\n", eSciI2cResult);
#endif // #ifdef DEBUG_PRINTF
		return eSciI2cResult;
	}

	return eSciI2cResult;
}


//========================================================================
// カーソルリセット
//========================================================================
sci_iic_return_t ACM1602_ReturnHome(void)
{
	sci_iic_return_t				eSciI2cResult = SCI_IIC_SUCCESS;
	uint8_t							Command[2] = { 0x00, 0x02 };


	// マスター送信
	g_tACM1602.tSciI2cInfo.p_data1st = &Command[0];
	g_tACM1602.tSciI2cInfo.cnt1st = 1;
	g_tACM1602.tSciI2cInfo.p_data2nd = &Command[1];
	g_tACM1602.tSciI2cInfo.cnt2nd = 1;
	eSciI2cResult = master_send(&g_tACM1602.tSciI2cInfo);
	if (eSciI2cResult != SCI_IIC_SUCCESS)
	{
#ifdef DEBUG_PRINTF
		printf("master_send Error. [eSciI2cResult:%d]\n", eSciI2cResult);
#endif // #ifdef DEBUG_PRINTF
		return eSciI2cResult;
	}

	return eSciI2cResult;
}


//========================================================================
// エントリモード設定
//    CoursorMobing : カーソル移動有無 [0：OFF / 1:ON]
//    EnableShift   : シフト有無             [0:OFF / 1:ON]
//========================================================================
sci_iic_return_t ACM1602_SetEntryMode(uint8_t CursorMoving, uint8_t EnableShift)
{
	sci_iic_return_t				eSciI2cResult = SCI_IIC_SUCCESS;
	uint8_t							Command[2] = { 0x00, 0x04 };


	// CursorMoving(0:OFF, 1:ON)
	Command[1] |= ((CursorMoving != 0) ? 0x02 : 0x00);

	// EnableShift(0:OFF, 1:ON)
	Command[1] |= ((EnableShift != 0) ? 0x01 : 0x00);

	// マスター送信
	g_tACM1602.tSciI2cInfo.p_data1st = &Command[0];
	g_tACM1602.tSciI2cInfo.cnt1st = 1;
	g_tACM1602.tSciI2cInfo.p_data2nd = &Command[1];
	g_tACM1602.tSciI2cInfo.cnt2nd = 1;
	eSciI2cResult = master_send(&g_tACM1602.tSciI2cInfo);
	if (eSciI2cResult != SCI_IIC_SUCCESS)
	{
#ifdef DEBUG_PRINTF
		printf("master_send Error. [eSciI2cResult:%d]\n", eSciI2cResult);
#endif // #ifdef DEBUG_PRINTF
		return eSciI2cResult;
	}

	return eSciI2cResult;
}


//========================================================================
// ディスプレイ制御
//     Display : ディスプレイON/OFF  [0:OFF / 1:ON]
//     Cursor  : カーソルON/OFF    [0:OFF / 1:ON]
//     Blink   : カーソル点滅ON/OFF [0:OFF / 1:ON]
//========================================================================
sci_iic_return_t ACM1602_DisplayControl(uint8_t Display, uint8_t Cursor, uint8_t Blink)
{
	sci_iic_return_t				eSciI2cResult = SCI_IIC_SUCCESS;
	uint8_t							Command[2] = { 0x00, 0x08 };


	// Display(0:OFF, 1:ON)
	Command[1] |= ((Display != 0) ? 0x04 : 0x00);

	// Cursor(0:OFF, 1:ON)
	Command[1] |= ((Cursor != 0) ? 0x02 : 0x00);

	// Blink(0:OFF, 1:ON)
	Command[1] |= ((Blink != 0) ? 0x01 : 0x00);

	// マスター送信
	g_tACM1602.tSciI2cInfo.p_data1st = &Command[0];
	g_tACM1602.tSciI2cInfo.cnt1st = 1;
	g_tACM1602.tSciI2cInfo.p_data2nd = &Command[1];
	g_tACM1602.tSciI2cInfo.cnt2nd = 1;
	eSciI2cResult = master_send(&g_tACM1602.tSciI2cInfo);
	if (eSciI2cResult != SCI_IIC_SUCCESS)
	{
#ifdef DEBUG_PRINTF
		printf("master_send Error. [eSciI2cResult:%d]\n", eSciI2cResult);
#endif // #ifdef DEBUG_PRINTF
		return eSciI2cResult;
	}

	return eSciI2cResult;
}


//========================================================================
// ファンクション設定
//     DataLentgh  : データ長                [0:4-bit / 1:8-bit]
//     DisplayLine : ディスプレイライン   [0:1-Line / 1:2-Line]
//     DisplayFont : ディスプレイフォント [0:5x8 dots / 1:5x10 dots]
//========================================================================
sci_iic_return_t ACM1602_SetFunction(uint8_t DataLength, uint8_t DisplayLine, uint8_t DisplayFont)
{
	sci_iic_return_t				eSciI2cResult = SCI_IIC_SUCCESS;
	uint8_t							Command[2] = { 0x00, 0x20 };

	// DataLength(0:4-bit, 1:8-bit)
	Command[1] |= ((DataLength != 0) ? 0x10 : 0x00);

	// DisplayLine(0:1-Line, 1:2-Line)
	Command[1] |= ((DisplayLine != 0) ? 0x08 : 0x00);

	// DisplayFont(0:5x8dots, 1:5x10dots)
	Command[1] |= ((DisplayFont != 0) ? 0x04 : 0x00);

	// マスター送信
	g_tACM1602.tSciI2cInfo.p_data1st = &Command[0];
	g_tACM1602.tSciI2cInfo.cnt1st = 1;
	g_tACM1602.tSciI2cInfo.p_data2nd = &Command[1];
	g_tACM1602.tSciI2cInfo.cnt2nd = 1;
	eSciI2cResult = master_send(&g_tACM1602.tSciI2cInfo);
	if (eSciI2cResult != SCI_IIC_SUCCESS)
	{
#ifdef DEBUG_PRINTF
		printf("master_send Error. [eSciI2cResult:%d]\n", eSciI2cResult);
#endif // #ifdef DEBUG_PRINTF
		return eSciI2cResult;
	}

	return eSciI2cResult;
}


//========================================================================
// カーソル位置設定
//     x : ディスプレイX座標 [0 - 15]
//     y : ディスプレイy座標 [0 - 1]
//========================================================================
sci_iic_return_t ACM1602_SetLocate(uint8_t x, uint8_t y)
{
	sci_iic_return_t				eSciI2cResult = SCI_IIC_SUCCESS;
	uint8_t							Addr = 0x00;


	if (x > 15)
	{
		return SCI_IIC_ERR_INVALID_ARG;
	}
	Addr |= x;

	if (y > 1)
	{
		return SCI_IIC_ERR_INVALID_ARG;
	}
	Addr |= ((y == 1) ? 0x40 : 0x00);

	// DDRAMアドレス設定
	eSciI2cResult = ACM1602_SetDdramAddress(Addr);
	if (eSciI2cResult != SCI_IIC_SUCCESS)
	{
#ifdef DEBUG_PRINTF
		printf("ACM1602_SetDdramAddress Error. [eSciI2cResult:%d]\n", eSciI2cResult);
#endif // #ifdef DEBUG_PRINTF
		return eSciI2cResult;
	}

	return eSciI2cResult;
}


//========================================================================
// 表示
//     *pData   : 表示データ
//     DataSize : 表示データ数
//========================================================================
sci_iic_return_t ACM1602_Print(const uint8_t *pData, uint8_t DataSize)
{
	sci_iic_return_t				eSciI2cResult = SCI_IIC_SUCCESS;


	for(uint8_t i = 0 ; i < DataSize ; i++)
	{
		eSciI2cResult = ACM1602_WriteDataRam(pData[i]);
		if (eSciI2cResult != SCI_IIC_SUCCESS)
		{
#ifdef DEBUG_PRINTF
			printf("ACM1602_WriteDataRam Error. [eSciI2cResult:%d, i:%d]\n", eSciI2cResult, i);
#endif // #ifdef DEBUG_PRINTF
			break;
		}
	}

	return eSciI2cResult;
}





//========================================================================
// DDRAMアドレス設定
//========================================================================
static sci_iic_return_t ACM1602_SetDdramAddress(uint8_t Addr)
{
	sci_iic_return_t				eSciI2cResult = SCI_IIC_SUCCESS;
	uint8_t							Command[2] = { 0x00, 0x80 };


	// 引数チェック
	if (!(((Addr >= 0x40) && (Addr <= 0x4F)) || (Addr <= 0x0F)))
	{
		return SCI_IIC_ERR_INVALID_ARG;
	}
	Command[1] |= (0x7F & Addr);

	// マスター送信
	g_tACM1602.tSciI2cInfo.p_data1st = &Command[0];
	g_tACM1602.tSciI2cInfo.cnt1st = 1;
	g_tACM1602.tSciI2cInfo.p_data2nd = &Command[1];
	g_tACM1602.tSciI2cInfo.cnt2nd = 1;
	eSciI2cResult = master_send(&g_tACM1602.tSciI2cInfo);
	if (eSciI2cResult != SCI_IIC_SUCCESS)
	{
#ifdef DEBUG_PRINTF
		printf("master_send Error. [eSciI2cResult:%d]\n", eSciI2cResult);
#endif // #ifdef DEBUG_PRINTF
		return eSciI2cResult;
	}

	return eSciI2cResult;
}

//========================================================================
// データ書込み
//========================================================================
static sci_iic_return_t ACM1602_WriteDataRam(uint8_t Data)
{
	sci_iic_return_t				eSciI2cResult = SCI_IIC_SUCCESS;
	uint8_t							Command[2] = { 0x80, 0x00 };


	Command[1] = Data;

	// マスター送信
	g_tACM1602.tSciI2cInfo.p_data1st = &Command[0];
	g_tACM1602.tSciI2cInfo.cnt1st = 1;
	g_tACM1602.tSciI2cInfo.p_data2nd = &Command[1];
	g_tACM1602.tSciI2cInfo.cnt2nd = 1;
	eSciI2cResult = master_send(&g_tACM1602.tSciI2cInfo);
	if (eSciI2cResult != SCI_IIC_SUCCESS)
	{
#ifdef DEBUG_PRINTF
		printf("master_send Error. [eSciI2cResult:%d]\n", eSciI2cResult);
#endif // #ifdef DEBUG_PRINTF
		return eSciI2cResult;
	}

	return eSciI2cResult;
}



//========================================================================
// ACM1602 コールバック
//========================================================================
static void ACM1602_Callback(void)
{
	BaseType_t 						bHigherPriorityTaskWoken = pdFALSE;


	xSemaphoreGiveFromISR(g_tACM1602.pCallbackSemaphorHandle,&bHigherPriorityTaskWoken);
	portYIELD_FROM_ISR(bHigherPriorityTaskWoken);
}


//========================================================================
// マスター送信処理
//========================================================================
static sci_iic_return_t master_send(sci_iic_info_t *p_sci_iic_info)
{
	sci_iic_return_t				eSciI2cResult = SCI_IIC_SUCCESS;
	sci_iic_info_t					tSciI2cInfo;
	sci_iic_mcu_status_t			tSciI2cStatus;
	uint8_t							RetryCount = 0;
	uint8_t							Loop = 1;
	BaseType_t 						bRet = pdFALSE;


	while (Loop)
	{
		// リトライ回数オーバー？
		if (RetryCount >= ACM1602_RETRY_COUNT)
		{
#ifdef DEBUG_PRINTF
			printf( "Retry Count Error.\n");
#endif	// #ifdef DEBUG_PRINTF
			return SCI_IIC_ERR_OTHER;
		}

		// マスター送信
#ifdef DEBUG_PRINTF
		printf( "Command[0]:0x%02X, Command[1]:0x%02X\n", *p_sci_iic_info->p_data1st, *p_sci_iic_info->p_data2nd);
#endif // #ifdef DEBUG_PRINTF
		eSciI2cResult = R_SCI_IIC_MasterSend(p_sci_iic_info);
		if (eSciI2cResult != SCI_IIC_SUCCESS)
		{
#ifdef DEBUG_PRINTF
			printf("R_SCI_IIC_MasterSend Error. [eSciI2cResult:%d]\n", eSciI2cResult);
#endif // #ifdef DEBUG_PRINTF
			return eSciI2cResult;
		}

		// コールバック応答待ち
		bRet = xSemaphoreTake(g_tACM1602.pCallbackSemaphorHandle, ACM1602_WAIT_TIMEOUT);
		if (bRet == pdFALSE)
		{
#ifdef DEBUG_PRINTF
			printf("xSemaphoreTake Error.\n");
#endif // #ifdef DEBUG_PRINTF
			return SCI_IIC_ERR_OTHER;
		}

		// 状態取得
		tSciI2cInfo.ch_no = ACM1602_I2C_CH;
		eSciI2cResult = R_SCI_IIC_GetStatus(&tSciI2cInfo,&tSciI2cStatus);
		if (eSciI2cResult != SCI_IIC_SUCCESS)
		{
#ifdef DEBUG_PRINTF
			printf("R_SCI_IIC_GetStatus Error. [eSciI2cResult:%d]\n", eSciI2cResult);
#endif // #ifdef DEBUG_PRINTF
			RetryCount++;
			continue;
		}
		// NACK?
		if (tSciI2cStatus.BIT.NACK == 1)
		{
#ifdef DEBUG_PRINTF
			printf("[NACK]\n");
#endif // #ifdef DEBUG_PRINTF
			RetryCount++;
			continue;
		}

		switch(p_sci_iic_info->dev_sts){
		case SCI_IIC_FINISH:
			eSciI2cResult = SCI_IIC_SUCCESS;
			Loop = 0;
			break;
		case SCI_IIC_NO_INIT:
		case SCI_IIC_IDLE:
		case SCI_IIC_NACK:
		case SCI_IIC_COMMUNICATION:
		case SCI_IIC_ERROR:
		default:
			eSciI2cResult = SCI_IIC_ERR_OTHER;
#ifdef DEBUG_PRINTF
			printf("Status Error. [p_sci_iic_info->dev_sts:%d]\n",p_sci_iic_info->dev_sts);
#endif // #ifdef DEBUG_PRINTF
			RetryCount++;
			continue;
		}
	}

	return eSciI2cResult;
}




