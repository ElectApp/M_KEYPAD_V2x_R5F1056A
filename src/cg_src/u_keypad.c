/*
 * u_keypad.c
 *
 *  Created on: 10 มี.ค. 2563
 *      Author: Elect
 */

//=========== Library =========//
#include "r_cg_macrodriver.h"					///< PORT
#include "r_cg_sau.h"
#include "u_modbus2.h"
#include "u_keypad.h"

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

#define FUNC_BT			1
#define MOVE_BT			2
#define UP_BT			3
#define STR_BT			4
#define DOWN_BT			5
#define SET_BT			6
#define ENTER_BT		7
#define RUN_BT			8

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
const uint8_t SOFT_VER = 100;	//Software Version

KEYPAD_DATA key;		//Keypad
O_STATUS oSt;			//Storage Driver Status
O_COMMAND oCom;			//Storage Driver Command

//Modbus interval reading
//Interval Time
const unsigned short INTERVAL_STOP = 1000;
const unsigned short INTERVAL_RUN = 50;


//Index on Main Display
typedef enum{
	D_SP = 0,
	D_Hz,
	D_A,
	D_OFF
}MAIN_DISPLAY_INDEX;

unsigned short main_v[5];
unsigned short fault_v, set_point, para_v;

//MODBUS
MB_CONFIG mbCon;
MB_MEMORY_SIZE memory;

//Other
extern unsigned short ms_counter;
unsigned char test = 0, index = 0;


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

    //Initial and start MB
    mbCon.slave_id = 1;
    mbCon.interval_time = 0;
    MB_Init(&mbCon, &ModbusResponse);
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

void DisplaySingleDigit(unsigned char index, unsigned char value){
	//Clear All
	ShiftOutAndLatch(0);
	DG1(0); DG2(0); DG3(0); DG4(0); LED(0);	//Clear Bias
	//On Bias
	switch(index){
		case 4: LED(1); break;	//Same digit
		case 3: DG1(1); break;
		case 2: DG2(1); break;
		case 1: DG3(1); break;
		case 0: DG4(1); break;
	}
	ShiftOutAndLatch(value); 		//Latch and display
}


