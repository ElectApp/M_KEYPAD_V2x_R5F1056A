/*
 * u_keypad.c
 *
 *  Created on: 10 มี.ค. 2563
 *      Author: Elect
 */

//=========== Library =========//
#include "r_cg_macrodriver.h"					///< PORT
#include "u_keypad.h"
#include "u_data_flash2.h"

//=========== Button Input Pin ==============//
#define SW_LINE1			P12_bit.no2
#define SW_LINE2			P12_bit.no1

//=================== 7HC595 PIN, 1 = High, 0 = Low ==========================//

#define SCLK(x)		P5_bit.no6 = x			///< Shift Register Clock Input
#define RCLK(x)		P5_bit.no5 = x			///< Storage Register Clock Input
#define SDAT(x)		P5_bit.no4 = x			///< Serial Data Input


//================== DIGIT & LED, 1 = Enable, 0 = Disable, DON't all enable in same time ================//

#define DG1(x)		P3_bit.no0 = x
#define DG2(x)		P2_bit.no3 = x
#define DG3(x)		P2_bit.no2 = x
#define DG4(x)		P2_bit.no1 = x
#define LED(x)		P2_bit.no0 = x


//============== Constant ================//

//Time
//const unsigned short BLINK_TIME = 1000;
//const unsigned short HOLD_SET_TIME = 1000;

//Blink Flag
#define ALL_DIGIT_BLINK	0b00001111
//Main LED Display
#define MAIN_LED_OFF	0b00000111


//=================== Push Button value match SW[][] (Convert name to Inverter's Keypad) ============================//

//#define FUNC_BT			1
//#define MOVE_BT			2
//#define UP_BT				3
//#define STR_BT			4
//#define DOWN_BT			5
//#define SET_BT			6
//#define ENTER_BT			7
//#define RUN_BT			8

// Button Position on keypad (KEY-AIR-ES00), EX. 1 = SW1 (ON/OFF) Use with SwitchCallback function
const unsigned char SW[2][4] = 	{ 	{	ENTER_BT,	DOWN_BT,	UP_BT,	FUNC_BT		},
									{	RUN_BT,		SET_BT,		STR_BT,	MOVE_BT		}
								};

//Use with DataDisplay function
const char Num_code[16]={0xEE,0x82,0xDC,0xDA,0xB2,0x7A,0x7E,0xC2,0xFE,0xFA,0xF6,0x3E,0x6C,0x9E,0x7C,0x74};

//==================== Password ====================//
const char PASS_LEN = 4;
const unsigned char password[] = { MOVE_BT, SET_BT, SET_BT, RUN_BT };
unsigned char lastPass[4];		//Save sequence of button that user press for checking password correct!
unsigned char pass_count;

//============================== Other ============================//
const unsigned char SOFT_VER = 120;		//Version: Edit interval reading of running mode, reset fault, emergency stop. fix bug a group of parameter setting
const unsigned char PARA_LEN = 50;		//Number of parameter per group
enum{
	G_A,
	G_B,
	G_C,
	G_D,
	G_E,
	G_F,
	G_H,
	G_MAX
}GROUP_TYPE;
const unsigned char GROUP_NAME[G_MAX] = {DG_A, DG_b, DG_C, DG_d, DG_E, DG_F, DG_H};

//Parameter Setting Group
unsigned char GROUP_LEN;
para_g GROUP[G_MAX];

KEYPAD_DATA key;		//Keypad
O_STATUS oSt;			//Storage Driver Status
O_COMMAND oCom;			//Storage Driver Command

//=============== MODBUS ==============//
#define SET_POINT_ADDR				memSize.ram_start+conAddr.f_set
#define A00_ADDR					memSize.rom_start
#define DISPLAY1_ADDR				memSize.ram_start+conD1Addr.d[key.A00]
#define SET_SIZE_ADDR				memSize.rom_start+conAddr.setSize
#define VERSION_ADDR				memSize.rom_start+conAddr.softVer
#define COMM_ADDR					memSize.ram_start+conAddr.operation
#define WriteSetPoint()				Set_MB_WriteSingle(SET_POINT_ADDR, key.main_v[D_SP])
#define WriteControlCommand(x)		Set_MB_WriteSingle(COMM_ADDR, x)
//Interval Time
const unsigned short INTERVAL_STOP = 1000;
const unsigned short INTERVAL_RUN = 1000;	//Changed by P'M 18/03/2020
//Variable
MB_CONFIG mbCon;
MB_MEMORY_SIZE memSize;
MB_CONFIG_ADDR conAddr;
MB_CONFIG_D1_ADDR conD1Addr;
MB_DETAIL_ADDR f_setDetail;
MB_GROUP mbG;

//Other
extern unsigned short ms_counter;

//================ Data Flash ============//
extern ROM_DATA rom;


//===================== Program ============//
void ModbusResponse(MB_RESPONE *mb, MB_SPECIAL *mbSpec);

/*
 * DON't forget set before use
 *
 * */
void InitialKeypad(void){

	//Display 'APY' in first time
	key.digitData[0] = 0;
	key.digitData[1] = DG_A;
	key.digitData[2] = DG_P;
	key.digitData[3] = DG_Y;

	//Clear
	key.last_sw = 0;	//Last SW
	key.mode.word = 0;  //Function Mode
	key.double_sw = 0;
	key.count_double_sw = 0;


    //Initial and start MB
    mbCon.slave_id = 1;
    mbCon.interval_time = 10;
    MB_Init(&mbCon, &ModbusResponse);
    Set_MB_Special(Fn_ReadMemorySize, SOFT_VER);
    mbCon.status = MB_Ready;

}


