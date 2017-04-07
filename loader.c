#ifndef CLoader_C
#define CLoader_C

#include "StdArm.h"

/* 说明：
	1. 状态描述
		1）初始状态：升级指示灯快闪，5秒后跳转到主应用程序。
					如果接收到串口属性设置和查询，进行相关处理，并进入属性状态；如果接收到进入升级状态，记录升级信息，并进入接收状态；不接收其它串口命令
					如果查询FLASH区域存在符合格式的间接升级数据，则进入更新状态
		2）属性状态：升级指示灯快闪，10秒后跳转到主应用程序。
					如果接收到串口属性设置和查询，进行相关处理；如果接收到进入升级状态，记录升级信息，并进入接收状态；不接收其它串口命令
					如果查询FLASH区域存在符合格式的间接升级数据，则进入更新状态
		3）接收状态：升级指示灯慢闪，60秒没有接收有效数据后跳转到主应用程序。告之EPU进入升级状态。
					如果接收到串口属性设置和查询，进行处理；如果接收到进入升级状态，记录升级信息；
					如果接收到波特率更改和查询，则进行处理；如果接收到退出升级状态，则返回到初始状态；
					如果收到写入命令，则写入数据；如果收到比较命令，则比较数据；如果收到清除命令，则清除数据；如果收到读取命令，则返回数据（最好进行加密处理）；
					如果收到读写驻留区，则读写驻留区；如果收到复位命令，则复位
		4）更新状态：升级指示灯快闪，30秒后跳转到主应用程序。告之EPU进入升级状态。
					按照更新内容，逐步更新区域，实现间接升级。升级完毕跳转到主应用程序
	2 修改记录 :
	2009.7.3 对Loader_EnterProp()做了修改，目的是使当前为CLoader_S_NULL时进不了属性状态。
	2009.7.15 在CLoader_On1HZ中CLoader_S_UPDATE状态时添加更新flash数据函数CLoader_UpdateFlashObject
	2009.7.20 在CMath_GetAddChecksum中增加喂狗操作，这操作可能会影响系统安全性，若在此处出现死循环设备不会复位
*/

CLoader_PARTINFO CLoader_uNowPart;
volatile u8  CLoader_uState;				// 升级状态
volatile u16 CLoader_tmState;			// 升级状态计时.【秒】
#ifdef FUNC_COMPLICATED
u16 CLoader_uUpdateLed;			// 升级指示灯
BYTE CLoader_tmLedFlash;		// 升级指示灯计时。单位【100ms】
#endif

// 初始化
void CLoader_Init( void )
{
	memset(&CLoader_uNowPart,0,sizeof(CLoader_uNowPart));
	CLoader_uState = CLoader_S_NULL;
	CLoader_tmState = 0;	

#ifdef FUNC_COMPLICATED
	CLoader_uUpdateLed = PortGpsLed;
	CLoader_tmLedFlash = 0;
#endif

#if HARDMODEL_Is2220Serial	
	TEpu_Init();
#endif
}

// 1秒事件
UDATA CLoader_On1HZ( void )
{
	if ( CLoader_tmState != 0xFFFF )
		CLoader_tmState++;	

	//TRACE2('M','M',"CLoader_ScanState %x,%x",CLoader_uState,CLoader_tmState);
	switch ( CLoader_uState )
	{
	case CLoader_S_NULL:	// 初始状态	
		if ( CLoader_tmState >= CLoader_OT_INIT )                    //超1秒
			Sys_JumpMainApp();
		break;
	case CLoader_S_PROP:	// 属性状态
		if ( CLoader_tmState >= CLoader_OT_PROP )
			Sys_JumpMainApp();
		break;
	case CLoader_S_INCEPT:	// 接收状态
		if ( CLoader_tmState >= CLoader_OT_INCEPT )
			Sys_JumpMainApp();
		break;

	case CLoader_S_UPDATE:	// 更新状态
		if ( CLoader_tmState >= CLoader_OT_UPDATE )
			Sys_JumpMainApp();
	#ifdef FUNC_E2P_SETS
		if(CLoader_UpdateFlashObject())
			CLoader_SetState(CLoader_S_UPDATE_OK);
		else
			CLoader_SetState(CLoader_S_UPDATE_FAIL);
	#else
		CLoader_UpdateFlashObject();
		CLoader_ExitUpdateStatus();
	#endif
		#if HARDMODEL_Is0863Serial
		CLoader_WriteProPart();		// 开始间接升级
		Sys_TFlash_EraseSector(TFlash_UPDATAINFO_ADDR);	// 擦除源数据
		CLoader_SetState(CLoader_S_NULL);	// 返回NULL状态
		#endif		
		break;

	#ifdef FUNC_E2P_SETS
	case CLoader_S_UPDATE_OK:
		CUpdate_HandleUpdateOK();
		break;
	case CLoader_S_UPDATE_FAIL:
		CUpdate_HandleUpdateFail();
		break;
	#endif
	default:
		break;
	}
	return TRUE;
}

#ifdef FUNC_COMPLICATED
// 100毫秒事件
UDATA CLoader_On10HZ(void)
{
	static u8 count = 0;
	CLoader_tmLedFlash++;	
	
	switch ( CLoader_uState )
	{
	case CLoader_S_NULL:	// 初始状态	
		if(++count>=5)		// 等待500ms后，跳转
         	Sys_JumpMainApp();
		if ( (CLoader_tmLedFlash%2)==0 )
        {      
			Sys_GSMLedOn();
        }
        else
        {
			Sys_GSMLedOff();	
        }
		break;
	case CLoader_S_PROP:	// 属性状态
		if ( (CLoader_tmLedFlash%8)==0 )
			{
		#if HARDMODEL_Is2220Serial
			Sys_OutputLow( CLoader_uUpdateLed );
        #elif HARDMODEL_Is313XSerial
            Sys_OutputLow(LED_2);
		#endif
			}
		else if ( (CLoader_tmLedFlash%8)==1 )
			{
		#if HARDMODEL_Is2220Serial
			Sys_OutputHigh( CLoader_uUpdateLed );
        #elif HARDMODEL_Is313XSerial
            //
            Sys_OutputHigh(LED_2);
		#endif
			}
		break;
	case CLoader_S_INCEPT:	// 接收状态
		if ( (CLoader_tmLedFlash%8)==0 )
			{
		#if HARDMODEL_Is2220Serial
			Sys_OutputHigh( CLoader_uUpdateLed );
        #elif HARDMODEL_Is313XSerial
            Sys_OutputHigh(LED_2);
		#endif
			}
		else if ( (CLoader_tmLedFlash%8)==4 )
			{
		#if HARDMODEL_Is2220Serial
			Sys_OutputLow( CLoader_uUpdateLed );
        #elif HARDMODEL_Is313XSerial
            Sys_OutputLow(LED_2);
        #endif
			}
		break;
	////- case CLoader_S_UPDATE:	// 更新状态
	default:
		if ( (CLoader_tmLedFlash%8)==0 )
			{
		#if HARDMODEL_Is2220Serial
			Sys_OutputHigh( CLoader_uUpdateLed );
        #elif HARDMODEL_Is313XSerial
            Sys_OutputHigh(LED_2);
		#endif
			}
		else if ( (CLoader_tmLedFlash%8)==1)
			{
		#if HARDMODEL_Is2220Serial
			Sys_OutputLow( CLoader_uUpdateLed );
        #elif HARDMODEL_Is313XSerial
            Sys_OutputLow(LED_2);
        #endif
			}
		break;
	}
	return TRUE ;
}
#endif

