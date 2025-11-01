//----------------------------------------------------------------------------------------------------------------------------------

#include "arm32.h"
#include "ccu_control.h"

#include "touchpanel.h"	    //add 0.025c

#include "dram_control.h"
#include "display_control.h"
#include "spi_control.h"
#include "bl_display_lib.h"
#include "bl-uart.h"
#include "bl_fpga_control.h"
#include "bl_sd_card_interface.h"
#include "bl_variables.h"

#include <string.h>

#define BLACK_COLOR            0x00000000   //*
#define DARKGREY_COLOR         0x00181818   //*
#define LIGHTGREY_COLOR        0x00333333   //*
#define GREY_COLOR             0x00444444   //* 
#define LIGHTGREY1_COLOR       0x00606060   //* 
#define WHITE_COLOR            0x00FFFFFF   //*
#define RED_COLOR              0x00FF0000   //*
#define YELLOW_COLOR           0x00FFFF00   //*
#define DARKGREEN_COLOR        0x0000BB00   //*
#define GREEN_COLOR            0x0000FF00   //*
#define BLUE_COLOR             0x0000FFFF   //*
#define DARKBLUE_COLOR         0x00000078   //* 
#define ORANGE_COLOR           0x00FF8000   //*
#define MAGENTA_COLOR          0x00FF00FF   //*

//----------------------------------------------------------------------------------------------------------------------------------
// This code now works but needs a debug flag to disable the displaying of information
//
// For the 1014D a new version that only loads the main code without showing messages and does not wait for the FPGA
// can be distilled and tested.
//
// There is a problem with the SD card code though when used with a specific 4GB card. Could be a speed issue
//----------------------------------------------------------------------------------------------------------------------------------
// The bootloader itself starts on sector 16, so in the package the firmware should be copied in from address 0x8000
//  to allow for 32KB boot loader
//----------------------------------------------------------------------------------------------------------------------------------

#define PROGRAM_START_SECTOR      80
#define DISPLAY_CONFIG_SECTOR    710

#define BASE_ADDRESS    0x80000000
#define FEL_ADDRESS     0xFFFF0020

//----------------------------------------------------------------------------------------------------------------------------------

//Buffer for reading header from SD card
unsigned char buffer[512];

//----------------------------------------------------------------------------------------------------------------------------------