void ShiftOutAndLatch(unsigned char data){
	signed char i;   //Can't use only 'char i'
	RCLK(0); //Clear latch
	SDAT(0); SCLK(0);  //Clear state
	//Shift bit
	for(i=7; i>=0; i--){
		SCLK(0);
		SDAT((data>>i)&0x01);
		SCLK(1);
		SDAT(0);
	}
	SDAT(0); SCLK(0);  //Clear
	RCLK(1); //Latch
}

void DisplaySingleDigit(unsigned char *index, unsigned char *value){
	//Clear All
	ShiftOutAndLatch(0);
	DG1(0); DG2(0); DG3(0); DG4(0); LED(0);	//Clear Bias
	//On Bias
	switch(*index){
		case 4: LED(1); break;	//Same digit
		case 3: DG1(1); break;
		case 2: DG2(1); break;
		case 1: DG3(1); break;
		case 0: DG4(1); break;
	}
	ShiftOutAndLatch(*value); 		//Latch and display
}


void LoopDisplay2(unsigned char state_off){
	//Count index
	key.current_digit_on++;
	if(key.current_digit_on>4){ key.current_digit_on = 0; }

	//Display
	if(key.current_digit_on==4){
		DisplaySingleDigit(&key.current_digit_on, &key.led.byte);
	}else{
		//Check DG off: 0-3
		if(state_off && bitRead(key.digit_blink.byte, key.current_digit_on)){
			DisplaySingleDigit(&key.current_digit_on, 0);
		}else{
			DisplaySingleDigit(&key.current_digit_on, &key.digitData[key.current_digit_on]);
		}
	}
}

void RunDisplay(void){

	//Check blink flag
	if(key.led_blink.byte || key.digit_blink.byte){
		//========= Blink Mode =========//
		//count time
		key.time_up[T_BLINK]++;
		if(key.time_up[T_BLINK]>500){
			//Hold Off

			//Set LED Off
			key.led.byte &= ~key.led_blink.byte;

			//Off
			LoopDisplay2(1);

			//Clear counter
			if(key.time_up[T_BLINK]>1000){ key.time_up[T_BLINK] = 0; }

		}else{
			//Set LED On
			key.led.byte |= key.led_blink.byte;

			//Hold ON
			LoopDisplay2(0);
		}
	}else{
		//Hold On
		LoopDisplay2(0);
	}

}

/***********************************************************************/

//  FUNCTION   	:SetNumberDisplay
//  DESCRIPTION :Show HEX or DEC
//  Developed By: Somrat

