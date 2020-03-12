/*
 * u_modbus2.h
 *
 *  Created on: 11 มี.ค. 2563
 *      Author: Elect
 */

#ifndef CG_SRC_U_MODBUS2_H_
#define CG_SRC_U_MODBUS2_H_



/**
@def lowWord(ww) ((uint16_t) ((ww) & 0xFFFF))
Macro to return low word of a 32-bit integer.
*/
#define lowWord(ww) 					((uint16_t) ((ww) & 0xFFFF))

/**
@def highWord(ww) ((uint16_t) ((ww) >> 16))
Macro to return high word of a 32-bit integer.
*/
#define highWord(ww) 					((uint16_t) ((ww) >> 16))

/**
@def LONG(hi, lo) ((uint32_t) ((hi) << 16 | (lo)))
Macro to generate 32-bit integer from (2) 16-bit words.
*/
#define LONG(hi, lo) 					((uint32_t) ((hi) << 16 | (lo)))

#define lowByte(w) 						((uint8_t) ((w) & 0xff))
#define highByte(w) 					((uint8_t) ((w) >> 8))

#define bitRead(value, bit) 			(((value) >> (bit)) & 0x01)
#define bitSet(value, bit) 				((value) |= (1UL << (bit)))
#define bitClear(value, bit) 			((value) &= ~(1UL << (bit)))
#define bitWrite(value, bit, bitvalue) 	(bitvalue ? bitSet(value, bit) : bitClear(value, bit))

#define u16Value(hi, lo)				(unsigned short)(hi<<8) | lo;


#define MB_RESPONSE_MAX         125

typedef struct{
    unsigned char slave_id;
    unsigned char t3_5;             // 3.5char (ms)
    unsigned short interval_time;   // Time to Try Again
    unsigned char next_fn;          // Next function
    unsigned char status;           // Current Status
}MB_CONFIG;

typedef struct{
    unsigned char error;
    unsigned char fn;
    unsigned short response[MB_RESPONSE_MAX];
    unsigned short response_len;
}MB_RESPONE;

typedef enum{
    MB_None = 0,
    MB_Ready,
    MB_SendEnd,
    MB_ResponseEnd,
}MB_SATUS;

typedef enum{
    ERR_None = 0,   //Success
    ERR_IllegalFunction = 0x01,
    ERR_IllegalDataAddress = 0x02,
    ERR_IllegalDataValue = 0x03,
    ERR_SlaveDeviceFailure = 0x04,
    ERR_InvalidSlaveID = 0xA0,
    ERR_InvalidFunction = 0xA1,
    ERR_ResponseTimeout = 0xA2,
    ERR_InvalidCRC = 0xA3
}MB_ERROR;

typedef enum{
    Fn_None = 0,
	//Standard Function
    Fn_ReadHolding = 3,
    Fn_WriteSingle = 6,
    Fn_WriteMultiple = 16,
	//Special Function
	Fn_ReadMemorySize = 32,
	Fn_ReadSizeInvName = 33,
	Fn_ReadFaultName = 34,
	Fn_ReadFirmwareName = 35,
	Fn_ReadConfigAddr = 36,
	Fn_ReadConfigD1Addr = 37,
	Fn_ReadDetailAddr = 40
}MB_FUNCTION;

//F#32
typedef struct {
	unsigned char productID;
	unsigned short rom_start;
	unsigned short rom_len;
	unsigned short ram_start;
	unsigned short ram_len;
	unsigned char setSize_len;
	unsigned char fault_len;
	unsigned char rsv;
}MB_MEMORY_SIZE;

//F#33, 34, 35
typedef struct {
	unsigned char dg3;		//7-segment value
	unsigned char dg2;
	unsigned char dg1;
	unsigned char dg0;
}SEGMENT_VALUE;

//F#36
typedef struct {
	unsigned char setSize;
	unsigned char softVer;
	unsigned char rsv;
	unsigned char operation;
	unsigned char f_set;
	unsigned char f_run;
	unsigned char status1;
	unsigned char status2;
	unsigned char fault;
	unsigned char hz;
	unsigned char a;
	unsigned char n_f_set;
	unsigned char n_hz;
	unsigned char n_a;
}MB_CONFIG_ADDR;

//F#37
typedef struct {
	unsigned char d[16];
	unsigned char n[16];
}MB_CONFIG_D1_ADDR;

//F#40
typedef struct {
	unsigned short addr;
	unsigned short data;
	unsigned short min;
	unsigned short max;
	unsigned short def;
	unsigned char rw;
	unsigned char dotFormat;		//0-3 = Point, 16 = HEX
}MB_DETAIL_ADDR;

typedef struct{
	MB_MEMORY_SIZE memory;
	SEGMENT_VALUE segment;
	MB_CONFIG_ADDR config;
	MB_CONFIG_D1_ADDR configD1;
	MB_DETAIL_ADDR detail;
}MB_SPECIAL;


typedef void MB_CALLBACK(MB_RESPONE *mb, MB_SPECIAL *mbSpec);

//Global Function
void MB_Init(MB_CONFIG *mbCon, MB_CALLBACK *callback);	//Don't forget initial before use other function
void MB_Handle(void);           //Use at loop 1 ms
void MB_Receive_Init(void);		//Use at r_uart1_callback_sendend() of r_cg_sau_user.c
void Set_MB_Response(void);		//Use at r_uart1_callback_receiveend() of r_cg_sau_user.c

void Set_MB_ReadHolding(unsigned short startAddress, unsigned short len);	//F#3
void Set_MB_WriteSingle(unsigned short address, unsigned short value);	//F#6
void Set_MB_WriteMultiple(unsigned short startAddress, unsigned short value[], unsigned short len);	//F#16
void Set_MB_Special(MB_FUNCTION fn, unsigned short v0);	//F#32 - F#40


#endif /* CG_SRC_U_MODBUS2_H_ */
