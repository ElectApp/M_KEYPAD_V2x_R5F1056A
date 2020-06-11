/*
 * u_data_flash2.h
 *
 *  Created on: Mar 16, 2020
 *      Author: Asus
 */

#ifndef CG_SRC_U_DATA_FLASH2_H_
#define CG_SRC_U_DATA_FLASH2_H_

#define ROM_Header 0x2020

typedef enum{
	RI_SP=0,
	RI_CRC,		//CRC
	RI_LEN,
}ROM_DATA_INDEX;

typedef enum{
	ROM_None = 0,
	ROM_Write,
	ROM_Read,
	ROM_Factory,
}ROM_FLAG;

typedef struct{
	unsigned short data[RI_LEN];
	unsigned char exe_flag;
}ROM_DATA;

//Extern variable
extern ROM_DATA rom;

//Function
void DF_Read(void);
void DF_Write(void);
void DF_Handle(void);

#endif /* CG_SRC_U_DATA_FLASH2_H_ */