/***********************************************************************/
void SetNumberDisplay(unsigned char Hex_Dec, unsigned short Data, unsigned char Full_display, unsigned char Dot_count, unsigned char Digit_total)
{	//Hex_Dec = 0 (Display Hex number), 1 (Display Decimal number)
	//Data is the information we want to display at keypad.
	//Full_display = 0(Show only data at keypad), 1(Show 4 Digit at keypad.)
	//	For example: Data = 3, Full_display = 0 Keypad will display Digit 4 = 3
	//						   Full_display = 1 Keypad will display 0003.
	//Dot_count is the number of point we want to show. For example: Data = 5000, Dot_count = 2 => Keypad shows 50.00
	//Digit_total is the total Digit we want to show at keypad. For example: Data is in range of 1-1000. Digit_total must be 4.

	char i, HDG_flag;
	unsigned short buff;
	float tmp;
	unsigned short Data_buff	=	Data;

	HDG_flag=0;
	//Clear
	for(i=0; i<4; i++)	key.digitData[i]=0;

//Hex display
	if(Hex_Dec==0)
	{
		buff=(Data_buff>>12)&0x000F			;	// 4 bit high

		if(buff>0)
		{
			key.digitData[0]=Num_code[buff]	;	//	DG4 is display
			HDG_flag=1;
		}
		else if(Full_display==1)	key.digitData[0]=Num_code[0];

		buff=(Data_buff>>8)&0x000F;

		if(buff>0)
		{
			key.digitData[1]=Num_code[buff];
			HDG_flag=1;
		}
		else if((Full_display==1)||(HDG_flag==1))	key.digitData[1]=Num_code[0];
		buff=(Data_buff>>4)&0x000F;
		if(buff>0)
		{
			key.digitData[2]=Num_code[buff];
			HDG_flag=1;
		}
		else if((Full_display==1)|(HDG_flag==1))	key.digitData[2]=Num_code[0];
		buff=Data_buff&0x000F;
		if(buff>0)	key.digitData[3]=Num_code[buff];
		else		key.digitData[3]=Num_code[0];
	}
//Dec display
	else
	{
		//==============================
		//-- Check data overflow dot? --
		//==============================
		if(Data > 999999){
			//	Not over 999,999.
			//	And cut 2 left digit.	=>	9,999
			//
			Data = 9999.99	;
			Dot_count -= 2	;
		}
		else if(Data>99999){		//	Scale down data with 0.01 and adjust dot.
			tmp	=	Data * 0.01				;		//	Limit 99,999 max
			Data	=	tmp	;
			Dot_count -= 2					;
		}
		else if(Data>9999){
			tmp	=	Data * 0.1				;		//	Limit 9,999 max
			Data=	tmp	;
			Dot_count -= 1					;
		}

		//	From above function, Data is not over 9,999	.

		Data_buff	= 	(unsigned short)Data	;
		//===============================
		if(Digit_total==1)
		{
			Data_buff%=10;
			goto DG4;
		}
		else if(Digit_total==2)
		{
			Data_buff%=100;
			goto DG3;
		}
		else if(Digit_total==3)
		{
			Data_buff%=1000;
			goto DG2;
		}
		// In case of Digit_total == 4
		buff=Data_buff*0.001;
		Data_buff%=1000;
		if(buff>0)
		{
			key.digitData[0]=Num_code[buff];
			HDG_flag=1;
		}
		else if(Full_display==1)	key.digitData[0]=Num_code[0];
DG2:	buff=Data_buff*0.01;
		Data_buff%=100;
		if(buff>0)
		{
			key.digitData[1]=Num_code[buff];
			HDG_flag=1;
		}
		else if((Full_display==1)||(HDG_flag==1))	key.digitData[1]=Num_code[0];
DG3:	buff=Data_buff*0.1;
		Data_buff%=10;
		if(buff>0)
		{
			key.digitData[2]=Num_code[buff];
			HDG_flag=1;
		}
		else if((Full_display==1)||(HDG_flag==1))	key.digitData[2]=Num_code[0];
DG4:	buff=Data_buff;
		if(buff>0)	key.digitData[3]=Num_code[buff];
		else		key.digitData[3]=Num_code[0];



	//Add dot '.' (Tos-Sa-Ni-Yom)
		switch(Dot_count)
		{
			case 1:	//DG3 (1 dot point)
			{
				if(key.digitData[2]==0)	key.digitData[2]=Num_code[0];
				if(key.digitData[3]==0)	key.digitData[3]=Num_code[0];
				key.digitData[2]|=0x01;
				break;
			}
			case 2:	//DG2 (2 dot point)
			{
				if(key.digitData[1]==0)	key.digitData[1]=Num_code[0];
				if(key.digitData[2]==0)	key.digitData[2]=Num_code[0];
				if(key.digitData[3]==0)	key.digitData[3]=Num_code[0];
				key.digitData[1]|=0x01;
				break;
			}
			case 3://DG1 (3 dot point)
			{
				if(key.digitData[0]==0)	key.digitData[0]=Num_code[0];
				if(key.digitData[1]==0)	key.digitData[1]=Num_code[0];
				if(key.digitData[2]==0)	key.digitData[2]=Num_code[0];
				if(key.digitData[3]==0)	key.digitData[3]=Num_code[0];
				key.digitData[0]|=0x01;
				break;
			}
		}

	}


}

void DisplayDEC(unsigned short modbus_value, unsigned char decimal){
	SetNumberDisplay(1, modbus_value, 0, decimal, 4);
}


void DisplayHEX(unsigned short modbus_value){
	SetNumberDisplay(0, modbus_value, 1, 0, 4);	//Show all 4 digit
}


void DisplayCode(unsigned char dg4, unsigned char dg3, unsigned char dg2, unsigned char dg1){
	key.digitData[0] = dg4;
	key.digitData[1] = dg3;
	key.digitData[2] = dg2;
	key.digitData[3] = dg1;
}

void DisplaySigned(unsigned short modbus_value, unsigned char decimal){
	//Check last bit
	if(bitRead(modbus_value, 15) == 1){
		//Negative Value
		modbus_value = 65536 - modbus_value;
		SetNumberDisplay(1, modbus_value, 0, decimal, 4);
		//Check DG4 off?
		if(key.digitData[0]!=0){
			//Overflow?
			if((modbus_value/(10*decimal))>999){
				//Show '999'
				key.digitData[3] = Num_code[9];
				key.digitData[2] = Num_code[9];
				key.digitData[1] = Num_code[9];
			}else{
				//Shift all digit to right side
				key.digitData[3] = key.digitData[2] & 0b11111110; //Remove dot end
				key.digitData[2] = key.digitData[1];
				key.digitData[1] = key.digitData[0];
			}
		}
		//Set - to DG4
		key.digitData[0] = 0x10;
	}else{
		//Positive Value
		SetNumberDisplay(1, modbus_value, 0, decimal, 4);
	}
}

void SetParameterMBAddress(void){
	unsigned char i;
	key.parameter.detail.addr = 0;
	for(i=0; i<key.parameter.group; i++){
		key.parameter.detail.addr += PARA_LEN;
	}
	key.parameter.detail.addr += key.parameter.number;
}

/*
 * Parameter Number Display
 *
 * input:	Pointer of index_group and number
 *
 * Using Pointer variable for input variable,
 * will render Input variable update same pointer variable in function
 * EX. *number is limited = 5, Input variable that send to function will is limited = 5 also.
 *
 * */
void DisplayParameterName(signed char *index_group, signed char *number){
	unsigned char buff, max;

	//Limit group
	if(*index_group>GROUP_LEN-1){
		//MAX
		*index_group = GROUP_LEN-1;
	}else if(*index_group<0){
		//MIN
		*index_group = 0;
	}

	//Limit parameter number
	max = GROUP[*index_group].max;
	if(*number>max){
		//MAX
		*number = max;
	}else if(*number<0){
		//MIN
		*number = 0;
	}

	//Display
	buff = *number;
	//EX. F-00
	key.digitData[0] = GROUP[*index_group].name;
	key.digitData[1] = DG_;
	key.digitData[2] = Num_code[buff/10];	//Can't use Num_code[*number/10]
	key.digitData[3] = Num_code[buff%10];	//Can't use Num_code[*number%10] due to *number is updated since top line

	//Update MB Address
	SetParameterMBAddress();

}