// 设置状态
void CLoader_SetState(UDATA uState)
{
	if ( CLoader_uState == uState )
		return;

	//TRACE2('K','2',"Isp State From %d to %d.",CLoader_uState,uState);
	CLoader_uState = uState;
	CLoader_tmState = 0;
	switch ( CLoader_uState )
	{
	case CLoader_S_NULL:	// 初始状态
		break;
	case CLoader_S_PROP:	// 属性状态
		#if HARDMODEL_Is3104Serial || HARDMODEL_Is9030Serial
			Sys_OpenPower();
		#endif
		// Flash解锁
		FLASH_Unlock();
		break;
	case CLoader_S_INCEPT:	// 接收状态
	case CLoader_S_UPDATE:	// 更新状态
		#if HARDMODEL_Is3104Serial || HARDMODEL_Is9030Serial
			Sys_OpenPower();
		#endif
		// Flash解锁
		FLASH_Unlock();
	    #if HARDMODEL_Is2220Serial
		    CLoader_NoticeEnterDownload();
        #endif
        #if HARDMODEL_Is313XSerial
           // 喂狗操作
           Sys_ResetInterDog();
        #endif 
		WatchDog();
        break;
	}
}

// 设置下一个接收信息。返回真表示允许进入，否则不允许进入
BOOL CLoader_SetInceptInfo(const CLoader_PARTINFO * pStart)
{
	memcpy(&CLoader_uNowPart,pStart,sizeof(CLoader_PARTINFO));
	return	TRUE;
}

// 进入属性状态
BOOL CLoader_EnterProp()
{
	switch ( CLoader_uState )
	{
	case CLoader_S_UPDATE:	// 更新状态
		return FALSE;
	case CLoader_S_INCEPT:	// 接收状态
		CLoader_ClearOpeartorTimer();
		break;
	case CLoader_S_NULL:
		//- return FALSE;
	default:
		CLoader_SetState(CLoader_S_INCEPT);
		break;
	}
	return	TRUE;

	////- return CLoader_EnterBootStatus();
	/* ////-switch ( CLoader_uState ) 
	{
	case CLoader_S_UPDATE:	// 更新状态
	return FALSE;
	case CLoader_S_INCEPT:	// 接收状态
	CLoader_ClearOpeartorTimer();
	return	TRUE;
	default:
	CLoader_SetState(CLoader_S_PROP);
	return	TRUE;
	}*/
}

// 进入接收状态。返回真表示允许进入，否则不允许进入
BOOL CLoader_EnterBootStatus()
{
	switch ( CLoader_uState )
	{
	case CLoader_S_UPDATE:	// 更新状态
		return FALSE;
	case CLoader_S_INCEPT:	// 接收状态
		CLoader_ClearOpeartorTimer();
        #if HARDMODEL_Is2220Serial  
		    CLoader_NoticeEnterDownload();
        #endif
		//#if HARDMODEL_Is1062Serial 
		    WatchDog();
        //#endif
		break;
	default:
		CLoader_SetState(CLoader_S_INCEPT);
		break;
	}
	return	TRUE;
}

// 退出接收状态。
BOOL CLoader_ExitBootStatus()
{
	switch ( CLoader_uState )
	{
	case CLoader_S_UPDATE:	// 更新状态
		return FALSE;
	default:
		CLoader_SetState(CLoader_S_NULL);
		Sys_JumpMainApp();
		return	TRUE;
	}
}

// 进入更新状态
BOOL CLoader_EnterUpdateStatus()
{
	switch ( CLoader_uState )
	{
	default:
		CLoader_SetState(CLoader_S_UPDATE);
		return TRUE;
	}
}

// 退出更新状态
BOOL CLoader_ExitUpdateStatus()
{
	switch ( CLoader_uState)
	{
	default:
		CLoader_SetState(CLoader_S_NULL);
		Sys_JumpMainApp();
		return	TRUE;
	}
}

#ifdef FUNC_COMPLICATED	// 终端和ARM9下载
#ifndef FUNC_SYS_DEBUG
// 开始接收数据
UDATA CLoader_RevDataOperator( UDATA uCmdId,u32 uAddr,UINT uLen,const u8 *pData )
{
	if ( !CLoader_IsAllowWrite() )
		return CLoader_ANS_DATA_NOSTATE; 		// 状态不正确。
	CLoader_ClearOpeartorTimer();

	// 数据校验
	if( CLoader_uNowPart.uCheckKind==CLoader_CSK_CRC8 ) 			// 存在CRC8校验
	{
		u8 uCalcSum,uRevSum;
		
		uRevSum = pData[uLen-1];
		uLen -= 1;	// 最后CLoader_uNowPart.CrcLen个字符为校验
		// 校验数据
		uCalcSum = CCrc_GetCrc8(pData,uLen,0);			
		//TRACE3('M','M',"CLoader_RevData %x,%x,%x",uLen,uCalcSum,uRevSum);
		if ( uCalcSum != uRevSum )			// 校验失败
			{
				TRACE3('M','M',"CLoader_RevData %x,%x,%x",uLen,uCalcSum,uRevSum);
				return CLoader_ANS_DATA_FARMECHECKERR;
			}
	}else if ( CLoader_uNowPart.uCheckKind != CLoader_CSK_NULL )
		return CLoader_ANS_DATA_ERRCHECKSUM;
		
	switch( uCmdId )
	{
	case 2:		// 写数据	
		return CLoader_WriteData( uAddr,pData,uLen,FALSE ); 
	case 9:		// 比较数据
		return CLoader_CompData( uAddr,pData,uLen ); 
	case 0x12:	// 写数据并自动擦除扇区。当地址是扇区的起始地址时，就先擦除扇区，然后再进行写操作。
		////+ return CLoader_WriteData( uAddr,pData,uLen,TRUE ); 
		return CLoader_ANS_DATA_WHOERR;
	default:			
		return CLoader_ANS_DATA_FAILURE;
	}
}

