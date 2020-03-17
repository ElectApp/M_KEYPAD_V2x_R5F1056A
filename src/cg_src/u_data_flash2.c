/*
 * u_data_flash2.c
 *
 *  Created on: Mar 16, 2020
 *      Author: Asus
 *
 *
 */

/**
 * How to Use Data Flash
 *
 * 1. Click 'Code Generator' and 'Enable data flash access' on 'Data flash' tab
 * 2. Create new directory 'data_flash'
 * 3. Copy pfdl_types.h, pfdl.h, pfdl.lib to 'data_flash'
 * 4. Click right on project >> Properties >> C/C++ Builder >> Settings >> Linker
 * 5. Select 'Input' and add library file 'pfdl.lib'
 * 6. Select 'Section' >> add 'PFDL_POD' at 0x100 >> edit '.dataR' to 0xFF988
 * 7. Copy u_data_flash.c and .h to 'src/cg_src' directory
 * 8. Click right on 'src/cg_src' directory >> Properties >> C/C++ General >> Path and Symbol >> Add 'data_flash' directory
 * 9. Insert new element of 'ROM_DATA_INDEX' on u_data_flash.h (Don't remove 'RI_CRC' and 'RI_LEN') for your project
 * Original:
		typedef enum{
			RI_SP=0,
			RI_CRC,
			RI_LEN,
		}ROM_DATA_INDEX;

 * New:
		typedef enum{
			RI_W=0,
			RI_Hz,
			RI_A,
			RI_CRC,
			RI_LEN,
		}ROM_DATA_INDEX;
 *
 *	10. EX. Test read: rom.data[RI_SP] = 1234; DF_Read();
 *	11. NOTE!!! Don't read or write before initial 'USART'
 *
 **/

#include "pfdl.h"
#include "pfdl_types.h"
#include "u_data_flash2.h"
#include "u_keypad.h"

pfdl_status_t  		pfdl_stt;
pfdl_request_t		pfdl_req;
pfdl_descriptor_t   pfdl_des;
unsigned char create_flag = 0, open_flag = 0;

ROM_DATA rom;
//extern KEYPAD_DATA key;

unsigned char data_flash_write(unsigned short* add_buf, unsigned short w_len);
unsigned char data_flash_read(unsigned short* add_buf, unsigned short w_len);


/**
 * Calculates the CRC of the passed byte array from zero up to the
 * passed length.
 *
 * @param buffer the byte array containing the data.
 * @param length the length of the byte array.
 *
 * @return the calculated CRC as an unsigned 16 bit integer.
 *
 * Thank: https://github.com/yaacov/ArduinoModbusSlave/blob/master/src/ModbusSlave.cpp#L941-L964
 */
unsigned short GetCRC(unsigned char *buffer, unsigned short length)
{
	unsigned short i, j;
    short crc = 0xFFFF;
    unsigned short tmp;

    // calculate crc
    for (i = 0; i < length; i++)
    {
        crc = crc ^ buffer[i];

        for (j = 0; j < 8; j++)
        {
            tmp = crc & 0x0001;
            crc = crc >> 1;
            if (tmp)
            {
                crc = crc ^ 0xA001;
            }
        }
    }

    return crc;
}

void DF_Read(void){
	unsigned char err;
	unsigned short crc;
	//Clear
	rom.exe_flag = ROM_None;
	//Read ROM
	err = data_flash_read(rom.data, RI_LEN);
	//DisplayHEX(err);
	//Check CRC
	crc = GetCRC((unsigned char*)rom.data, RI_CRC*2);
	if(err || rom.data[RI_CRC]!=crc){
		//Factory Reset
		rom.exe_flag = ROM_Factory;
	}
	//DisplayDEC(rom.data[RI_SP], 0);

	/*
	err = data_flash_read(rom.data, RI_LEN);
	if(err){
		DisplayHEX(err);
		key.led.bit.stop = 1U;
	}else{
		key.led.bit.run = 1U;
		DisplayDEC(rom.data[RI_CRC], 0);
	}
	*/

}

void DF_Write(void){
	//unsigned char err;
	//rom.data[RI_SP] = 3567;
	//Clear
	rom.exe_flag = ROM_None;
	//CRC
	rom.data[RI_CRC] = GetCRC((unsigned char*)rom.data, RI_CRC*2);
	//Write to ROM
	data_flash_write(rom.data, RI_LEN);
	/*
	DisplayDEC(rom.data[RI_CRC], 0);
	err = data_flash_write(rom.data, RI_LEN);
	if(err){
		key.led.bit.stop = 1U;
		DisplayDEC(err, 0);
	}else{
		key.led.bit.run = 1U;
	}
	*/
}


/***********************************************************************************************************************
* Function Name: R_FDL_Create
* Description  : This function initializes the flash data library.
* Arguments    : None
* Return Value : None
***********************************************************************************************************************/
void FDL_Create(void)
{
	pfdl_des.fx_MHz_u08 = 48;   /* Set an integer of the range from 1 to 48 according to GUI setting of HOCO. */
	pfdl_des.wide_voltage_mode_u08 = 0; /* Voltage mode: 0 :full speed mode ,1:Wide voltage mode */

    create_flag = 1;	//Save Flag

}