void DisplayParameterValue(unsigned short *mb_value, unsigned char *decimal){
	//Check flag
	if(*decimal==16){
		//HEX display
		DisplayHEX(*mb_value);
	}else{
		//DEC display
		DisplayDEC(*mb_value, *decimal);
	}
}


void DisplayMain(void){

	//Allow?
	if(key.mode.bit.function || key.mode.bit.editing){ return; }
	//Clear LED main display
	key.led.byte &= MAIN_LED_OFF;
	//Keypad timeout?
	if(!key.mode.bit.ready){ DisplayCode(DG_, DG_t, DG_O, DG_); return; }
	//Fault?
	if(oSt.bit.trip){ return; }
	//Display
	switch(key.mode.bit.display){
		case D_SP: //SP
			key.led.bit.sp = 1U;
			//Block when editing set point
			if(key.mode.bit.set || key.mode.bit.editing || key.mode.bit.count_move){ return; }
			//Value
			if(oSt.bit.stop_run){
				DisplayDEC(key.main_v[D_RUN], conAddr.n_f_set);
			}else{
				DisplayDEC(key.main_v[D_SP], conAddr.n_f_set);
			}
			break;
		case D_Hz: //Hz
			key.led.bit.hz = 1U;
			DisplayDEC(key.main_v[D_Hz], conAddr.n_hz);
			break;
		case D_A: //Amp
			key.led.bit.a = 1U;
			DisplayDEC(key.main_v[D_A], conAddr.n_a);
			break;
		case D_D1: //Display 1
			//Signed?
			if(bitRead(key.main_v[D_D1], 15)){
				DisplaySigned(key.main_v[D_D1], conD1Addr.n[key.A00]);
			}else{
				DisplayDEC(key.main_v[D_D1], conD1Addr.n[key.A00]);
			}
			break;
	}
}

//UP: flag = 1, DOWN: flag = -1
void ChangeValue(signed char flag, unsigned short min, unsigned short max){
	unsigned char m = 1;
	//Set multiple
	if(key.mb_buffer>9999){ m = 10; }

	switch(key.mode.bit.count_move){
	case 1: //DG4
		key.mb_buffer += 1000*m*flag;
		break;
	case 2: //DG3
		key.mb_buffer += 100*m*flag;
		break;
	case 3: //DG2
		key.mb_buffer += 10*m*flag;
		break;
	default: //DG1
		key.mb_buffer += m*flag;
	}

	//Limiter
	if(key.mb_buffer>max){
		//MAX
		key.mb_buffer = max;
	}else if(key.mb_buffer<min){
		//MIN
		key.mb_buffer = min;
		//Clear
		key.mode.bit.count_move = 0;
	}


}


//UP: flag = 1, DOWN: flag = -1
void SetUpDown(signed char flag){
	//Check state
	switch(key.mode.bit.function){
	case 1: //Change parameter group
		switch(key.mode.bit.count_move){
			case 1: //Change group name
				key.parameter.group += flag;
				key.parameter.number = 0;
				break;
			case 3: //Change 10 step
				key.parameter.number += 10*flag;
				break;
			default: //Change 1 step
				key.parameter.number += flag;
		}
		//Display
		DisplayParameterName(&key.parameter.group, &key.parameter.number);
		break;
	case 2: //Change parameter value
		//Check read only bit
		if(!key.parameter.detail.rw){ return; }
		//Change value
		key.mb_buffer = key.parameter.detail.data;
		ChangeValue(flag, key.parameter.detail.min, key.parameter.detail.max);
		key.parameter.detail.data = (unsigned short)key.mb_buffer;
		//Set Size Address?
		if(key.parameter.detail.addr==SET_SIZE_ADDR)
		{
			//Read Size Inverter Name
			Set_MB_Special(Fn_ReadSizeInvName, key.parameter.detail.data);
			mbCon.interval_time = 0;
		}
		else
		{
			//Show Parameter Value
			DisplayParameterValue(&key.parameter.detail.data, &key.parameter.detail.dotFormat);
		}
		break;
	default: //Change Set Point
		//Allow?
		if(oSt.bit.trip || key.mode.bit.display || !key.mode.bit.ready || !f_setDetail.rw){ return; }
		//Set flag and clear timer
		key.mode.bit.editing = oSt.bit.stop_run;
		key.time_up[T_SET] = 0;
		key.time_up[T_MOVE] = 0;
		//Change value
		key.mb_buffer = key.main_v[D_SP];
		ChangeValue(flag, f_setDetail.min, f_setDetail.max);
		key.main_v[D_SP] = (unsigned short)key.mb_buffer;
		DisplayDEC(key.main_v[D_SP], f_setDetail.dotFormat);
		//Write to MB
		WriteSetPoint();
	}
}


//Note!!! size of *from must = size of *to
void CopyU8Array(unsigned char *from, unsigned char *to, unsigned short len){
	while(len--){
		to[len] = from[len];
	}
}