// 开始接收数据
UDATA CLoader_RevBlockOperator( UDATA uCmdId,u32 uAddr,UINT uLen,UDATA chValue )
{
	if ( !CLoader_IsAllowWrite() )
		return CLoader_ANS_DATA_NOSTATE; 		// 状态不正确。
	CLoader_ClearOpeartorTimer();

	switch( uCmdId )
	{
	case 3:		// 写入相同数据块数据
		return CLoader_WriteCharBlock( uAddr,chValue,uLen,FALSE ); 
	case 0xA:		// 比较相同数据块数据
		return CLoader_CompCharBlock( uAddr,chValue,uLen ); 
	case 0x13:	// 写数据并自动擦除扇区。当地址是扇区的起始地址时，就先擦除扇区，然后再进行写操作。
		////+ return CLoader_WriteCharBlock( uAddr,chValue,uLen,TRUE ); 
		return CLoader_ANS_DATA_WHOERR;
	default:			
		return CLoader_ANS_DATA_FAILURE;
	}
}

/*函数功能: 写数据。
**返回值　: 返回写数据的结果.
**参数说明: uStartAddr 为norflash 的相对地址
**			*pBuf擦除的缓冲区
**			uBufLen缓冲区的长度
**			fAutoEraseSector是否自动擦除数据
*/
UDATA CLoader_WriteData( u32 uStartAddr,const void *pBuf,UINT uBufLen,BOOL fAutoEraseSector )
{
	if ( CLoader_uNowPart.uObjectKind==CLoader_ODK_RAM )
	{		
		// TRACE4('M','M',"CLoader_WriteData %x,%x,%x,%x",uStartAddr,uStartAddr+uBufLen,uBufLen,isNewData );
		if( Sys_WriteDataRam( uStartAddr,pBuf,uBufLen ) == uBufLen )
			return CLoader_ANS_DATA_SUCESS;			// 数据写入成功
		else
			return CLoader_ANS_DATA_WRITERERR;		// 写数据错误
	}
	else
	{
	    //#if HARDMODEL_Is2220Serial || HARDMODEL_Is1062Serial 
		if( uStartAddr<PROP_NOR_WRITE_MINADDR )	// 对FlashLoader的地址范围起保护作用,不能进行写操作
			return CLoader_ANS_DATA_WRITERERR;
        //#endif
        
		if( Sys_WriteCheckDataFlash( uStartAddr,pBuf,uBufLen,fAutoEraseSector )==uBufLen )
		{	
			return CLoader_ANS_DATA_SUCESS;			// 数据写入成功
		}
		else
			return CLoader_ANS_DATA_WRITERERR;		// 写数据错误
	}
}

// 写字符块。返回写数据的结果.
UDATA CLoader_WriteCharBlock( u32 uStartAddr,UDATA chValue,UINT uBufLen,BOOL fAutoEraseSector )
{
	if ( CLoader_uNowPart.uObjectKind==CLoader_ODK_RAM )
	{		
		// TRACE4('M','M',"CLoader_WriteData %x,%x,%x,%x",uStartAddr,uStartAddr+uBufLen,uBufLen,isNewData );
		if( Sys_WriteCharBlockRam( uStartAddr,chValue,uBufLen ) == uBufLen )
			return CLoader_ANS_DATA_SUCESS;			// 数据写入成功
		else
			return CLoader_ANS_DATA_WRITERERR;		// 写数据错误
	}
	else
	{
		if( uStartAddr<PROP_NOR_WRITE_MINADDR )	// 对FlashLoader的地址范围起保护作用,不能进行写操作
			return CLoader_ANS_DATA_WRITERERR;

		if( Sys_WriteCharBlockFlash( uStartAddr,chValue,uBufLen,fAutoEraseSector )==uBufLen )
		{	
			return CLoader_ANS_DATA_SUCESS;			// 数据写入成功
		}
		else
			return CLoader_ANS_DATA_WRITERERR;		// 写数据错误
	}
}

// 读数据。返回读数据的结果
UDATA CLoader_ReadDataOperator( u32 uStartAddr,void *pBuf,UINT uBufLen,UINT * pReadLen )
{	
	UDATA uResult;

	if ( !CLoader_IsAllowRead() )
		return CLoader_ANS_DATA_NOSTATE; 		// 状态不正确。
	CLoader_ClearOpeartorTimer();

	if ( CLoader_uNowPart.uObjectKind==CLoader_ODK_RAM )
	{		
		if( Sys_ReadDataRam( uStartAddr,pBuf,uBufLen ) == uBufLen )
			uResult = CLoader_ANS_OK;				// 读数据成功
		else
			uResult = CLoader_ANS_ERROR;			// 读数据失败
	}
	else
	{
		if( Sys_ReadDataFlash( uStartAddr,pBuf,uBufLen )==uBufLen )
			uResult = CLoader_ANS_OK;				// 读数据成功
		else
			uResult = CLoader_ANS_ERROR;			// 读数据失败
	}
	*pReadLen = uBufLen;
	if ( CLoader_uNowPart.uCheckKind==CLoader_CSK_CRC8 )
	{
		*((u8 *)pBuf+uBufLen) = CCrc_GetCrc8( pBuf,uBufLen,0 );
		*pReadLen += 1;
	}
	return uResult;
}

// 比较数据。返回比较数据的结果
UDATA CLoader_CompData( u32 uStartAddr,const void *pBuf,UDATA uBufLen )
{
	if ( CLoader_uNowPart.uObjectKind==CLoader_ODK_RAM )
	{
		if ( !Sys_CompDataRam( uStartAddr,pBuf,uBufLen ) )
			return CLoader_ANS_DATA_COMPERR;		// 比较数据不一致
		else
			return CLoader_ANS_DATA_SUCESS;			// 比较数据一致
	}
	else
	{
		if ( !Sys_CompDataFlash( uStartAddr,pBuf,uBufLen ) )
			return CLoader_ANS_DATA_COMPERR;		// 比较数据不一致
		else
			return CLoader_ANS_DATA_SUCESS;			// 比较数据一致
	}
}

// 比较字符块。返回比较数据的结果
UDATA CLoader_CompCharBlock( u32 uStartAddr,UDATA chValue,UDATA uBufLen )
{
	if ( CLoader_uNowPart.uObjectKind==CLoader_ODK_RAM )
	{
		if ( !Sys_CompCharBlockRam( uStartAddr,chValue,uBufLen ) )
			return CLoader_ANS_DATA_COMPERR;		// 比较数据不一致
		else
			return CLoader_ANS_DATA_SUCESS;			// 比较数据一致
	}
	else
	{
		if ( !Sys_CompCharBlockFlash( uStartAddr,chValue,uBufLen ) )
			return CLoader_ANS_DATA_COMPERR;		// 比较数据不一致
		else
			return CLoader_ANS_DATA_SUCESS;			// 比较数据一致
	}
}