int main(void)
{
  //Set the address to run the loaded main program
  unsigned int address = BASE_ADDRESS;
  unsigned int length;
  unsigned int blocks;
  uint32 choice;
  int i,j, retval;
  uint32 waittime = 0;
  uint32 time = 0;
  
  //Initialize the clock system
  sys_clock_init();
  
  //Initialize the internal DRAM
  sys_dram_init();

  //Instead of full memory management just the caches enabled
  arm32_icache_enable();
  arm32_dcache_enable();
  
  //Initialize display (PORT D + DEBE)
  sys_init_display(SCREEN_WIDTH, SCREEN_HEIGHT, (uint16 *)maindisplaybuffer);
  
  //Setup the display lib
  display_set_screen_buffer((uint16 *)maindisplaybuffer);
  display_set_dimensions(SCREEN_WIDTH, SCREEN_HEIGHT);
  
  //Initialize FPGA (PORT E)
  fpga_init();
  
  //Got some time to spare
  for(j=0;j<1000;j++)
  {
    //At 600MHz CPU_CLK 1000 = ~200uS
    for(i=0;i<1000;i++);
  }
  
  //Wait and make sure FPGA is ready
  fpga_check_ready();

  //Turn on the display brightness
  //fpga_set_backlight_brightness(0x4E20);//0xEA60-100%
  
  //Turn of the display brightness
  fpga_set_backlight_brightness(0x20);
  
  //Setup for error message display
  display_set_font(&font_0);
  
  //************* Display configuration reading and setting ********************
  
  //Initialize the SD card
  if(sd_card_init() != SD_OK)
  {
    display_set_fg_color(RED_COLOR);
    display_text(328, 300, "SD card init failed"); 

    //On error start FEL
    address = FEL_ADDRESS;
    goto out;
  }
  
  //Load the display configuration sector to DRAM before the startup screen program
  if(sd_card_read(DISPLAY_CONFIG_SECTOR, 1, (uint8 *)DISPLAY_CONFIG_ADDRESS) != SD_OK)
  {
    display_set_fg_color(BLACK_COLOR);
    //Fill the settings background
    display_fill_rect(0, 0, 800, 480); 
          
    display_set_fg_color(RED_COLOR);
    display_text(190, 300, "SD card, Failed reading display configuration sector");
      
    //On error start FEL
    address = FEL_ADDRESS;
    goto out;
  }
  
  //Set display with load configuration from DRAM (SD card)
  set_display_config();
  
  //Display the text with yellow color
  display_set_fg_color(YELLOW_COLOR);
  display_text(304, 430, "Harin Lee <me@hrin.org>");
  
 //*****************************************************************************
  
  //Setup the touch panel interface
  tp_i2c_setup();
  havetouch = 0;
  xtouch = 0;
  ytouch = 0;
  
  //Scan the touch panel to see if there is user input
  //tp_i2c_read_status();
  
  //check SDcart startup settings
  choice = STARTUP_CONFIG_ADDRESS[0];  //choice 1 for fnirsi firmware, 0 for peco firmware, 2 for FEL mode, 4 view choice menu
  //if ((choice > 3)||(havetouch == 1))  // choice>3 or touch detected view choice menu
  if (choice > 3)  // choice>3 view choice menu
  { choice &= 0x03; //choice 1 for fnirsi firmware, 0 for peco firmware, 2 for FEL mode
//------------------------------------------------------------------------------
  
    display_set_fg_color(LIGHTGREY_COLOR);
    //Fill the settings background
    display_fill_rect(40,  50, 106, 35);  //y55 d25 y52 d31
    display_fill_rect(340, 50, 130, 35);  
    display_fill_rect(620, 50, 114, 35);  
  
    //Set green rect on default settings
    display_set_fg_color(GREEN_COLOR);
    if(choice == 0) display_draw_rect(40,  50, 106, 35);  //SD 40,  55, 106, 25
    if(choice == 2) display_draw_rect(340, 50, 130, 35);  //FEL mode 340, 55, 130, 25
    if(choice == 1) display_draw_rect(620, 50, 114, 35);  //SPI 620, 55, 114, 25
  
    display_set_fg_color(WHITE_COLOR);
    //Setup the choices texts
    display_text(50,  60, "SD firmware");
    display_text(350, 60, "Start FEL mode");
    display_text(630, 60, "SPI firmware");
  
    display_text(300, 150, "Reaming time to action:  s"); //Reaming time to action
    display_text( 40, 430, VERSION_STRING); 
    
    //Display the text with green color
    display_set_fg_color(GREEN_COLOR);
    display_text(300, 200, "Choices mode on the screen"); 
  
    waittime = 7000;  //~2s
  
    //Scan the touch panel to see if there is user input
    while(havetouch == 0) 
    //while(choice == 0)    
    {
        tp_i2c_read_status();
        if(havetouch == 1) waittime = 30000; //if touch increse time to 10sec
        havetouch = 0; 
      
        /*
        display_set_fg_color(BLACK_COLOR);
        //Fill the settings background
        display_fill_rect(50, 200, 200, 13);  //x , y , sirka, vyska
        display_set_fg_color(WHITE_COLOR);
        display_text(50,  180, "xtouch  ytouch");
        display_decimal(50,   200, xtouch);
        display_decimal(100,  200, ytouch);
    
        display_set_fg_color(RED_COLOR);
        display_text(337, 300, "Not an executable");
        //display_text(305, 320, "SD card first read failed");
        display_text(295, 320, "SD card program read failed");
        display_text(190, 340, "SD card, Failed reading display configuration sector");
        display_text(328, 360, "SD card init failed");
        */  
        //Display the button with green color
        display_set_fg_color(GREEN_COLOR);
    
        //Scan for where the touch is applied, PECO firmware - SDcart
        //if((xtouch >= 40) && (xtouch <= 160) && (ytouch >= 50) && (ytouch <= 90)) 
        if((xtouch >= 40) && (xtouch < 146) && (ytouch >= 40) && (ytouch < 95))  
        {   choice = 0; havetouch = 1; waittime = 1;
            //Fill the settings background
            display_fill_rect(40,  50, 106, 35);  
            display_set_fg_color(LIGHTGREY_COLOR);
            //Fill the settings background
            display_draw_rect(340, 50, 130, 35);  //340, 55, 130, 25
            display_draw_rect(620, 50, 114, 35); 
            display_set_fg_color(BLACK_COLOR);
            //Setup the choices texts
            display_text(50,  60, "SD firmware");
        }
         
        //Scan for where the touch is applied, FEL mode - RAM
        if((xtouch >= 340) && (xtouch < 480) && (ytouch >= 40) && (ytouch < 95)) 
        {   choice = 2; havetouch = 1; waittime = 1;
            //Fill the settings background
            display_fill_rect(340, 50, 130, 35);  
            display_set_fg_color(LIGHTGREY_COLOR);
            //Fill the settings background
            display_draw_rect(40,  50, 106, 35); 
            display_draw_rect(620, 50, 114, 35); 
            display_set_fg_color(BLACK_COLOR);
            //Setup the choices texts
            display_text(350, 60, "Start FEL mode");
        }
          
        //Scan for where the touch is applied, Fnirsi firmware - SPI Flash
        //if((xtouch >= 620) && (xtouch <= 760) && (ytouch >= 50) && (ytouch <= 90)) 
        if((xtouch >= 620) && (xtouch < 734) && (ytouch >= 40) && (ytouch < 95)) 
        {   choice = 1; havetouch = 1; waittime = 1;
            //Fill the settings background
            display_fill_rect(620, 50, 114, 35); 
            display_set_fg_color(LIGHTGREY_COLOR);
            //Fill the settings background
            display_draw_rect(40,  50, 106, 35); 
            display_draw_rect(340, 50, 130, 35);  
            display_set_fg_color(BLACK_COLOR);
            //Setup the choices texts
            display_text(630, 60, "SPI firmware");
        } 
    
        if(waittime == 10) {havetouch = 1;} //10500 is 5sec, 5500 2.5s 
        //if(waittime>5500) choice = 1; //10500 is 5sec, 5500 2.5s
        waittime--;
    
    
        if (time != waittime/3200) 
        {   
            time = waittime/3200;
            display_set_fg_color(BLACK_COLOR);
            //Fill the settings background
            display_fill_rect(490, 150, 10, 13);  //x , y , sirka, vyska
    
            display_set_fg_color(WHITE_COLOR);
            display_decimal(490, 150, time);
        }
          
    }// end while(havetouch == 0) 
  } //end if (choice>3)

  //display_set_fg_color(WHITE_COLOR);
  //Fill the settings background
  //display_fill_rect(0, 0, 800, 480);  //x , y , sirka, vyska

  //Display the text with green color
  display_set_fg_color(ORANGE_COLOR);
  display_text(360, 250, "LOADING...");

  //Wait until the 500 milliseconds have passed
  for(j=0;j<500;j++)       //500 is 500ms ,1000 is 1s
  {
    //At 600MHz CPU_CLK 5000 = ~2000uS
    for(i=0;i<5000;i++);
  }
 
  choice = choice & 3;    //mask view menu
  //Load the chosen program
  
  //***************************** SD CARD **************************************  
  if(choice == 0)
  {
    //Load the first program sector from the SD card
    if(sd_card_read(PROGRAM_START_SECTOR, 1, buffer) != SD_OK)
    {
      display_set_fg_color(RED_COLOR);
      //display_text(305, 300, "SD card first read failed");
      display_text(295, 300, "SD card program read failed");

      //On error start FEL
      address = FEL_ADDRESS;
      goto out;
    }

    //Check if there is a brom header there
    if(memcmp(&buffer[4], "eGON.EXE", 8) != 0)
    {
      display_set_fg_color(RED_COLOR);
      display_text(337, 300, "Not an executable");

      //On error start FEL
      address = FEL_ADDRESS;
      goto out;
    }

    //Get the length from the header
    length = ((buffer[19] << 24) | (buffer[18] << 16) | (buffer[17] << 8) | buffer[16]);

    //Calculate the number of sectors to read
    blocks = (length + 511) / 512;

    //Copy the first program bytes to DRAM (So the eGON header is skipped)
    memcpy((void *)0x80000000, &buffer[32], 480);

    //Check if more data needs to be read
    if(blocks > 1)
    {
      //Already read the first block
      blocks--;

      //Load the remainder of the program from the SD card
      if((retval = sd_card_read(PROGRAM_START_SECTOR + 1, blocks, (void *)0x800001E0)) != SD_OK)
      {
        display_set_fg_color(BLACK_COLOR);
        //Fill the settings background
        display_fill_rect(0, 0, 800, 480);  //x , y , sirka, vyska
          
        display_set_fg_color(RED_COLOR);
        display_text(10, 10, "SD card, Failed reading program sector");
        display_text(10, 26, "Error: ");
        display_text(10, 42, "Sector: ");
        display_set_fg_color(WHITE_COLOR);
        display_decimal(150, 26, retval);
        display_decimal(150, 42, blocks);

        //On error start FEL
        address = FEL_ADDRESS;
        goto out;
      }
    }
  }
  //***************************** SPI Flash ************************************
  else if(choice == 1)
  {
    //Initialize SPI for flash (PORT C + SPI0)
    sys_spi_flash_init();
  
    //Load the main program from flash
    //Get the header first
    sys_spi_flash_read(0x27000, buffer, 32);

    //Original boot loader checks on eGON.EXE file identifier

    //Get the length from the header and take of the header
    length = ((buffer[19] << 24) | (buffer[18] << 16) | (buffer[17] << 8) | buffer[16]) - 32;

    //Read the main program into DRAM. 
    sys_spi_flash_read(0x27020, (unsigned char *)0x80000000, length);
  }
  else
  //********************************* FEL **************************************
  {
    //Clear the screen
    display_set_fg_color(BLACK_COLOR);
    display_fill_rect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    
    //Show the scope is in FEL mode
    display_set_fg_color(WHITE_COLOR);
    display_text(340, 230, "Running FEL mode");
    
    //Set the address to run the FEL code
    address = FEL_ADDRESS;
  }
  //****************************************************************************
  
  out:
  //Start the chosen code
  __asm__ __volatile__ ("mov pc, %0\n" :"=r"(address):"0"(address));
}