unsigned char GetIndexStart(MB_DETAIL_ADDR *d){
	unsigned char index = 0;
	if(d->dotFormat < 16){
		unsigned char pDg[sizeof(key.digitData)];
		unsigned char i;
		CopyU8Array(key.digitData, pDg, sizeof(key.digitData));
		DisplayDEC(d->max, d->dotFormat);
		for(i=0; i<4; i++){
			if(key.digitData[i]>0 && ((key.digitData[i]&DG_O)!=DG_O)){
				index = i; break;
			}
		}
		//Back to display
		CopyU8Array(pDg, key.digitData, sizeof(key.digitData));
	}
	return index;
}

void SwitchCallback(unsigned char sw, BOOLEAN isDoubleClicked){
	//Clear
	if(isDoubleClicked){
		key.double_sw = 0;
		key.count_double_sw = 0;
	}
	//Check SW
	switch(sw){
	case FUNC_BT:
		//Clear
		key.mode.bit.count_move = 0;
		key.mode.bit.editing = 0;
		key.mode.bit.set = 0;
		key.mode.bit.wait_unlock = 0;
		key.mode.bit.write_mb = 0;
		key.mode.bit.read_mb = 0;
		//Check state
		if(!key.mode.bit.function){
			//Setting Mode
			key.mode.bit.function = 1U; //Set flag
			key.led.bit.func = 1U;	//Show 'FUNC' LED
			key.led.byte &= MAIN_LED_OFF;
			//Display first parameter
			key.parameter.group = 0;
			key.parameter.number = 0;
			DisplayParameterName(&key.parameter.group, &key.parameter.number);
		}else{
			//Clear all
			key.led.bit.func = 0;
			key.mode.bit.function = 0;
			//Back to Operating mode
			//Re-read Set Point detail
			Set_MB_Special(Fn_ReadDetailAddr, SET_POINT_ADDR);
			//Main
			DisplayMain();
		}
		break;
	case MOVE_BT:
		//Operating Mode?
		if(!key.mode.bit.function){
			//Allow?
			if(oSt.bit.trip || key.mode.bit.display || !key.mode.bit.ready || !f_setDetail.rw){ return; }
			//Show Set Point in short time in running mode
			if(oSt.bit.stop_run){
				//Display
				DisplayDEC(key.main_v[D_SP], conAddr.n_f_set);
				//Set flag for display Set Point
				key.mode.bit.editing = 1;
				key.time_up[T_SET] = 0;
			}
			//Initial 'MOVE' index
			if(!key.mode.bit.count_move){ key.mode.bit.count_move = GetIndexStart(&f_setDetail); }
		}else if(key.mode.bit.function==2){
			//Allow?
			if(!key.parameter.detail.rw){ return; }
			//Initial 'MOVE' index
			if(!key.mode.bit.count_move){ key.mode.bit.count_move = GetIndexStart(&key.parameter.detail); }
		}

		//Clear all
		key.digit_blink.byte = 0;
		key.time_up[T_MOVE] = 0;
		//Shift
		key.mode.bit.count_move++;
		//Digit off?
		if(key.mode.bit.count_move>0 && key.mode.bit.count_move<4
				&& key.digitData[key.mode.bit.count_move-1] == 0){ key.digitData[key.mode.bit.count_move-1] = DG_O; }
		//Set digit blink
		switch(key.mode.bit.count_move){
			case 1:
				key.digit_blink.bit.dg4 = 1;
				//if(key.digitData[0] == 0){ key.digitData[0] = DG_O; }
				break;
			case 2:
				if(key.mode.bit.function==1){
					//Shift '-' on parameter name
					key.digit_blink.bit.dg2 = 1;
					key.mode.bit.count_move = 3;
				}else{
					key.digit_blink.bit.dg3 = 1;
					//if(key.digitData[1] == 0){ key.digitData[1] = DG_O; }
				}
				break;
			case 3:
				key.digit_blink.bit.dg2 = 1;
				//if(key.digitData[2] == 0){ key.digitData[2] = DG_O; }
				break;
			case 4:
				key.digit_blink.bit.dg1 = 1;
				//if(key.digitData[3] == 0){ key.digitData[3] = DG_O; }
				break;
			default:
				key.mode.bit.count_move = 0;
		}
		break;
	case UP_BT:
		SetUpDown(1);
		break;
	case DOWN_BT:
		SetUpDown(-1);
		break;
	case ENTER_BT:
		//Clear
		key.mode.bit.count_move = 0;
		key.mode.bit.read_mb = 0;
		//Check state
		switch(key.mode.bit.function){
		case 1:
			//Read Current Parameter Value
			key.mode.bit.read_mb = 1U; //Set flag
			Set_MB_Special(Fn_ReadDetailAddr, key.parameter.detail.addr);
			break;
		case 2:
			//Display last parameter
			DisplayParameterName(&key.parameter.group, &key.parameter.number);
			//Set state
			key.mode.bit.function = 1;
			break;
		default:
			//Allow?
			if(oSt.bit.trip || !key.mode.bit.ready){ return; }
			//Select display on operating mode
			key.mode.bit.display++;
			if(key.mode.bit.display>3){ key.mode.bit.display = 0; }
			//Display
			DisplayMain();
		}
		break;
	case SET_BT:
		//Clear
		key.mode.bit.count_move = 0;
		key.time_up[T_SET] = 0;
		key.time_up[T_BLINK] = 0;
		//Check state
		if(key.mode.bit.function==2){
			//Read only?
			if(!key.parameter.detail.rw){ return; }
			//Write to MB
			Set_MB_WriteSingle(key.parameter.detail.addr, key.parameter.detail.data); //Action
			key.mode.bit.write_mb = 1U; //Set flag
		}else if(!key.mode.bit.function && !key.mode.bit.display && !oSt.bit.trip){
			//Save to ROM
			rom.data[RI_SP] = key.main_v[D_SP];
			//rom.exe_flag = ROM_Write;
			DF_Write();
			//Write to MB
			WriteSetPoint();
			key.mode.bit.write_mb = 1U; //Set flag
		}
		break;
	case RUN_BT:
		//Allow?
		if(key.mode.bit.function || oSt.bit.trip || !key.mode.bit.ready){ return; }
		//RUN
		WriteControlCommand(1);
		break;
	case STR_BT:
		//Allow?
		if(key.mode.bit.function || !key.mode.bit.ready){ return; }
		//Set Command
		oCom.word = 0;
		oCom.bit.stop_run = 0;
		oCom.bit.emergency = isDoubleClicked;
		oCom.bit.reset = oSt.bit.trip;
		//Write to MB
		WriteControlCommand(oCom.word);
		break;
	}
}