// 擦除指定地址，指定长度内容。
UDATA CLoader_ClearDataOperator( u32 uStartAddr,u32 uLen )
{
	if ( !CLoader_IsAllowWrite() )
		return CLoader_ANS_DATA_NOSTATE; 		// 状态不正确。
	CLoader_ClearOpeartorTimer();

	if ( CLoader_uNowPart.uObjectKind==CLoader_ODK_RAM )
		return CLoader_ANS_DATA_SUCESS; 			// 扇区擦除成功
	else
	{    
	    //#if HARDMODEL_Is2220Serial || HARDMODEL_Is1062Serial 
		if ( uStartAddr<(PROP_NOR_WRITE_MINADDR) )	// 对FlashLoader的地址范围起保护作用,不能进行擦除操作
			return CLoader_ANS_DATA_ERASEERR;
       // #endif
		// TRACE2('M','M',"CLoader_ClearData %x,%x",uStartAddr,uLen );
		if ( Sys_ClearDataFlash(uStartAddr,uLen) )
			return CLoader_ANS_DATA_SUCESS;			// 扇区擦除成功
		else
			return CLoader_ANS_DATA_ERASEERR;		// 数据擦除失败
	}
}

// 复位设备
BOOL CLoader_ResetDevice( void )
{
	return TRUE;
}
#endif
#else // 大屏下载
UDATA CLoader_RevDataOperator( UDATA uCmdId,u32 uAddr,UINT uLen,const u8 *pData )
{
	if ( !CLoader_IsAllowWrite() )
		return CLoader_ANS_DATA_NOSTATE; 		// 状态不正确。
	CLoader_ClearOpeartorTimer();

	// 数据校验
	if( CLoader_uNowPart.uCheckKind==CLoader_CSK_CRC8 ) 			// 存在CRC8校验
	{
		u8 uCalcSum,uRevSum;
		
		uRevSum = pData[uLen-1];
		uLen -= 1;	// 最后CLoader_uNowPart.CrcLen个字符为校验
		// 校验数据
		uCalcSum = CCrc_GetCrc8(pData,uLen,0);			
		//TRACE3('M','M',"CLoader_RevData %x,%x,%x",uLen,uCalcSum,uRevSum);
		if ( uCalcSum != uRevSum )			// 校验失败
		{
			TRACE3('M','M',"CLoader_RevData %x,%x,%x",uLen,uCalcSum,uRevSum);
			return CLoader_ANS_DATA_FARMECHECKERR;
		}
	}else if ( CLoader_uNowPart.uCheckKind != CLoader_CSK_NULL )
		return CLoader_ANS_DATA_ERRCHECKSUM;
		
	switch( uCmdId )
	{
	case 2:		// 写数据	
		return CLoader_WriteData( uAddr,pData,uLen,FALSE ); 
//-	case 9:		// 比较数据
//-		return CLoader_CompData( uAddr,pData,uLen ); 
	case 0x12:	// 写数据并自动擦除扇区。当地址是扇区的起始地址时，就先擦除扇区，然后再进行写操作。
		////+ return CLoader_WriteData( uAddr,pData,uLen,TRUE ); 
		return CLoader_ANS_DATA_WHOERR;
	default:			
		return CLoader_ANS_DATA_FAILURE;
	}
}

UDATA CLoader_WriteData( u32 uStartAddr,const void *pBuf,UINT uBufLen,BOOL fAutoEraseSector )
{

	if( uStartAddr<PROP_MAINAPP_STARTADDR )	// 对FlashLoader的地址范围起保护作用,不能进行写操作
		return CLoader_ANS_DATA_WRITERERR;
	TRACE3('M','M',"CLoader_WriteData uStartAddr=%d,uBufLen=0x%x,pBuf=0x%x",uStartAddr,uBufLen,(UINT)pBuf);
	/*
	 开始下载数据到片内flash中，将直接按每个sector来操作
	*/ 
	SLpcIap_WriteFlash(uStartAddr,(UINT)pBuf,4096);
	return CLoader_ANS_DATA_SUCESS;
	/*
	if (Sys_WriteCheckDataFlash( uStartAddr,(UINT)pBuf,4096,fAutoEraseSector ) )	// 串口下载工具缓冲区是4096大小的数据
		return CLoader_ANS_DATA_SUCESS;
	else 
		return CLoader_ANS_DATA_FAILURE;
	*/
}

UDATA CLoader_ClearDataOperator( u32 uStartAddr,u32 uLen )
{
	if ( !CLoader_IsAllowWrite() )
		return CLoader_ANS_DATA_NOSTATE; 		// 状态不正确。
	CLoader_ClearOpeartorTimer();

	if ( uStartAddr<(PROP_MAINAPP_STARTADDR) )	// 对FlashLoader的地址范围起保护作用,不能进行擦除操作
		return CLoader_ANS_DATA_ERASEERR;

	// TRACE2('M','M',"CLoader_ClearData %x,%x",uStartAddr,uLen );
	if( !Sys_ClearDataFlash(uStartAddr,uLen+uStartAddr) )
		return CLoader_ANS_DATA_SUCESS;			// 扇区擦除成功
	else
		return CLoader_ANS_DATA_ERASEERR;		// 数据擦除失败

}

UDATA CLoader_ReadDataOperator( u32 uStartAddr,void *pBuf,UINT uBufLen,UINT * pReadLen )
{
	UDATA uResult;

	if ( !CLoader_IsAllowRead() )
		return CLoader_ANS_DATA_NOSTATE; 		// 状态不正确。
	CLoader_ClearOpeartorTimer();
	TRACE0('M','M',"CLoader_ReadDataOperator!" );
	{	
		if( Sys_ReadDataFlash( uStartAddr,pBuf,uBufLen )==uBufLen )
			uResult = CLoader_ANS_OK;				// 读数据成功
		else
			uResult = CLoader_ANS_ERROR;			// 读数据失败
	}
	*pReadLen = uBufLen;
	WatchDog();
	if ( CLoader_uNowPart.uCheckKind==CLoader_CSK_CRC8 )
	{
		*((u8 *)pBuf+uBufLen) = CCrc_GetCrc8( pBuf,uBufLen,0 );
		*pReadLen += 1;
	}
	return uResult;	
}


