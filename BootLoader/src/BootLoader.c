/*
 * BootLoader.c
 *
 *  Created on: 2017年4月19日
 *      Author: admin
 */
#include "BootLoader.h"
#define DATA_LEN  520
typedef  void (*pFunction)(void);
bootloader_data Boot_ID_info;
u8	   data_temp[DATA_LEN*2];
Uint16 write_temp[DATA_LEN];
u8 can_cmd    = 0x00;//ID的bit0~bit3位为命令码
u16 can_addr  = 0x00;//ID的bit4~bit15位为节点地址
u32 start_addr = 0x0000;//每一包数据的起始地址
u32 data_size=0;//数据包的大小
u32 data_index=0;//数据指针
u16 crc_data;
u32 FlashSize = (u32)0x00;
uint32_t exe_type = 0x00;
Boot_CMD_LIST cmd_list =
{
	.Erase       = 0x00, //擦除APP区域数据
	.Write       = 0x02, //以多字节形式写数据
	.Check       = 0x03, //检测节点是否在线，同时返回固件信息
	.Excute      = 0x05, //执行固件
	.CmdFaild    = 0x09, //命令执行失败
	.WriteInfo   = 0x01, //设置多字节写数据相关参数(写起始地址,数据量)
	.CmdSuccess  = 0x08, //命令执行成功
	.SetBaudRate = 0x04, //设置节点波特率
};
Device_INFO DEVICE_INFO =
{
.FW_Version = 0x0010001,
};
void __disable_irq(void)
{
	DINT;
	DRTM;
}
void __enable_irq(void)
{
	EINT;
	ERTM;
}
void __set_PRIMASK(u8 state)
{
	if(state == 1)
	{
		__disable_irq();
	}
	else if(state == 0)
	{
		__enable_irq();
	}
	else
	{
		return;
	}
}
unsigned short int CRCcalc16 (unsigned char *data,unsigned short int len)
{
	int i;
	unsigned short int crc_res =  0xFFFF;
	while(len--)
	{
		crc_res^=*data++;
		for(i = 0;i < 8;i++)
		{
			if(crc_res&0x01)
			{
				crc_res = (crc_res>>1)^0xa001;
			}
			else
			{
				crc_res = (crc_res>>1);
			}
		}
	}
	return crc_res;
}
void CAN_BOOT_JumpToApplication(uint32_t Addr)
{
	//  asm(" LB  0x310000");
   // (*((void(*)(void))(Addr)))();
	//(*(pFunction)(Addr))();
	pFunction jump;
	jump = (pFunction)(Addr);
	jump();

}
#pragma CODE_SECTION(CAN_BOOT_ExecutiveCommand,"ramfuncs");
void CAN_BOOT_ExecutiveCommand(CanRxMsg *pRxMessage)
{
	u8 i;
	DEVICE_INFO.FW_TYPE.bits.FW_TYPE = 0x555555;
	DEVICE_INFO.FW_TYPE.bits.Chip_Value = TMS320F28335;
	CanTxMsg TxMessage;//发送对应消息
	Uint16 ret = 0x01;
	FLASH_ST Flash_Status;
	for(i = 0;i <8;i++)
	{
		TxMessage.CAN_Tx_msg_data.msg_byte.data[i] = 0x00;
	}
	TxMessage.CAN_num = CANA;
	TxMessage.DLC = 1;
	TxMessage.ExtId.bit.resved = 0x00;
	TxMessage.IDE = CAN_ID_EXT;
	TxMessage.MBox_num = 0x02;
	TxMessage.Tx_timeout_cnt = 0x00;
	TxMessage.SAE_J1939_Flag = 0;
	//获取地址信息
	Boot_ID_info.ExtId.all = pRxMessage->ExtId;
	can_cmd = Boot_ID_info.ExtId.bit.cmd;
	can_addr = Boot_ID_info.ExtId.bit.addr;
	if((can_addr!=DEVICE_ADDR)&&(can_addr!=0))
	{
		TxMessage.ExtId.bit.ExtId = (0x0134<<CMD_WIDTH)|cmd_list.CmdFaild;
		TxMessage.DLC = 1;
		//TxMessage.Data[0] = 0x01;
		TxMessage.CAN_Tx_msg_data.msg_byte.data[0] = 0x01;
		CAN_Tx_Msg(&TxMessage);
		return;
	}
	//CMD_List.Erase，擦除Flash中的数据，需要擦除的Flash大小存储在Data[0]到Data[3]中
	//该命令必须在Bootloader程序中实现，在APP程序中可以不用实现
	//主要是擦除APP所用扇区
		if(can_cmd == cmd_list.Erase)
		{
			__set_PRIMASK(1); //关闭全局中断
			/*----------------------------------------------------------------------
			//擦除扇区操作
			FLASH_Unlock();
			FlashSize = (pRxMessage->Data[0]<<24)|(pRxMessage->Data[1]<<16)|(pRxMessage->Data[2]<<8)|(pRxMessage->Data[3]<<0);
			ret =  CAN_BOOT_ErasePage(APP_EXE_FLAG_START_ADDR,APP_START_ADDR+FlashSize);
			FLASH_Lock();
			---------------------------------------------------------------------------------*/
			FlashSize = (((u32)(pRxMessage->CAN_Rx_msg_data.msg_byte.data[0])&0xFFFFFFFF)<<0x18)|\
						(((u32)(pRxMessage->CAN_Rx_msg_data.msg_byte.data[1])&0x00FFFFFF)<<0x10)|\
						(((u32)(pRxMessage->CAN_Rx_msg_data.msg_byte.data[2])&0x0000FFFF)<<0x08)|\
						(((u32)(pRxMessage->CAN_Rx_msg_data.msg_byte.data[3])&0x000000FF)<<0x00);
			FlashSize = FlashSize>>1;
			u8 SECT_num = 0;
			u32 SEC_temp = 0x00;
			SECT_num = FlashSize/0x8000;
			if(FlashSize%0x8000 != 0)
			{
				SECT_num = SECT_num+1;
			}
			else
			{
				SECT_num = SECT_num;
			}
			if(SECT_num >5 )
			{
				SECT_num = 5;
			}
			switch (SECT_num)
			{
				case 1:
					SEC_temp = SECTORF;
					break;
				case 2:
					SEC_temp = SECTORF|SECTORE;
					break;
				case 3:
					SEC_temp = SECTORF|SECTORE|SECTORD;
					break;
				case 4:
					SEC_temp = SECTORF|SECTORE|SECTORD|SECTORC;
					break;
				case 5:
					SEC_temp = SECTORF|SECTORE|SECTORD|SECTORC|SECTORB;
					break;
				default:
					break;
			}
			ret = Flash_Erase(SEC_temp,&Flash_Status);
			__set_PRIMASK(0); //打开全局中断;
			if(can_addr != 0x00)
			{
				if(ret==STATUS_SUCCESS)//擦除成功
				{
					TxMessage.ExtId.bit.ExtId = (DEVICE_ADDR<<CMD_WIDTH)|cmd_list.CmdSuccess;
					TxMessage.DLC = 1;
					//TxMessage.Data[0] = 1;
					TxMessage.CAN_Tx_msg_data.msg_byte.data[0] = 1;
				}
				else//擦除失败
				{
					TxMessage.ExtId.bit.ExtId = (DEVICE_ADDR<<CMD_WIDTH)|cmd_list.CmdFaild;
					TxMessage.DLC = 2;
					TxMessage.CAN_Tx_msg_data.msg_byte.data[0] = 2;
					TxMessage.CAN_Tx_msg_data.msg_byte.data[1] = (ret&0xFF);
				}
				CAN_Tx_Msg(&TxMessage);
			}
			return;
		}
		//CMD_List.WriteInfo，设置写Flash数据的相关信息，比如数据起始地址，数据大小
		//数据偏移地址存储在Data[0]到Data[3]中,该偏移量表示当前数据包针针对文件起始的偏移量,同时还表示写入FLASH的偏移
		//  数据大小存储在Data[4]到Data[7]中，该函数必须在Bootloader程序中实现，APP程序可以不用实现
		if(can_cmd == cmd_list.WriteInfo)
		{
			__set_PRIMASK(1);
			//__disable_irq();
			start_addr = (((u32)(pRxMessage->CAN_Rx_msg_data.msg_byte.data[0])&0xFFFFFFFF)<<0x18)|\
						 (((u32)(pRxMessage->CAN_Rx_msg_data.msg_byte.data[1])&0x00FFFFFF)<<0x10)|\
						 (((u32)(pRxMessage->CAN_Rx_msg_data.msg_byte.data[2])&0x0000FFFF)<<0x08)|\
						 (((u32)(pRxMessage->CAN_Rx_msg_data.msg_byte.data[3])&0x000000FF)<<0x00);
				//start_addr   = APP_START_ADDR+addr_offset;
			data_size = (((u32)(pRxMessage->CAN_Rx_msg_data.msg_byte.data[4])&0xFFFFFFFF)<<0x18)|\
						(((u32)(pRxMessage->CAN_Rx_msg_data.msg_byte.data[5])&0x00FFFFFF)<<0x10)|\
					    (((u32)(pRxMessage->CAN_Rx_msg_data.msg_byte.data[6])&0x0000FFFF)<<0x08)|\
					    (((u32)(pRxMessage->CAN_Rx_msg_data.msg_byte.data[7])&0x000000FF)<<0x00);
			data_index   = 0;
			__set_PRIMASK(0);


			if(can_addr != 0x00)
			{
				TxMessage.ExtId.bit.ExtId = (DEVICE_ADDR<<CMD_WIDTH)|cmd_list.CmdSuccess;
				TxMessage.DLC       = 1;
				//TxMessage.Data[0] = 3;
				TxMessage.CAN_Tx_msg_data.msg_byte.data[0] = 3;
				CAN_Tx_Msg(&TxMessage);
			}
		}
		//CMD_List.Write，先将数据存储在本地缓冲区中，然后计算数据的CRC，若校验正确则写数据到Flash中
		//每次执行该数据，数据缓冲区的数据字节数会增加pRxMessage->DLC字节，
		//当数据量达到data_size（包含2字节CRC校验码）字节后
		//对数据进行CRC校验，若数据校验无误，则将数据写入Flash中
		//该函数在Bootloader程序中必须实现，APP程序可以不用实现
		if(can_cmd == cmd_list.Write)
		{
			if((data_index<data_size)&&(data_index<DATA_LEN*2))
			{
				__set_PRIMASK(1);//关闭全局中断
				for(i=0;i<pRxMessage->DLC;i++)
				{
					data_temp[data_index++] =pRxMessage->CAN_Rx_msg_data.msg_byte.data[i];
				}
				__set_PRIMASK(0);//打开全局中断
			}
			if((data_index>=data_size)||(data_index>=(DATA_LEN*2-2)))
			{
				crc_data = CRCcalc16(data_temp,data_size-2);//对接收到的数据做CRC校验，保证数据完整性
				if(crc_data==((data_temp[data_size-2])|(data_temp[data_size-1]<<8)))
				{
					__set_PRIMASK(1);
					/*
					FLASH_Unlock();
					ret =  CAN_BOOT_ProgramDatatoFlash(start_addr,data_temp,data_size-2);
					FLASH_Lock();
					*/
					//此处是将接收到的数据写入FLASH,关键之处,需要仔细考虑
					for(i = 0;i<(data_size-2)>>1;i++)
					{
						write_temp[i] = data_temp[2*i]<<8|data_temp[2*i+1];
					}
					ret = Flash_WR(start_addr,write_temp,(data_size-2)>>1);
					__set_PRIMASK(0);
					if(can_addr != 0x00)
					{
						if(ret==STATUS_SUCCESS)//FLASH写入成功,再次进行CRC校验
						{
							/*
								crc_data = CRCcalc16((unsigned char*)(start_addr),data_size-2);//再次对写入Flash中的数据进行CRC校验，确保写入Flash的数据无误
								if(crc_data!=((data_temp[data_size-2])|(data_temp[data_size-1]<<8)))
								{
									TxMessage.ExtId.bit.ExtId   = (CAN_BOOT_GetAddrData()<<CMD_WIDTH)|cmd.CmdFaild;
									TxMessage.DLC     = 2;
									//TxMessage.Data[0] = 4;
									TxMessage.CAN_Tx_msg_data.msg_byte.data[0] = 4;
									TxMessage.CAN_Tx_msg_data.msg_byte.data[1] = 1;
								}
								else
								{
									TxMessage.ExtId.bit.ExtId    = (CAN_BOOT_GetAddrData()<<CMD_WIDTH)|cmd_list.CmdSuccess;
									TxMessage.DLC      = 0;
								//	TxMessage.Data[0]  = 0;
								}
								CAN_Tx_Msg(&TxMessage);
								*/
							//此处无需再次进行CRC检验,因为在写入数据时已经检验过数据.
							TxMessage.ExtId.bit.ExtId    = (CAN_BOOT_GetAddrData()<<CMD_WIDTH)|cmd_list.CmdSuccess;
							TxMessage.DLC      = 1;
							TxMessage.CAN_Tx_msg_data.msg_byte.data[0] = 0;
							CAN_Tx_Msg(&TxMessage);
						}
						else
						{
							//写入错误
							TxMessage.ExtId.bit.ExtId   = (CAN_BOOT_GetAddrData()<<CMD_WIDTH)|cmd_list.CmdFaild;
							TxMessage.DLC     = 2;
							//TxMessage.Data[0] = 5;
							TxMessage.CAN_Tx_msg_data.msg_byte.data[0] = 4;
							TxMessage.CAN_Tx_msg_data.msg_byte.data[1] = ret&0xFF;
							CAN_Tx_Msg(&TxMessage);
						}
					}
					else
					{
						//地址错误
						TxMessage.ExtId.bit.ExtId   = (CAN_BOOT_GetAddrData()<<CMD_WIDTH)|cmd_list.CmdFaild;
						TxMessage.DLC     = 1;
						//TxMessage.Data[0] = 6;
						//CAN_WriteData(&TxMessage);
						//TxMessage.Data[0] = 5;
						TxMessage.CAN_Tx_msg_data.msg_byte.data[0] = 5;
						TxMessage.CAN_Tx_msg_data.msg_byte.data[1] =0x01;
						CAN_Tx_Msg(&TxMessage);
					}
				}
			return;
			}
		}
		//CMD_List.Check，节点在线检测
		//节点收到该命令后返回固件版本信息和固件类型，
		//该命令在Bootloader程序和APP程序都必须实现
		if(can_cmd == cmd_list.Check)//DSP中尚未实现,相对比较容易实现,主要是为实现APP再次更新应用程序
		{
			if(can_addr != 0x00)
			{
				DEVICE_INFO.Device_addr.bits.Device_addr = DEVICE_ADDR;
				TxMessage.ExtId.bit.ExtId = (DEVICE_INFO.Device_addr.bits.Device_addr<<CMD_WIDTH)|cmd_list.CmdSuccess;
				TxMessage.CAN_Tx_msg_data.msg_byte.data[0] = (u8)(DEVICE_INFO.FW_Version>>24);;//主版本号，两字节
				TxMessage.CAN_Tx_msg_data.msg_byte.data[1] = (u8)(DEVICE_INFO.FW_Version>>16);
				TxMessage.CAN_Tx_msg_data.msg_byte.data[2] = (u8)(DEVICE_INFO.FW_Version>>8);//次版本号，两字节
				TxMessage.CAN_Tx_msg_data.msg_byte.data[3] = (u8)(DEVICE_INFO.FW_Version>>0);
				TxMessage.CAN_Tx_msg_data.msg_byte.data[4] = (u8)(DEVICE_INFO.FW_TYPE.bits.FW_TYPE>>16);
				TxMessage.CAN_Tx_msg_data.msg_byte.data[5] = (u8)(DEVICE_INFO.FW_TYPE.bits.FW_TYPE>>8);
				TxMessage.CAN_Tx_msg_data.msg_byte.data[6] = (u8)(DEVICE_INFO.FW_TYPE.bits.FW_TYPE>>0);
				TxMessage.CAN_Tx_msg_data.msg_byte.data[7] = (u8)(DEVICE_INFO.FW_TYPE.bits.Chip_Value>>0);
				TxMessage.DLC = 8;
				CAN_Tx_Msg(&TxMessage);
			}
		}
		//CMD_List.Excute，控制程序跳转到指定地址执行
		//该命令在Bootloader和APP程序中都必须实现
		if(can_cmd == cmd_list.Excute)//该命令在DSP中已经实现
		{
			exe_type  = (((u32)(pRxMessage->CAN_Rx_msg_data.msg_byte.data[0])&0xFFFFFFFF)<<0x10)|\
						(((u32)(pRxMessage->CAN_Rx_msg_data.msg_byte.data[1])&0x00FFFFFF)<<0x08)|\
						(((u32)(pRxMessage->CAN_Rx_msg_data.msg_byte.data[2])&0x0000FFFF)<<0x00);
			if(exe_type == CAN_BL_APP)
			{
				if((*((uint32_t *)APP_START_ADDR)!=0xFFFFFFFF))
				{
					CAN_BOOT_JumpToApplication(APP_START_ADDR);
				}
			}
			return;
		}
		return;

}
unsigned short int Check_APP(uint32_t Addr)
{
	return 0;
}
