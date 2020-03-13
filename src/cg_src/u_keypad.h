/*
 * u_keypad.h
 *
 *  Created on: 10 มี.ค. 2563
 *      Author: Elect
 */

#ifndef CG_SRC_U_KEYPAD_H_
#define CG_SRC_U_KEYPAD_H_

//=============== Segment Value ================//
#define	DG_A		0xF6		//	A
#define DG_b		0x3E		//	b
#define	DG_C		0x6C		//	C
#define	DG_d		0x9E		//	d
#define	DG_E		0x7C		//	E
#define DG_F		0x74		//	F
#define DG_H		0xB6		//	H
#define	DG_t		0x3C		//	t
#define DG_O		0xEE		//	O, 0
#define DG_S		0x7A		//	S
#define DG_P		0xF4		//	P
#define DG_U		0xAE		//	U
#define DG_Y		0xBA		//	Y
#define DG_n		0x16		//	n
#define DG_L		0x2C		//	L
#define DG_o		0x1E		//	o
#define DG_c		0x1C		//	c
#define DG_r		0x14		//	r
#define DG_i		0x02		//	i
#define DG_1		0x82		//	1
#define DG_2		0xDC		//	2
#define DG_3		0xDA		//	3
#define DG_4		0xB2		//	4
#define DG_			0x10		//	-

typedef union {
	unsigned char byte;
	struct {
		unsigned char run 		:1;
		unsigned char func		:1;
		unsigned char stop		:1;
		unsigned char a			:1;
		unsigned char sp		:1;
		unsigned char hz		:1;
		unsigned char reserve	:2;		//Not use
	} bit;

}LED_DATA;

typedef union {
	unsigned char byte;
	struct {
		unsigned char dg4 				:1;
		unsigned char dg3				:1;
		unsigned char dg2				:1;
		unsigned char dg1				:1;
		unsigned char reserve			:4;		//Not use
	} bit;

}DIGIT_BLINK;

typedef union {
	unsigned char byte;
	struct {
		unsigned char FUNC 		:1;			//Bit0: SW1
		unsigned char MOVE		:1;			//Bit1: SW2
		unsigned char UP		:1;			//Bit2: SW3
		unsigned char STOP_RST	:1;			//Bit3: SW4
		unsigned char DOWN		:1;			//Bit4: SW5
		unsigned char SET		:1;			//Bit5: SW6
		unsigned char ENTER		:1;			//Bit6: SW7
		unsigned char RUN		:1;			//Bit7: SW8
	} bit;
}SW_DATA;

typedef union {
	unsigned short word;
	struct {
		unsigned short display		:2;			//Bit0-1: 0=Hz, 1=Amp, 2=F-00, 3=Set point
		//Bit2-3: 0=Operation mode, >0=Function Mode
		//(1=Read parameter name, 2=Read value success)
		unsigned short function 	:2;			//NOTE! 2bit => DEC value < 4
		unsigned short read_mb		:1;			//Bit4:	1=Request read MB
		unsigned short write_mb		:1;			//Bit5: 1=Request write MB
		unsigned short unlock		:1;			//Bit6: 1=unlock
		unsigned short wait_unlock	:1;			//Bit7: 1=Request password
		unsigned short set			:1;			//Bit8: 0=Set point, 1=SEt
		unsigned short editing		:1;			//Bit9: 1=Editing set point
		unsigned short count_move	:3;			//Bit10-12:
		unsigned short ready		:1;			//Bit13: 1=Ready response when press
		unsigned short display_offset		:2;			//Bit14-15: Offset index on display
	} bit;

}HELD_FLAG;

//Index match time_up[]
typedef enum{
	T_BLINK = 0,
	T_SET,
	T_MOVE,
	T_HOLD,
	T_MAX
}TIME_UP_INDEX;


//Index match main_v[]
typedef enum{
	D_SP = 0,
	D_D1,
	D_Hz,
	D_A,
	D_RUN,
	D_MAX
}MAIN_DISPLAY_INDEX;

typedef struct{

	unsigned char digitData[4];			///< Current digit data on display, 0=DG4, 1=DG3, 2=DG2, 3=DG1 (Left to Right)
	LED_DATA led;						///< LED on keypad
	unsigned char current_digit_on;		///< Save current the digit that turn on, 4 = LED
	unsigned short time_up[T_MAX];		///< 0 = Blink, 1 = SEt, 2 = Move timeout, 3 = Hold press time
	unsigned char last_sw;				///< Save last switch active
	unsigned short sw_stack;			///< Stack Same SW
	unsigned short last_time;			///< Save last time that switch active
	HELD_FLAG mode;
	struct {
		MB_DETAIL_ADDR detail;
		signed char group;				///< Save current parameter group
		signed char number;				///< Save current parameter number
	}parameter;
	DIGIT_BLINK digit_blink;			///< 0=Normal, 1=Blink (Rang 0-3)
	LED_DATA led_blink;					///< 0=Normal, 1=Blink
	signed long mb_buffer;				///< Buffer for write to MB
	unsigned short main_v[D_MAX];		///< Save Main value: SP, Hz, A, D1
	unsigned char mb_timeout;			///< Count MB timeout to display -tO-
	unsigned char A00;					///< Save value of A00
} KEYPAD_DATA;

//Control Command (1409)
typedef union{
	unsigned short word;
	struct {
		unsigned short stop_run 	:1;	//Bit0: 0=Stop, 1=Run
		unsigned short emergency	:1;	//Bit1: 0=Disable, 1=Enable
		unsigned short reset		:1;	//Bit2: "-------"
		unsigned short preheat		:1;	//Bit3: 0=Stop, 1=Run
		unsigned short rsv1			:4;	//Bit4-7
		unsigned short aux1			:1; //Bit8: 0=Disable, 1=Enable
		unsigned short aux2			:1; //Bit9: "-------"
		unsigned short way_valve	:1; //Bit10: "------"
		unsigned short rsv2			:5; //Bit11-15
	} bit;
}O_COMMAND;

//Operating Status (1415)
typedef union{
	unsigned short word;
	struct {
		unsigned short stop_run 	:1;	//Bit0: 0=Stop, 1=Run
		unsigned short trip			:1;	//Bit1
		unsigned short steady		:1;	//Bit2
		unsigned short accel		:1;	//Bit3
		unsigned short decel		:1;	//Bit4
		unsigned short wait			:1; //Bit5
		unsigned short rsv1			:1; //Bit6
		unsigned short spd			:1;	//Bit7
		unsigned short rsv2			:4;	//Bit8-11
		unsigned short oil_return	:1; //Bit12
		unsigned short decel_stall	:1; //Bit13
		unsigned short comp_warning	:1; //Bit14
		unsigned short pm_warning	:1; //Bit15
	} bit;
}O_STATUS;

typedef struct{
	unsigned char name;			//Parameter group name on 7-Segment value
	unsigned char max;			//The final parameter number
}para_g;

//========= Extern ==========//
extern const uint8_t SOFT_VER;

//========= Global Function ==========//
void InitialKeypad(void);	//Initial one time
void RunKeypad(void);		//Loop 1ms
void DisplayDEC(unsigned short modbus_value, unsigned char decimal);
void DisplayHEX(unsigned short modbus_value);
void DisplayCode(unsigned char dg4, unsigned char dg3, unsigned char dg2, unsigned char dg1);


#endif /* CG_SRC_U_KEYPAD_H_ */