void ClearLastPassword(void){
	unsigned char i;
	for(i=0; i<PASS_LEN; i++){
		lastPass[i] = 0;	//Clear last password
	}
	pass_count = 0;			//Clear counter
}



/*
 * Check User press password
 *
 * */
void CheckPassword(unsigned char sw){
	char x, match;

	//Back to Operation Mode
	if(sw==FUNC_BT){
		SwitchCallback(sw, FALSE);
		return;
	}

	//Start new counting, if user press "MOVE"
	if(sw==MOVE_BT){ ClearLastPassword(); }

	//Save pass
	lastPass[pass_count] = sw;

	//Shift index
	pass_count++;
	if(pass_count==PASS_LEN){ pass_count = 0; }

	//Check password
	match = 1;
	for(x=0; x<PASS_LEN; x++){
		if(lastPass[x]!=password[x]){
			match = 0;
			break;
		}
	}

	//Password correct!
	if(match){
		//Set flag but auto clear at SwitchCallback(ENTER_BT)
		key.mode.bit.unlock = 1;
		//Unlock
		SwitchCallback(ENTER_BT, FALSE);
	}

}


void HoldUpDownActive(unsigned char sw){
	//Active 0 -> 250 in 25s
	key.time_up[T_HOLD]++;
	if(key.time_up[T_HOLD]>10){
		//Clear
		key.time_up[T_HOLD] = 0;
		//Callback
		SwitchCallback(sw, FALSE);
	}
}

void BlockHoldSwitch(unsigned char sw){
	//Clear double-click
	if(sw!=key.double_sw || (ms_counter-key.last_time)>500){
		key.double_sw = 0;
		key.count_double_sw = 0;
		//key.led.bit.hz = 1U;
	}
	//Lock SW
	if(ms_counter-key.last_time>100){
		//Lock?
		if(key.mode.bit.wait_unlock){
			//Check Password
			CheckPassword(sw);
			//key.led.bit.run = 1;
		}else{
			//Count double-click
			if(sw == key.double_sw){ key.count_double_sw++; }
			//Callback
			SwitchCallback(sw, (BOOLEAN)key.count_double_sw);
			//DisplayDEC(key.count_double_sw, 0);
		}
	}
	//Save
	key.last_time = ms_counter;
	key.double_sw = sw;

}

void SwitchModeSelector(unsigned char sw){

	//Filter
	if(key.last_sw == sw){
		key.sw_stack++;

		//Make sure user press this SW
		if(key.sw_stack>2){

			/*
			//Check mode
			if(key.mode.bit.function>0){
				//Check button
				if((sw==UP_BT || sw==DOWN_BT) && !key.mode.bit.wait_unlock){
					HoldUpDownActive(sw);
				}else{
					BlockHoldSwitch(sw);
				}
			}else{
				BlockHoldSwitch(sw);
			}
			*/

			if(sw==UP_BT || sw==DOWN_BT){
				HoldUpDownActive(sw);
			}else{
				BlockHoldSwitch(sw);
			}

			//Clear stack
			key.sw_stack = 0;
		}

	}else{
		key.sw_stack = 0;
	}

	//Save SW
	key.last_sw = sw;

}

