/*
 * u_modbus2.c
 *
 *  Created on: 11 มี.ค. 2563
 *      Author: Elect
 */


#include "r_cg_macrodriver.h"				///< For Pin name
#include "r_cg_sau.h"						///< For UART, DON't forget Start at r_cg_main.c
#include "u_modbus2.h"

//================ Define =================//
#define SetDR(x)               		P3_bit.no1 = x   //1 = Drive Mode, 0 = Receive Mode
#define SendData()			   		R_UART1_Send(tx_rx, tx_rx_len)	///< Must include "r_cg_sau.h"
#define TX_RX_MAX              		260
const unsigned char FRAME_MIN = 5;	//Minimum frame size of MB: Slave ID, Function Code, Error Code, CRC Hi, CRC Lo

typedef struct{
    unsigned char fn;
    unsigned short request[MB_RESPONSE_MAX];
    unsigned short request_len;
}MB_REQUEST;


//================ Variable ===============//
MB_CONFIG *_mbCon;    	//Config
MB_REQUEST mbReq;    	//Request message
MB_RESPONE mbResp;   	//Response message
MB_SPECIAL mbSpec;

unsigned char tx_rx[TX_RX_MAX]; //TX RX Buffer
unsigned short tx_rx_len, exp_rx_len, count_up;

//Callback
MB_CALLBACK *_callback;


//================ Program ===============//
void ClearU16Array(unsigned short *arr, unsigned short len){
    unsigned short l;
    for(l=0; l<len; l++){ arr[l] = 0; }
}

void ClearU8Array(unsigned char *arr, unsigned short len){
    unsigned short l;
    for(l=0; l<len; l++){ arr[l] = 0; }
}


void Clear_MB_SPECIAL(MB_SPECIAL *sp){
    sp->memory.productID = 0;
    sp->memory.fault_len = 0;
    sp->memory.ram_len = 0;
    sp->memory.ram_start = 0;
    sp->memory.rom_len = 0;
    sp->memory.rom_start = 0;
    sp->memory.rsv = 0;
    sp->memory.setSize_len = 0;
    sp->segment.dg0 = 0;
    sp->segment.dg1 = 0;
    sp->segment.dg2 = 0;
    sp->segment.dg3 = 0;
    sp->config.a = 0;
    sp->config.f_run = 0;
    sp->config.f_set = 0;
    sp->config.fault = 0;
    sp->config.hz = 0;
    sp->config.n_a = 0;
    sp->config.n_f_set = 0;
    sp->config.n_hz = 0;
    sp->config.operation = 0;
    sp->config.rsv = 0;
    sp->config.setSize = 0;
    sp->config.softVer = 0;
    sp->config.status1 = 0;
    sp->config.status2 = 0;
    ClearU8Array(sp->configD1.d, 16);
    ClearU8Array(sp->configD1.n, 16);
    sp->detail.addr = 0;
    sp->detail.data = 0;
    sp->detail.def = 0;
    sp->detail.dotFormat = 0;
    sp->detail.max = 0;
    sp->detail.min = 0;
    sp->detail.rw = 0;
}

void MB_Init(MB_CONFIG *mbCon, MB_CALLBACK *callback){
    //Clear
	tx_rx_len = 0;
	ClearU8Array(tx_rx, TX_RX_MAX);
    mbReq.request_len = 0;
    ClearU16Array(mbReq.request, MB_RESPONSE_MAX);
    mbResp.response_len = 0;
    ClearU16Array(mbResp.response, MB_RESPONSE_MAX);
    count_up = 0;
    Clear_MB_SPECIAL(&mbSpec);
    //Drive Mode
    SetDR(1);
    //Config
    _mbCon = mbCon;
    _mbCon->t3_5 = 4;	 //9600 bps
    //Callback
    _callback = callback;

}


void Set_MB_ReadHolding(unsigned short startAddress, unsigned short len){
    //Copy
    mbReq.request[0] = startAddress;
    mbReq.request[1] = len;
    mbReq.request_len = 2;
    //Flag for exe in next loop
    _mbCon->next_fn = Fn_ReadHolding;
}

