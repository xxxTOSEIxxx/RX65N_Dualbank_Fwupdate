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
#include "r_flash_rx_if.h"
#include "MotorolaStype.h"
#include "string.h"
#include "ACM1602.h"


#include "Global.h"
extern GLOBAL_INFO_TABLE			g_tGlobalInfo;


//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
//#define DEBUG_PRINTF				// プログラム書き込み情報をprintf出力する
//#define NOT_CF_WRITE				// プログラム書き込みを無効にする
//#define NOT_BANK_TOGGLE
//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@


#define READ_FILE_NAME				( "update02.mot" )
#define	CF_WRITE_MIN_SIZE			( 128 )
#define CF_WRITE_BLOCK_SIZE			( CF_WRITE_MIN_SIZE )


typedef struct
{
	uint32_t						StartAddress;
	uint32_t						EndAddress;

	uint8_t							Data[CF_WRITE_BLOCK_SIZE];
	uint32_t						Size;

} CF_WRITE_INFO_TABLE;






typedef struct
{
	STYPE_RECORD_TABLE				tStypeRecord;
	STYPE_FLASH_INFO_TABLE			tStypeFlashInfo;

	CF_WRITE_INFO_TABLE				tCfWriteInfo;

	uint8_t							szACM1602_Buff[32 + 1];

} FWUPDATE_GLOBAL_TABLE;

FWUPDATE_GLOBAL_TABLE				g_Fwupdate;



//---------------------------------------------------------------------------------
// プログラム書込み処理
// 戻り値： 0:書込み成功 , 1:書込み失敗
//---------------------------------------------------------------------------------
uint8_t Cf_Write(const CF_WRITE_INFO_TABLE* ptCfWriteInfo)
{
	uint8_t							iRet = 0;
	flash_err_t						eFlashResult = FLASH_SUCCESS;
	uint32_t						WriteAddress = 0x00000000;


	WriteAddress = ptCfWriteInfo->StartAddress - (FLASH_CF_HI_BANK_LO_ADDR - FLASH_CF_LO_BANK_LO_ADDR);

#ifdef DEBUG_PRINTF
	printf("---[CF_WRITE_INFO]--------------------------------------------------------\n");
	for (uint8_t j = 0; j < (ptCfWriteInfo->Size / 16); j++)
	{
		uint8_t							k = 0;
		printf("[%08X (%08X)] : ", (ptCfWriteInfo->StartAddress + (16 * j)), (WriteAddress +(16 * j)));
		k = ptCfWriteInfo->Size - (16 * j);
		k = (k >= 16) ? 16 : k;
		for (uint8_t i = 0; i < k; i++)
		{
			printf("%02X ", ptCfWriteInfo->Data[(16 * j) + i]);
		}
		printf("\n");
	}
	printf("--------------------------------------------------------------------------\n\n");
#endif	// #ifdef DEBUG_PRINTF

#ifndef NOT_CF_WRITE
	// プログラム書込み
	eFlashResult = R_FLASH_Write(ptCfWriteInfo->Data, WriteAddress, ptCfWriteInfo->Size);
	if (eFlashResult != FLASH_SUCCESS)
	{
		printf("R_FLASH_Write Error. [eFlashResult:%d, Address:%08X]\n",eFlashResult,ptCfWriteInfo->Size);
		iRet = 1;
	}
#endif	// #ifndef NOT_CF_WRITE

	return iRet;
}


DispErrMsg(const char* pszFuncName, void* pErrorNo)
{
	ACM1602_ClearDisplay();
	ACM1602_SetLocate(0,0);
	sprintf(g_Fwupdate.szACM1602_Buff,"%s",pszFuncName);
	ACM1602_Print(g_Fwupdate.szACM1602_Buff,strlen(g_Fwupdate.szACM1602_Buff));
	ACM1602_SetLocate(0,1);
	if (pErrorNo != NULL)
	{
		sprintf(g_Fwupdate.szACM1602_Buff,"Err => %02d", (uint8_t)(*(uint8_t*)pErrorNo));
	}
	else
	{
		sprintf(g_Fwupdate.szACM1602_Buff,"Err => none");
	}
	ACM1602_Print(g_Fwupdate.szACM1602_Buff,strlen(g_Fwupdate.szACM1602_Buff));
}
/* End user code. Do not edit comment generated here */