// 写数据。返回当前保存的数据长度
void CLoader_WriteProPart(void)
{
	u8 i;
	u32 tempadd;
	u32 proadd;
	__align(4) BYTE tempbuff[4096];
	//- BYTE tempbuff[4096];

	tempadd=TFlash_UPDATABUF_START;
	for (i=CUPDATE_START_SECTOR;i<=CUPDATE_END_SECTOR;i++)
	{
		SLpcIap_SelSector(i, i);
		SLpcIap_EraseSector(i, i);
		WatchDog();
	}
	for (proadd=PROP_MAINAPP_STARTADDR;proadd<(CUpdate_uNowPart.uTotalBytes+PROP_MAINAPP_STARTADDR+1024);)
	{
		TRACE1('K','T',"writeADD=0x%x",proadd);
		Sys_TFlash_ReadBlock(tempbuff,4096,tempadd);
		SLpcIap_WriteFlash(proadd,(UINT)tempbuff,4096);
		WatchDog();
		tempadd=tempadd+4096;
		proadd=proadd+4096;		
	}

}


#endif


#ifdef FUNC_SYS_DEBUG
	#if HARDMODEL_Is2220Serial
	typedef struct TEpu_tagMPUDOWNLOADCTRL{		// 睡眠控制
		u8	stx;		// STX	头字符
		u8	len;		// 长度Len	数据帧的总长度，包括长度和校验和。
		u8	id;			// 命令ID
		u8  ref;		// 是MPU发送帧的序号，从0开始，每发送一帧就加1
		u8  flag;		// 命令标记.
		u8  mode;		// 。
		u8  check;		// 校验和Check	是从STX开始到校验和之前所有字节的异或值
		u8  fillbyte[1];	// 填充字节
	}TEpu_MPUDOWNLOADCTRL;
	#define TEpu_MPUDOWNLOADCTRL_SIZE	(sizeof(TEpu_MPUDOWNLOADCTRL)-1)

	typedef struct TEpu_tagEPUVERSION{		// EPU的版本信息
		u8	len;		// 长度Len	数据帧的总长度，包括长度和校验和。
		u8	id;			// 命令ID
		u8  ref;		// 是EPU发送帧的序号，从0开始，每发送一帧就加1
		u8  flag;		// 命令标记.// Bit0： 0－RX脚是IO端口(缺省值)1－RX(串行发送)有效
		// Bit1： 0－TX脚是IO端口(缺省值)1－TX(串行发送)有效
		u8  handcode;	// 握手码。
		u8  func[2];	// EPU支持的功能组合
		u8  pv;			// EPU支持的协议版本(pv>1.1=0x11时,最后为日期)
		u8  sv;			// EPU支持的软件版本
		u8  hv;			// EPU支持的硬件版本
		u8  epu;		// EPU的型号
		u8  uSciOutMax;	// EPU发送缓冲区的最大字节数目
		u8  uSciInMax;	// EPU输入缓冲区的最大字节数目
		u8  lastref;	// 最后收到的命令的参考号
		// u8  runtime[2];	// MPU从开机以来的运行时间。单位秒。
		u8  year_month;	// 高4位为（年－2007）
		u8  year_day;	// 高3位为（年－2007）/ 16
		u8  check;		// 校验和Check	是从len开始到校验和之前所有字节的异或值
		u8  fillbyte[3];	// 填充字节
	}TEpu_EPUVERSION;
	#define TEpu_EPUVERSION_SIZE	(sizeof(TEpu_EPUVERSION)-3)

	// EPU ID
	enum TEpu_ID_ENUM{
		TEpu_ID_MPUVERSION=0X1,		// MPU向EPU发送注册命令，用于初始化EPU
		TEpu_ID_MPUOUTCTRL=0X10,	// MPU向EPU发送控制输出的命令
		TEpu_ID_MPURESETCTRL=0X11,	// MPU向EPU发送控制复位的命令
		TMpu_ID_MPUSLEEP=0X12,		// MPU向EPU发送睡眠通知的命令
		TMpu_ID_MPUDOWNLOAD=0X13,	// MPU向EPU发送升级通知的命令
		TMpu_ID_MPUSCIRATE=0X15,	// MPU向EPU发送修改波特率的命令
		TEpu_ID_MPUOUTCTRL2=0X16,	// MPU向EPU发送控制输出的命令
		TEpu_ID_EPUSTATUS=0X41,		// MPU读取地址REG_STATUS返回的数据。
		TEpu_ID_EPUVERSION=0X42,	// MPU读取地址REG_COMMAND返回的数据
	};

	typedef struct TEpu_tagEPUSTATUS_VER11{// EPU的状态信息
		u8	len;		// 长度Len	数据帧的总长度，包括长度和校验和。
		u8	id;			// 命令ID
		u8  ref;		// 是EPU发送帧的序号，从0开始，每发送一帧就加1
		u8  handcode;	// 握手码。
		u8	ss[2];		// EPU状态位，高字节在前。按位取。高位在前。
		// Bit0：串口输入缓冲区是否有数据。1＝有
		// Bit1：串口输出缓冲区是否有数据。1＝有
		// Bit2：是否由MPU设置了低功耗模式。1＝是
		u8  incv[2];	// 输入口的信号值。按位取。高位在前。1＝高电平。该信号值是EPU当前的通知值，与实际采样值可能不一样，主要用于保存变化。
		u8  inMask[2];	// 输入口的通知掩码
		u8  outv;		// 输出端口值
		u8  lastRef;	// 最后收到的命令的参考号
		u8  uSciInLen;	// 串口输入缓冲区数据长度
		u8  ad1[2];		// AD采样值。高字节在前
		u8  ad2[2];		// AD采样值。高字节在前
		u8  check;		// 校验和Check	是从len开始到校验和之前所有字节的异或值
		u8  fillbyte[2];	// 填充字节
	}TEpu_EPUSTATUS_VER11;
	#define TEpu_EPUSTATUS_VER11_SIZE	(sizeof(TEpu_EPUSTATUS_VER11)-2)	//1.1版本尺寸
	typedef struct TEpu_tagEPUSTATUS_VER12{// EPU的状态信息
		u8	len;		// 长度Len	数据帧的总长度，包括长度和校验和。
		u8	id;			// 命令ID
		u8  ref;		// 是EPU发送帧的序号，从0开始，每发送一帧就加1
		u8  handcode;	// 握手码。
		u8	ss[2];		// EPU状态位，高字节在前。按位取。高位在前。
		// Bit0：串口输入缓冲区是否有数据。1＝有
		// Bit1：串口输出缓冲区是否有数据。1＝有
		// Bit2：是否由MPU设置了低功耗模式。1＝是
		u8  incv[2];	// 输入口的信号值。按位取。高位在前。1＝高电平。该信号值是EPU当前的通知值，与实际采样值可能不一样，主要用于保存变化。
		u8  inMask[2];	// 输入口的通知掩码
		u8  outv;		// 输出端口值
		u8  lastRef;	// 最后收到的命令的参考号
		u8  uSciInLen;	// 串口输入缓冲区数据长度
		u8  ad1[2];		// AD采样值。高字节在前
		u8  ad2[2];		// AD采样值。高字节在前
		u8  uTimer5ms[2];	// 5ms定时计数。只有1.2版本才支持，之前版本无此项。高字节在前
		u8  check;		// 校验和Check	是从len开始到校验和之前所有字节的异或值
		// 	u8  fillbyte[2];	// 填充字节
	}TEpu_EPUSTATUS_VER12;
	#define TEpu_EPUSTATUS_VER12_SIZE		(sizeof(TEpu_EPUSTATUS_VER12))

	typedef struct TEpu_tagEPUSTATUS_ODO{	// EPU的状态信息－－－带里程计数
		u8	len;		// 长度Len	数据帧的总长度，包括长度和校验和。
		u8	id;			// 命令ID
		u8  ref;		// 是EPU发送帧的序号，从0开始，每发送一帧就加1
		u8  handcode;	// 握手码。
		u8	ss[2];		// EPU状态位，高字节在前。按位取。高位在前。
		// Bit0：串口输入缓冲区是否有数据。1＝有
		// Bit1：串口输出缓冲区是否有数据。1＝有
		// Bit2：是否由MPU设置了低功耗模式。1＝是
		u8  incv[2];	// 输入口的信号值。按位取。高位在前。1＝高电平。该信号值是EPU当前的通知值，与实际采样值可能不一样，主要用于保存变化。
		u8  inMask[2];	// 输入口的通知掩码
		u8  outv;		// 输出端口值
		u8  lastRef;	// 最后收到的命令的参考号
		u8  uSciInLen;	// 串口输入缓冲区数据长度
		u8  ad1[2];		// AD采样值。高字节在前
		u8  ad2[2];		// AD采样值。高字节在前
		u8  uTimer5ms[2];	// 5ms定时计数。只有1.2版本才支持，之前版本无此项。高字节在前
		u8  uOdoCount[4];	// 里程脉冲计数。单位：4个脉冲。
		u8  check;		// 校验和Check	是从len开始到校验和之前所有字节的异或值
		// 	u8  fillbyte[2];	// 填充字节
	}TEpu_EPUSTATUS_ODO;
	#define TEpu_EPUSTATUS_ODO_SIZE		(sizeof(TEpu_EPUSTATUS_ODO))

	typedef struct TEpu_tagEPUSTATUS_3AD{	// EPU的状态信息－－－带里程计数，3路AD
		u8	len;		// 长度Len	数据帧的总长度，包括长度和校验和。
		u8	id;			// 命令ID
		u8  ref;		// 是EPU发送帧的序号，从0开始，每发送一帧就加1
		u8  handcode;	// 握手码。
		u8	ss[2];		// EPU状态位，高字节在前。按位取。高位在前。
		// Bit0：串口输入缓冲区是否有数据。1＝有
		// Bit1：串口输出缓冲区是否有数据。1＝有
		// Bit2：是否由MPU设置了低功耗模式。1＝是
		u8  incv[2];		// 输入口的信号值。按位取。高位在前。1＝高电平。该信号值是EPU当前的通知值，与实际采样值可能不一样，主要用于保存变化。
		u8  inMask[2];		// 输入口的通知掩码
		u8  outv;			// 输出端口值
		u8  lastRef;		// 最后收到的命令的参考号
		u8  uSciInLen;		// 串口输入缓冲区数据长度
		u8  ad1[2];			// AD采样值。高字节在前
		u8  ad2[2];			// AD采样值。高字节在前
		u8  uTimer5ms[2];	// 5ms定时计数。只有1.2版本才支持，之前版本无此项。高字节在前
		u8  uOdoCount[4];	// 里程脉冲计数。单位：4个脉冲。
		u8  ad3[2];			// AD采样值。高字节在前
		u8  check;			// 校验和Check	是从len开始到校验和之前所有字节的异或值
		u8  fillbyte[2];	// 填充字节
	}TEpu_EPUSTATUS_3AD;
	#define TEpu_EPUSTATUS_3AD_SIZE		offsetof(TEpu_EPUSTATUS_3AD,fillbyte)	////- (sizeof(TEpu_EPUSTATUS_3AD))

	// EPU状态标记
	enum TEpu_STATEFLAG_ENUM{
		TEpu_SF_SCIINDATA=0x1,		// 串口输入缓冲区是否有数据。1＝有
		TEpu_SF_SCIOUTDATA=0x2,		// 串口输出缓冲区是否有数据。1＝有
		TEpu_SF_SLEEPMODE=0x4,		// 是否由MPU设置了低功耗模式。1＝是
		TEpu_SF_UPDATE=0x8,			// 是否MPU处于调试（或升级）状态。1＝是
		TEpu_SF_OVERTIMENOTIFY=0x40,// 是否EPU超时没有收到MPU的定时查询。1＝是
	};

	#define TEpu_VERSION_PROTOCOL_I2C			0x17
	#define TEpu_VERSION_PROTOCOL_3ADODO		0x17	// 支持第3路AD的采样（也就是外部AD2），同时支持里程计数的状态命令
	#define TEpu_VERSION_PROTOCOL_ODOSTATUS		0x16	// 支持里程计数的状态命令
	#define TEpu_VERSION_PROTOCOL_NEWSTATUS		0x12	// 支持新的状态命令
	#define TEpu_VERSION_PROTOCOL_NEWOUT		0x13	// 支持新的输出命令
	
	UDATA TEpu_uOutMpuRef = 0;
	UDATA TEpu_uEpuInfo_pv = 0x17;

	void TEpu_Init()
	{
		TEpu_uOutMpuRef = 0;
		TEpu_uEpuInfo_pv = TEpu_VERSION_PROTOCOL_I2C;
	}

	// 校验
	u8 TEpu_GetCheckResult(void * pBuffer,UDATA uLen)
	{
		u8 uCheck = 0;
		for ( ; uLen ; --uLen,((u8 *)pBuffer)++ )
		{
			uCheck ^= *((u8 *)pBuffer);
		}
		return uCheck;
	}

	// 获得EPU版本
	BOOL TEpu_GetEpuVersion()
	{
		TEpu_EPUVERSION epuVersion;
		u8 uCheck;

		memset(&epuVersion,0,sizeof(epuVersion));
		//Sys_I2CReadData(TEpu_REG_COMMAND,(u8 *)&epuVersion,TEpu_EPUVERSION_SIZE);   // 调试loader313x修改
		if ( ( epuVersion.len <  TEpu_EPUVERSION_SIZE ) || ( epuVersion.id != TEpu_ID_EPUVERSION ) )
		{
			TRACE3('K','E',"GetEpuVersion Error.readlen=%d,len=%d,id=%d",TEpu_EPUVERSION_SIZE,epuVersion.len,epuVersion.id);
			return FALSE;
		}

		// 校验
		uCheck = TEpu_GetCheckResult((u8 *)&epuVersion,epuVersion.len-1);
		if ( uCheck != epuVersion.check )
		{ // 校验失败
			TRACE4('K','E',"GetEpuVersion Check Error.len=%d,id=%d,check=%d,result=%d",epuVersion.len,epuVersion.id,epuVersion.check,uCheck);
			return FALSE;
		}

		if ( epuVersion.handcode != 0 )
		{// MPU发生过复位，EPU没有复位。
			TRACE1('K','E',"Mpu has reset.epu_handcode=%d",epuVersion.handcode);
		}


		TRACE10('I','E',"GetEpuVersion OK.len=%d,id=%d,OutSize=%d,InSize=%d,pv=0x%x,sv=0x%x,hv=0x%x,epu=%d,flag=%d,func=0x%x",
			epuVersion.len,epuVersion.id,epuVersion.uSciOutMax,epuVersion.uSciInMax,epuVersion.pv,epuVersion.sv,epuVersion.hv,epuVersion.epu,epuVersion.flag,GetWordFromPtr(epuVersion.func));

		return TRUE;
	}

	// 发送MPU确认帧
	BOOL TEpu_OutMpuAffirm()
	{
		u8 buffer[2];
		buffer[0] = TEpu_CHAR_AFFIRM;
		//Sys_I2CWriteData(TEpu_REG_STATUS,buffer,1);   // 调试loader313x修改
		return TRUE;
	}

	/*************************************************************************************************
	purpose: 控制进入下载。用于调试
	return: 成功返回真
	*************************************************************************************************/
	BOOL TEpu_EnterDebugMode(BOOL fEnter)
	{
		TEpu_MPUDOWNLOADCTRL info;
		u8 uCheck;

		++TEpu_uOutMpuRef;
		info.stx = TEpu_CHAR_STX;
		info.len = TEpu_MPUDOWNLOADCTRL_SIZE-1;
		info.id = TMpu_ID_MPUDOWNLOAD;
		info.ref = TEpu_uOutMpuRef;
		info.flag = 0; 
		info.mode = (fEnter)?1:2;
		uCheck = TEpu_GetCheckResult(&info,TEpu_MPUDOWNLOADCTRL_SIZE-1);
		info.check = uCheck;

		//Sys_I2CWriteData(TEpu_REG_COMMAND,&info,TEpu_MPUDOWNLOADCTRL_SIZE);

		return TRUE;
	}

	// 获得EPU状态
	UDATA TEpu_GetEpuStatus()
	{
		TEpu_EPUSTATUS_3AD epuStatus;
		u8  uCheck,uReadChecksum;
		UINT incv,uLastEpuFlags;
		u8  uReadLen;
		UDATA fResult = TRUE;
		UDATA uLastEpuSciInLen;
		u32 uOdoCount;
		Sys_OSTICK tk;

		if ( TEpu_uEpuInfo_pv >= TEpu_VERSION_PROTOCOL_3ADODO )
			uReadLen = TEpu_EPUSTATUS_3AD_SIZE;
		else if ( TEpu_uEpuInfo_pv >= TEpu_VERSION_PROTOCOL_ODOSTATUS )
			uReadLen = TEpu_EPUSTATUS_ODO_SIZE;
		else if ( TEpu_uEpuInfo_pv >= TEpu_VERSION_PROTOCOL_NEWSTATUS )
			uReadLen = TEpu_EPUSTATUS_VER12_SIZE;
		else uReadLen = TEpu_EPUSTATUS_VER11_SIZE;

		memset(&epuStatus,0,sizeof(epuStatus));
		//Sys_I2CReadData(TEpu_REG_STATUS,(u8 *)&epuStatus,uReadLen);
		tk = Sys_GetOsTickCounter();
		if ( ( epuStatus.len <  uReadLen ) || ( epuStatus.id != TEpu_ID_EPUSTATUS ) )
		{
			TRACE3('K','E',"GetEpuStatus Error.readlen=%d,len=%d,id=%d",uReadLen,epuStatus.len,epuStatus.id);
			return FALSE;
		}
		uReadChecksum = ((u8 *)&epuStatus)[epuStatus.len-1];

		// 校验
		uCheck = TEpu_GetCheckResult((u8 *)&epuStatus,epuStatus.len-1);
		if ( uCheck != uReadChecksum )
		{ // 校验失败
			TRACE6('K','E',"GetEpuStatus Check Error.getlen=%d,id=%d,check=%d,result=%d,pv=0x%x,rlen=%d.",epuStatus.len,epuStatus.id,uReadChecksum,uCheck,TEpu_uEpuInfo_pv,uReadLen);
			return FALSE;
		}

		uLastEpuFlags = GetWordFromPtr(epuStatus.ss);
		uLastEpuSciInLen = epuStatus.uSciInLen;


		if ( TestFlag ( uLastEpuFlags , TEpu_SF_UPDATE ) )
		{
			if ( !CLoader_IsAllowWrite() )
			{
				TRACE0('K','E',"Mpu no debug,but epu debug");
			}
		}else if ( CLoader_IsAllowWrite() )
		{
			TRACE0('K','E',"Mpu debug,but epu no debug");
		}


		incv = GetWordFromPtr(epuStatus.incv);
		uOdoCount = GetDwordFromPtr(epuStatus.uOdoCount);
        /*
		TRACE14('I','E',"ES=%d.ref=%d,hc=%d;ss=0x%x;in=0x%x@0x%x;outv=0x%x;sci=%d;,ad=%d,%d,%d;5ms=%d,odo=%d,os=%d", // ,ad=%d,%d
			epuStatus.len,epuStatus.lastRef,epuStatus.handcode,uLastEpuFlags,incv,GetWordFromPtr(epuStatus.inMask),epuStatus.outv,epuStatus.uSciInLen,GetWordFromPtr(epuStatus.ad1),GetWordFromPtr(epuStatus.ad2),GetWordFromPtr(epuStatus.ad3),
			GetWordFromPtr(epuStatus.uTimer5ms),Sys_GetInputBit(PortExtInt),tk);// Sys_GetInputBit(PortEpuInt)); // ,
    */
		TEpu_OutMpuAffirm();
		return fResult;
	}

	// 获得EPU版本
	void TEpu_Test(UDATA uTest)
	{
		switch ( uTest )
		{
		case 1:
			TEpu_EnterDebugMode(TRUE);
			break;
		case 2:
			TEpu_EnterDebugMode(FALSE);
			break;
		case 3:
			TEpu_GetEpuVersion();
			break;
		case 4:
			TEpu_GetEpuStatus();
			break;
		case 5:
			{
				u8 buffer[2];
				buffer[0] = 0xA3;
				Sys_I2CWriteData(TEpu_REG_STATUS,buffer,1);
			}
			break;
		}
	}

	#endif