void RunKeypad(void){

	//========== Digit Display =================//
	//Check writing success flag
	if(key.mode.bit.set){
		//Hold 'SEt' with no blink
		key.digit_blink.byte = 0;
		DisplayCode(0, DG_S, DG_E, DG_t);
		key.time_up[T_SET]++;
		if(key.time_up[T_SET]>500){
			key.mode.bit.set = 0;
			key.time_up[T_SET] = 0;
			key.time_up[T_BLINK] = 0;
			//Check state
			if(key.mode.bit.function){
				//Display last parameter
				DisplayParameterName(&key.parameter.group, &key.parameter.number);
				//Set state
				key.mode.bit.function = 1;
			}else{
				//Main
				DisplayMain();
			}
		}
	}else if(key.mode.bit.count_move){
		//Press 'MOVE' BTN?
		key.time_up[T_MOVE]++;
		if(key.time_up[T_MOVE]>10000){
			//Blink timeout
			key.time_up[T_MOVE] = 0;
			key.mode.bit.count_move = 0;
			//Check mode
			if(key.mode.bit.function || oSt.bit.stop_run){
				key.digit_blink.byte = 0;
			}else if(!oSt.bit.stop_run){
				key.digit_blink.byte = ALL_DIGIT_BLINK;
			}
		}
	}else if(!key.mode.bit.function && !oSt.bit.stop_run){
		//All blink
		key.digit_blink.byte = ALL_DIGIT_BLINK;
	}else{
		//No blink
		key.digit_blink.byte = 0;
	}

	//Hold display set point when 'RUN' mode
	if(!key.mode.bit.function && oSt.bit.stop_run && key.mode.bit.editing){
		//Count time
		key.time_up[T_SET]++;
		if(key.time_up[T_SET]>1000){
			//Clear
			key.mode.bit.editing = 0;
			key.mode.bit.count_move = 0;
			key.time_up[T_SET] = 0;
			key.time_up[T_BLINK] = 0;
			//Main display
			DisplayMain();
		}
	}

	//Update Display
	RunDisplay();


	//========== Detect SW ============//
	//Should call the finally in this function due to want to the current digit on
	if(key.current_digit_on<4){

		//Switch Active
		if(SW_LINE1<1 && SW_LINE2<1){
			//Press 2 SW in same time
		}else if(SW_LINE1<1){
			SwitchModeSelector(SW[0][key.current_digit_on]);
		}else if(SW_LINE2<1){
			SwitchModeSelector(SW[1][key.current_digit_on]);
		}

	}


}

unsigned char GetMaxValueOfArray(unsigned char *arr, unsigned short offset, unsigned short len){
	unsigned char max = 0;
	while(len--){
		if(len<offset){ break; }
		if(max<arr[len]){
			max = arr[len];
		}
	}
	return max;
}

