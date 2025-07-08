//----------------------------------------------------------------------------------------------------------------------------------

#include "types.h"

#include "menu.h"
#include "test.h"
#include "ccu_control.h"
#include "spi_control.h"
#include "timer.h"
#include "interrupt.h"
#include "display_control.h"
#include "fpga_control.h"
#include "touchpanel.h"
#include "power_and_battery.h"
#include "DS3231.h"

#include "fnirsi_1013d_scope.h"
#include "display_lib.h"
#include "scope_functions.h"
#include "statemachine.h"

#include "sd_card_interface.h"
#include "ff.h"

#include "usb_interface.h"
#include "ch340_class.h"

#include "arm32.h"

#include "variables.h"

#include <string.h>

//----------------------------------------------------------------------------------------------------------------------------------

extern IRQHANDLERFUNCION interrupthandlers[];

//----------------------------------------------------------------------------------------------------------------------------------

int main(void)
{
  //Initialize data in BSS section
  memset(&BSS_START, 0, &BSS_END - &BSS_START);

  //Initialize the clock system
  sys_clock_init();

  //Instead of full memory management just the caches enabled
  arm32_icache_enable();
  arm32_dcache_enable();

  //Clear the interrupt variables
  memset(interrupthandlers, 0, 256);

  //Setup timer interrupt
  timer0_setup();

  //Setup power monitoring
  power_interrupt_init();

  //Setup battery monitoring
  battery_check_init();

  //Enable interrupts only once. In the original code it is done on more then one location.
  arm32_interrupt_enable();

  //Initialize SPI for flash (PORT C + SPI0)
  sys_spi_flash_init();

  //Initialize FPGA (PORT E)
  fpga_init();

  //Turn off the display brightness
  fpga_set_backlight_brightness(0x0000);

  //Initialize display (PORT D + DEBE)
  sys_init_display(SCREEN_WIDTH, SCREEN_HEIGHT, (uint16 *)maindisplaybuffer);

  //Setup the display library for the scope hardware
  scope_setup_display_lib();

  //Setup the touch panel interface
  tp_i2c_setup();

  //Setup and check SD card on file system being present
  if(f_mount(&fs, "0", 1))
  {
    //Show SD card error message on failure
    //Set max screen brightness
    fpga_set_backlight_brightness(0xEA60);

    //Clear the display
    display_set_fg_color(BLACK_COLOR);
    display_fill_rect(0, 0, 800, 480);

    //Display the message in red
    display_set_fg_color(RED_COLOR);
    display_set_font(&font_3);
    display_text(30, 50, "SD ERROR");

    //On error just hang
    while(1);
  }

#ifndef USE_TP_CONFIG
#ifdef SAVE_TP_CONFIG
  //Save the touch panel configuration to be save
  save_tp_config();
#endif
#endif

  //Load configuration data from FLASH
  scope_load_configuration_data();
  
  //Setup the USB interface
  usb_device_init();
  
  //Serial active sent HELLO message
  //if(USB_CH340==1) {SendString("Fnirsi 1013D"); ch340Send('\n');}
  
  //ch340Send('A');
 
  //Enable or disable the channels based on the scope loaded settings
  fpga_set_channel_enable(&scopesettings.channel1);
  fpga_set_channel_enable(&scopesettings.channel2);

  //Set the volts per div for each channel based on the loaded scope settings
  fpga_set_channel_voltperdiv(&scopesettings.channel1);
  fpga_set_channel_voltperdiv(&scopesettings.channel2);

  //Set the channels AC or DC coupling based on the loaded scope settings
  fpga_set_channel_coupling(&scopesettings.channel1);
  fpga_set_channel_coupling(&scopesettings.channel2);
  
  //Enable something in the FPGA
  fpga_enable_system();
  
  fpga_set_sample_rate(scopesettings.samplerate);       //Short time base  
          //Setup the trigger system in the FPGA based on the loaded scope settings
  if(scopesettings.long_mode)  fpga_set_long_timebase(scopesettings.timeperdiv);
    else fpga_set_time_base(scopesettings.timeperdiv);  //Send the time base command for the longer settings
      //Set the sample rate that belongs to the selected time per div setting
    //scopesettings.samplerate = time_per_div_sample_rate[scopesettings.timeperdiv];  

  fpga_set_trigger_channel();
  fpga_set_trigger_edge();
  fpga_set_trigger_level();
  fpga_set_trigger_mode();

  //Set the trace offsets in the FPGA
  fpga_set_channel_offset(&scopesettings.channel1);
  fpga_set_channel_offset(&scopesettings.channel2);

  //Some initialization of the FPGA??. Data written with command 0x3C
  //fpga_set_battery_level();      //Only called here and in hardware check
  
  //Set screen brightness
  fpga_set_translated_brightness();
  
  //View message if load a default configuration in case of settings
  if (restore) scope_setup_restore_screen();
  
  //Set battery level to max for next calculations
  scopesettings.batterychargelevel = 20;    
  
  //Set the user value screen brightness
  scope_set_maxlight(scopesettings.maxlight);       //1 set max, 0 user value
  
  //Setup the main parts of the screen
  scope_setup_main_screen();
  
  //Modification on 09-03-2022. Begin
  //Clear the sample memory
  //memset(channel1tracebuffer, 128, sizeof(channel1tracebuffer));
  //memset(channel2tracebuffer, 128, sizeof(channel2tracebuffer));
  
  memset(channel1tracebufferAVG, 128, sizeof(channel1tracebufferAVG));
  memset(channel2tracebufferAVG, 128, sizeof(channel2tracebufferAVG));
   
  //Set the number of samples in use
  scopesettings.nofsamples  = SAMPLES_PER_ADC;
  scopesettings.samplecount = SAMPLE_COUNT;
  
  //Show initial trace data. When in NORMAL or SINGLE mode the display needs to be drawn because otherwise if there is no signal it remains black
  //if set long time base, no drawn.
  //if(!scopesettings.long_mode) scope_display_trace_data();
  //else scope_preset_long_mode();
  
  //scopesettings.display_data_done = 1
  
  //Switch to RUN
  //scopesettings.runstate = 1;//ok
  //scopesettings.display_data_done = 1;
  
  //scopesettings.batterychargelevel = 20;    //set battery level to max for next calculations
  
  //Flag READY to start convert
  scopesettings.display_data_done = 1;
  scopesettings.conversion_done = 0;
  havetouch = 0;
  
  scope_preset_values();
   
  //Wait until the analog part start
  timer0_delay(50);
  
  //only test and set calibration value, on setings menu
  //dc_shift_cal=1;
  
  //timerH=1000;
    
       //scope_open_keyboard_menu();
  
  //Monitor the battery, process and display trace data and handle user input until power is switched off
  while(1)
  {
    //Monitor the battery status
    battery_check_status();
    
    
 //-*************************************************************************   
    //scope_test_longbase_value();
  
    //scope_test_ADoffset_value();    //view values for ad offset min max center p2p
    //scope_test_calibration_value();
    
    //scope_test_trigerposition_value();
    
    //Serial active sent HELLO message
  //if(USB_CH340==1) {SendString("Fnirsi 1013D"); ch340Send('\n');}
    
    //scope_test_TP();
//     scope_open_keyboard_menu();
   // while(1){
 
  //handle_generator_menu_touch();
//    }
  //-************************************************************************
    //Select the mode 1-Long time base or 0-short time base
    if(scopesettings.long_mode)
    {
        scope_get_long_timebase_data();
    }
    else
    {   //Long memory mode, trigger is the beginning buffer
        //----------------------long memory mode --------------------------------------------------------
        if(scopesettings.long_memory)
        {
            //Check if: run or stop mode - 1-Run mode, 0-no triger long, trigger mode 1 or 2
            //if((scopesettings.runstate)&&(!triggerlong)&&(scopesettings.triggermode)) scope_check_long_trigger();
            //Check if run or stop mode - 1-Run mode
            //if((scopesettings.runstate)&&(triggerlong))
            //{

            //fpga_set_trigger_mode();
            //Set the new setting in the FPGA
            //fpga_set_sample_rate(0);    //200MSa
            //fpga_set_sample_rate(scopesettings.samplerate);
            
            //
            
                //Write the time base setting to the FPGA
    //fpga_set_time_base(scopesettings.timeperdiv);

    //Sampling with trigger circuit enabled (standard memory mode)
    //if(scopesettings.long_memory) scopesettings.samplemode = 0; else scopesettings.samplemode = 1;

    //Start the conversion
    //fpga_do_conversion();
            
            //Go through the trace data and display the trace data
              if((scopesettings.runstate)&&(!triggerlong))
        {scope_check_long_trigger();}
  
  //Check if run or stop mode - 1-Run mode
  if((scopesettings.runstate)&&(triggerlong)) {scope_acquire_trace_data_long();triggerlong=0;}
            
           // while(fpga_done_conversion()==0);
            //scopesettings.display_data_done = 1; 
                
            //if(scopesettings.display_data_done)
            //{
                //Set the new setting in the FPGA
                //fpga_set_sample_rate(scopesettings.samplerate);
    
                //scopesettings.samplemode = 0; 
    
                //fpga_write_cmd(0x1A);

                //Set trigger mode to auto
                //fpga_write_byte(0x00);

    
                //Go through the trace data and display the trace data
                //scope_acquire_trace_data();
            //}
            
        }
        //--------------------- short mode --------------------
        else
        //Standard memory mode, trigger is the middle buffer
        {
            scopesettings.samplemode = 1;//1
            
            
            //Go through the trace data and display the trace data
            scope_acquire_trace_data();
             //if (timerRTC==2) {scope_acquire_trace_data(); timerRTC=100;}
        }
    }

    //Handle the touch panel input
    touch_handler();
    
    //READ and show RTC time
    if (!(timerRTC)&&(onoffRTC))
    {
      timerRTC=500;
      //Reading time and date of DS3231
      readtime();                     
      //Show time on the screen
      display_set_fg_color(BLACK_COLOR);
      //Fill the settings background
      display_fill_rect(647, 5, 50, 13);  //x , y , sirka, vyska
      display_set_fg_color(WHITE_COLOR);
      display_set_font(&font_2);
      display_text(650, 5, buffertime);
    }
  }
}

//----------------------------------------------------------------------------------------------------------------------------------