/***********************************************************************************************************************
* Function Name: R_FDL_Open
* Description  : This function opens the RL78 data flash library.
* Arguments    : None
* Return Value : None
***********************************************************************************************************************/
void FDL_Open(void)
{
	if(!create_flag){ FDL_Create(); }

	if(!open_flag){
		PFDL_Open(&pfdl_des);
		open_flag = 1;
	}

}

/***********************************************************************************************************************
* Function Name: R_FDL_Close
* Description  : This function closes the RL78 data flash library.
* Arguments    : None
* Return Value : None
***********************************************************************************************************************/
void FDL_Close(void)
{
    PFDL_Close();
    open_flag = 0;
}


//***************************//
// Write data flash function //
//***************************//
unsigned char data_flash_write(unsigned short* add_buf, unsigned short w_len){
	unsigned char err = 0;

	//initial//
	pfdl_stt = PFDL_BUSY;

	//OPEN
	FDL_Open();

	//====BLANK CHECK COMMAND=== //
	pfdl_req.command_enu = PFDL_CMD_BLANKCHECK_BYTES;	//blank_check
	pfdl_req.index_u16 = 0;								//block 0 (always =1024 bytes/block)
	pfdl_req.bytecount_u16 = 2*w_len;					//length.(words)
	pfdl_stt=PFDL_Execute(&pfdl_req);
	// wait for process end
	while (pfdl_stt==PFDL_BUSY)
	{
	    pfdl_stt=PFDL_Handler();
	}

	//Check status oc check blank
	if(pfdl_stt==PFDL_OK  || pfdl_stt==PFDL_ERR_MARGIN  )
	{

		//===ERASE COMMAND ==//
		pfdl_req.command_enu = PFDL_CMD_ERASE_BLOCK;	//blank_check
		pfdl_req.index_u16 = 0;							//block 0 (always =1024 bytes/block)
		pfdl_stt=PFDL_Execute(&pfdl_req);

		// wait for process end //
		while (pfdl_stt==PFDL_BUSY)
		{
			pfdl_stt=PFDL_Handler();
		}

		//check status of erase process //
		if(pfdl_stt==PFDL_OK)	//erase completed //
		{

			// ===== WRITE COMMAND=====//
			pfdl_req.command_enu = PFDL_CMD_WRITE_BYTES;	//write //
			pfdl_req.index_u16 = 0;			//block 0 (always =1024 bytes/block)//
			pfdl_req.bytecount_u16 = 2*w_len;		//length.(words)//
			pfdl_req.data_pu08 = (pfdl_u08*)add_buf;	//specifics address buffer//
			pfdl_stt=PFDL_Execute(&pfdl_req);

			// wait for process end //
			while(pfdl_stt==PFDL_BUSY)
			{
				pfdl_stt=PFDL_Handler();
			}
			//check status of write process//
			if(pfdl_stt==PFDL_OK)
			{
				// Write data completed //


				//=== IVERIFY COMMAND====//
				pfdl_req.command_enu = PFDL_CMD_IVERIFY_BYTES;	//write //
				pfdl_req.index_u16 = 0;				//block 0 (always =1024 bytes/block)//
				pfdl_req.bytecount_u16 = 2*w_len;			//length.(words)//
				pfdl_stt=PFDL_Execute(&pfdl_req);

				// wait for process end //
				while(pfdl_stt==PFDL_BUSY)
				{
					pfdl_stt=PFDL_Handler();
				}
				//check status of i verify //
				if(pfdl_stt==PFDL_OK)
				{	// verify data ok//

					err=0;
				}
				else
				{	//verify data error //
					err=4;
				}

			}
			else
			{
				//write data error //
				err=3;
			}


		}else
		{	//erase error //
			err=2;
		}

	}
	else
	{	//check blank error //
		err=1;
	}

	//===CLOSE  ==//
	FDL_Close();

	return err;
}

//**************************//
//read data flash function  //
//**************************//
unsigned char data_flash_read(unsigned short* add_buf, unsigned short w_len){
	pfdl_u16    i;									//Index
	pfdl_u08 u8Buf[sizeof(w_len)*2];				//Read buffer

	//Open
	FDL_Open();

	//Read data
	pfdl_req.command_enu = PFDL_CMD_READ_BYTES;		//Read
	pfdl_req.index_u16 = 0;							//block 0 (always =1024 bytes/block)
	pfdl_req.bytecount_u16 = w_len*2;				//length.(words)
	pfdl_req.data_pu08 = u8Buf;
	pfdl_stt = PFDL_Execute(&pfdl_req);

	//Copy to result
	for(i=0; i<w_len; i++){
		add_buf[i] = u8Buf[i] | (u8Buf[i+1]<<8);
	}

	//Close
	FDL_Close();

	return (unsigned char)pfdl_stt;

}