void ModbusResponse(MB_RESPONE *mb, MB_SPECIAL *mbSpec){
	//Check Result
	if(mb->error==ERR_None)
	{
		unsigned char i, j, k;
		//Clear
		key.mb_timeout = 0;
		//EXE with Function
		switch(mb->fn)
		{
		case Fn_ReadMemorySize:	//key.led.bit.func = 1U;
			memSize = mbSpec->memory;
			//Setting group
			if(memSize.rom_len){
				k = memSize.rom_len-1;
				i = k/PARA_LEN;
				j = k%PARA_LEN;
				if(!i){ i = 1; }

				GROUP_LEN = i + (j>0);
				//Out of group rang?
				if(GROUP_LEN>G_MAX){
					GROUP_LEN = G_MAX;
					j = PARA_LEN;
				}
				//Set Group name and max
				for(i=0; i<GROUP_LEN; i++){
					GROUP[i].name = GROUP_NAME[i];
					if(i==GROUP_LEN-1){
						GROUP[i].max = j;
					}else{
						GROUP[i].max = PARA_LEN;
					}
				}
			}
			//Next
			Set_MB_Special(Fn_ReadConfigAddr, 0);
			break;
		case Fn_ReadConfigAddr: //key.led.bit.run = 1U;
			conAddr = mbSpec->config;
			Set_MB_Special(Fn_ReadConfigD1Addr, 0);
			break;
		case Fn_ReadConfigD1Addr: //key.led.bit.stop = 1U;
			conD1Addr = mbSpec->configD1;
			//Set MB for Interval Reading
			mbG.group[0].start_address = memSize.ram_start;	//Main
			mbG.group[0].length = GetMaxValueOfArray((unsigned char*)&conAddr, 2, sizeof(conAddr))+1;
			mbG.group[1].start_address = A00_ADDR;	//A-00
			mbG.group[1].length = 1;
			mbG.total = 2;
			mbG.counter = 0;
			//Read Set Point detail
			Set_MB_Special(Fn_ReadDetailAddr, SET_POINT_ADDR);
			break;
		case Fn_ReadDetailAddr:
			//Setting mode?
			if(!key.mode.bit.function)
			{
				//Save 'F_set' detail
				if(mbSpec->detail.addr == SET_POINT_ADDR){
					f_setDetail = mbSpec->detail;
					//Initial Set point
					if(rom.exe_flag == ROM_Factory || rom.data[RI_SP]>f_setDetail.max || rom.data[RI_SP]<f_setDetail.min){
						//Use Default Value
						key.main_v[D_SP] = f_setDetail.def;
						//Save ROM
						rom.data[RI_SP] = key.main_v[D_SP];
						DF_Write();
					}else{
						//Use value from ROM
						key.main_v[D_SP] = rom.data[RI_SP];
					}
					//Write Set point to MB
					WriteSetPoint();
				}
			}
			else
			{
				if(mbSpec->detail.addr == key.parameter.detail.addr){
					//Save parameter detail
					key.parameter.detail = mbSpec->detail;
					//Special Address?
					if(key.parameter.detail.addr==SET_SIZE_ADDR)
					{
						//Read size inverter name
						Set_MB_Special(Fn_ReadSizeInvName, key.parameter.detail.data);
						mbCon.interval_time = 0;
					}
					else if(key.parameter.detail.addr==VERSION_ADDR)
					{
						//Read Firmware name
						Set_MB_Special(Fn_ReadFirmwareName, 0);
						mbCon.interval_time = 0;
					}
					else
					{
						//Show Parameter Value
						DisplayParameterValue(&key.parameter.detail.data, &key.parameter.detail.dotFormat);
						//Set flag
						key.mode.bit.function = 2;
						//Back to Interval reading
						mbCon.next_fn = Fn_ReadHolding;
					}
				}
			}
			break;
		case Fn_ReadHolding: //key.led.bit.hz = 1U;
			//Ready
			key.mode.bit.ready = 1;
			//Set MB interval reading for display 1
			if(mb->request == A00_ADDR){
				key.A00 = mb->response[0];
				mbG.group[1].start_address = DISPLAY1_ADDR;
				mbG.group[1].length = 1;
			}
			//Interval Checking
			if(mb->request == memSize.ram_start)
			{
				//Status
				oSt.word = mb->response[conAddr.operation];
				//Fault
				if(mb->response[conAddr.fault]){
					//Read Fault code
					Set_MB_Special(Fn_ReadFaultName, mb->response[conAddr.fault]);
					oSt.bit.trip = 1;
				}
				else{
					oSt.bit.trip = 0;	//Clear
					mbCon.next_fn = Fn_ReadHolding;
				}
				//Update Main Display
//				if(!key.mode.bit.editing && !key.mode.bit.count_move){
//					key.main_v[D_SP] = mb->response[conAddr.f_set]; 	//Set point
//				}
				//key.main_v[D_SP] = mb->response[conAddr.f_set];
				key.main_v[D_RUN] = mb->response[conAddr.f_run]; //Running
				key.main_v[D_Hz] = mb->response[conAddr.hz];	 //Hz
				key.main_v[D_A] = mb->response[conAddr.a];		 //A
				//LED status
				key.led.bit.run = oSt.bit.stop_run;
				key.led.bit.stop = !oSt.bit.stop_run;
			}
			if(mb->request == mbG.group[1].start_address)
			{
				//if(!key.mode.bit.ready){ key.mode.bit.ready = 1; }
				key.main_v[D_D1] = mb->response[0];	//Display 1 (All LED Off)
			}
			//Display Main
			DisplayMain();
			//Next group
			mbG.counter++;
			mbCon.interval_time = 10;	//Delay polls
			if(mbG.counter>mbG.total-1){
				mbG.counter = 0;	//Clear group counter
				if(oSt.bit.stop_run && !key.mode.bit.function){
					//High interval reading only 'RUN' mode and 'Operating Mode'
					mbCon.interval_time = INTERVAL_RUN;
				}else{
					mbCon.interval_time = INTERVAL_STOP;
				}
			}
			break;
		case Fn_ReadFaultName:
			//Display fault only Operating Mode
			if(!key.mode.bit.function){
				DisplayCode(mbSpec->segment.dg3, mbSpec->segment.dg2, mbSpec->segment.dg1, mbSpec->segment.dg0);	//Code
			}
			//Back to Reading
			mbCon.next_fn = Fn_ReadHolding;
			break;
		case Fn_ReadSizeInvName:
		case Fn_ReadFirmwareName:
			if(key.mode.bit.function){
				DisplayCode(mbSpec->segment.dg3, mbSpec->segment.dg2, mbSpec->segment.dg1, mbSpec->segment.dg0);
				key.mode.bit.function = 2;
			}
			//Back to Reading
			mbCon.next_fn = Fn_ReadHolding;
			break;
		case Fn_WriteSingle:
			//Check flag
			if(key.mode.bit.write_mb)
			{
				//Set flag
				key.mode.bit.write_mb = 0;	//blocking update again
				key.mode.bit.set = 1U; 		//Set flag for show SEt
			}

			//Update Display1 Address
			if(mb->response[0]==A00_ADDR){
				key.A00 = mb->response[1];
				mbG.group[1].start_address = DISPLAY1_ADDR;
			}

			//Verify 'set point' value
			if(mb->response[0]==SET_POINT_ADDR)
			{
				key.main_v[D_SP] = mb->response[1];
			}

			//Emergency Stop?
//			if(mb->response[0]==COMM_ADDR)
//			{
//				if(mb->)
//			}

			//Back to Reading
			mbCon.next_fn = Fn_ReadHolding;
			break;
		}
	}
	else if(mb->error==ERR_ResponseTimeout)
	{
		//Count Timeout to display -tO-
		key.mb_timeout++;
		if(key.mb_timeout>5){
			key.mb_timeout = 0;
			//Block 'MOVE' BTN
			key.mode.bit.ready = 0;
			//Display -tO-
			DisplayMain();
			//
			mbCon.interval_time = INTERVAL_STOP;
		}
	}
	else
	{

//		unsigned short d = (unsigned short)mb->fn<<8 | mb->error;	//FFEE by FF = Function Code, EE = Error Code
//		DisplayHEX(d);
//		mbCon.interval_time = INTERVAL_STOP;

		//Back to Interval Reading
		mbCon.interval_time = INTERVAL_STOP;
		if(mbCon.next_fn == Fn_WriteSingle && key.mode.bit.ready){
			mbCon.next_fn = Fn_ReadHolding;
		}

	}

	//Back to Interval Reading
	if(mbCon.next_fn == Fn_ReadHolding)
	{
		Set_MB_ReadHolding(mbG.group[mbG.counter].start_address, mbG.group[mbG.counter].length);
	}

}