void LoopDisplay2(unsigned char state_off){
	//Count index
	key.current_digit_on++;
	if(key.current_digit_on>4){ key.current_digit_on = 0; }

	//Display
	if(key.current_digit_on==4){
		DisplaySingleDigit(key.current_digit_on, key.led.byte);
	}else{
		//Check DG off: 0-3
		if(state_off && bitRead(key.digit_blink.byte, key.current_digit_on)){
			DisplaySingleDigit(key.current_digit_on, 0);
		}else{
			DisplaySingleDigit(key.current_digit_on, key.digitData[key.current_digit_on]);
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
void SetNumberDisplay(unsigned char Hex_Dec, float Data, unsigned char Full_display, unsigned char Dot_count, unsigned char Digit_total)
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
	unsigned short Data_buff	=	(unsigned short)Data;

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



}

void DisplayParameterValue(unsigned short mb_value, unsigned char decimal){
	//Check flag
	if(decimal==16){
		//HEX display
		DisplayHEX(mb_value);
	}else{
		//DEC display
		DisplayDEC(mb_value, decimal);
	}
}

//uint16_t getConfigResponseValue(CONFIG_ADDR_INDEX index){
//	return mb.response_buff[mb.config_addr[index]];
//}


void DisplayMain(void){
	//para_t dmT;

	//Allow?
	if(key.mode.bit.function || key.mode.bit.editing){ return; }
	//Keypad timeout?
	if(!key.mode.bit.ready){
		DisplayCode(DG_, DG_t, DG_O, DG_);
		return;
	}
	//Clear LED main display
	key.led.byte &= MAIN_LED_OFF;
	//Fault?
	if(oSt.bit.trip){
		return;
	}
	//Display
	switch(key.mode.bit.display){
		case D_SP: //Set point
			//Block when editing set point
			if(key.mode.bit.set || key.mode.bit.editing || key.mode.bit.count_move){ return; }
			//Value
			//DisplayDEC(getConfigResponseValue(CAI_F_Set), getConfigResponseValue(CAI_N_F_Set));
			break;
		case D_Hz: //Hz
			key.led.bit.hz = 1U;
			//Block when editing set point
			if(key.mode.bit.set || key.mode.bit.editing || key.mode.bit.count_move){ return; }
			//Value
			//DisplayDEC(getConfigResponseValue(CAI_Hz), getConfigResponseValue(CAI_N_Hz));
			/*
			if(oSt.bit.stop_run){
				//DisplayDEC(main_v[D_HZ], 2);	//Running
				DisplayDEC(getConfigResponseValue(CAI_Hz), getConfigResponseValue(CAI_N_Hz));
			}else{
				//DisplayDEC(set_point, 2);	//Stop
				DisplayDEC(getConfigResponseValue(CAI_F_Set), getConfigResponseValue(CAI_N_F_Set));
			}
			*/
			break;
		case D_A: //Amp
			key.led.bit.a = 1U;
			DisplayDEC(main_v[D_A], 1);
			break;
		case D_OFF: //Display 1
			key.led.bit.sp = 1U;
			/*
			dmT = PARA_F00[rom.data[F00]-1];
			//Signed?
			if(bitRead(dmT.edit_display, SIGNED_BIT)){
				DisplaySigned(main_v[D_F00], dmT.decimal);
			}else{
				DisplayDEC(main_v[D_F00], dmT.decimal);
			}*/
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

void SwitchCallback(unsigned char sw){
	unsigned short d[] = {5, 6, 7};
	switch(sw){
	case FUNC_BT: break;
	case MOVE_BT: break;
	case UP_BT:
		break;
	case DOWN_BT:
		break;
	case ENTER_BT:
		test++;
		switch(test){
		case 1:
			Set_MB_WriteSingle(0, 1);
			test = 2;
			break;
		case 2:
			Set_MB_WriteMultiple(10, d, 3);
			break;
		case 3:
			Set_MB_Special(Fn_ReadMemorySize, SOFT_VER);
			break;
		case 4:
			Set_MB_Special(Fn_ReadSizeInvName, 0);
			break;
		case 5:
			Set_MB_Special(Fn_ReadFaultName, 1);
			break;
		case 6:
			Set_MB_Special(Fn_ReadFirmwareName, 0);
			break;
		case 7:
			Set_MB_Special(Fn_ReadConfigAddr, 0);
			break;
		case 8:
			Set_MB_Special(Fn_ReadConfigD1Addr, 0);
			break;
		case 9:
			Set_MB_Special(Fn_ReadDetailAddr, 0);
			break;
		default:
			test = 0;
			Set_MB_ReadHolding(0, 1);
		}
		break;
	case SET_BT: break;
	case RUN_BT: break;
	case STR_BT: break;
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
		SwitchCallback(sw);
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
		SwitchCallback(ENTER_BT);
	}

}



void HoldUpDownActive(unsigned char sw){
	//Active 0 -> 250 in 25s
	key.time_up[T_HOLD]++;
	if(key.time_up[T_HOLD]>10){
		//Clear
		key.time_up[T_HOLD] = 0;
		//Callback
		SwitchCallback(sw);
	}
}

void BlockHoldSwitch(unsigned char sw){
	//Lock SW
	if(ms_counter-key.last_time>100){
		if(key.mode.bit.wait_unlock){
			//Check Password
			CheckPassword(sw);
			//key.led.bit.run = 1;
		}else{
			//Callback
			SwitchCallback(sw);
		}
	}
	//Save time
	key.last_time = ms_counter;

}

void SwitchModeSelector(unsigned char sw){

	//Filter
	if(key.last_sw == sw){
		key.sw_stack++;

		//Make sure user press this SW
		if(key.sw_stack>2){

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

void ModbusResponse(MB_RESPONE *mb, MB_SPECIAL *mbSpec){
	//unsigned short d = (unsigned short)mb->fn<<8 | mb->error;	//FFEE by FF = Function Code, EE = Error Code
	//DisplayHEX(d);
	//DisplayHEX(mb.error);
	//DisplayDEC(mb.response[0], 0);
	//DisplayDEC(mbSpec->memory.ram_start, 0);

    switch (mb->fn)
    {
    case Fn_ReadHolding:
    	DisplayDEC(mb->response[0], 0);
        break;
    case Fn_WriteSingle:
    	DisplayDEC(mb->response[1], 0);
        break;
    case Fn_ReadMemorySize:
    	memory = mbSpec->memory;
    	DisplayDEC(memory.ram_start, 0);
    	break;
	case Fn_ReadSizeInvName:
	case Fn_ReadFaultName:
	case Fn_ReadFirmwareName:
		DisplayCode(mbSpec->segment.dg3, mbSpec->segment.dg2, mbSpec->segment.dg1, mbSpec->segment.dg0);
		break;
	case Fn_ReadConfigAddr:
		DisplayDEC(mbSpec->config.f_run, 0);
		break;
	case Fn_ReadConfigD1Addr:
		DisplayDEC(mbSpec->configD1.d[0], mbSpec->configD1.n[0]);
		break;
	case Fn_ReadDetailAddr:
		DisplayDEC(mbSpec->detail.max, mbSpec->detail.dotFormat);
		break;
    }
	mbCon.interval_time = 1000;

}