#endif

#if HARDMODEL_Is2220Serial
// 通知外部EPU进入下载模式
void CLoader_NoticeEnterDownload( void )
{
	u8 buffer[2];
	buffer[0] = 0xA3;
	//TRACE0('K','M',"NoticeEnterDownload");

	Sys_I2CWriteData(TEpu_REG_STATUS,buffer,1);
	Sys_I2CWriteData(TEpu_REG_STATUS,buffer,1);

/*
	TEpu_MPUDOWNLOADCTRL info;
	u8 uCheck;

	TRACE0('K','M',"NoticeEnterDownload");

	info.stx = TEpu_CHAR_STX;
	info.len = TEpu_MPUDOWNLOADCTRL_SIZE-1;
	info.id = 0X13;
	info.ref = 1;
	info.flag = 0; 
	info.mode = 1 ; ////- (fEnter)?1:2;
	uCheck = TEpu_GetCheckResult(&info,TEpu_MPUDOWNLOADCTRL_SIZE-1);
	info.check = uCheck;

	Sys_I2CWriteData(TEpu_REG_COMMAND,&info,TEpu_MPUDOWNLOADCTRL_SIZE);*/
}
#endif

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#ifndef FUNC_SYS_DEBUG
static const u8 CCrc_uCrc8Table[256]   =   // CRC8查找表
{
	0x00,   0x31,   0x62,   0x53,   0xc4,   0xf5,   0xa6,   0x97,     0x88,   0xb9,   0xea,   0xdb,   0x4c,   0x7d,   0x2e,   0x1f,
	0x21,   0x10,   0x43,   0x72,   0xe5,   0xd4,   0x87,   0xb6,     0xa9,   0x98,   0xcb,   0xfa,   0x6d,   0x5c,   0x0f,   0x3e,
	0x73,   0x42,   0x11,   0x20,   0xb7,   0x86,   0xd5,   0xe4,     0xfb,   0xca,   0x99,   0xa8,   0x3f,   0x0e,   0x5d,   0x6c,
	0x52,   0x63,   0x30,   0x01,   0x96,   0xa7,   0xf4,   0xc5,     0xda,   0xeb,   0xb8,   0x89,   0x1e,   0x2f,   0x7c,   0x4d,
	0xe6,   0xd7,   0x84,   0xb5,   0x22,   0x13,   0x40,   0x71,     0x6e,   0x5f,   0x0c,   0x3d,   0xaa,   0x9b,   0xc8,   0xf9,
	0xc7,   0xf6,   0xa5,   0x94,   0x03,   0x32,   0x61,   0x50,     0x4f,   0x7e,   0x2d,   0x1c,   0x8b,   0xba,   0xe9,   0xd8,
	0x95,   0xa4,   0xf7,   0xc6,   0x51,   0x60,   0x33,   0x02,     0x1d,   0x2c,   0x7f,   0x4e,   0xd9,   0xe8,   0xbb,   0x8a, 
	0xb4,   0x85,   0xd6,   0xe7,   0x70,   0x41,   0x12,   0x23,     0x3c,   0x0d,   0x5e,   0x6f,   0xf8,   0xc9,   0x9a,   0xab,
	0xcc,   0xfd,   0xae,   0x9f,   0x08,   0x39,   0x6a,   0x5b,     0x44,   0x75,   0x26,   0x17,   0x80,   0xb1,   0xe2,   0xd3,
	0xed,   0xdc,   0x8f,   0xbe,   0x29,   0x18,   0x4b,   0x7a,     0x65,   0x54,   0x07,   0x36,   0xa1,   0x90,   0xc3,   0xf2,
	0xbf,   0x8e,   0xdd,   0xec,   0x7b,   0x4a,   0x19,   0x28,     0x37,   0x06,   0x55,   0x64,   0xf3,   0xc2,   0x91,   0xa0,
	0x9e,   0xaf,   0xfc,   0xcd,   0x5a,   0x6b,   0x38,   0x09,     0x16,   0x27,   0x74,   0x45,   0xd2,   0xe3,   0xb0,   0x81,
	0x2a,   0x1b,   0x48,   0x79,   0xee,   0xdf,   0x8c,   0xbd,     0xa2,   0x93,   0xc0,   0xf1,   0x66,   0x57,   0x04,   0x35,
	0x0b,   0x3a,   0x69,   0x58,   0xcf,   0xfe,   0xad,   0x9c,     0x83,   0xb2,   0xe1,   0xd0,   0x47,   0x76,   0x25,   0x14, 
	0x59,   0x68,   0x3b,   0x0a,   0x9d,   0xac,   0xff,   0xce,     0xd1,   0xe0,   0xb3,   0x82,   0x15,   0x24,   0x77,   0x46,
	0x78,   0x49,   0x1a,   0x2b,   0xbc,   0x8d,   0xde,   0xef,     0xf0,   0xc1,   0x92,   0xa3,   0x34,   0x05,   0x56,   0x67
};  
#endif

