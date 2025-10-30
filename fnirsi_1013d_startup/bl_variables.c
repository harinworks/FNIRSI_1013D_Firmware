//----------------------------------------------------------------------------------------------------------------------------------

#include "bl_variables.h"

//----------------------------------------------------------------------------------------------------------------------------------
//Display data
//----------------------------------------------------------------------------------------------------------------------------------

//This first buffer is defined as 32 bits to be able to write it to file
uint32 maindisplaybuffer[SCREEN_SIZE / 2] __attribute__((section(".dmem")));

//----------------------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------------------
//Touch data
//----------------------------------------------------------------------------------------------------------------------------------

#ifndef USE_TP_CONFIG
uint8  tp_config_data[186];

uint32 xscaler;
uint32 yscaler;
#endif

uint8  havetouch;
uint16 xtouch;
uint16 ytouch;

uint16 xtouch_tmp;  //for development purposes only
uint16 ytouch_tmp;  //for development purposes only

//To allow for the different touch panel configurations in the field these variables can be controlled in the display configuration file
uint8 xswap;
uint8 yswap;

uint8 config_valid;

//----------------------------------------------------------------------------------------------------------------------------------