void Set_MB_WriteSingle(unsigned short address, unsigned short value){
    //Copy
    mbReq.request[0] = address;
    mbReq.request[1] = value;
    mbReq.request_len = 2;
    //Flag for exe in next loop
    _mbCon->next_fn = Fn_WriteSingle;
}

void Set_MB_WriteMultiple(unsigned short startAddress, unsigned short *value, unsigned short len){
    unsigned short i;
    //Copy
    mbReq.request[0] = startAddress;
    mbReq.request[1] = 0;
    for(i=0; i<len; i++){
        if(i<MB_RESPONSE_MAX){
            mbReq.request[2 + (mbReq.request[1]++)] = value[i];
        }
    }
    mbReq.request_len = mbReq.request[1] + 1;
    //Flag for exe in next loop
    _mbCon->next_fn = Fn_WriteMultiple;
}

void Set_MB_Special(MB_FUNCTION fn, unsigned short v0){
    mbReq.request[0] = v0;
    mbReq.request_len = 1;
    //Flag for exe in next loop
    _mbCon->next_fn = fn;
}

unsigned short MB_CRC16(const unsigned  char *nData, unsigned short wLength){
unsigned char nTemp;
unsigned short wCRCWord = 0xFFFF;
const unsigned short wCRCTable[] = {
   0X0000, 0XC0C1, 0XC181, 0X0140, 0XC301, 0X03C0, 0X0280, 0XC241,
   0XC601, 0X06C0, 0X0780, 0XC741, 0X0500, 0XC5C1, 0XC481, 0X0440,
   0XCC01, 0X0CC0, 0X0D80, 0XCD41, 0X0F00, 0XCFC1, 0XCE81, 0X0E40,
   0X0A00, 0XCAC1, 0XCB81, 0X0B40, 0XC901, 0X09C0, 0X0880, 0XC841,
   0XD801, 0X18C0, 0X1980, 0XD941, 0X1B00, 0XDBC1, 0XDA81, 0X1A40,
   0X1E00, 0XDEC1, 0XDF81, 0X1F40, 0XDD01, 0X1DC0, 0X1C80, 0XDC41,
   0X1400, 0XD4C1, 0XD581, 0X1540, 0XD701, 0X17C0, 0X1680, 0XD641,
   0XD201, 0X12C0, 0X1380, 0XD341, 0X1100, 0XD1C1, 0XD081, 0X1040,
   0XF001, 0X30C0, 0X3180, 0XF141, 0X3300, 0XF3C1, 0XF281, 0X3240,
   0X3600, 0XF6C1, 0XF781, 0X3740, 0XF501, 0X35C0, 0X3480, 0XF441,
   0X3C00, 0XFCC1, 0XFD81, 0X3D40, 0XFF01, 0X3FC0, 0X3E80, 0XFE41,
   0XFA01, 0X3AC0, 0X3B80, 0XFB41, 0X3900, 0XF9C1, 0XF881, 0X3840,
   0X2800, 0XE8C1, 0XE981, 0X2940, 0XEB01, 0X2BC0, 0X2A80, 0XEA41,
   0XEE01, 0X2EC0, 0X2F80, 0XEF41, 0X2D00, 0XEDC1, 0XEC81, 0X2C40,
   0XE401, 0X24C0, 0X2580, 0XE541, 0X2700, 0XE7C1, 0XE681, 0X2640,
   0X2200, 0XE2C1, 0XE381, 0X2340, 0XE101, 0X21C0, 0X2080, 0XE041,
   0XA001, 0X60C0, 0X6180, 0XA141, 0X6300, 0XA3C1, 0XA281, 0X6240,
   0X6600, 0XA6C1, 0XA781, 0X6740, 0XA501, 0X65C0, 0X6480, 0XA441,
   0X6C00, 0XACC1, 0XAD81, 0X6D40, 0XAF01, 0X6FC0, 0X6E80, 0XAE41,
   0XAA01, 0X6AC0, 0X6B80, 0XAB41, 0X6900, 0XA9C1, 0XA881, 0X6840,
   0X7800, 0XB8C1, 0XB981, 0X7940, 0XBB01, 0X7BC0, 0X7A80, 0XBA41,
   0XBE01, 0X7EC0, 0X7F80, 0XBF41, 0X7D00, 0XBDC1, 0XBC81, 0X7C40,
   0XB401, 0X74C0, 0X7580, 0XB541, 0X7700, 0XB7C1, 0XB681, 0X7640,
   0X7200, 0XB2C1, 0XB381, 0X7340, 0XB101, 0X71C0, 0X7080, 0XB041,
   0X5000, 0X90C1, 0X9181, 0X5140, 0X9301, 0X53C0, 0X5280, 0X9241,
   0X9601, 0X56C0, 0X5780, 0X9741, 0X5500, 0X95C1, 0X9481, 0X5440,
   0X9C01, 0X5CC0, 0X5D80, 0X9D41, 0X5F00, 0X9FC1, 0X9E81, 0X5E40,
   0X5A00, 0X9AC1, 0X9B81, 0X5B40, 0X9901, 0X59C0, 0X5880, 0X9841,
   0X8801, 0X48C0, 0X4980, 0X8941, 0X4B00, 0X8BC1, 0X8A81, 0X4A40,
   0X4E00, 0X8EC1, 0X8F81, 0X4F40, 0X8D01, 0X4DC0, 0X4C80, 0X8C41,
   0X4400, 0X84C1, 0X8581, 0X4540, 0X8701, 0X47C0, 0X4680, 0X8641,
   0X8201, 0X42C0, 0X4380, 0X8341, 0X4100, 0X81C1, 0X8081, 0X4040 };

   while (wLength--)
   {
      nTemp = (unsigned char)((*nData++) ^ (wCRCWord));
      wCRCWord >>= 8;
      wCRCWord  ^= wCRCTable[nTemp];
   }

   return wCRCWord;
} // End: CRC16