void Task_Fwupdate(void * pvParameters)
{
/* Start user code for function. Do not edit comment generated here */
	MOTOROLA_STYPE_RESULT_ENUM 		eResult = MOTOROLA_STYPE_RESULT_SUCCESS;
	FRESULT							eFileResult = FR_OK;
	FIL								file;
	flash_err_t						eFlashResult = FLASH_SUCCESS;
	flash_bank_t 					eBankInfo;							// 起動バンク情報
	uint8_t							NewRecordFlag = 0;
	uint8_t							Ret = 0;
	USB_KIND_ENUM					eBackupUsbKind = USB_KIND_DISCONNECT;


	ACM1602_Initalize();

	// SW
	PORTA.PDR.BIT.B2 = 0;
	PORTA.PCR.BIT.B2 = 1;


	memset(g_Fwupdate.szACM1602_Buff, 0x00, sizeof(g_Fwupdate.szACM1602_Buff));
	ACM1602_SetLocate(0,0);
	sprintf(g_Fwupdate.szACM1602_Buff,"Dual Bank");
	ACM1602_Print(g_Fwupdate.szACM1602_Buff,strlen(g_Fwupdate.szACM1602_Buff));
	ACM1602_SetLocate(0,1);
	sprintf(g_Fwupdate.szACM1602_Buff," Fwupdate Test!");
	ACM1602_Print(g_Fwupdate.szACM1602_Buff,strlen(g_Fwupdate.szACM1602_Buff));

	// 必ずメッセージを見せるため、3秒待つ
	vTaskDelay(3000);

	// USB接続待ち
	while(1)
	{
		if (g_tGlobalInfo.eUsbKind == USB_KIND_CONNECT)
		{
			if (eBackupUsbKind != g_tGlobalInfo.eUsbKind)
			{
				eBackupUsbKind = g_tGlobalInfo.eUsbKind;
				ACM1602_ClearDisplay();
				ACM1602_SetLocate(0,0);
				sprintf(g_Fwupdate.szACM1602_Buff,"USB Connect");
				ACM1602_Print(g_Fwupdate.szACM1602_Buff,strlen(g_Fwupdate.szACM1602_Buff));
				ACM1602_SetLocate(0,1);
				sprintf(g_Fwupdate.szACM1602_Buff,"Push SW : FwUp");
				ACM1602_Print(g_Fwupdate.szACM1602_Buff,strlen(g_Fwupdate.szACM1602_Buff));
			}

			// SWが押された場合
			if (PORTA.PIDR.BIT.B2 == 0)
			{
				break;
			}
		}
		else
		{
			if (eBackupUsbKind != g_tGlobalInfo.eUsbKind)
			{
				eBackupUsbKind = g_tGlobalInfo.eUsbKind;
				ACM1602_ClearDisplay();
				ACM1602_SetLocate(0,0);
				sprintf(g_Fwupdate.szACM1602_Buff,"USB Disconnect");
				ACM1602_Print(g_Fwupdate.szACM1602_Buff,strlen(g_Fwupdate.szACM1602_Buff));
				ACM1602_SetLocate(0,1);
				sprintf(g_Fwupdate.szACM1602_Buff,"- Please USB -");
				ACM1602_Print(g_Fwupdate.szACM1602_Buff,strlen(g_Fwupdate.szACM1602_Buff));
			}
		}

		vTaskDelay(100);
	}

	g_tGlobalInfo.eLedKind = LED_KIND_PROCESS;
	ACM1602_ClearDisplay();
	ACM1602_SetLocate(0,0);
	sprintf(g_Fwupdate.szACM1602_Buff,"Fwupdate");
	ACM1602_Print(g_Fwupdate.szACM1602_Buff,strlen(g_Fwupdate.szACM1602_Buff));
	ACM1602_SetLocate(0,1);
	sprintf(g_Fwupdate.szACM1602_Buff," - START -");
	ACM1602_Print(g_Fwupdate.szACM1602_Buff,strlen(g_Fwupdate.szACM1602_Buff));
	vTaskDelay(1000);

	// ファイルオープン
	eFileResult = f_open(&file, READ_FILE_NAME, (FA_OPEN_EXISTING | FA_READ));
	if (eFileResult != FR_OK)
	{
		DispErrMsg("f_open", &eFileResult);
		g_tGlobalInfo.eLedKind = LED_KIND_ERROR;
		goto Task_Fwupdate_EndProc_Label;
	}

	// フラッシュモジュールオープン
	eFlashResult = R_FLASH_Open();
	if (eFlashResult != FLASH_SUCCESS)
	{
		DispErrMsg("R_FLASH_Open", &eFlashResult);
		g_tGlobalInfo.eLedKind = LED_KIND_ERROR;
		goto Task_Fwupdate_EndProc_Label;
	}

#if 0
	// 起動バンク取得
	eFlashResult = R_FLASH_Control(FLASH_CMD_BANK_GET,&eBankInfo);
	if (eFlashResult != FLASH_SUCCESS)
	{
		DispErrMsg("R_FLASH_Control", &eFlashResult);
		g_tGlobalInfo.eLedKind = LED_KIND_ERROR;
		goto Task_Fwupdate_EndProc_Label;
	}
#endif


	// イレース
	eFlashResult = R_FLASH_Erase(FLASH_CF_BLOCK_38,38);
	if (eFlashResult != FLASH_SUCCESS)
	{
		DispErrMsg("R_FLASH_Erase", &eFlashResult);
		g_tGlobalInfo.eLedKind = LED_KIND_ERROR;
		goto Task_Fwupdate_EndProc_Label;
	}

	// モトローラ S-Typeファイルの解析
	while(1)
	{
		// モトローラ S-Typeレコード1件分を読込み
		eResult = ReadStypeRecord(&file,&g_Fwupdate.tStypeRecord, &g_Fwupdate.tStypeFlashInfo);
		if (eResult != MOTOROLA_STYPE_RESULT_SUCCESS)
		{
			if (eResult == MOTOROLA_STYPE_RESULT_FILE_EOF)
			{
				// ループを抜ける
				break;
			}
			else
			{
				DispErrMsg("ReadStypeRecord", &eFlashResult);
				g_tGlobalInfo.eLedKind = LED_KIND_ERROR;
				goto Task_Fwupdate_EndProc_Label;
			}
		}

		NewRecordFlag = 1;
		//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
		// 解析成功したので、ROMにプログラムを書込む（但しS3レコードのみ）
		if (g_Fwupdate.tStypeRecord.eStypeRecordKind == STYPE_RECORD_KIND_S3)
		{
			// 0xFFF00000 - 0xFFFFFFFFの場合のみ、書込みOK!
			if ((g_Fwupdate.tStypeFlashInfo.Address >= FLASH_CF_HI_BANK_LO_ADDR) && (g_Fwupdate.tStypeFlashInfo.Address <= FLASH_CF_HI_BANK_HI_ADDR))
			{
				// プログラム書込み情報に何も設定されていない場合
				if ((g_Fwupdate.tCfWriteInfo.StartAddress == 0) && (g_Fwupdate.tCfWriteInfo.EndAddress == 0))
				{
					g_Fwupdate.tCfWriteInfo.StartAddress = (g_Fwupdate.tStypeFlashInfo.Address / CF_WRITE_BLOCK_SIZE) * CF_WRITE_BLOCK_SIZE;
					g_Fwupdate.tCfWriteInfo.EndAddress = g_Fwupdate.tCfWriteInfo.StartAddress + (CF_WRITE_BLOCK_SIZE - 1);
					g_Fwupdate.tCfWriteInfo.Size = g_Fwupdate.tStypeFlashInfo.Address % CF_WRITE_BLOCK_SIZE;
				}

				// 1バイトずつ、プログラム書込み情報の書込み範囲を超えていないかをチェックする
				for(uint32_t i = 0 ; i < g_Fwupdate.tStypeFlashInfo.DataSize ; i++)
				{
					// プログラム書込み情報範囲内の場合
					if (g_Fwupdate.tCfWriteInfo.EndAddress >= (g_Fwupdate.tStypeFlashInfo.Address + i))
					{
						// 新しいS-Typeレコードの場合
						if (NewRecordFlag == 1)
						{
							NewRecordFlag = 0;
							g_Fwupdate.tCfWriteInfo.Size += (((g_Fwupdate.tStypeFlashInfo.Address + i) - g_Fwupdate.tCfWriteInfo.StartAddress) - g_Fwupdate.tCfWriteInfo.Size);
						}

						// プログラム書込み情報に書込みデータをセット
						g_Fwupdate.tCfWriteInfo.Data[g_Fwupdate.tCfWriteInfo.Size] = g_Fwupdate.tStypeFlashInfo.Data[i];
						g_Fwupdate.tCfWriteInfo.Size++;

						// プログラム書込み情報の書込みサイズが書込みブロックサイズを超えた場合
						if (g_Fwupdate.tCfWriteInfo.Size >= CF_WRITE_BLOCK_SIZE)
						{
							// 書込み処理
							Ret = Cf_Write(&g_Fwupdate.tCfWriteInfo);
							if (Ret != 0)
							{
								printf("Cf_Write Error.\n");
								g_tGlobalInfo.eLedKind = LED_KIND_ERROR;
								goto Task_Fwupdate_EndProc_Label;
							}

							// 書込み情報の書込み範囲を新たに設定・プログラム書込み情報に書込みデータをセット
							g_Fwupdate.tCfWriteInfo.StartAddress = ((g_Fwupdate.tCfWriteInfo.EndAddress + 1) / CF_WRITE_BLOCK_SIZE) * CF_WRITE_BLOCK_SIZE;
							g_Fwupdate.tCfWriteInfo.EndAddress = g_Fwupdate.tCfWriteInfo.StartAddress + (CF_WRITE_BLOCK_SIZE - 1);
							g_Fwupdate.tCfWriteInfo.Size = 0;
							memset(g_Fwupdate.tCfWriteInfo.Data, 0x00, sizeof(g_Fwupdate.tCfWriteInfo.Data));
						}
					}
					// プログラム書込み情報範囲を超えた場合
					else if (g_Fwupdate.tCfWriteInfo.EndAddress < (g_Fwupdate.tStypeFlashInfo.Address + i))
					{
						// 書込み処理
						g_Fwupdate.tCfWriteInfo.Size = CF_WRITE_BLOCK_SIZE;
						Ret = Cf_Write(&g_Fwupdate.tCfWriteInfo);
						if (Ret != 0)
						{
							DispErrMsg("Cf_Write", NULL);
							g_tGlobalInfo.eLedKind = LED_KIND_ERROR;
							goto Task_Fwupdate_EndProc_Label;
						}


						if (i == 0)
						{
							g_Fwupdate.tCfWriteInfo.StartAddress = (g_Fwupdate.tStypeFlashInfo.Address / CF_WRITE_BLOCK_SIZE) * CF_WRITE_BLOCK_SIZE;
							g_Fwupdate.tCfWriteInfo.EndAddress = g_Fwupdate.tCfWriteInfo.StartAddress + (CF_WRITE_BLOCK_SIZE - 1);
							g_Fwupdate.tCfWriteInfo.Size = g_Fwupdate.tStypeFlashInfo.Address % CF_WRITE_BLOCK_SIZE;
							memset(g_Fwupdate.tCfWriteInfo.Data, 0x00, sizeof(g_Fwupdate.tCfWriteInfo.Data));
							g_Fwupdate.tCfWriteInfo.Data[g_Fwupdate.tCfWriteInfo.Size] = g_Fwupdate.tStypeFlashInfo.Data[i];
							g_Fwupdate.tCfWriteInfo.Size++;
						}
						else
						{
							// 書込み情報の書込み範囲を新たに設定・プログラム書込み情報に書込みデータをセット
							g_Fwupdate.tCfWriteInfo.StartAddress = ((g_Fwupdate.tCfWriteInfo.EndAddress + 1) / CF_WRITE_BLOCK_SIZE) * CF_WRITE_BLOCK_SIZE;
							g_Fwupdate.tCfWriteInfo.EndAddress = g_Fwupdate.tCfWriteInfo.StartAddress + (CF_WRITE_BLOCK_SIZE - 1);
							g_Fwupdate.tCfWriteInfo.Size = 0;
							memset(g_Fwupdate.tCfWriteInfo.Data, 0x00, sizeof(g_Fwupdate.tCfWriteInfo.Data));
							g_Fwupdate.tCfWriteInfo.Data[g_Fwupdate.tCfWriteInfo.Size] = g_Fwupdate.tStypeFlashInfo.Data[i];
							g_Fwupdate.tCfWriteInfo.Size++;
						}
					}
					else
					{
						// 処理がここにくることはあり得ないのでエラーとする
						DispErrMsg("Oops...", NULL);
						g_tGlobalInfo.eLedKind = LED_KIND_ERROR;
						goto Task_Fwupdate_EndProc_Label;
					}
				}
			}
		}
		//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
	}

	// 書込み処理
	if ((g_Fwupdate.tCfWriteInfo.StartAddress != 0) && (g_Fwupdate.tCfWriteInfo.EndAddress != 0))
	{
		Ret = Cf_Write(&g_Fwupdate.tCfWriteInfo);
		if (Ret != 0)
		{
			DispErrMsg("Cf_Write", NULL);
			g_tGlobalInfo.eLedKind = LED_KIND_ERROR;
			goto Task_Fwupdate_EndProc_Label;
		}
	}

#ifndef NOT_BANK_TOGGLE
	// 起動バンク変更
    clrpsw_i();
	eFlashResult = R_FLASH_Control(FLASH_CMD_BANK_TOGGLE, NULL);
    setpsw_i();
	if (eFlashResult != FLASH_SUCCESS)
	{
		DispErrMsg("R_FLASH_Control", &eFlashResult);
		g_tGlobalInfo.eLedKind = LED_KIND_ERROR;
		goto Task_Fwupdate_EndProc_Label;
	}
#endif	// #ifndef NOT_BANK_TOGGLE

	ACM1602_ClearDisplay();
	ACM1602_SetLocate(0,0);
	sprintf(g_Fwupdate.szACM1602_Buff,"Fwupdate");
	ACM1602_Print(g_Fwupdate.szACM1602_Buff,strlen(g_Fwupdate.szACM1602_Buff));
	ACM1602_SetLocate(0,1);
	sprintf(g_Fwupdate.szACM1602_Buff,"    Success!");
	ACM1602_Print(g_Fwupdate.szACM1602_Buff,strlen(g_Fwupdate.szACM1602_Buff));
	g_tGlobalInfo.eLedKind = LED_KIND_ON;

Task_Fwupdate_EndProc_Label:

	// ファイルクローズ
	f_close(&file);

	while(1)
	{
		vTaskDelay(100);
	}
/* End user code. Do not edit comment generated here */
}
/* Start user code for other. Do not edit comment generated here */
/* End user code. Do not edit comment generated here */