// 获得CRC8的校验和. uInitCrc是初始值，从0开始，代表之前数据的校验和
u8 CCrc_GetCrc8( const void *pData, u32 uSize,u8 uInitCrc)
{	
#ifdef FUNC_SYS_DEBUG
	return 0;
#else
	u8 uCrc ;
	const u8 * pBuf;
	
	pBuf = (const u8 *)pData;	
	uCrc = uInitCrc ^ 0xff;
	
	while(uSize--)
		uCrc = CCrc_uCrc8Table[uCrc^(*pBuf++)];   
	return uCrc ^ 0xff;     
#endif
}

// 获得奇偶校验和.按字节。
u8 CMath_GetXorChecksum(const void * pBuf,UINT ulen)
{
	u8 uCheck=0;
        const u8 *p = pBuf;
	for( ;ulen; ulen--,p++ )
		uCheck ^= *p;
	return uCheck;
}

// 获得累加校验和.按字节。
UINT CMath_GetAddChecksum(const void * pBuf,UINT ulen)
{
	UINT uCheck=0;
	UINT uCount;
	
	// 每相加64K数据，进行喂狗操作
	// 	for( uCount=0;ulen;((const u8 *)pBuf)++,ulen-- )
	// 	{
	// 		uCheck += *((const u8 *)pBuf);	
	// 		if ( ++uCount >= CMath_CHECKCOUNT )
	// 		{
	// 			uCount = 0;
	// 			WatchDog();
	// 		}
	// 	}
	const u8 * p=pBuf;
	for ( ; ulen ; )
	{
		WatchDog();
		uCount = min(CMath_CHECKCOUNT,ulen);
		ulen -= uCount;
		// 每相加64K数据，进行喂狗操作
		for( ;uCount;p++,uCount-- )
		{
			uCheck += *p;	
		}
	}
	return uCheck;
}

#endif