void Send_MB_Request(void){
    //Local Variable
    unsigned short crc, i;
    //Clear
    ClearU16Array(mbResp.response, mbResp.response_len);
    mbResp.response_len = 0;
    mbResp.error = ERR_None;
    //Update
    mbReq.fn = _mbCon->next_fn;
    mbResp.request = mbReq.request[0];
    //Set Byte Array
    //Header
    tx_rx_len = 0;
    tx_rx[tx_rx_len++] = _mbCon->slave_id;
    tx_rx[tx_rx_len++] = mbReq.fn;
    //Data
    switch (mbReq.fn){
    case Fn_ReadHolding:
    case Fn_WriteSingle:
        tx_rx[tx_rx_len++] = highByte(mbReq.request[0]);
        tx_rx[tx_rx_len++] = lowByte(mbReq.request[0]);
        tx_rx[tx_rx_len++] = highByte(mbReq.request[1]);
        tx_rx[tx_rx_len++] = lowByte(mbReq.request[1]);
        break;
    case Fn_WriteMultiple:
        tx_rx[tx_rx_len++] = highByte(mbReq.request[0]);    //Start Address
		tx_rx[tx_rx_len++] = lowByte(mbReq.request[0]);
		tx_rx[tx_rx_len++] = highByte(mbReq.request[1]);    //Number Of Registers
		tx_rx[tx_rx_len++] = lowByte(mbReq.request[1]);
		tx_rx[tx_rx_len++] = lowByte(mbReq.request[1] << 1); //Byte Count
		for (i = 0; i < lowByte(mbReq.request[1]); i++){     //Data
			tx_rx[tx_rx_len++] = highByte(mbReq.request[i+2]);
			tx_rx[tx_rx_len++] = lowByte(mbReq.request[i+2]);
		}
        break;
    case Fn_ReadMemorySize:
        tx_rx[tx_rx_len++] = lowByte(mbReq.request[0]);	//Software version
        tx_rx[tx_rx_len++] = 0;	//RSV
    	break;
	case Fn_ReadSizeInvName:
	case Fn_ReadFaultName:
	case Fn_ReadFirmwareName:
		tx_rx[tx_rx_len++] = lowByte(mbReq.request[0]);	//Fault code
		break;
	case Fn_ReadDetailAddr:
        tx_rx[tx_rx_len++] = highByte(mbReq.request[0]); 	//MB Address Hi
        tx_rx[tx_rx_len++] = lowByte(mbReq.request[0]);		//MB Address Lo
        tx_rx[tx_rx_len++] = 0;	//RSV
        tx_rx[tx_rx_len++] = 0;	//RSV
		break;
    }
    //Expect RX length
    switch (mbReq.fn)
    {
    case Fn_ReadHolding:
        exp_rx_len = 5 + (mbReq.request[1]<<1);
        break;
    case Fn_WriteSingle:
    case Fn_WriteMultiple:
        exp_rx_len = 8;
        break;
    case Fn_ReadMemorySize:
    	exp_rx_len = 16;
    	break;
	case Fn_ReadSizeInvName:
	case Fn_ReadFaultName:
		exp_rx_len = 10;
		break;
	case Fn_ReadFirmwareName:
		exp_rx_len = 9;
		break;
	case Fn_ReadConfigAddr:
		exp_rx_len = 18;
		break;
	case Fn_ReadConfigD1Addr:
		exp_rx_len = 36;
		break;
	case Fn_ReadDetailAddr:
		exp_rx_len = 16;
		break;
    }
    //CRC
    crc = MB_CRC16(tx_rx, tx_rx_len);
    tx_rx[tx_rx_len++] = lowByte(crc);
    tx_rx[tx_rx_len++] = highByte(crc);
    //Send to Slave
    SetDR(1);       //Drive Mode
    SendData();     //Send Byte Array
}

void MB_Receive_Init(void){
    //Clear
    count_up = 0;
    tx_rx_len = 0;
    //Switch Mode
    SetDR(0);
    R_UART1_Receive(tx_rx, FRAME_MIN);
    //Status
    _mbCon->status = MB_SendEnd;
}

void Callback_Response(void){
	//Block
	SetDR(1);
    //Callback
	if(_callback){ _callback(&mbResp, &mbSpec); }
	//End
	_mbCon->status = MB_ResponseEnd;
	count_up = 0;
}

void Set_MB_Response(void){
	SEGMENT_VALUE *seg;
	MB_CONFIG_ADDR *con;
	MB_CONFIG_D1_ADDR *cond1;
	unsigned short crc;
	//Get RX length = FRAM_SIZE
	if(!tx_rx_len){
		//Get only function code
		mbResp.fn = (MB_FUNCTION)(tx_rx[1] & 0x7F);
		//Check
	    if(tx_rx[0] != _mbCon->slave_id)
	    {
	        //Invalid Slave ID
	        mbResp.error = ERR_InvalidSlaveID;
	    }
	    else if((tx_rx[1] & 0x7F) != mbReq.fn)
	    {
	        //Invalid Function
	        mbResp.error = ERR_InvalidFunction;
	    }
	    else if(bitRead(tx_rx[1], 7))
	    {
	        //MB Exception
	        mbResp.error = tx_rx[2];
	    }
	    if(mbResp.error)
	    {
		    //Callback
		    Callback_Response();
	    }
	    else
	    {
	    	//Waiting to finish
	    	tx_rx_len = FRAME_MIN;
	    	R_UART1_Receive(&tx_rx[FRAME_MIN], exp_rx_len-FRAME_MIN);
	    }
	}else{
		//Receive Finish
		tx_rx_len = exp_rx_len;
	    //Check CRC
		crc = MB_CRC16(tx_rx, tx_rx_len-2);
	    if (tx_rx[tx_rx_len-2] != lowByte(crc) || tx_rx[tx_rx_len-1] != highByte(crc))
	    {
	        //Invalid CRC
	        mbResp.error = ERR_InvalidCRC;
	    }
	    else
	    {
	        //Normal
	        unsigned short i;
	        mbResp.error = ERR_None;
	        switch (mbResp.fn)
	        {
	        case Fn_ReadHolding:
	            for(i=0; i<(tx_rx[2]>>1); i++){
	            	mbResp.response[mbResp.response_len++] = u16Value(tx_rx[(2*i)+3], tx_rx[(2*i)+4]);
	            }
	            break;
	        case Fn_WriteSingle:
	        case Fn_WriteMultiple:
	            for(i=0; i<2; i++){
	            	mbResp.response[mbResp.response_len++] = u16Value(tx_rx[(2*i)+2], tx_rx[(2*i)+3]);
	            }
	            break;
	        case Fn_ReadMemorySize:
	        	mbSpec.memory.productID = tx_rx[2];
	        	mbSpec.memory.rom_start = u16Value(tx_rx[3], tx_rx[4]);
	        	mbSpec.memory.rom_len = u16Value(tx_rx[5], tx_rx[6])
	        	mbSpec.memory.ram_start = u16Value(tx_rx[7], tx_rx[8])
	        	mbSpec.memory.ram_len = u16Value(tx_rx[9], tx_rx[10])
	        	mbSpec.memory.setSize_len = tx_rx[11];
	        	mbSpec.memory.fault_len = tx_rx[12];
	        	break;
	    	case Fn_ReadSizeInvName:
	    	case Fn_ReadFaultName:
	    		seg = (SEGMENT_VALUE*)&tx_rx[4]; //&tx_rx[4]: Complier look pointer variable that start from index = 4
	    		mbSpec.segment = *seg;
	    		break;
	    	case Fn_ReadFirmwareName:
	    		seg = (SEGMENT_VALUE*)&tx_rx[3];
	    		mbSpec.segment = *seg;
	    		break;
	    	case Fn_ReadConfigAddr:
	    		con = (MB_CONFIG_ADDR*)&tx_rx[2];
	    		mbSpec.config = *con;
	    		break;
	    	case Fn_ReadConfigD1Addr:
	    		cond1 = (MB_CONFIG_D1_ADDR*)&tx_rx[2];
	    		mbSpec.configD1 = *cond1;
	    		break;
	    	case Fn_ReadDetailAddr:
	    		mbSpec.detail.addr = u16Value(tx_rx[2], tx_rx[3]);
	    		mbSpec.detail.data = u16Value(tx_rx[4], tx_rx[5]);
	    		mbSpec.detail.min = u16Value(tx_rx[6], tx_rx[7]);
	    		mbSpec.detail.max = u16Value(tx_rx[8], tx_rx[9]);
	    		mbSpec.detail.def = u16Value(tx_rx[10], tx_rx[11]);
	    		mbSpec.detail.rw = tx_rx[12];
	    		mbSpec.detail.dotFormat = tx_rx[13];
	    		break;
	        }
	    }
	    //Callback
	    Callback_Response();
	}
}

/*
 * MODBUS driving function
 *
 * Call this by loop 1ms
 * STOP by _mbCon->next_fn = Fn_None;
 *
 * */
void MB_Handle(void){
	if(_mbCon->next_fn && _mbCon->status==MB_Ready){
		count_up++;
		//Wait 3.5T before send request
		if(count_up > (_mbCon->t3_5 + _mbCon->interval_time)){
			count_up = 0;
			_mbCon->status = MB_None;
			Send_MB_Request();
		}
	}else if(_mbCon->status == MB_SendEnd){
		//Check response timeout
		count_up++;
    	if(count_up>950){
    	    //Callback
    	    Callback_Response();
    	}
	}else if(_mbCon->status == MB_ResponseEnd){
		count_up++;
		//Wait 3.5T to set Ready
		if(count_up > _mbCon->t3_5){
			count_up = 0;
			_mbCon->status = MB_Ready;
		}
	}
}
