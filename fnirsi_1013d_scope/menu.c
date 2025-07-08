//----------------------------------------------------------------------------------------------------------------------------------
#include "types.h"
#include "menu.h"
#include "test.h"
#include "scope_functions.h"
#include "statemachine.h"
#include "touchpanel.h"
#include "timer.h"
#include "fpga_control.h"
#include "spi_control.h"
#include "sd_card_interface.h"  //only diagnostic menu?
#include "display_lib.h"
#include "ff.h"
#include "DS3231.h"

#include "usb_interface.h"
#include "variables.h"

#include "sin_cos_math.h"

#include <string.h>
//----------------------------------------------------------------------------------------------------------------------------------

void scope_setup_display_lib(void)
{
  display_set_bg_color(BLACK_COLOR);

  display_set_screen_buffer((uint16 *)maindisplaybuffer);

  display_set_dimensions(SCREEN_WIDTH, SCREEN_HEIGHT);
}

//----------------------------------------------------------------------------------------------------------------------------------

void scope_setup_main_screen(void)
{
  //Prepare the screen in a working buffer
//  display_set_screen_buffer(displaybuffer1);

  //Setup for clearing top and right menu bars
  display_set_fg_color(BLACK_COLOR);

  //Top bar for the main, channel and trigger menu and information
  display_fill_rect(0, 0, 730, 48);

  //Right bar for control buttons
  display_fill_rect(730, 0, 70, 480);

  //Setup the menu bar on the right side
  scope_setup_right_control_menu();

  //Check if normal or wave view mode
  if(scopesettings.waveviewmode)
  {
    //Wave view mode so show return button
    scope_main_return_button(0);
  }    
  else
  {
    //Normal mode so show menu button
    scope_menu_button(0);
  }

  //Show the user if the acquisition is running or stopped
  scope_run_stop_text();

  //Display the channel menu select buttons and their settings
  scope_channel_settings(&scopesettings.channel1, 0);
  scope_channel_settings(&scopesettings.channel2, 0);

  //Display the current time per div setting
  scope_acqusition_settings(0);

  //Show the user selected move speed
  scope_move_speed(0);
  
  //Show the icon to white sun or yellow sun-max light
  scope_maxlight_item(scopesettings.maxlight);      

  //Display the trigger menu select button and the settings
  scope_trigger_settings(0);

  //Show the battery charge level and charging indicator
  scope_battery_status();
  
  //Show version information
  display_set_fg_color(WHITE_COLOR);
  display_set_font(&font_2);
  display_text(VERSION_STRING_XPOS, VERSION_STRING_YPOS, VERSION_STRING);
}

//----------------------------------------------------------------------------------------------------------------------------------

void scope_setup_restore_screen(void)
{
  //Clear the whole screen
  display_set_fg_color(BLACK_COLOR);
  display_fill_rect(0, 0, 800, 480);
  
  //Fill in the screens with a blue color
  display_set_fg_color(DARKBLUE_COLOR);
  display_fill_rect(10, 10, 780, 460);
  
  //Display the text with font 5 and the white color
  display_set_font(&font_5);
  display_set_fg_color(WHITE_COLOR);
  display_text(370, 150, VERSION_STRING);    //Show version information 
  display_text(270, 200, "Load default settings value");    
  
  if (calibrationfail == 1)
    {
    //Display the text with red color
    display_set_fg_color(RED_COLOR);
    display_text(240, 240, "Need input calibration procedure !");  
    }
  else
    {
     //Display the text with green color
    display_set_fg_color(GREEN_COLOR);
    display_text(240, 240, "LOAD input calibration data OK !"); 
    }
  
  //Display the text with green color
  display_set_fg_color(GREEN_COLOR);
  display_text(320, 300, "Touch the screen");  
   
  //Scan the touch panel to see if there is user input
  while((havetouch == 0)) tp_i2c_read_status();
  
  //Clear the whole screen
  display_set_fg_color(BLACK_COLOR);
  display_fill_rect(0, 0, 800, 480);
   
}

//----------------------------------------------------------------------------------------------------------------------------------

void scope_setup_view_screen(void)
{
  //Switch to view mode so disallow saving of settings on power down
  viewactive = VIEW_ACTIVE;

  //Set scope run state to running to have it sample fresh data on exit
  scopesettings.runstate = 1;//ok

  //Only needed for waveform view. Picture viewing does not change the scope settings
  if(viewtype == VIEW_TYPE_WAVEFORM)
  {
    //Save the current settings
    scope_save_setup(&savedscopesettings1);
  }
  
  //Initialize the view mode variables
  //Used for indicating if select all or select button is active
  viewselectmode = 0;

  //Start at first page
  viewpage = 0;

  //Clear the item selected flags
  memset(viewitemselected, VIEW_ITEM_NOT_SELECTED, VIEW_ITEMS_PER_PAGE);

  //Set storage buffer for screen capture under selected signs and messages
  display_set_destination_buffer(displaybuffer2);
  display_set_source_buffer(displaybuffer2);

  //Display the file actions menu on the right side of the screen
  scope_setup_right_file_menu();

  //Load the thumbnail file for the current view type
  if(scope_load_thumbnail_file() != 0)
  {
    //Restore the main screen
    scope_setup_main_screen();
    
    //Loading the thumbnail file failed so no sense in going on
    return;
  }

  //Display the available thumbnails for the current view type
  scope_initialize_and_display_thumbnails();

  //Handle touch for this part of the user interface
  handle_view_mode_touch();

  //This is only needed when an actual waveform has been viewed, but that needs an extra flag
  //Only needed for waveform view. Picture viewing does not change the scope settings
  if(viewtype == VIEW_TYPE_WAVEFORM)
  {
    //Restore the current settings
    scope_restore_setup(&savedscopesettings1);

    //Make sure view mode is normal
    scopesettings.waveviewmode = 0;

    //And resume with auto trigger mode
    //scopesettings.triggermode = 0;

    //Need to restore the original scope data and fpga settings

    //Is also part of startup, so could be done with a function
    //Set the volts per div for each channel based on the loaded scope settings
    fpga_set_channel_voltperdiv(&scopesettings.channel1);
    fpga_set_channel_voltperdiv(&scopesettings.channel2);

    //These are not done in the original code
    //Set the channels AC or DC coupling based on the loaded scope settings
    fpga_set_channel_coupling(&scopesettings.channel1);
    fpga_set_channel_coupling(&scopesettings.channel2);

    //Setup the trigger system in the FPGA based on the loaded scope settings
    fpga_set_sample_rate(scopesettings.samplerate);
    fpga_set_trigger_channel();
    fpga_set_trigger_edge();
    fpga_set_trigger_level();
    fpga_set_trigger_mode();

    //Set channel screen offsets
    fpga_set_channel_offset(&scopesettings.channel1);
    fpga_set_channel_offset(&scopesettings.channel2);
  }

  //Reset the screen to the normal scope screen
  scope_setup_main_screen();
  
  //Display the trace data
  scope_display_trace_data();   //add for single mode screen mismas

  //Back to normal mode so allow saving of settings on power down
  viewactive = VIEW_NOT_ACTIVE;
}

//----------------------------------------------------------------------------------------------------------------------------------

void scope_setup_usb_screen(void)
{
  //Clear the whole screen
  display_set_fg_color(BLACK_COLOR);
  display_fill_rect(0, 0, 800, 480);

  //Set the light color for the equipment borders
  display_set_fg_color(0x00AAAAAA);

  //Draw the computer screen
  display_fill_rounded_rect(470, 115, 250, 190, 2);
  display_fill_rect(580, 305, 30, 20);
  display_fill_rounded_rect(550, 325, 90, 10, 2);
  display_fill_rect(550, 331, 89, 4);

  //Draw the scope
  display_fill_rounded_rect(80, 200, 180, 135, 2);

  //Draw the cable
  display_fill_rect(210, 188, 10, 12);
  display_fill_rect(213, 154, 4, 36);
  display_fill_rect(213, 150, 152, 4);
  display_fill_rect(361, 154, 4, 106);
  display_fill_rect(361, 260, 98, 4);
  display_fill_rect(458, 257, 12, 10);

  //Fill in the screens with a blue color
  display_set_fg_color(DARKBLUE_COLOR);
  display_fill_rect(477, 125, 235, 163);
  display_fill_rect(88, 210, 163, 112);

  //Draw a dark border around the blue screens
  display_set_fg_color(0x00111111);
  display_draw_rect(477, 125, 235, 163);
  display_draw_rect(88, 210, 163, 112);

  //Display the text with font 5 and the light color
  display_set_font(&font_5);
  display_set_fg_color(WHITE_COLOR);
  display_text(130, 254, "Off USB");    //130 bolo

  //Start the USB interface
  usb_device_enable();

  //Wait for the user to touch the scope Off USB section to quit
  while(1)
  {
    //Scan the touch panel for touch
    tp_i2c_read_status();

    //Check if there is touch
    if(havetouch)
    {
      //Check if touch within the text field  //Check if touch within the USB OFF button
      if((xtouch >= 90) && (xtouch <= 250) && (ytouch >= 210) && (ytouch <= 320))
        {
        //Touched the text field so wait until touch is released and then quit
        tp_i2c_wait_for_touch_release();

        break;
        }
    }
  }
  
  //Stop the USB interface
  usb_device_disable();

  //Re-sync the system files
  scope_sync_thumbnail_files();
}

//----------------------------------------------------------------------------------------------------------------------------------

void scope_setup_right_control_menu(void)
{
  //Setup for clearing right menu bar
  display_set_fg_color(BLACK_COLOR);

  //Clear the right bar for the control buttons
  display_fill_rect(730, 0, 70, 480);

  //Display the control button
  scope_control_button(0);

  //Check in which state the right menu is in
  if(scopesettings.rightmenustate == 0)
  {
    //Main control state so draw the always used buttons
    scope_t_cursor_button(0);
    scope_v_cursor_button(0);
    scope_measures_button(0);
    scope_save_picture_button(0);

    //Check if in wave view mode (Scope mode)
    if(scopesettings.waveviewmode)
    {
      //Wave view mode buttons
      scope_previous_wave_button(0);
      scope_next_wave_button(0);
      scope_delete_wave_button(0);
    }    
    else
    {
      //Main control mode buttons
      scope_run_stop_button(0);
      scope_auto_set_button(0);
      scope_save_wave_button(0);
    }
  }
  //else
  //Check in which RIGHT sensitivity menu
  if(scopesettings.rightmenustate == 1)
  {
    //Channel sensitivity state
    scope_channel_sensitivity_control(&scopesettings.channel1, 0, 0);
    scope_channel_sensitivity_control(&scopesettings.channel2, 0, 0);

    //Check if in wave view mode (Scope mode)
    if(scopesettings.waveviewmode)
    {
      //Wave view mode
      scope_show_grid_button(0);
    }    
    else
    {
      //Main control mode
      scope_50_percent_trigger_button(0);
    }
  }
   //Check in which DC shift calibration set menu - center position on signal
  if(scopesettings.rightmenustate == 2)
  {
    //Channel sensitivity state, this menu for - center position on signal
    scope_channel_sensitivity_control(&scopesettings.channel1, 0, 0);
    scope_channel_sensitivity_control(&scopesettings.channel2, 0, 0);
    
    //add visible value dc_shift_center
    scope_channel_dcshift_value();
      
    //Next calibration page
    scope_next_cal_button(0);
  } 
  //Check in which DC shift calibration set menu - size displayed signal
  if(scopesettings.rightmenustate == 3)
  {
    //Channel sensitivity state, this menu for - size displayed signal
    scope_channel_sensitivity_control(&scopesettings.channel1, 0, 0);
    scope_channel_sensitivity_control(&scopesettings.channel2, 0, 0);
    
    //add visible value dc_shift_size
    scope_channel_dcshift_value();
      
    //Next calibration page
    scope_next_cal_button(0);
  }  
  //Check in which DC shift calibration set menu - adjust measurement values
  if(scopesettings.rightmenustate == 4)
  {
    //Channel sensitivity state, this menu for - adjust measurement values
    scope_channel_sensitivity_control(&scopesettings.channel1, 0, 0);
    scope_channel_sensitivity_control(&scopesettings.channel2, 0, 0);
    
    //add visible value dc_shift_value
    scope_channel_dcshift_value();
      
    //Next calibration page
    scope_end_cal_button(0);
  }  
  //-----------------------------
}

//----------------------------------------------------------------------------------------------------------------------------------

void scope_setup_right_file_menu(void)
{
  //Set black color for background
  display_set_fg_color(BLACK_COLOR);

  //Clear the right menu bar
  display_fill_rect(730, 0, 70, 480);

  //Add the buttons
  scope_return_button(0);
  scope_select_all_button(0);
  scope_select_button(0);
  scope_delete_button(0);
  scope_page_up_button(0);
  scope_page_down_button(0);
}

//----------------------------------------------------------------------------------------------------------------------------------

void scope_setup_bottom_file_menu(int mode)
{
  PTHUMBNAILDATA thumbnaildata;   
    
  //Check if background needs to be saved
  if(mode == VIEW_BOTTON_MENU_INIT)
  {
    //Save the screen rectangle where the menu will be displayed
    display_copy_rect_from_screen(0, 390, 800, 90);
  }

  //Check if it needs to be drawn
  if(mode & VIEW_BOTTON_MENU_SHOW)
  {
    //Draw the background in grey
    display_set_fg_color(DARKGREY_COLOR);
    display_fill_rect(0, 390, 800, 90);

    //Draw the filename in white
    display_set_fg_color(WHITE_COLOR);
    display_set_font(&font_5);
    //display_text(20, 392, viewfilename);
    
      if(onoffRTC) //if RTCon view rtc info
        {
        thumbnaildata = &viewthumbnaildata[viewcurrentindex];
        decodethumbnailfilename(thumbnaildata->filename);
        display_text(20, 392, filenameRTC);
        }   else display_text(20, 392, viewfilename);
    
    //Setup the buttons
    scope_bmp_return_button(0);    
    scope_bmp_delete_button(0);
    scope_bmp_previous_button(0);
    scope_bmp_next_button(0);
    
    //Signal menu is visible
    viewbottommenustate = VIEW_BOTTON_MENU_SHOW | VIEW_BOTTOM_MENU_ACTIVE;
  }
  else
  {
    //Hide the menu bar
    display_copy_rect_to_screen(0, 390, 800, 90);

    //Signal menu is not visible
    viewbottommenustate = VIEW_BOTTON_MENU_HIDE | VIEW_BOTTOM_MENU_ACTIVE;
  }
}

//----------------------------------------------------------------------------------------------------------------------------------
// Right side bar functions
//----------------------------------------------------------------------------------------------------------------------------------

void scope_control_button(int mode)
{
  //Check if inactive or active mode
  if(mode == 0)
  {
    //Grey color for inactive button
    display_set_fg_color(GREY_COLOR);
  }
  else
  {
    //Orange color for activated button
    display_set_fg_color(ITEM_ACTIVE_COLOR);
  }

  //Draw the body of the button
  display_fill_rounded_rect(739, 5, 51, 50, 2);

  //Draw the edge
  display_set_fg_color(0x00606060);
  display_draw_rounded_rect(739, 5, 51, 50, 2);

  //Check if inactive or active mode
  if(mode == 0)
  {
    //White text color for inactive button
    display_set_fg_color(WHITE_COLOR);
  }
  else
  {
    //Black text color for activated button
    display_set_fg_color(BLACK_COLOR);
  }

  //Display the text
  display_set_font(&font_3);
  display_text(748, 22, "CTRL");
}

//----------------------------------------------------------------------------------------------------------------------------------

void scope_run_stop_button(int mode)
{ 
  //Check if inactive or active mode
  if(mode == 0)
  {
  //Check if run or stop mode  
  if(scopesettings.runstate) //ok
    {//Run mode:Green box
    display_set_fg_color(GREEN_COLOR);
    }    
    else 
    {//Stop mode:Red box 
    display_set_fg_color(RED_COLOR);
    }
  }
  else
  {
    //Orange color for activated button
    display_set_fg_color(ITEM_ACTIVE_COLOR);
  }

  //Draw the body of the button
  display_fill_rounded_rect(739, 65, 51, 50, 2);

  //Draw the edge
  display_set_fg_color(0x00505050);
  display_draw_rounded_rect(739, 65, 51, 50, 2);
        
  //Check if inactive or active mode
  if(mode == 0)
  {
      //Check if run or stop mode - Run mode: black text 
    if(scopesettings.runstate) display_set_fg_color(BLACK_COLOR);//ok
          //Stop mode: white text
          else display_set_fg_color(WHITE_COLOR);  
  }
  else
  {
    //Black text color for activated button
    display_set_fg_color(BLACK_COLOR);
  }

  //Display the text
  display_set_font(&font_3);
  display_text(746, 75, "RUN/");
  display_text(746, 90, "STOP");
}

//----------------------------------------------------------------------------------------------------------------------------------

void scope_auto_set_button(int mode)
{
  //Check if inactive or active mode
  if(mode == 0)
  {
    //Black color for inactive button
    display_set_fg_color(DARKGREY_COLOR);  //black?0x00202020
  }
  else
  {
    //Orange color for activated button
    display_set_fg_color(ITEM_ACTIVE_COLOR);
  }

  //Draw the body of the button
  display_fill_rounded_rect(739, 125, 51, 50, 2);

  //Draw the edge
  display_set_fg_color(0x00505050);
  display_draw_rounded_rect(739, 125, 51, 50, 2);

  //Check if inactive or active mode
  if(mode == 0)
  {
    //White text color for inactive button
    display_set_fg_color(WHITE_COLOR);
  }
  else
  {
    //Black text color for activated button
    display_set_fg_color(BLACK_COLOR);
  }

  //Display the text
  display_set_font(&font_3);
  display_text(746, 135, "AUTO");
  display_text(753, 150, "SET");
}

//----------------------------------------------------------------------------------------------------------------------------------

void scope_next_wave_button(int mode)
{
  //Check if inactive or active mode
  if(mode == 0)
  {
    //Black color for inactive button
    display_set_fg_color(DARKGREY_COLOR);
  }
  else
  {
    //Orange color for activated button
    display_set_fg_color(ITEM_ACTIVE_COLOR);
  }

  //Draw the body of the button
  display_fill_rounded_rect(739, 65, 51, 50, 2);

  //Draw the edge
  display_set_fg_color(0x00505050);
  display_draw_rounded_rect(739, 65, 51, 50, 2);

  //Check if inactive or active mode
  if(mode == 0)
  {
    //White text color for inactive button
    display_set_fg_color(WHITE_COLOR);
  }
  else
  {
    //Black text color for activated button
    display_set_fg_color(BLACK_COLOR);
  }

  //Display the text
  display_set_font(&font_3);
  display_text(749, 75, "Next");
  display_text(748, 90, "wave");
}

//----------------------------------------------------------------------------------------------------------------------------------

void scope_previous_wave_button(int mode)
{
  //Check if inactive or active mode
  if(mode == 0)
  {
    //Black color for inactive button
    display_set_fg_color(DARKGREY_COLOR);
  }
  else
  {
    //Orange color for activated button
    display_set_fg_color(ITEM_ACTIVE_COLOR);
  }

  //Draw the body of the button
  display_fill_rounded_rect(739, 125, 51, 50, 2);
  
  //Draw the edge
  display_set_fg_color(0x00505050);
  display_draw_rounded_rect(739, 125, 51, 50, 2);

  //Check if inactive or active mode
  if(mode == 0)
  {
    //White text color for inactive button
    display_set_fg_color(WHITE_COLOR);
  }
  else
  {
    //Black text color for activated button
    display_set_fg_color(BLACK_COLOR);
  }

  //Display the text
  display_set_font(&font_3);
  display_text(749, 135, "Prev");
  display_text(748, 150, "wave");
}

//----------------------------------------------------------------------------------------------------------------------------------

void scope_t_cursor_button(int mode)
{
  //Check if inactive or active mode
  if(mode == 0)
  {
    //Black color for inactive button
    display_set_fg_color(DARKGREY_COLOR);
  }
  else
  {
    //Orange color for activated button
    display_set_fg_color(ITEM_ACTIVE_COLOR);
  }

  //Draw the body of the button
  display_fill_rounded_rect(739, 185, 51, 50, 2);

  //Draw the edge
  display_set_fg_color(0x00505050);
  display_draw_rounded_rect(739, 185, 51, 50, 2);

  //Check if inactive or active mode
  if(mode == 0)
  {
    //White text color for inactive button
    display_set_fg_color(WHITE_COLOR);
  }
  else
  {
    //Black text color for activated button
    display_set_fg_color(BLACK_COLOR);
  }

  //Display the text
  display_set_font(&font_3);
  display_text(746, 195, "T");
  display_text(765, 195, "CU");
  display_text(746, 210, "RSOR");

}

//----------------------------------------------------------------------------------------------------------------------------------

void scope_v_cursor_button(int mode)
{
  //Check if inactive or active mode
  if(mode == 0)
  {
    //Black color for inactive button
    display_set_fg_color(DARKGREY_COLOR);
  }
  else
  {
    //Orange color for activated button
    display_set_fg_color(ITEM_ACTIVE_COLOR);
  }

  //Draw the body of the button
  display_fill_rounded_rect(739, 245, 51, 50, 2);

  //Draw the edge
  display_set_fg_color(0x00505050);
  display_draw_rounded_rect(739, 245, 51, 50, 2);

  //Check if inactive or active mode
  if(mode == 0)
  {
    //White text color for inactive button
    display_set_fg_color(WHITE_COLOR);
  }
  else
  {
    //Black text color for activated button
    display_set_fg_color(BLACK_COLOR);
  }

  //Display the text
  display_set_font(&font_3);
  display_text(746, 255, "V");
  display_text(765, 255, "CU");
  display_text(746, 270, "RSOR");
  
}

//----------------------------------------------------------------------------------------------------------------------------------

void scope_measures_button(int mode)
{
  //Check if inactive or active mode
  if(mode == 0)
  {
    //Black color for inactive button
    display_set_fg_color(DARKGREY_COLOR);
  }
  else
  {
    //Orange color for activated button
    display_set_fg_color(ITEM_ACTIVE_COLOR);
  }

  //Draw the body of the button
  display_fill_rounded_rect(739, 305, 51, 50, 2);

  //Draw the edge
  display_set_fg_color(0x00505050);
  display_draw_rounded_rect(739, 305, 51, 50, 2);

  //Check if inactive or active mode
  if(mode == 0)
  {
    //White text color for inactive button
    display_set_fg_color(WHITE_COLOR);
  }
  else
  {
    //Black text color for activated button
    display_set_fg_color(BLACK_COLOR);
  }

  //Display the text
  display_set_font(&font_3);
  display_text(746, 315, "MEAS");
  display_text(746, 330, "URES");
}

//----------------------------------------------------------------------------------------------------------------------------------

void scope_save_picture_button(int mode)
{
  //Check if inactive or active mode
  if(mode == 0)
  {
    //Black color for inactive button
    display_set_fg_color(DARKGREY_COLOR);
  }
  else
  {
    //Orange color for activated button
    display_set_fg_color(ITEM_ACTIVE_COLOR);
  }

  //Draw the body of the button
  display_fill_rounded_rect(739, 365, 51, 50, 2);

  //Draw the edge
  display_set_fg_color(0x00505050);
  display_draw_rounded_rect(739, 365, 51, 50, 2);

  //Check if inactive or active mode
  if(mode == 0)
  {
    //White text color for inactive button
    display_set_fg_color(WHITE_COLOR);
  }
  else
  {
    //Black text color for activated button
    display_set_fg_color(BLACK_COLOR);
  }

  //Display the text
  display_set_font(&font_3);
  display_text(746, 375, "SAVE");
  display_text(753, 390, "PIC");
}

//----------------------------------------------------------------------------------------------------------------------------------

void scope_save_wave_button(int mode)
{
  //Check if inactive or active mode
  if(mode == 0)
  {
    //Black color for inactive button
    display_set_fg_color(DARKGREY_COLOR);
  }
  else
  {
    //Orange color for activated button
    display_set_fg_color(ITEM_ACTIVE_COLOR);
  }

  //Draw the body of the button
  display_fill_rounded_rect(739, 425, 51, 50, 2);

  //Draw the edge
  display_set_fg_color(0x00505050);
  display_draw_rounded_rect(739, 425, 51, 50, 2);

  //Check if inactive or active mode
  if(mode == 0)
  {
    //White text color for inactive button
    display_set_fg_color(WHITE_COLOR);
  }
  else
  {
    //Black text color for activated button
    display_set_fg_color(BLACK_COLOR);
  }

  //Display the text
  display_set_font(&font_3);
  display_text(746, 435, "SAVE");
  display_text(746, 450, "WAVE");
}

//----------------------------------------------------------------------------------------------------------------------------------

void scope_delete_wave_button(int mode)
{
  //Check if inactive or active mode
  if(mode == 0)
  {
    //Black color for inactive button
    display_set_fg_color(DARKGREY_COLOR);
  }
  else
  {
    //Orange color for activated button
    display_set_fg_color(ITEM_ACTIVE_COLOR);
  }

  //Draw the body of the button
  display_fill_rounded_rect(739, 425, 51, 50, 2);

  //Draw the edge
  display_set_fg_color(0x00505050);
  display_draw_rounded_rect(739, 425, 51, 50, 2);

  //Check if inactive or active mode
  if(mode == 0)
  {
    //White text color for inactive button
    display_set_fg_color(WHITE_COLOR);
  }
  else
  {
    //Black text color for activated button
    display_set_fg_color(BLACK_COLOR);
  }

  //Display the text
  display_set_font(&font_3);
  display_text(745, 436, "Delete");
  display_text(748, 449, "wave");
}

//----------------------------------------------------------------------------------------------------------------------------------

void scope_50_percent_trigger_button(int mode)
{
  //Check if inactive or active mode
  if(mode == 0)
  {
    //Black color for inactive button
    display_set_fg_color(DARKGREY_COLOR);
  }
  else
  {
    //Orange color for activated button
    display_set_fg_color(ITEM_ACTIVE_COLOR);
  }

  //Draw the body of the button
  display_fill_rounded_rect(739, 425, 51, 50, 2);

  //Draw the edge
  display_set_fg_color(0x00505050);
  display_draw_rounded_rect(739, 425, 51, 50, 2);

  //Check if inactive or active mode
  if(mode == 0)
  {
    //White text color for inactive button
    display_set_fg_color(WHITE_COLOR);
  }
  else
  {
    //Black text color for activated button
    display_set_fg_color(BLACK_COLOR);
  }

  //Display the text
  display_set_font(&font_3);
  display_text(753, 435, "50%");//752 436
  display_text(749, 450, "TRIG");//449
}

//----------------------------------------------------------------------------------------------------------------------------------

void scope_show_grid_button(int mode)      
{
  //Check if inactive or active mode
  if(mode == 0)
  {
    //Black color for inactive button
    display_set_fg_color(DARKGREY_COLOR);
  }
  else
  {
    //Orange color for activated button
    display_set_fg_color(ITEM_ACTIVE_COLOR);
  }

  //Draw the body of the button
  display_fill_rounded_rect(739, 425, 51, 50, 2);

  //Draw the edge
  display_set_fg_color(0x00505050);
  display_draw_rounded_rect(739, 425, 51, 50, 2);

  //Check if inactive or active mode
  if(mode == 0)
  {
    //White text color for inactive button
    display_set_fg_color(WHITE_COLOR);
  }
  else
  {
    //Black text color for activated button
    display_set_fg_color(BLACK_COLOR);
  }

  //Display the text
  display_set_font(&font_3);
  display_text(746, 436, "Show");
  display_text(752, 449, "grid");
}
//----------------------------------------------------------------------------------------------------------------------------------

void scope_next_cal_button(int mode)      
{
  //Check if inactive or active mode
  if(mode == 0)
  {
    //Darkgray color for inactive button
    display_set_fg_color(DARKGREY_COLOR);
  }
  else
  {
    //Orange color for activated button
    display_set_fg_color(ITEM_ACTIVE_COLOR);
  }

  //Draw the body of the button
  display_fill_rounded_rect(739, 425, 51, 50, 2);

  //Draw the edge
  display_set_fg_color(0x00505050);
  display_draw_rounded_rect(739, 425, 51, 50, 2);

  //Check if inactive or active mode
  if(mode == 0)
  {
    //White text color for inactive button
    display_set_fg_color(RED_COLOR);        //white
  }
  else
  {
    //Black text color for activated button
    display_set_fg_color(BLACK_COLOR);
  }

  //Display the text
  display_set_font(&font_3);
  display_text(748, 436, "Next");
  display_text(752, 449, "page");
}
//----------------------------------------------------------------------------------------------------------------------------------

void scope_end_cal_button(int mode)      
{
  //Check if inactive or active mode
  if(mode == 0)
  {
    //Darkgray color for inactive button
    display_set_fg_color(DARKGREY_COLOR);
  }
  else
  {
    //Orange color for activated button
    display_set_fg_color(ITEM_ACTIVE_COLOR);
  }

  //Draw the body of the button
  display_fill_rounded_rect(739, 425, 51, 50, 2);

  //Draw the edge
  display_set_fg_color(0x00505050);
  display_draw_rounded_rect(739, 425, 51, 50, 2);

  //Check if inactive or active mode
  if(mode == 0)
  {
    //White text color for inactive button
    display_set_fg_color(GREEN_COLOR);        //white
  }
  else
  {
    //Black text color for activated button
    display_set_fg_color(BLACK_COLOR);
  }

  //Display the text
  display_set_font(&font_3);
  display_text(751, 436, "END");    //748
  display_text(752, 449, "cal.");
}
//----------------------------------------------------------------------------------------------------------------------------------

void scope_channel_sensitivity_control(PCHANNELSETTINGS settings, int type, int mode)
{
  uint32 y;

  //Check if V+ is active or inactive
  if((type == 0) && (mode != 0))
  {
    //Orange color for activated button
    display_set_fg_color(ITEM_ACTIVE_COLOR);
  }
  else
  {
    //Black color for inactive button
    display_set_fg_color(DARKGREY_COLOR);
  }

  //Top button
  display_fill_rounded_rect(739, settings->voltdivypos, 51, 50, 2);
  
  //Draw the edge
  display_set_fg_color(0x00505050);
  display_draw_rounded_rect(739, settings->voltdivypos, 51, 50, 2);

  //Check if V- is active or inactive
  if((type != 0) && (mode != 0))
  {
    //Orange color for activated button
    display_set_fg_color(ITEM_ACTIVE_COLOR);
  }
  else
  {
    //Black color for inactive button
    display_set_fg_color(DARKGREY_COLOR);
  }

  //Y position o f bottom button
  y = settings->voltdivypos + 86;
  
  //Bottom button
  display_fill_rounded_rect(739, y, 51, 50, 2);

  //Draw the edge
  display_set_fg_color(0x00505050);
  display_draw_rounded_rect(739, y, 51, 50, 2);
  
  //Display V+ and V- the text in larger font
  display_set_font(&font_0);
  
  //Check if V+ is active or inactive
  if((type == 0) && (mode != 0))
  {
    //Black text color for activated button
    display_set_fg_color(BLACK_COLOR);
  }
  else
  {
    //White text color for inactive button
    display_set_fg_color(WHITE_COLOR);
  }
  
  //Top button text
  display_text(757, settings->voltdivypos + 18, "V+");

  //Check if V- is active or inactive
  if((type != 0) && (mode != 0))
  {
    //Black text color for activated button
    display_set_fg_color(BLACK_COLOR);
  }
  else
  {
    //White text color for inactive button
    display_set_fg_color(WHITE_COLOR);
  }
  
  //Bottom button text
  display_text(757, settings->voltdivypos + 104, "V-");

  //Display the channel identifier bar with the channel color
  display_set_fg_color(settings->color);
  display_fill_rounded_rect(739, settings->voltdivypos + 56, 51, 24, 2);

  //Display the channel identifier in black
  display_set_fg_color(BLACK_COLOR);
  display_set_font(&font_2);
  display_text(754, settings->voltdivypos + 61, settings->buttontext);
}

//----------------------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------------------

void scope_channel_dcshift_value(void)
{

  //Black color for inactive button
  display_set_fg_color(BLACK_COLOR);
  display_fill_rect(755,  83, 50, 15);//99
  display_fill_rect(755, 248, 50, 15);//264

  display_set_font(&font_2);

  if(scopesettings.rightmenustate == 2)
  {
    //channel color for text
    display_set_fg_color(CHANNEL1_COLOR);
    display_decimal(755,  83, scopesettings.channel1.dc_shift_center);
    //channel color for text
    display_set_fg_color(CHANNEL2_COLOR);
    display_decimal(755, 248, scopesettings.channel2.dc_shift_center);
  }
  
    if(scopesettings.rightmenustate == 3)
  {
    //channel color for text
    display_set_fg_color(CHANNEL1_COLOR);
    display_decimal(755,  83, scopesettings.channel1.dc_shift_size);
    //channel color for text
    display_set_fg_color(CHANNEL2_COLOR);
    display_decimal(755, 248, scopesettings.channel2.dc_shift_size);
  }
  
    if(scopesettings.rightmenustate == 4)
  {
    //channel color for text
    display_set_fg_color(CHANNEL1_COLOR);
    display_decimal(755,  83, scopesettings.channel1.dc_shift_value);
    //channel color for text
    display_set_fg_color(CHANNEL2_COLOR);
    display_decimal(755, 248, scopesettings.channel2.dc_shift_value);
  }

}

//----------------------------------------------------------------------------------------------------------------------------------

void scope_return_button(int mode)
{
  //Check if inactive or active mode
  if(mode == 0)
  {
    //Black color for inactive button
    display_set_fg_color(LIGHTGREY_COLOR);
  }
  else
  {
    //Active color for activated button
    display_set_fg_color(ITEM_ACTIVE_COLOR);
  }

  //Draw the body of the button
  display_fill_rounded_rect(734, 14, 63, 58, 2);

  //Outline the button
  display_set_fg_color(0x00606060);
  display_draw_rounded_rect(734, 14, 63, 58, 2);
  
  //Check if inactive or active mode
  if(mode == 0)
  {
    //White text color for inactive button
    display_set_fg_color(WHITE_COLOR);
  }
  else
  {
    //Black text color for activated button
    display_set_fg_color(BLACK_COLOR);
  }

  //Display the text
  display_set_font(&font_3);
  display_text(745, 34, "Return");//747
}

//----------------------------------------------------------------------------------------------------------------------------------

void scope_select_all_button(int mode)
{
  //Check if inactive or active mode
  if(mode == 0)
  {
    //Black color for inactive button
    display_set_fg_color(LIGHTGREY_COLOR);
  }
  else if(mode == 1)
  {
    //Active color for activated button
    display_set_fg_color(ITEM_ACTIVE_COLOR);
  }
  else
  {
    //White for enabled button
    display_set_fg_color(WHITE_COLOR);
  }

  //Draw the body of the button
  display_fill_rounded_rect(734, 93, 63, 58, 2);

  //Outline the button
  display_set_fg_color(0x00606060);
  display_draw_rounded_rect(734, 93, 63, 58, 2);
  
  //Check if inactive or active mode
  if(mode == 0)
  {
    //White text color for inactive button
    display_set_fg_color(WHITE_COLOR);
  }
  else
  {
    //Black text color for activated button
    display_set_fg_color(BLACK_COLOR);
  }

  //Display the text
  display_set_font(&font_3);
  display_text(746, 106, "Select");
  display_text(758, 120, "all");
}

//----------------------------------------------------------------------------------------------------------------------------------

void scope_select_button(int mode)
{
  //Check if inactive or active mode
  if(mode == 0)
  {
    //Black color for inactive button
    display_set_fg_color(LIGHTGREY_COLOR);
  }
  else if(mode == 1)
  {
    //Active color for activated button
    display_set_fg_color(ITEM_ACTIVE_COLOR);
  }
  else
  {
    //White for enabled button
    display_set_fg_color(WHITE_COLOR);
  }

  //Draw the body of the button
  display_fill_rounded_rect(734, 173, 63, 58, 2);

  //Outline the button
  display_set_fg_color(0x00606060);
  display_draw_rounded_rect(734, 173, 63, 58, 2);

  //Check if inactive or active mode
  if(mode == 0)
  {
    //White text color for inactive button
    display_set_fg_color(WHITE_COLOR);
  }
  else
  {
    //Black text color for activated button
    display_set_fg_color(BLACK_COLOR);
  }

  //Display the text
  display_set_font(&font_3);
  display_text(746, 193, "Select");
}

//----------------------------------------------------------------------------------------------------------------------------------

void scope_delete_button(int mode)
{
  //Check if inactive or active mode
  if(mode == 0)
  {
    //Black color for inactive button
    display_set_fg_color(LIGHTGREY_COLOR);
  }
  else
  {
    //Active color for activated button
    display_set_fg_color(ITEM_ACTIVE_COLOR);
  }

  //Draw the body of the button
  display_fill_rounded_rect(734, 253, 63, 58, 2);

  //Outline the button
  display_set_fg_color(0x00606060);
  display_draw_rounded_rect(734, 253, 63, 58, 2);

  //Check if inactive or active mode
  if(mode == 0)
  {
    //White text color for inactive button
    display_set_fg_color(WHITE_COLOR);
  }
  else
  {
    //Black text color for activated button
    display_set_fg_color(BLACK_COLOR);
  }

  //Display the text
  display_set_font(&font_3);
  display_text(746, 273, "Delete");
}

//----------------------------------------------------------------------------------------------------------------------------------

void scope_page_up_button(int mode)
{
  //Check if inactive or active mode
  if(mode == 0)
  {
    //Black color for inactive button
    display_set_fg_color(LIGHTGREY_COLOR);
  }
  else
  {
    //Active color for activated button
    display_set_fg_color(ITEM_ACTIVE_COLOR);
  }

  //Draw the body of the button
  display_fill_rounded_rect(734, 333, 63, 58, 2);

  //Outline the button
  display_set_fg_color(0x00606060);
  display_draw_rounded_rect(734, 333, 63, 58, 2);

  //Check if inactive or active mode
  if(mode == 0)
  {
    //White text color for inactive button
    display_set_fg_color(WHITE_COLOR);
  }
  else
  {
    //Black text color for activated button
    display_set_fg_color(BLACK_COLOR);
  }

  //Display the text
  display_set_font(&font_3);
  display_text(750, 345, "Page");
  display_text(758, 360, "up");
}

//----------------------------------------------------------------------------------------------------------------------------------

void scope_page_down_button(int mode)
{
  //Check if inactive or active mode
  if(mode == 0)
  {
    //Black color for inactive button
    display_set_fg_color(LIGHTGREY_COLOR);
  }
  else
  {
    //Active color for activated button
    display_set_fg_color(ITEM_ACTIVE_COLOR);
  }

  //Draw the body of the button
  display_fill_rounded_rect(734, 413, 63, 58, 2);

  //Outline the button
  display_set_fg_color(0x00606060);
  display_draw_rounded_rect(734, 413, 63, 58, 2);

  //Check if inactive or active mode
  if(mode == 0)
  {
    //White text color for inactive button
    display_set_fg_color(WHITE_COLOR);
  }
  else
  {
    //Black text color for activated button
    display_set_fg_color(BLACK_COLOR);
  }

  //Display the text
  display_set_font(&font_3);
  display_text(750, 425, "Page");
  display_text(748, 442, "down");
}

//----------------------------------------------------------------------------------------------------------------------------------
// Bitmap control bar functions
//----------------------------------------------------------------------------------------------------------------------------------

void scope_bmp_return_button(int mode)
{
  //Check if inactive or active
  if(mode == 0)
  {
    //Inactive so black background
    display_set_fg_color(LIGHTGREY_COLOR);
  }
  else
  {
    //Active so active color background
    display_set_fg_color(ITEM_ACTIVE_COLOR);
  }

  //Fill in the body of the button
  display_fill_rounded_rect(40, 425, 120, 50, 3);

  //Draw rounded rectangle as button border in black
  display_set_fg_color(BLACK_COLOR);
  display_draw_rounded_rect(40, 425, 120, 50, 3);

  //Check if inactive or active
  if(mode == 0)
  {
    //Inactive so white foreground and grey background
    display_set_fg_color(WHITE_COLOR);
    display_set_bg_color(LIGHTGREY_COLOR);
  }
  else
  {
    //Active so black foreground and active color background
    display_set_fg_color(LIGHTGREY_COLOR);
    display_set_bg_color(ITEM_ACTIVE_COLOR);
  }

  //Display the icon with the set colors
  display_copy_icon_use_colors(return_arrow_icon, 79, 436, 41, 27);
}

//----------------------------------------------------------------------------------------------------------------------------------

void scope_bmp_delete_button(int mode)
{
  //Check if inactive or active
  if(mode == 0)
  {
    //Inactive so black background
    display_set_fg_color(LIGHTGREY_COLOR);
  }
  else
  {
    //Active so active color background
    display_set_fg_color(ITEM_ACTIVE_COLOR);
  }

  //Fill in the body of the button
  display_fill_rounded_rect(240, 425, 120, 50, 3);

  //Draw rounded rectangle as button border in black
  display_set_fg_color(BLACK_COLOR);
  display_draw_rounded_rect(240, 425, 120, 50, 3);

  //Check if inactive or active
  if(mode == 0)
  {
    //Inactive so white foreground and grey background
    display_set_fg_color(WHITE_COLOR);
    display_set_bg_color(LIGHTGREY_COLOR);
  }
  else
  {
    //Active so black foreground and active color background
    display_set_fg_color(LIGHTGREY_COLOR);
    display_set_bg_color(ITEM_ACTIVE_COLOR);
  }

  //Display the icon with the set colors
  display_copy_icon_use_colors(waste_bin_icon, 284, 433, 31, 33);
}

//----------------------------------------------------------------------------------------------------------------------------------

void scope_bmp_previous_button(int mode)
{
  //Check if inactive or active
  if(mode == 0)
  {
    //Inactive so black background
    display_set_fg_color(LIGHTGREY_COLOR);
  }
  else
  {
    //Active so active color background
    display_set_fg_color(ITEM_ACTIVE_COLOR);
  }

  //Fill in the body of the button
  display_fill_rounded_rect(440, 425, 120, 50, 3);

  //Draw rounded rectangle as button border in black
  display_set_fg_color(BLACK_COLOR);
  display_draw_rounded_rect(440, 425, 120, 50, 3);

  //Check if inactive or active
  if(mode == 0)
  {
    //Inactive so white foreground and grey background
    display_set_fg_color(WHITE_COLOR);
    display_set_bg_color(LIGHTGREY_COLOR);
  }
  else
  {
    //Active so black foreground and active color background
    display_set_fg_color(LIGHTGREY_COLOR);
    display_set_bg_color(ITEM_ACTIVE_COLOR);
  }

  //Display the icon with the set colors
  display_copy_icon_use_colors(previous_picture_icon, 483, 438, 33, 24);
}

//----------------------------------------------------------------------------------------------------------------------------------

void scope_bmp_next_button(int mode)
{
  //Check if inactive or active
  if(mode == 0)
  {
    //Inactive so black background
    display_set_fg_color(LIGHTGREY_COLOR);
  }
  else
  {
    //Active so active color background
    display_set_fg_color(ITEM_ACTIVE_COLOR);
  }

  //Fill in the body of the button
  display_fill_rounded_rect(640, 425, 120, 50, 3);

  //Draw rounded rectangle as button border in black
  display_set_fg_color(BLACK_COLOR);
  display_draw_rounded_rect(640, 425, 120, 50, 3);

  //Check if inactive or active
  if(mode == 0)
  {
    //Inactive so white foreground and grey background
    display_set_fg_color(WHITE_COLOR);
    display_set_bg_color(LIGHTGREY_COLOR);
  }
  else
  {
    //Active so black foreground and active color background
    display_set_fg_color(LIGHTGREY_COLOR);
    display_set_bg_color(ITEM_ACTIVE_COLOR);
  }

  //Display the icon with the set colors
  display_copy_icon_use_colors(next_picture_icon, 683, 438, 33, 24);
}

//----------------------------------------------------------------------------------------------------------------------------------
// Top bar function
//----------------------------------------------------------------------------------------------------------------------------------

void scope_menu_button(int mode)
{
  //Check if inactive or active
  if(mode == 0)
  {
    //Inactive so dark blue background
    display_set_fg_color(DARKBLUE_COLOR);//78
  }
  else
  {
    //Active so pale yellow background
    display_set_fg_color(PALEYELLOW_COLOR);
  }

  //Draw the background
  display_fill_rounded_rect(0, 0, 80, 38, 3);

  //Check if inactive or active
  if(mode == 0)
  {
    //Inactive so white foreground
    display_set_fg_color(WHITE_COLOR);
  }
  else
  {
    //Active so black foreground
    display_set_fg_color(BLACK_COLOR);
  }

  //Draw the menu symbol
  display_fill_rect(6,  11, 7, 7);
  display_fill_rect(15, 11, 7, 7);
  display_fill_rect(6,  20, 7, 7);
  display_fill_rect(15, 20, 7, 7);

  //Display the text
  display_set_font(&font_3);
  display_text(32, 11, "MENU");
}

//----------------------------------------------------------------------------------------------------------------------------------

void scope_main_return_button(int mode)
{
  //Check if inactive or active
  if(mode == 0)
  {
    //Inactive so black background
    display_set_fg_color(BLACK_COLOR);
  }
  else
  {
    //Active so white background
    display_set_fg_color(WHITE_COLOR);
  }

  //Draw the background
  display_fill_rect(0, 0, 80, 38);

  //Check if inactive or active
  if(mode == 0)
  {
    //Inactive so white foreground and grey background
    display_set_fg_color(WHITE_COLOR);
    display_set_bg_color(BLACK_COLOR);
  }
  else
  {
    //Active so black foreground and white background
    display_set_fg_color(BLACK_COLOR);
    display_set_bg_color(WHITE_COLOR);
  }

  //Display the icon with the set colors
  display_copy_icon_use_colors(return_arrow_icon, 20, 5, 41, 27);
}

//----------------------------------------------------------------------------------------------------------------------------------

void scope_run_stop_text(void)
{
  //Check if run or stop mode
  if(scopesettings.runstate)//ok
  {
    //Run mode. Green box
    if((scopesettings.triggermode == 1)&&(!triggerlong))
        display_set_fg_color(ORANGE_COLOR); 
        else display_set_fg_color(GREEN_COLOR);    
  }
  else
  {
    //Stop mode. Red box
    display_set_fg_color(RED_COLOR);
  }
  
  //Fill the box
  display_fill_rounded_rect(RUN_STOP_TEXT_XPOS, RUN_STOP_TEXT_YPOS, RUN_STOP_TEXT_WIDTH, RUN_STOP_TEXT_HEIGHT, 2);

  //Select the font for the text
  display_set_font(&font_3);

  //Check if run or stop mode
  if(scopesettings.runstate)//ok
  {
    //Run mode. Black text 
    display_set_fg_color(BLACK_COLOR);
    if((scopesettings.triggermode==1)&&(!triggerlong))
        display_text(RUN_STOP_TEXT_XPOS + 3, RUN_STOP_TEXT_YPOS + 1, "WAIT");
        else display_text(RUN_STOP_TEXT_XPOS + 5, RUN_STOP_TEXT_YPOS + 1, "RUN");
  }
  else
  {
    //Stop mode. White text
    display_set_fg_color(WHITE_COLOR);
    display_text(RUN_STOP_TEXT_XPOS + 1, RUN_STOP_TEXT_YPOS + 1, "STOP");
  }
}

//----------------------------------------------------------------------------------------------------------------------------------

void scope_channel_settings(PCHANNELSETTINGS settings, int mode)
{
  int8 **vdtext = 0;

  //Clear the area first
  display_set_fg_color(BLACK_COLOR);
  display_fill_rounded_rect(settings->buttonxpos, CH_BUTTON_YPOS, CH_BUTTON_BG_WIDTH, CH_BUTTON_BG_HEIGHT, 2);

  //Check if channel is enabled or disabled
  if(settings->enable == 0)
  {
    //Disabled so off colors
    //Check if inactive or active
    if(mode == 0)
    {
      //Inactive, grey menu button
      display_set_fg_color(GREY_COLOR);
    }
    else
    {
      //Active, light grey menu button
      display_set_fg_color(0x00BBBBBB);
    }
  }
  else
  {
    //Enabled so on colors
    //Check if inactive or active
    if(mode == 0)
    {
      //Inactive, channel 1 color menu button (inactive & invert channel = black button)
      if (settings->invert) { display_set_fg_color(BLACK_COLOR);}
        else { display_set_fg_color(settings->color);  }// ak invert black
    }
    else
    {
      //Active, blue menu button
      display_set_fg_color(settings->touchedcolor);
    }
  }

  //Fill the button
  display_fill_rounded_rect(settings->buttonxpos, CH_BUTTON_YPOS, CH_BUTTON_WIDTH, CH_BUTTON_HEIGHT, 2);

  //Select the font for the text
  display_set_font(&font_2);

  //Check if inactive or active
  if(mode == 0)
  {
    //Inactive, black text (inactive & invert channel = yellow text)
    if ((settings->invert)&&(settings->enable)) {display_set_fg_color(settings->color); display_draw_rounded_rect(settings->buttonxpos, CH_BUTTON_YPOS, CH_BUTTON_WIDTH, CH_BUTTON_HEIGHT, 2);}
        else display_set_fg_color(BLACK_COLOR);//zlty text ak inver kanal
  }
  else
  {
    //Active, white text
    display_set_fg_color(WHITE_COLOR);

    //Fill the settings background
    display_fill_rounded_rect(settings->buttonxpos + 30, CH_BUTTON_YPOS, CH_BUTTON_BG_WIDTH - 30, CH_BUTTON_BG_HEIGHT, 2);
  }

  //Display the channel identifier text
  display_text(settings->buttonxpos + 5, CH_BUTTON_YPOS + 11, settings->buttontext);

  //Check if inactive or active
  if(mode == 0)
  {
    //Inactive, white text
    display_set_fg_color(WHITE_COLOR);
  }
  else
  {
    //Active, black text
    display_set_fg_color(BLACK_COLOR);
  }

  //Check on which coupling is set
  if(settings->coupling == 0)
  {
    //DC coupling
    display_text(settings->buttonxpos + 38, CH_BUTTON_YPOS + 3, "DC");
  }
  else
  {
    //AC coupling
    display_text(settings->buttonxpos + 38, CH_BUTTON_YPOS + 3, "AC");
  }

  //Print the probe magnification factor
  switch(settings->magnification)
  {
    case 0:
      //Times 0.5 magnification
      display_text(settings->buttonxpos + 63, CH_BUTTON_YPOS + 3, "0.5X");

      //Set the (volts per div or ampere per div) text range to be used for this magnification
      if(settings->V_A) vdtext = (int8 **)ampere_div_texts[0]; else vdtext = (int8 **)volt_div_texts[0];
      break;

    case 1:
      //Times 1 magnification
      display_text(settings->buttonxpos + 61, CH_BUTTON_YPOS + 3, "1X");

      //Set the volts per div text range to be used for this magnification
      if(settings->V_A) vdtext = (int8 **)ampere_div_texts[1]; else vdtext = (int8 **)volt_div_texts[1];
      break;
      
    case 2:
      //Times 10 magnification
      display_text(settings->buttonxpos + 61, CH_BUTTON_YPOS + 3, "10X");

      //Set the volts per div text range to be used for this magnification
      if(settings->V_A) vdtext = (int8 **)ampere_div_texts[2]; else vdtext = (int8 **)volt_div_texts[2];
      break;
      
    case 3:
      //Times 20 magnification
      display_text(settings->buttonxpos + 61, CH_BUTTON_YPOS + 3, "20X");

      //Set the volts per div text range to be used for this magnification
      if(settings->V_A) vdtext = (int8 **)ampere_div_texts[3]; else vdtext = (int8 **)volt_div_texts[3];
      break;
      
    case 4:
      //Times 50 magnification
      display_text(settings->buttonxpos + 61, CH_BUTTON_YPOS + 3, "50X");

      //Set the volts per div text range to be used for this magnification
      if(settings->V_A) vdtext = (int8 **)ampere_div_texts[4]; else vdtext = (int8 **)volt_div_texts[4];
      break;
      
    case 5:
      //Times 100 magnification
      display_text(settings->buttonxpos + 61, CH_BUTTON_YPOS + 3, "100X");

      //Set the volts per div text range to be used for this magnification
      if(settings->V_A) vdtext = (int8 **)ampere_div_texts[5]; else vdtext = (int8 **)volt_div_texts[5];
      break;
      
    case 6:
      //Times 1000 magnification
      display_text(settings->buttonxpos + 61, CH_BUTTON_YPOS + 3, "1000X");

      //Set the volts per div text range to be used for this magnification
      if(settings->V_A) vdtext = (int8 **)ampere_div_texts[6]; else vdtext = (int8 **)volt_div_texts[6];
      break;
      
    default: break;
      //Times 100 magnification
      //display_text(settings->buttonxpos + 59, CH_BUTTON_YPOS + 3, "1000X");

      //Set the volts per div text range to be used for this magnification
      //if(settings->V_A) vdtext = (int8 **)ampere_div_texts[2]; else vdtext = (int8 **)volt_div_texts[2];
      
  }

  //Display the sensitivity when in range
  if(settings->displayvoltperdiv < 7)
  {
    display_text(settings->buttonxpos + 38, CH_BUTTON_YPOS + 19, vdtext[settings->displayvoltperdiv]);
  }
}

//----------------------------------------------------------------------------------------------------------------------------------

void scope_acqusition_settings(int mode)
{
  uint8 a = 0;
  //Clear the area first
  display_set_fg_color(BLACK_COLOR);
  display_fill_rounded_rect(ACQ_BUTTON_XPOS, ACQ_BUTTON_YPOS, ACQ_BUTTON_BG_WIDTH, ACQ_BUTTON_BG_HEIGHT, 2);

  //Check if inactive or active
  if(mode == 0)
  {
    //Inactive, short time base green menu button, if long time base orange
    if (scopesettings.long_mode) display_set_fg_color(ORANGE_COLOR); else display_set_fg_color(GREEN_COLOR);
  }
  else
  {
    //Active, magenta menu button
    display_set_fg_color(MAGENTA_COLOR);
  }

  //Fill the button
  display_fill_rounded_rect(ACQ_BUTTON_XPOS, ACQ_BUTTON_YPOS, ACQ_BUTTON_WIDTH, ACQ_BUTTON_HEIGHT, 2);

  //Select the font for the text
  display_set_font(&font_2);

  //Check if inactive or active
  if(mode == 0)
  {
    //Inactive, black text
    display_set_fg_color(BLACK_COLOR);
  }
  else
  {
    //Active, white text
    display_set_fg_color(WHITE_COLOR);

    //Fill the settings background
    display_fill_rounded_rect(ACQ_BUTTON_XPOS + 30, ACQ_BUTTON_YPOS, ACQ_BUTTON_BG_WIDTH - 30, ACQ_BUTTON_BG_HEIGHT, 2);
  }

  //Display the acquisition identifier text
  if (scopesettings.long_mode)
  {
      display_text(ACQ_BUTTON_XPOS + 4, ACQ_BUTTON_YPOS + 3, "ACQ");        //long time base mode
      display_text(ACQ_BUTTON_XPOS + 2, ACQ_BUTTON_YPOS + 17, "Long"); 
  } 
  else 
  {
      if(scopesettings.average_mode) {display_text(ACQ_BUTTON_XPOS + 5, ACQ_BUTTON_YPOS + 17, "avg"); a=8;}    //Average mode 
      display_text(ACQ_BUTTON_XPOS + 4, ACQ_BUTTON_YPOS + 11-a, "ACQ");    //short time base mode
  } 
  
  //Check if inactive or active
  if(mode == 0)
  {
    //Inactive, white text
    display_set_fg_color(WHITE_COLOR);
  }
  else
  {
    //Active, black text
    display_set_fg_color(BLACK_COLOR);
  }

  //Only display the text when in range of the text array
  if(scopesettings.samplerate < (sizeof(acquisition_speed_texts) / sizeof(int8 *)))
  {
    //Display the text from the table
    display_text(ACQ_BUTTON_XPOS + 38, ACQ_BUTTON_YPOS + 3, (int8 *)acquisition_speed_texts[scopesettings.samplerate]);
  }

  //Only display the text when in range of the text array
  if(scopesettings.timeperdiv < (sizeof(time_div_texts) / sizeof(int8 *)))
  {
    //Display the text from the table
    display_text(ACQ_BUTTON_XPOS + 38, ACQ_BUTTON_YPOS + 19, (int8 *)time_div_texts[scopesettings.timeperdiv]);
  }
}

//----------------------------------------------------------------------------------------------------------------------------------

void scope_move_speed(int mode)
{
  //Check if inactive or active
  if(mode == 0)
  {
    //Inactive so dark blue background
    display_set_fg_color(DARKBLUE_COLOR);
  }
  else
  {
    //Active so pale yellow background
    display_set_fg_color(PALEYELLOW_COLOR);
  }

  //Draw the background
  display_fill_rounded_rect(487, 5, 40, 35, 2);//493 5 40 35

  //Check if inactive or active
  if(mode == 0)
  {
    //Inactive so white text
    display_set_fg_color(WHITE_COLOR);
  }
  else
  {
    //Active so black text
    display_set_fg_color(BLACK_COLOR);
  }

  //Select the font for the text
  display_set_font(&font_3);

  //Display the common text
  display_text(490, 6, "Move");//496 6
  
  //Check on which speed is set
  if(scopesettings.movespeed == 0)
  {
    display_text(496, 21, "fast");//502
  }
  else
  {
    display_text(493, 21, "slow");//499
  }
}

//----------------------------------------------------------------------------------------------------------------------------------

void scope_maxlight_item(int mode)
{
  if(mode == 0)
  {
    //Inactive so white foreground and grey background
    display_set_bg_color(BLACK_COLOR);
    display_set_fg_color(WHITE_COLOR);
  }
  else if(mode == 2)
  {
    //Active so black foreground and yellow background
    display_set_fg_color(BLACK_COLOR);
    //Active, magenta menu button
    display_set_bg_color(MAGENTA_COLOR);
     //display_set_fg_color(BLACK_COLOR);
    //display_set_bg_color(YELLOW_COLOR); 
  }
  else if(mode == 1)
  {
    //Active so black foreground and yellow background
    display_set_fg_color(YELLOW_COLOR);
    //Active, magenta menu button
    //display_set_fg_color(MAGENTA_COLOR);
    display_set_bg_color(BLACK_COLOR); 
  }
 
  //Display the icon with the set colors
  display_copy_icon_use_colors(light_icon, 532, 10, 24, 24);    //light-sun icon
}

//----------------------------------------------------------------------------------------------------------------------------------


void scope_set_maxlight(int mode)
{
  if(mode == 0)
  {
    scopesettings.maxlight = 0;   //flag set to 0 - no max light
    //Update the actual screen brightness
    fpga_set_translated_brightness();
  }
  else if(mode == 1)
  {
    scopesettings.maxlight = 1;   //flag set to 1 - max light
    //Update the screen brightness to max
    fpga_set_backlight_brightness(60000);   //max brightness
  }
}

//----------------------------------------------------------------------------------------------------------------------------------

void scope_trigger_settings(int mode)
{
  int8 *modetext = 0;

  //Clear the area first black
  display_set_fg_color(LIGHTGREY_COLOR);    //BLACK_COLOR
  display_fill_rounded_rect(560, 5, 80, 35, 2);

  //Check if inactive or active
  if(mode == 0)
  {
    //Inactive, green menu button
    display_set_fg_color(TRIGGER_COLOR);
  }
  else
  {
    //Active, magenta menu button
    display_set_fg_color(MAGENTA_COLOR);
  }

  //Fill the button
  display_fill_rounded_rect(560, 5, 31, 35, 2);

  //Select the font for the text
  display_set_font(&font_4);

  //Check if inactive or active
  if(mode == 0)
  {
    //Inactive, black text
    display_set_fg_color(BLACK_COLOR);
  }
  else
  {
    //Active, white text
    display_set_fg_color(WHITE_COLOR);

    //Fill the settings background
    display_fill_rounded_rect(591, 5, 48, 35, 2);
  }

  //Display the channel identifier text
  display_text(572, 15, "T");

  //Check if inactive or active
  if(mode == 0)
  {
    //Inactive, white text
    display_set_fg_color(WHITE_COLOR);
  }
  else
  {
    //Active, black text
    display_set_fg_color(BLACK_COLOR);
  }

  //Check on which trigger mode is set
  switch(scopesettings.triggermode)
  {
    case 0:
      modetext = "  Auto";
      break;

    case 1:
      modetext = " Single";
      break;

    case 2:
      modetext = "Normal";
      break;
  }

  //Select the font for the texts
  display_set_font(&font_2);

  //Check if valid setting
  if(modetext)
  {
    //Display the selected text if so
    display_text(596, 7, modetext);
  }

  //Draw the trigger edge symbol
  display_draw_vert_line(632, 27, 38);

  //Draw the arrow based on the selected edge
  if(scopesettings.triggeredge == 0)
  {
    //rising edge
    display_draw_horz_line(27, 632, 635);
    display_draw_horz_line(38, 629, 632);
    display_draw_horz_line(32, 631, 633);
    display_draw_horz_line(33, 630, 634);
  }
  else
  {
    //falling edge
    display_draw_horz_line(27, 629, 632);
    display_draw_horz_line(38, 632, 635);
    display_draw_horz_line(32, 630, 634);
    display_draw_horz_line(33, 631, 633);
  }

  //Check on which channel is used for triggering
  switch(scopesettings.triggerchannel)
  {
    //Channel 1
    case 0:
      //Check if inactive or active
      if(mode == 0)
      {
        //Inactive, dark channel 1 trigger color box
        display_set_fg_color(CHANNEL1_TRIG_COLOR);
      }
      else
      {
        //Active, some blue box
        display_set_fg_color(0x003333FF);
      }

      //Fill the channel background
      display_fill_rounded_rect(595, 25, 28, 15, 2);

      //Check if inactive or active
      if(mode == 0)
      {
        //Inactive, black text
        display_set_fg_color(BLACK_COLOR);
      }
      else
      {
        //Active, white text
        display_set_fg_color(WHITE_COLOR);
      }

      //Display the text
      display_text(598, 26, "CH1");
      break;

    //Channel 2
    case 1:
      //Check if inactive or active
      if(mode == 0)
      {
        //Inactive, dark cyan box
        display_set_fg_color(CHANNEL2_TRIG_COLOR);
      }
      else
      {
        //Active, some red box
        display_set_fg_color(0x00FF3333);
      }

      //Fill the channel background
      display_fill_rounded_rect(595, 25, 28, 15, 2);

      //Check if inactive or active
      if(mode == 0)
      {
        //Inactive, black text
        display_set_fg_color(BLACK_COLOR);
      }
      else
      {
        //Active, white text
        display_set_fg_color(WHITE_COLOR);
      }

      //Display the text
      display_text(598, 26, "CH2");
      break;
  }
}

//----------------------------------------------------------------------------------------------------------------------------------

void scope_battery_status(void)
{   
  uint32    batterychargelevel;
  uint32    batterycharging; 
  //uint32    xpos = 703;   //position text xxx % battery
  
  batterychargelevel = scopesettings.batterychargelevel;
  batterycharging = scopesettings.batterycharging;
  
  //Prepare the battery symbol in a working buffer to avoid flicker
  display_set_screen_buffer(displaybuffertmp);//1

  //Clear the background
  display_set_fg_color(BLACK_COLOR);
  display_fill_rect(701, 5, 25, 13);//700 5 26 13

  //Draw an empty battery symbol in white
  display_set_fg_color(WHITE_COLOR);
  display_fill_rect(701, 9, 2, 4);//700
  display_fill_rect(703, 5, 22, 12);//702 5 23 12

  //Check if there is any charge
  if(batterychargelevel)
  {
    //Keep charge level on max if above
    if(batterychargelevel > 20)
    {
      //Max for displaying the level
      batterychargelevel = 20;
    }

    //Check if charge level is low
    if(batterychargelevel < 4)
    {
      //Draw the level indicator in red
      display_set_fg_color(RED_COLOR);
    }
    else
    {
      //Draw the level indicator in dark green
      display_set_fg_color(DARKGREEN_COLOR);
    }

    //Draw the indicator based on the level
    display_fill_rect(724 - batterychargelevel, 6, batterychargelevel, 10);//723
  }

  //Draw the battery charging indicator when plugged in
  if(batterycharging)
  {
#if 0
    //Some light blue color
    display_set_fg_color(0x002222FF);

    //Draw an arrow when charging
    display_draw_horz_line(10, 708, 718);
    display_draw_horz_line(11, 708, 718);
    display_draw_horz_line(12, 708, 718);
    display_draw_vert_line(719, 8, 14);
    display_draw_vert_line(720, 9, 13);
    display_draw_vert_line(721, 10, 12);
    display_draw_vert_line(722, 11, 11);
#else
    //Some orange color
    display_set_fg_color(0x00FF6A00);

    //Draw a lightning bolt when charging
    display_draw_horz_line( 7, 715, 716);
    display_draw_horz_line( 8, 713, 716);
    display_draw_horz_line( 9, 711, 715);
    display_draw_horz_line(10, 709, 715);
    display_draw_horz_line(11, 707, 711);
    display_draw_horz_line(12, 705, 709);
    display_draw_horz_line(11, 713, 715);
    display_draw_horz_line(10, 719, 723);
    display_draw_horz_line(11, 717, 721);
    display_draw_horz_line(12, 713, 719);
    display_draw_horz_line(13, 713, 717);
    display_draw_horz_line(14, 712, 715);
    display_draw_horz_line(15, 712, 713);
#endif
  } 
  ///*
  else
  {
      
    if (batterychargelevel < 20)
    {
        //Display the text xx capacity in %
        display_set_fg_color(DARKBLUE_COLOR);
        display_set_font(&font_2);
        display_decimal(708, 4, batterychargelevel*5 );//703 5
    //if (batterychargelevel < 20) xpos += 5;// move text right if <100%
    //display_decimal(xpos, 5, batterychargelevel*6 );//703 5
    }
  }
  //*/
  //Copy it to the actual screen
  display_set_source_buffer(displaybuffertmp);//1
  display_set_screen_buffer((uint16 *)maindisplaybuffer);
  display_copy_rect_to_screen(701, 5, 25, 13);//700 5 26 13
}

//----------------------------------------------------------------------------------------------------------------------------------
// Menu functions
//----------------------------------------------------------------------------------------------------------------------------------

void scope_open_main_menu(void)
{
  //Setup the menu in a separate buffer to be able to slide it onto the screen
  display_set_screen_buffer(displaybuffertmp);//1

  //Draw the background in dark grey
  display_set_fg_color(DARKGREY_COLOR);

  //Fill the background
  display_fill_rect(2, 46, 147, 292);//0  149 233

  //Draw the edge in a lighter grey
  display_set_fg_color(LIGHTGREY_COLOR);

  //Draw the edge
  display_draw_rect(2, 46, 147, 292);//0  149 233

  //Three black lines between the settings
  display_set_fg_color(BLACK_COLOR);
  display_draw_horz_line(104, 9, 140);//59
  display_draw_horz_line(163, 9, 140);
  display_draw_horz_line(222, 9, 140);
  display_draw_horz_line(281, 9, 140);//222+59

  //Display the menu items
  scope_main_menu_system_settings(0);
  scope_main_menu_picture_view(0);
  scope_main_menu_waveform_view(0);
  scope_main_menu_diagnostic_view(0);
  scope_main_menu_usb_connection(0);

  //Set source and target for getting it on the actual screen
  display_set_source_buffer(displaybuffertmp);//1
  display_set_screen_buffer((uint16 *)maindisplaybuffer);

  //Slide the image onto the actual screen. The speed factor makes it start fast and end slow, Smaller value makes it slower.
  display_slide_top_rect_onto_screen(2, 46, 147, 292, 63039);//3  149 233
}

//----------------------------------------------------------------------------------------------------------------------------------

void scope_main_menu_system_settings(int mode)
{
  //Check if inactive or active
  if(mode == 0)
  {
    //Inactive so dark grey background
    display_set_fg_color(DARKGREY_COLOR);
  }
  else
  {
    //Active so yellow background
    display_set_fg_color(YELLOW_COLOR);
  }

  //Draw the background
  display_fill_rounded_rect(9, 59, 131, 35, 3);

  //Check if inactive or active
  if(mode == 0)
  {
    //Inactive so white foreground and grey background
    display_set_fg_color(WHITE_COLOR);
    display_set_bg_color(DARKGREY_COLOR);
  }
  else
  {
    //Active so black foreground and yellow background
    display_set_fg_color(BLACK_COLOR);
    display_set_bg_color(YELLOW_COLOR);
  }

  //Display the icon with the set colors
  display_copy_icon_use_colors(system_settings_icon, 21, 63, 15, 25);

  //Display the text
  display_set_font(&font_3);
  display_text(69, 60, "System");
  display_text(68, 76, "settings");
}

//----------------------------------------------------------------------------------------------------------------------------------

void scope_main_menu_picture_view(int mode)
{
  //Check if inactive or active
  if(mode == 0)
  {
    //Inactive so dark grey background
    display_set_fg_color(DARKGREY_COLOR);
  }
  else
  {
    //Active so white background
    display_set_fg_color(0x00CCCCCC);
  }

  //Draw the background
  display_fill_rounded_rect(9, 116, 131, 35, 3);

  //Check if inactive or active
  if(mode == 0)
  {
    //Inactive so white foreground and grey background
    display_set_fg_color(WHITE_COLOR);
    display_set_bg_color(DARKGREY_COLOR);
  }
  else
  {
    //Active so black foreground and white background
    display_set_fg_color(BLACK_COLOR);
    display_set_bg_color(0x00CCCCCC);
  }

  //Display the icon with the set colors
  display_copy_icon_use_colors(picture_view_icon, 17, 122, 24, 24);

  //Display the text
  display_set_font(&font_3);
  display_text(73, 119, "Picture");
  display_text(79, 135, "view");
}

//----------------------------------------------------------------------------------------------------------------------------------

void scope_main_menu_waveform_view(int mode)
{
  //Check if inactive or active
  if(mode == 0)
  {
    //Inactive so dark grey background
    display_set_fg_color(DARKGREY_COLOR);
  }
  else
  {
    //Active so white background
    display_set_fg_color(0x00CCCCCC);
  }

  //Draw the background
  display_fill_rounded_rect(9, 175, 131, 35, 3);

  //Check if inactive or active
  if(mode == 0)
  {
    //Inactive so white foreground and grey background
    display_set_fg_color(WHITE_COLOR);
    display_set_bg_color(DARKGREY_COLOR);
  }
  else
  {
    //Active so black foreground and white background
    display_set_fg_color(BLACK_COLOR);
    display_set_bg_color(0x00CCCCCC);
  }

  //Display the icon with the set colors
  display_copy_icon_use_colors(waveform_view_icon, 17, 181, 24, 24);

  //Display the text
  display_set_font(&font_3);
  display_text(62, 178, "Waveform");
  display_text(79, 194, "view");
}

//----------------------------------------------------------------------------------------------------------------------------------

void scope_main_menu_diagnostic_view(int mode)
{
  //Check if inactive or active
  if(mode == 0)
  {
    //Inactive so dark grey background
    display_set_fg_color(DARKGREY_COLOR);
  }
  else
  {
    //Active so white background
    display_set_fg_color(0x00CCCCCC);
  }

  //Draw the background
  display_fill_rounded_rect(9, 235, 131, 35, 3);

  //Check if inactive or active
  if(mode == 0)
  {
    //Inactive so white foreground and grey background
    display_set_fg_color(WHITE_COLOR);
    display_set_bg_color(DARKGREY_COLOR);
  }
  else
  {
    //Active so black foreground and white background
    display_set_fg_color(BLACK_COLOR);
    display_set_bg_color(0x00CCCCCC);
  }

  //Display the icon with the set colors
  display_copy_icon_use_colors(diagnostic_view_icon, 17, 239, 24, 24);

  //diagnostic_view_icon
  //Display the text
  display_set_font(&font_3);
  display_text(60, 237, "Diagnostic");
  display_text(79, 253, "view");
}

//----------------------------------------------------------------------------------------------------------------------------------

void scope_main_menu_usb_connection(int mode)
{
  //Check if inactive or active
  if(mode == 0)
  {
    //Inactive so dark grey background
    display_set_fg_color(DARKGREY_COLOR);
  }
  else
  {
    //Active so white background
    display_set_fg_color(0x00CCCCCC);
  }

  //Draw the background
  display_fill_rounded_rect(9, 295, 131, 35, 3);

  //Check if inactive or active
  if(mode == 0)
  {
    //Inactive so white foreground and grey background
    display_set_fg_color(WHITE_COLOR);
    display_set_bg_color(DARKGREY_COLOR);
  }
  else
  {
    //Active so black foreground and white background
    display_set_fg_color(BLACK_COLOR);
    display_set_bg_color(0x00CCCCCC);
  }

  //Display the icon with the set colors
  display_copy_icon_use_colors(usb_icon, 20, 297, 18, 25);

  //Display the text
  display_set_font(&font_3);
  display_text(80, 296, "USB");
  display_text(60, 312, "connection");
}

//----------------------------------------------------------------------------------------------------------------------------------

void scope_open_channel_menu(PCHANNELSETTINGS settings)
{
  uint32 xstart;
  uint32 xend;
  
  //Setup the menu in a separate buffer to be able to slide it onto the screen
  display_set_screen_buffer(displaybuffertmp);//1

  //Draw the background in dark grey
  display_set_fg_color(DARKGREY_COLOR);

  //Fill the background
  display_fill_rect(settings->menuxpos, CH_MENU_YPOS, CH_MENU_WIDTH, CH_MENU_HEIGHT);

  //Draw the edge in a lighter grey
  display_set_fg_color(LIGHTGREY_COLOR);

  //Draw the edge
  display_draw_rect(settings->menuxpos, CH_MENU_YPOS, CH_MENU_WIDTH, CH_MENU_HEIGHT);

  //Line start and end x positions
  xstart = settings->menuxpos + 4;//14
  xend   = settings->menuxpos + CH_MENU_WIDTH - 4;//-14

  //Three black lines between the settings
  display_set_fg_color(BLACK_COLOR);
  display_draw_horz_line(CH_MENU_YPOS +  56, xstart, xend); //60
  display_draw_horz_line(CH_MENU_YPOS + 112, xstart, xend);//120
  display_draw_horz_line(CH_MENU_YPOS + 168, xstart, xend); //180
  display_draw_horz_line(CH_MENU_YPOS + 224, xstart, xend); //
  
  //Set channel color
  //display_set_fg_color(settings->color);
  display_draw_horz_line(CH_MENU_YPOS + 280, xstart, xend); //240
  
  //One black lines between the sensitivity
  display_set_fg_color(BLACK_COLOR);
  display_draw_vert_line(settings->menuxpos + 252,CH_MENU_YPOS+4, CH_MENU_YPOS+279);//252 4 239
  
  //Set channel color
  display_set_fg_color(settings->color);
  //One channel_color lines the settings and sensitivity
  display_draw_vert_line(settings->menuxpos + 171,CH_MENU_YPOS+4, CH_MENU_YPOS+279);//171 4 239

  //Main texts in white
  display_set_fg_color(WHITE_COLOR);

  //Select the font for the texts
  display_set_font(&font_3);
  
  //Display the texts
  display_text(settings->menuxpos + 15, CH_MENU_YPOS +  10, "Open");    //12
  display_text(settings->menuxpos + 23, CH_MENU_YPOS +  29, "CH");      //31
  
  display_text(settings->menuxpos + 15, CH_MENU_YPOS +  66, "Open");    //72
  display_text(settings->menuxpos + 20, CH_MENU_YPOS +  85, "FFT");     //19 91
  
  display_text(settings->menuxpos + 14, CH_MENU_YPOS +  122, "Invert");  //13 72
  display_text(settings->menuxpos + 23, CH_MENU_YPOS +  141, "CH");      //91
  
  
  display_text(settings->menuxpos + 15, CH_MENU_YPOS + 178, "Coup");    //132
  display_text(settings->menuxpos + 19, CH_MENU_YPOS + 197, "ling");    //18 151
  
  display_text(settings->menuxpos + 15, CH_MENU_YPOS + 234, "V / A");   //192
  display_text(settings->menuxpos + 12, CH_MENU_YPOS + 253, "mode");    //211
  
  display_text(settings->menuxpos + 14, CH_MENU_YPOS + 290, "Probe");   //15 252
  display_text(settings->menuxpos + 15, CH_MENU_YPOS + 309, "mode");    //271
  
  //clear flag for touch 50% button in channel menu
  triger50 = 0;

  //Display the actual settings
  scope_channel_enable_select(settings);
  scope_channel_fft_show(settings);
  scope_channel_invert_select(settings);
  scope_channel_coupling_select(settings);
  scope_channel_VA_select(settings);
  scope_channel_probe_magnification_select(settings);
  scope_channel_sensitivity_select(settings);

  //Set source and target for getting it on the actual screen
  display_set_source_buffer(displaybuffertmp);//1
  display_set_screen_buffer((uint16 *)maindisplaybuffer);

  //Slide the image onto the actual screen. The speed factor makes it start fast and end slow, Smaller value makes it slower.
  display_slide_top_rect_onto_screen(settings->menuxpos, CH_MENU_YPOS, CH_MENU_WIDTH, CH_MENU_HEIGHT, 69906);
}

//----------------------------------------------------------------------------------------------------------------------------------

void scope_channel_enable_select(PCHANNELSETTINGS settings)
{
  //Select the font for the texts
  display_set_font(&font_3);

  //Set dark grey color for the box behind the not selected text
  display_set_fg_color(DARKGREY_COLOR);
  
  //Show the state (if channel is disabled or enabled)
  scope_display_slide_button(settings->menuxpos + 98, CH_MENU_YPOS + 18, settings->enable, settings->color);//98 22
}

//----------------------------------------------------------------------------------------------------------------------------------

void scope_channel_fft_show(PCHANNELSETTINGS settings)
{
  //Select the font for the texts
  display_set_font(&font_3);

  //Set dark grey color for the box behind the not selected text
  display_set_fg_color(DARKGREY_COLOR);

 //Show the state (if FFT is disabled or enabled)
  scope_display_slide_button(settings->menuxpos + 98, CH_MENU_YPOS + 74, settings->fftenable, GREEN_COLOR);//82
}
//----------------------------------------------------------------------------------------------------------------------------------

void scope_channel_invert_select(PCHANNELSETTINGS settings)
{
  //Select the font for the texts
  display_set_font(&font_3);

  //Set dark grey color for the box behind the not selected text
  display_set_fg_color(DARKGREY_COLOR);
 
 //Show the state (if invert is disabled or enabled)
  scope_display_slide_button(settings->menuxpos + 98, CH_MENU_YPOS + 130, settings->invert, GREEN_COLOR);//82
}

//----------------------------------------------------------------------------------------------------------------------------------

void scope_channel_coupling_select(PCHANNELSETTINGS settings)
{
  //Select the font for the texts
  display_set_font(&font_3);

  //Set dark grey color for the box behind the not selected text
  display_set_fg_color(DARKGREY_COLOR);

  //Check if coupling is dc or ac
  if(settings->coupling == 0)
  {
    //DC so dark grey box behind ac text
    display_fill_rect(settings->menuxpos + 130, CH_MENU_YPOS + 185, 32, 22);//139
  }
  else
  {
    //AC so dark grey box behind dc text
    display_fill_rect(settings->menuxpos + 78, CH_MENU_YPOS + 185, 32, 22);//139
  }

  //Set channel color for the box behind the selected text
  display_set_fg_color(settings->color);

  //Check if coupling is dc or ac
  if(settings->coupling == 0)
  {
    //DC so channel color box behind dc text
    display_fill_rounded_rect(settings->menuxpos + 78, CH_MENU_YPOS + 185, 32, 22, 2);
  }
  else
  {
    //AC so channel color box behind ac text
    display_fill_rounded_rect(settings->menuxpos + 130, CH_MENU_YPOS + 185, 32, 22, 2);
  }

  //Check if coupling is dc or ac
  if(settings->coupling == 0)
  {
    //DC so black DC text
    display_set_fg_color(BLACK_COLOR);
  }
  else
  {
    //AC so white or grey DC text
    if(scopesettings.waveviewmode)
    {  
      //Grey auto text when in waveform view mode
      display_set_fg_color(0x00606060);
    }
    else
    {
      //White auto text if in normal mode
      display_set_fg_color(WHITE_COLOR);
    }
  }

  //Display the DC text
  display_text(settings->menuxpos + 85, CH_MENU_YPOS + 188, "DC");//145

  //Check if coupling is DC or AC
  if(settings->coupling == 0)
  {
    //DC so white or grey AC text
    if(scopesettings.waveviewmode)
    {  
      //Grey auto text when in waveform view mode
      display_set_fg_color(0x00606060);
    }
    else
    {
      //White auto text if in normal mode
      display_set_fg_color(WHITE_COLOR);
    }
  }
  else
  {
    //AC so black AC text
    display_set_fg_color(BLACK_COLOR);
  }

  //Display the off text
  display_text(settings->menuxpos + 137, CH_MENU_YPOS + 188, "AC");//145
}
//----------------------------------------------------------------------------------------------------------------------------------

void scope_channel_VA_select(PCHANNELSETTINGS settings)
{
  //Select the font for the texts
  display_set_font(&font_3);

  //Set dark grey color for the box behind the not selected text
  display_set_fg_color(DARKGREY_COLOR);

  //Check if measures is V or A
  if(settings->V_A == 0)
  {
    //V so dark grey box behind A text
    display_fill_rect(settings->menuxpos + 130, CH_MENU_YPOS + 241, 32, 22);//199
  }
  else
  {
    //A so dark grey box behind V text
    display_fill_rect(settings->menuxpos + 78, CH_MENU_YPOS + 241, 32, 22);
  }

  //Set channel color for the box behind the selected text
  display_set_fg_color(settings->color);

  //Check if measures is V or A
  if(settings->V_A == 0)
  {
    //V so channel color box behind V text
    display_fill_rounded_rect(settings->menuxpos + 78, CH_MENU_YPOS + 241, 32, 22, 2);
  }
  else
  {
    //A so channel color box behind A text
    display_fill_rounded_rect(settings->menuxpos + 130, CH_MENU_YPOS + 241, 32, 22, 2);
  }

  //Check if measures is V or A
  if(settings->V_A == 0)
  {
    //V so black V text
    display_set_fg_color(BLACK_COLOR);
  }
  else
  {
    //V so white or grey A text
    if(scopesettings.waveviewmode)
    {  
      //Grey text when in waveform view mode
      display_set_fg_color(0x00606060);
    }
    else
    {
      //White text if in normal mode
      display_set_fg_color(WHITE_COLOR);
    }
  }

  //Display the V text
  display_text(settings->menuxpos + 90, CH_MENU_YPOS + 244, "V");//85 202

  //Check if measures is V or A
  if(settings->V_A == 0)
  {
    //V so white or grey A text
    if(scopesettings.waveviewmode)
    {  
      //Grey text when in waveform view mode
      display_set_fg_color(0x00606060);
    }
    else
    {
      //White auto text if in normal mode
      display_set_fg_color(WHITE_COLOR);
    }
  }
  else
  {
    //A so black A text
    display_set_fg_color(BLACK_COLOR);
  }

  //Display the off text
  display_text(settings->menuxpos + 142, CH_MENU_YPOS + 244, "A");//137 202
}

//----------------------------------------------------------------------------------------------------------------------------------

void scope_channel_probe_magnification_select(PCHANNELSETTINGS settings)
{
  //Select the font for the texts
  display_set_font(&font_3);

  //Set dark grey color for the boxes behind the not selected texts
  display_set_fg_color(DARKGREY_COLOR);

  //dark grey 0.5-1-10-20-50-100-1000 magnification
  display_fill_rect(settings->menuxpos + 67,  CH_MENU_YPOS + 286, 213, 40);//72 250
  
  //Set channel color for the box behind the selected text
  display_set_fg_color(settings->color);

  //Check if which magnification to highlight
  switch(settings->magnification)
  {
    case 0:
      //Highlight times 0.5 magnification //0
      display_fill_rounded_rect(settings->menuxpos + 67, CH_MENU_YPOS + 286, 25, 40, 2);//72 250
      break;

    case 1:
      //Highlight times 1 magnification    //1
      display_fill_rounded_rect(settings->menuxpos + 97, CH_MENU_YPOS + 286, 25, 40, 2);//102
      break;
      
    case 2:
      //Highlight times 10 magnification     //0 77
      display_fill_rounded_rect(settings->menuxpos + 127, CH_MENU_YPOS + 286, 25, 40, 2);//132
      break;

    case 3:
      //Highlight times 10 magnification    //1 107
      display_fill_rounded_rect(settings->menuxpos + 157, CH_MENU_YPOS + 286, 25, 40, 2);//162
      break;  
   
    case 4:
      //Highlight times 100 magnification   //default   137
      display_fill_rounded_rect(settings->menuxpos + 187, CH_MENU_YPOS + 286, 25, 40, 2);//192
      break;
      
    case 5:
      //Highlight times 500 magnification   
      display_fill_rounded_rect(settings->menuxpos + 217, CH_MENU_YPOS + 286, 25, 40, 2);//222
      break;
      
    case 6:
      //Highlight times 1000 magnification  
      display_fill_rounded_rect(settings->menuxpos + 249, CH_MENU_YPOS + 286, 31, 40, 2);//252
      break;
      
      default: break;
  }
  
  //--------------------------------------------------------------------
   //Check if magnification is 0.5X
  if(settings->magnification == 0)
  {
    //Yes so black 0.5X text
    display_set_fg_color(BLACK_COLOR);
  }
  else
  {
    //No so white or grey 0.5 text
    if(scopesettings.waveviewmode)
    {  
      //Grey auto text when in waveform view mode
      display_set_fg_color(0x00606060);
    }
    else
    {
      //White auto text if in normal mode
      display_set_fg_color(WHITE_COLOR);
    }
  }

  //Display the 0.5X text
  display_text(settings->menuxpos + 71, CH_MENU_YPOS + 290, "0.5"); //71 252
  display_text(settings->menuxpos + 75, CH_MENU_YPOS + 307, "X");   //75 271
  //--------------------------------------------------------------------
 //Check if magnification is 1x
  if(settings->magnification == 1)
  {
    //Yes so black 1X text
    display_set_fg_color(BLACK_COLOR);
  }
  else
  {
    //No so white or grey 1 text
    if(scopesettings.waveviewmode)
    {  
      //Grey auto text when in waveform view mode
      display_set_fg_color(0x00606060);
    }
    else
    {
      //White auto text if in normal mode
      display_set_fg_color(WHITE_COLOR);
    }
  }

  //Display the 1X text
  display_text(settings->menuxpos + 106, CH_MENU_YPOS + 290, "1");//+27
  display_text(settings->menuxpos + 105, CH_MENU_YPOS + 307, "X");// +31
   
  //--------------------------------------------------------------------
  //Check if magnification is 10x
  if(settings->magnification == 2)  //0
  {
    //Yes so black 10X text
    display_set_fg_color(BLACK_COLOR);
  }
  else
  {
    //No so white or grey 10X text
    if(scopesettings.waveviewmode)
    {  
      //Grey auto text when in waveform view mode
      display_set_fg_color(0x00606060);
    }
    else
    {
      //White auto text if in normal mode
      display_set_fg_color(WHITE_COLOR);
    }
  }

  //Display the 10X text
  display_text(settings->menuxpos + 132, CH_MENU_YPOS + 290, "10");// +27 na 10x
  display_text(settings->menuxpos + 135, CH_MENU_YPOS + 307, "X");// +31 na x10
  //--------------------------------------------------------------------
  //Check if magnification is 20x
  if(settings->magnification == 3)  //1
  {
    //Yes so black 20X text
    display_set_fg_color(BLACK_COLOR);
  }
  else
  {
    //No so white or grey 20X text
    if(scopesettings.waveviewmode)
    {  
      //Grey auto text when in waveform view mode
      display_set_fg_color(0x00606060);
    }
    else
    {
      //White auto text if in normal mode
      display_set_fg_color(WHITE_COLOR);
    }
  }

  //Display the 20X text
  display_text(settings->menuxpos + 163, CH_MENU_YPOS + 290, "20");// +30 na x100
  display_text(settings->menuxpos + 165, CH_MENU_YPOS + 307, "X");//  +34 na x100
  //--------------------------------------------------------------------
  //Check if magnification is 50x
  if(settings->magnification == 4)  //1
  {
    //Yes so black 50X text
    display_set_fg_color(BLACK_COLOR);
  }
  else
  {
    //No so white or grey 50X text
    if(scopesettings.waveviewmode)
    {  
      //Grey auto text when in waveform view mode
      display_set_fg_color(0x00606060);
    }
    else
    {
      //White auto text if in normal mode
      display_set_fg_color(WHITE_COLOR);
    }
  }

  //Display the 50X text
  display_text(settings->menuxpos + 193, CH_MENU_YPOS + 290, "50");// +30 na x100
  display_text(settings->menuxpos + 195, CH_MENU_YPOS + 307, "X");//  +34 na x100
  //--------------------------------------------------------------------
  //Check if magnification is 100x
  if(settings->magnification == 5)
  {
    //Yes so black 100X text
    display_set_fg_color(BLACK_COLOR);
  }
  else
  {
    //No so white or grey 100X text
    if(scopesettings.waveviewmode)
    {  
      //Grey auto text when in waveform view mode
      display_set_fg_color(0x00606060);
    }
    else
    {
      //White auto text if in normal mode
      display_set_fg_color(WHITE_COLOR);
    }
  }

  //Display the 100X text
  display_text(settings->menuxpos + 219, CH_MENU_YPOS + 290, "100");//194
  display_text(settings->menuxpos + 225, CH_MENU_YPOS + 307, "X");//200
  //--------------------------------------------------------------------
  /*
  //Check if magnification is 500x
  if(settings->magnification == 5)
  {
    //Yes so black 500X text
    display_set_fg_color(BLACK_COLOR);
  }
  else
  {
    //No so white or grey 500X text
    if(scopesettings.waveviewmode)
    {  
      //Grey auto text when in waveform view mode
      display_set_fg_color(0x00606060);
    }
    else
    {
      //White auto text if in normal mode
      display_set_fg_color(WHITE_COLOR);
    }
  }

  //Display the 500X text
  display_text(settings->menuxpos + 224, CH_MENU_YPOS + 252, "500");
  display_text(settings->menuxpos + 230, CH_MENU_YPOS + 271, "X");
   */
  //--------------------------------------------------------------------
  //Check if magnification is 1000x
  if(settings->magnification == 6)  //1
  {
    //Yes so black 1000X text
    display_set_fg_color(BLACK_COLOR);
  }
  else
  {
    //No so white or grey 1000X text
    if(scopesettings.waveviewmode)
    {  
      //Grey auto text when in waveform view mode
      display_set_fg_color(0x00606060);
    }
    else
    {
      //White auto text if in normal mode
      display_set_fg_color(WHITE_COLOR);
    }
  }

  //Display the 1000X text
  display_text(settings->menuxpos + 250, CH_MENU_YPOS + 290, "1000");//257 1k
  display_text(settings->menuxpos + 260, CH_MENU_YPOS + 307, "X");
//******************************************************************************
}

//----------------------------------------------------------------------------------------------------------------------------------

void scope_channel_sensitivity_select(PCHANNELSETTINGS settings)
{      
  uint32 i,x,y;
  
  //Clear the boxes for the not selected items
  for(i=0;i<7;i++)
  {
    if (i<4) x = 177; else x = 258;//177*258
    y = ((i & 3) * 56) + 8;    //*60 +6
    
    if(i == (6 - settings->displayvoltperdiv))
        //Set channel color for the box behind the selected text
        display_set_fg_color(settings->color);
    else   
        //Set dark grey color for the boxes behind the not selected texts
        display_set_fg_color(DARKGREY_COLOR); 
    
    display_fill_rounded_rect(settings->menuxpos + x, CH_MENU_YPOS + y, 70, 40, 3);//70*48 70*22 48*22
  }

  //Select the font for the texts
  display_set_font(&font_3);
  
  for(i=0;i<7;i++)
  {
    y = ((i & 3) * 56) + 19; //*60 +19
    
    if(i == (6 - settings->displayvoltperdiv))
        //Set channel color for the box behind the selected text
        display_set_fg_color(BLACK_COLOR);
    else   
        //Set dark grey color for the boxes behind the not selected texts
        display_set_fg_color(WHITE_COLOR); 

    //Calculate the position of this text
    x = volt_div_texts_x_offsets[settings->magnification][6-i];
    y = ((i & 3) * 56) + 20;//*60 +22

    //Check if measures is V or A
    //Display the text from the table
    if(settings->V_A == 0)
        display_text(settings->menuxpos + x, CH_MENU_YPOS + y,  (int8 *)volt_div_texts[settings->magnification][6-i]);
    else
        display_text(settings->menuxpos + x, CH_MENU_YPOS + y,  (int8 *)ampere_div_texts[settings->magnification][6-i]);
  }
  //********************************************************
  if(triger50)
    {
    //Set channel color for the box behind the selected text
    display_set_fg_color(settings->color);
    display_fill_rounded_rect(settings->menuxpos + 270, CH_MENU_YPOS + 185, 45, 22, 2);//270 199 45 22
    //Texts in black
    display_set_fg_color(BLACK_COLOR);
    display_text(settings->menuxpos + 283, CH_MENU_YPOS + 188, "50%");//283 202
    }
    else      
    {
    //Texts in white
    display_set_fg_color(WHITE_COLOR);
    display_text(settings->menuxpos + 283, CH_MENU_YPOS + 188, "50%");//283 202
    }
}

//----------------------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------------------

void scope_open_keyboard_menu(void)
{
  //Setup the menu in a separate buffer to be able to slide it onto the screen
  display_set_screen_buffer(displaybuffertmp);//1

  //Draw the background in dark grey
  display_set_fg_color(DARKGREY_COLOR);

  //Fill the background
  display_fill_rect(KEY_MENU_XPOS, KEY_MENU_YPOS, KEY_MENU_WIDTH, KEY_MENU_HEIGHT);

  //Draw the edge in a lighter grey
  display_set_fg_color(LIGHTGREY_COLOR);

  //Draw the edge
  display_draw_rect(KEY_MENU_XPOS, KEY_MENU_YPOS, KEY_MENU_WIDTH, KEY_MENU_HEIGHT);

  //A two black line between the settings
  //display_set_fg_color(BLACK_COLOR);
  //display_draw_horz_line(KEY_MENU_YPOS +  155, KEY_MENU_XPOS + 8, KEY_MENU_XPOS + KEY_MENU_WIDTH - 8);
  
  //display_set_fg_color(BLACK_COLOR);
  //display_draw_horz_line(KEY_MENU_YPOS +  211, KEY_MENU_XPOS + 8, KEY_MENU_XPOS + KEY_MENU_WIDTH - 8);

  //Draw the edge
  display_draw_rect(KEY_MENU_XPOS+5, KEY_MENU_YPOS+30, KEY_MENU_WIDTH-10, 31);
  
  //Main texts in white
  display_set_fg_color(WHITE_COLOR);
  
  //Select the font for the texts
  display_set_font(&font_5);//3

  //Display the texts
  display_text(KEY_MENU_XPOS + 170, KEY_MENU_YPOS + 34, "1.0008756");
  
  //Select the font for the texts
  display_set_font(&font_3);//3

  //Display the texts
  //display_text(KEY_MENU_XPOS + 111, KEY_MENU_YPOS +   8, "Sample Rate");
  //display_text(KEY_MENU_XPOS + 117, KEY_MENU_YPOS + 7, "Keyboard");
  display_text(KEY_MENU_XPOS + 117, KEY_MENU_YPOS + 7, "Generator");
  //display_text(KEY_MENU_XPOS +  97, KEY_MENU_YPOS + 216, "Time per Division");//166
  
  //Set dark grey color for the boxes behind the not selected texts
  display_set_fg_color(LIGHTGREY_COLOR);
  
  //Clear the boxes for the not selected items 
  display_fill_rect(KEY_MENU_XPOS +   5, KEY_MENU_YPOS + 284, 90, 50);
  display_fill_rect(KEY_MENU_XPOS + 101, KEY_MENU_YPOS + 284, 95, 50);
  display_fill_rect(KEY_MENU_XPOS + 202, KEY_MENU_YPOS + 284, 90, 50);
  
  display_fill_rect(KEY_MENU_XPOS +   5, KEY_MENU_YPOS + 338, 90, 50);
  display_fill_rect(KEY_MENU_XPOS + 101, KEY_MENU_YPOS + 338, 95, 50);
  display_fill_rect(KEY_MENU_XPOS + 202, KEY_MENU_YPOS + 338, 90, 50);
  
  //Select the font for the texts
  display_set_font(&font_3);
  
  //Set color for the text
  display_set_fg_color(BLUE_COLOR);
  display_text(KEY_MENU_XPOS + 38,  KEY_MENU_YPOS +  300, "MIN"); 
  //Set color for the text
  display_set_fg_color(DARKGREEN_COLOR);
  display_text(KEY_MENU_XPOS + 126, KEY_MENU_YPOS +  300, "Default");
  //Set color for the text
  display_set_fg_color(RED_COLOR);
  display_text(KEY_MENU_XPOS + 233, KEY_MENU_YPOS +  300, "MAX");
  
  //Set color for the text
  display_set_fg_color(YELLOW_COLOR);
  display_text(KEY_MENU_XPOS + 34,  KEY_MENU_YPOS +  354, "Back");
  //Set color for the text
  display_set_fg_color(WHITE_COLOR);
  display_text(KEY_MENU_XPOS + 131, KEY_MENU_YPOS +  354, "Clear");
  //Set color for the text
  display_set_fg_color(GREEN_COLOR);
  display_text(KEY_MENU_XPOS + 238, KEY_MENU_YPOS +  354, "OK");


  
  //display_set_fg_color(WHITE_COLOR);
  //***************************************************************

  //Display the actual settings
  scope_keyboard_select();
  //scope_acquisition_timeperdiv_select();
  ////scope_acquisition_ACQ_mode_select();
  //scope_acquisition_Average_mode_select();
  //scope_acquisition_Long_memory_select();

  //Set source and target for getting it on the actual screen
  display_set_source_buffer(displaybuffertmp);//1
  display_set_screen_buffer((uint16 *)maindisplaybuffer);

  //Slide the image onto the actual screen. The speed factor makes it start fast and end slow, Smaller value makes it slower.
  display_slide_top_rect_onto_screen(KEY_MENU_XPOS, KEY_MENU_YPOS, KEY_MENU_WIDTH, KEY_MENU_HEIGHT, 69906);
}

//----------------------------------------------------------------------------------------------------------------------------------

void scope_keyboard_select(void)
{
  uint32 i,x=0,y;

  //Select the font for the texts
  display_set_font(&font_5);

  //Clear the boxes for the not selected items
  for(i=0;i<((sizeof(keyboard_texts) / sizeof(int8 *)));i++)
  {
    if(((i%5)==0)) x = 5; else x += 58;
    y=((i/5)*53); 
    
    //Set dark grey color for the boxes behind the not selected texts
    display_set_fg_color(MIDLEGREY_COLOR);
  
    display_fill_rect(KEY_MENU_XPOS + x, KEY_MENU_YPOS + y + 68, 55, 50);//35
    
    //Selected texts in black
    display_set_fg_color(WHITE_COLOR);
      
    //o = x + keyboard_texts_x_offsets[i];
    //Display the text from the table
    display_text(KEY_MENU_XPOS + x + keyboard_texts_x_offsets[i], KEY_MENU_YPOS + 82 + y, (int8 *)keyboard_texts[i]);//49
    
    
  }

}

//----------------------------------------------------------------------------------------------------------------------------------

void scope_open_acquisition_menu(void)
{
  //Setup the menu in a separate buffer to be able to slide it onto the screen
  display_set_screen_buffer(displaybuffertmp);//1

  //Draw the background in dark grey
  display_set_fg_color(DARKGREY_COLOR);

  //Fill the background
  display_fill_rect(ACQ_MENU_XPOS, ACQ_MENU_YPOS, ACQ_MENU_WIDTH, ACQ_MENU_HEIGHT);

  //Draw the edge in a lighter grey
  display_set_fg_color(LIGHTGREY_COLOR);

  //Draw the edge
  display_draw_rect(ACQ_MENU_XPOS, ACQ_MENU_YPOS, ACQ_MENU_WIDTH, ACQ_MENU_HEIGHT);

  //A two black line between the settings
  display_set_fg_color(BLACK_COLOR);
  display_draw_horz_line(ACQ_MENU_YPOS +  155, ACQ_MENU_XPOS + 8, ACQ_MENU_XPOS + ACQ_MENU_WIDTH - 8);
  
  display_set_fg_color(BLACK_COLOR);
  display_draw_horz_line(ACQ_MENU_YPOS +  211, ACQ_MENU_XPOS + 8, ACQ_MENU_XPOS + ACQ_MENU_WIDTH - 8);

  //Main texts in white
  display_set_fg_color(WHITE_COLOR);

  //Select the font for the texts
  display_set_font(&font_3);

  //Display the texts
  display_text(ACQ_MENU_XPOS + 111, ACQ_MENU_YPOS +   8, "Sample Rate");
  display_text(ACQ_MENU_XPOS +  97, ACQ_MENU_YPOS + 216, "Time per Division");//166
  
    //**********************************************************
  
  //Set dark grey color for the boxes behind the not selected texts
  display_set_fg_color(LIGHTGREY_COLOR);
  
  //Clear the boxes for the not selected items (ACQ, Average, Long memory)
  display_fill_rounded_rect(ACQ_MENU_XPOS +   8, ACQ_MENU_YPOS + 163, 90, 40, 3);
  //display_fill_rect(ACQ_MENU_XPOS + 106, ACQ_MENU_YPOS + 163, 90, 40);
  //display_fill_rect(ACQ_MENU_XPOS + 204, ACQ_MENU_YPOS + 163, 90, 40);
  
  
  //Select the font for the texts
  display_set_font(&font_2);
    
  //Display Long/Short Mode text
  if (!scopesettings.long_mode) 
  { //Set orange color for the text
    display_set_fg_color(ORANGE_COLOR);
    display_text(ACQ_MENU_XPOS + 43, ACQ_MENU_YPOS +  168, "ACQ");
    display_text(ACQ_MENU_XPOS + 24, ACQ_MENU_YPOS +  182, "Long mode");
  }
  else 
  { //Set green color for the text
    display_set_fg_color(GREEN_COLOR);
    display_text(ACQ_MENU_XPOS + 43, ACQ_MENU_YPOS +  168, "ACQ");
    display_text(ACQ_MENU_XPOS + 20, ACQ_MENU_YPOS +  182, "Short mode");
  }
  /*
  if (scopesettings.long_memory)
  //Set black color for the text
  display_set_fg_color(BLACK_COLOR); else display_set_fg_color(WHITE_COLOR);
  display_text(ACQ_MENU_XPOS + 235, ACQ_MENU_YPOS +  168, "Long");
  display_text(ACQ_MENU_XPOS + 225, ACQ_MENU_YPOS +  182, "memory");
   */
  
  //display_set_fg_color(WHITE_COLOR);
  //***************************************************************

  //Display the actual settings
  scope_acquisition_speed_select();
  scope_acquisition_timeperdiv_select();
  //scope_acquisition_ACQ_mode_select();
  scope_acquisition_Average_mode_select();
  scope_acquisition_Long_memory_select();

  //Set source and target for getting it on the actual screen
  display_set_source_buffer(displaybuffertmp);//1
  display_set_screen_buffer((uint16 *)maindisplaybuffer);

  //Slide the image onto the actual screen. The speed factor makes it start fast and end slow, Smaller value makes it slower.
  display_slide_top_rect_onto_screen(ACQ_MENU_XPOS, ACQ_MENU_YPOS, ACQ_MENU_WIDTH, ACQ_MENU_HEIGHT, 69906);
}

//----------------------------------------------------------------------------------------------------------------------------------

void scope_acquisition_speed_select(void)
{
  uint32 i,x,y;

  //Select the font for the texts
  display_set_font(&font_2);

  //Set dark grey color for the boxes behind the not selected texts
  display_set_fg_color(0x00383838);

  //Clear the boxes for the not selected items
  for(i=0;i<((sizeof(acquisition_speed_texts) / sizeof(int8 *))-11);i++)//11 //9
  {
    if(i != scopesettings.samplerate)
    {
      x = ((i & 3) * 72) + 10;
      y = ((i >> 2) * 23) + 33;

      display_fill_rounded_rect(ACQ_MENU_XPOS + x, ACQ_MENU_YPOS + y, 68, 20, 2);
    }
  }

  //Set green color for the box behind the selected text
  display_set_fg_color(GREEN_COLOR);

  //Get the position of the selected item
  x = ((scopesettings.samplerate & 3) * 72) + 10;
  y = ((scopesettings.samplerate >> 2) * 23) + 33;

  //Highlight the selected item if short time mode
  if(scopesettings.timeperdiv>8) display_fill_rounded_rect(ACQ_MENU_XPOS + x, ACQ_MENU_YPOS + y, 68, 20, 2);

  for(i=0;i<((sizeof(acquisition_speed_texts) / sizeof(int8 *))-11);i++)
  {
    if(i != scopesettings.samplerate)
    {
      //Check if in stop mode
      if(((scopesettings.runstate)&&(!scopesettings.long_mode))||(scopesettings.triggermode == 1))
      {
        //When running available not selected texts shown in white
        display_set_fg_color(WHITE_COLOR);
        
      }
      else
      {
        //When stopped select option is disabled so texts shown in light grey
        display_set_fg_color(LIGHTGREY1_COLOR);     //
      }
    }
    else
    {
      //Selected texts in black
      display_set_fg_color(BLACK_COLOR);
    }

    //Calculate the position of this text
    x = ((i & 3) * 72) + acquisition_speed_text_x_offsets[i];
    y = ((i >> 2) * 23) + 36;

    //Display the text from the table
    display_text(ACQ_MENU_XPOS + x, ACQ_MENU_YPOS + y, (int8 *)acquisition_speed_texts[i]);
  }

}

//----------------------------------------------------------------------------------------------------------------------------------

void scope_acquisition_timeperdiv_select(void)
{
  uint32 c,i,x,y;
  #define ymp   241 //191   //position y for timeperdiv text and item

  //Select the font for the texts
  display_set_font(&font_2);

  //Set dark grey color for the boxes behind the not selected texts
  display_set_fg_color(0x00383838);

  //Clear the boxes for the not selected items
  for(i=0;i<((sizeof(time_div_texts) / sizeof(int8 *))-11);i++)
  {
    //Settings displayed from smallest to highest value
    c = ((sizeof(time_div_texts) / sizeof(int8 *)) - 1) - i;

    if(c != scopesettings.timeperdiv)
    {
      x = ((i & 3) * 72) + 10;
      y = ((i >> 2) * 23) + ymp;//191

      display_fill_rounded_rect(ACQ_MENU_XPOS + x, ACQ_MENU_YPOS + y, 68, 20, 2);
    }
  }

  //Set green color for the box behind the selected text
  display_set_fg_color(GREEN_COLOR);

  //Get the position of the selected item
  c = ((sizeof(time_div_texts) / sizeof(int8 *)) - 1) - scopesettings.timeperdiv;
  x = ((c & 3) * 72) + 10;
  y = ((c >> 2) * 23) + ymp;//191

  //Highlight the selected item if short time mode
  if(scopesettings.timeperdiv>8) display_fill_rounded_rect(ACQ_MENU_XPOS + x, ACQ_MENU_YPOS + y, 68, 20, 2);

  for(i=0;i<((sizeof(time_div_texts) / sizeof(int8 *))-11);i++)
  {
    //Settings displayed from smallest to highest value
      c = ((sizeof(time_div_texts) / sizeof(int8 *)) -1) - i;

    //Check if the current text is the selected on
    if(c != scopesettings.timeperdiv)
    {
      //When not check if the current on is a viable candidate for full screen trace display
      if(viable_time_per_div[scopesettings.samplerate][c])
      {
        //Available but viable not selected texts in white
        display_set_fg_color(WHITE_COLOR);
      }
      else
      {
        //Not viable but available not selected texts in grey
        display_set_fg_color(0x00686868);
      }
    }
    else
    {
      //Selected texts in black
      display_set_fg_color(BLACK_COLOR);
    }

    //Calculate the position of this text
    x = ((i & 3) * 72) + time_div_text_x_offsets[c];
    y = ((i >> 2) * 23) + ymp + 3;//194

    //Display the text from the table
    display_text(ACQ_MENU_XPOS + x, ACQ_MENU_YPOS + y, (int8 *)time_div_texts[c]);
  }
}

//----------------------------------------------------------------------------------------------------------------------------------

void scope_acquisition_ACQ_mode_select(void)
{
    //Set dark grey color for the boxes behind the not selected texts
    display_set_fg_color(LIGHTGREY_COLOR);
  
    //Clear the boxes for the not selected items (ACQ)
    display_fill_rounded_rect(ACQ_MENU_XPOS +   8, ACQ_MENU_YPOS + 163, 90, 40, 3);
                
    //Display Long/Short Mode text
    if (scopesettings.long_mode)
        {
        scopesettings.timeperdiv = 20;
        scopesettings.display_data_done = 1;
        //Send the time base command for the sort time base
        fpga_set_time_base(scopesettings.timeperdiv);
        //Set green color for the text
        display_set_fg_color(ORANGE_COLOR);
        display_text(ACQ_MENU_XPOS + 43, ACQ_MENU_YPOS +  168, "ACQ");
        display_text(ACQ_MENU_XPOS + 24, ACQ_MENU_YPOS +  182, "Long mode");
        scopesettings.long_mode = 0;
        }
        else 
        {
        scopesettings.timeperdiv = 8;
        //Send the time base command for the long time base
        fpga_set_long_timebase(scopesettings.timeperdiv);
        display_set_fg_color(GREEN_COLOR);
        display_text(ACQ_MENU_XPOS + 43, ACQ_MENU_YPOS +  168, "ACQ");
        display_text(ACQ_MENU_XPOS + 20, ACQ_MENU_YPOS +  182, "Short mode");
        scopesettings.long_mode = 1;
        }
          
    //if((scopesettings.runstate))
    //{
        //Set the sample rate that belongs to the selected time per div setting
        scopesettings.samplerate = time_per_div_sample_rate[scopesettings.timeperdiv];  

        //Set the new setting in the FPGA
        fpga_set_sample_rate(scopesettings.samplerate);
    //}
     
    //scope_open_acquisition_menu();
    //Update the top menu bar display
    scope_acqusition_settings(0);
                
    //Display the actual settings
    scope_acquisition_speed_select();
    scope_acquisition_timeperdiv_select();
    scope_acquisition_Average_mode_select();
    //scope_acquisition_Long_memory_select();
}

//----------------------------------------------------------------------------------------------------------------------------------

void scope_acquisition_Average_mode_select(void)
{                  
    if ((scopesettings.average_mode)&&(!scopesettings.long_mode))
    {
        //Set dark grey color for the boxes behind the not selected texts
        display_set_fg_color(GREEN_COLOR);
        //Clear the boxes for the not selected items (Average)
        display_fill_rounded_rect(ACQ_MENU_XPOS + 106, ACQ_MENU_YPOS + 163, 90, 40, 3);
        //Texts in black
        display_set_fg_color(BLACK_COLOR);        

    }
    else
    {
        //Set dark grey color for the boxes behind the not selected texts
        display_set_fg_color(LIGHTGREY_COLOR);
        //Clear the boxes for the not selected items (Average)
        display_fill_rounded_rect(ACQ_MENU_XPOS + 106, ACQ_MENU_YPOS + 163, 90, 40, 3);
        //Texts in white
        display_set_fg_color(WHITE_COLOR); 
    }
    
    //Select the font for the texts
    display_set_font(&font_2);
  
    //Display average text
    display_text(ACQ_MENU_XPOS + 128, ACQ_MENU_YPOS +  168, "Average");
    display_text(ACQ_MENU_XPOS + 136, ACQ_MENU_YPOS +  182, "mode");       

}

//----------------------------------------------------------------------------------------------------------------------------------
void scope_acquisition_Long_memory_select(void)
{                  
    if ((scopesettings.long_memory)&&(!scopesettings.long_mode))
    {
        //Set dark grey color for the boxes behind the not selected texts
        display_set_fg_color(GREEN_COLOR);
        //Clear the boxes for the not selected items (Long_memory)
        display_fill_rounded_rect(ACQ_MENU_XPOS + 204, ACQ_MENU_YPOS + 163, 90, 40, 3);
        //Texts in black
        display_set_fg_color(BLACK_COLOR);        

    }
    else
    {
        //Set dark grey color for the boxes behind the not selected texts
        display_set_fg_color(LIGHTGREY_COLOR);
        //Clear the boxes for the not selected items (Average)
        display_fill_rounded_rect(ACQ_MENU_XPOS + 204, ACQ_MENU_YPOS + 163, 90, 40, 3);
        //Texts in white
        display_set_fg_color(WHITE_COLOR); 
    }
    
    /*
     * 
     *   //Clear the boxes for the not selected items (ACQ, Average, Long memory)
  display_fill_rect(ACQ_MENU_XPOS +   8, ACQ_MENU_YPOS + 163, 90, 40);
  //display_fill_rect(ACQ_MENU_XPOS + 106, ACQ_MENU_YPOS + 163, 90, 40);
  display_fill_rect(ACQ_MENU_XPOS + 204, ACQ_MENU_YPOS + 163, 90, 40);
     * 
     * 
       if (scopesettings.long_memory)
  //Set black color for the text
  display_set_fg_color(BLACK_COLOR); else display_set_fg_color(WHITE_COLOR);
  display_text(ACQ_MENU_XPOS + 235, ACQ_MENU_YPOS +  168, "Long");
  display_text(ACQ_MENU_XPOS + 225, ACQ_MENU_YPOS +  182, "memory");
     
     */
    
    //Select the font for the texts
    display_set_font(&font_2);
  
    //Display long memory text
    display_text(ACQ_MENU_XPOS + 235, ACQ_MENU_YPOS +  168, "Long");
    display_text(ACQ_MENU_XPOS + 225, ACQ_MENU_YPOS +  182, "memory");     

}

//----------------------------------------------------------------------------------------------------------------------------------

void scope_open_trigger_menu(void)
{
  //Setup the menu in a separate buffer to be able to slide it onto the screen
  display_set_screen_buffer(displaybuffertmp);//1

  //Draw the background in dark grey
  display_set_fg_color(DARKGREY_COLOR);

  //Fill the background
  //display_fill_rect(560, 46, 166, 246);//186
  display_fill_rect(TRIGGER_MENU_XPOS, TRIGGER_MENU_YPOS, TRIGGER_MENU_WIDTH, TRIGGER_MENU_HEIGHT);

  //Draw the edge in a lighter grey
  display_set_fg_color(LIGHTGREY_COLOR);

  //Draw the edge
  //display_draw_rect(560, 46, 166, 246);//186
  display_draw_rect(TRIGGER_MENU_XPOS, TRIGGER_MENU_YPOS, TRIGGER_MENU_WIDTH, TRIGGER_MENU_HEIGHT);

  //Two black lines between the settings
  display_set_fg_color(BLACK_COLOR);
  display_draw_horz_line(107, 570, 716);//722
  display_draw_horz_line(168, 570, 716);//722
  display_draw_horz_line(229, 570, 716);//722

  //Main texts in white
  display_set_fg_color(WHITE_COLOR);

  //Select the font for the texts
  display_set_font(&font_3);

  //Display the texts
  display_text(570,  56, "Trigger");
  display_text(570,  75, "mode");//570
  display_text(570, 118, "Trigger");
  display_text(570, 137, "edge");
  display_text(570, 182, "Trigger");
  display_text(570, 200, "channel");
  display_text(570, 245, "Hold");
  display_text(570, 263, "on");

  //Display the actual settings
  scope_trigger_mode_select();
  scope_trigger_edge_select();
  scope_trigger_channel_select();

  //Set source and target for getting it on the actual screen
  display_set_source_buffer(displaybuffertmp);//1
  display_set_screen_buffer((uint16 *)maindisplaybuffer);

  //Slide the image onto the actual screen. The speed factor makes it start fast and end slow, Smaller value makes it slower.
  //display_slide_top_rect_onto_screen(560, 46, 166, 246, 56415);//186
  display_slide_top_rect_onto_screen(TRIGGER_MENU_XPOS, TRIGGER_MENU_YPOS, TRIGGER_MENU_WIDTH, TRIGGER_MENU_HEIGHT, 56415);
}

//----------------------------------------------------------------------------------------------------------------------------------

void scope_trigger_mode_select(void)
{
  //Select the font for the texts
  display_set_font(&font_3);

  //Set dark grey color for the boxes behind the not selected texts
  display_set_fg_color(DARKGREY_COLOR);

  //Check which trigger mode is selected
  switch(scopesettings.triggermode)
  {
    case 0:                                 //AUTO
      //dark grey single and normal
      display_fill_rounded_rect(661, 57, 20, 38, 2);
      display_fill_rounded_rect(692, 57, 21, 38, 2);
      break;

    case 1:                                 //SINGLE
      //dark grey auto and normal
      display_fill_rounded_rect(628, 57, 22, 38, 2);//629 57 20 38
      display_fill_rounded_rect(692, 57, 21, 38, 2);
      break;

    default:                                //NORMAL
      //dark grey auto and single
      display_fill_rounded_rect(628, 57, 22, 38, 2);//629 57 20 38
      display_fill_rounded_rect(661, 57, 20, 38, 2);
      break;
  }

  //Set trigger color for the box behind the selected text
  display_set_fg_color(TRIGGER_COLOR);

  //Check if which trigger mode to highlight
  switch(scopesettings.triggermode)
  {
    case 0:
      //Highlight auto mode
      display_fill_rounded_rect(628, 57, 22, 38, 2);//629 57 20 38
      break;

    case 1:
      //Highlight single mode
      display_fill_rounded_rect(661, 57, 20, 38, 2);
      break;

    default:
      //Highlight normal mode
      display_fill_rounded_rect(692, 57, 21, 38, 2);
      break;
  }

  //Check if trigger mode is auto
  if(scopesettings.triggermode == 0)
  {
    //Yes so black auto text
    display_set_fg_color(BLACK_COLOR);
  }
  else
  {
    //When not selected check if in normal view mode
    if(scopesettings.waveviewmode)
    {  
      //Grey auto text when in waveform view mode
      display_set_fg_color(0x00606060);
    }
    else
    {
      //White auto text if in normal mode
      display_set_fg_color(WHITE_COLOR);
    }
  }

  //Display the auto text
  display_text(631, 58, "Au");
  display_text(633, 75, "to");

  //Check if trigger mode is single
  if(scopesettings.triggermode == 1)
  {
    //Yes so black single text
    display_set_fg_color(BLACK_COLOR);
  }
  else
  {
    //When not selected check if in normal view mode
    if(scopesettings.waveviewmode)
    {
      //Grey single text when in waveform view mode
      display_set_fg_color(0x00606060);
    }
    else
    {
      //White single text if in normal mode
      display_set_fg_color(WHITE_COLOR);
    }
  }

  //Display the single text
  display_text(664, 56, "Si");//666
  display_text(663, 66, "ng");
  display_text(665, 79, "le");

  //Check if trigger mode is normal
  if(scopesettings.triggermode > 1)
  {
    //Yes so black normal text
    display_set_fg_color(BLACK_COLOR);
  }
  else
  {
    //When not selected check if in normal view mode
    if(scopesettings.waveviewmode)
    {
      //Grey normal text when in waveform view mode
      display_set_fg_color(0x00606060);        
    }
    else
    {
      //White normal text if in normal mode
      display_set_fg_color(WHITE_COLOR);
    }
  }

  //Display the normal text
  display_text(694, 56, "No");//695
  display_text(694, 66, "rm");
  display_text(696, 79, "al");
}

//----------------------------------------------------------------------------------------------------------------------------------

void scope_trigger_edge_select(void)
{
  //Select the font for the texts
  display_set_font(&font_3);

  //Set dark grey color for the box behind the not selected text
  display_set_fg_color(DARKGREY_COLOR);

  //Check which trigger edge is selected
  if(scopesettings.triggeredge == 0)
  {
    //Rising so dark grey box behind falling
    display_fill_rounded_rect(670, 125, 47, 22, 2);
  }
  else
  {
    //Falling so dark grey box behind rising
    display_fill_rounded_rect(625, 125, 42, 22, 2);
  }

  //Set trigger color for the box behind the selected text
  display_set_fg_color(TRIGGER_COLOR);

  //Check which trigger edge is selected
  if(scopesettings.triggeredge == 0)
  {
    //Rising so trigger color box behind rising
    display_fill_rounded_rect(625, 125, 42, 22, 2);//626 125 40 22
  }
  else
  {
    //Falling so trigger color box behind falling
    display_fill_rounded_rect(670, 125, 47, 22, 2);//671 125 45 22
  }

  //Check which trigger edge is selected
  if(scopesettings.triggeredge == 0)
  {
    //Rising so black rising text
    display_set_fg_color(BLACK_COLOR);
  }
  else
  {
    //For falling edge trigger check if in normal view mode
    if(scopesettings.waveviewmode)
    {
      //Grey text when in waveform view mode
      display_set_fg_color(0x00606060);        
    }
    else
    {
      //White text if in normal mode
      display_set_fg_color(WHITE_COLOR);
    }
  }

  //Display the rising text
  display_text(627, 127, "Rising");//629

  //Check which trigger edge is selected
  if(scopesettings.triggeredge == 0)
  {
    //For rising edge trigger check if in normal view mode
    if(scopesettings.waveviewmode)
    {
      //Grey text when in waveform view mode
      display_set_fg_color(0x00606060);        
    }
    else
    {
      //White text if in normal mode
      display_set_fg_color(WHITE_COLOR);
    }
  }
  else
  {
    //Falling so black falling text
    display_set_fg_color(BLACK_COLOR);
  }

  //Display the falling text
  display_text(672, 127, "Falling");//672//674
}

//----------------------------------------------------------------------------------------------------------------------------------

void scope_trigger_channel_select(void)
{
  //Select the font for the texts
  display_set_font(&font_3);

  //Set dark grey color for the box behind the not selected text
  display_set_fg_color(DARKGREY_COLOR);

  //Check if channel is 1 or 2
  if(scopesettings.triggerchannel == 0)
  {
    //1 so dark grey box behind CH2 text
    display_fill_rounded_rect(680, 188, 32, 22, 2);
  }
  else
  {
    //2 so dark grey box behind CH1 text
    display_fill_rounded_rect(632, 188, 32, 22, 2);
  }

  //Set trigger color for the box behind the selected text
  //display_set_fg_color(TRIGGER_COLOR);

  //Check if channel is 1 or 2
  if(scopesettings.triggerchannel == 0)
  {
    //Set channel color for the box behind the selected text
    display_set_fg_color(CHANNEL1_COLOR);
    //1 so trigger color box behind CH1 text
    display_fill_rounded_rect(632, 188, 32, 22, 2);
  }
  else
  {
      display_set_fg_color(CHANNEL2_COLOR);
    //2 so trigger color box behind CH2 text
    display_fill_rounded_rect(680, 188, 32, 22, 2);
  }

  //Check if channel is 1 or 2
  if(scopesettings.triggerchannel == 0)
  {
    //1 so black CH1 text
    display_set_fg_color(BLACK_COLOR);
  }
  else
  {
    //2 so white or grey CH1 text
    if((!scopesettings.waveviewmode) && scopesettings.channel1.enable)
    {
      //White text if in normal mode
      display_set_fg_color(WHITE_COLOR);
    }
    else
    {
      //Grey text when in waveform view mode
      display_set_fg_color(0x00606060);
    }
  }

  //Display the CH1 text
  display_text(635, 191, "CH1");

  //Check if channel is 1 or 2
  if(scopesettings.triggerchannel == 0)
  {
    //1 so white or grey CH2 text
    if((!scopesettings.waveviewmode) && scopesettings.channel2.enable)
    {
      //White text if in normal mode
      display_set_fg_color(WHITE_COLOR);
    }
    else
    {
      //Grey text when in waveform view mode
      display_set_fg_color(0x00606060);
    }
  }
  else
  {
    //2 so black CH2 text
    display_set_fg_color(BLACK_COLOR);
  }

  //Display the CH2 text
  display_text(683, 191, "CH2");
}

//----------------------------------------------------------------------------------------------------------------------------------

void scope_hold_on_select(void)
{
  //Select the font for the texts
  display_set_font(&font_3);

  //Set dark grey color for the box behind the not selected text
  display_set_fg_color(DARKGREY_COLOR);

  //Check if channel is 1 or 2
  if(scopesettings.triggerchannel == 0)
  {
    //1 so dark grey box behind CH2 text
    display_fill_rect(680, 188, 32, 22);
  }
  else
  {
    //2 so dark grey box behind CH1 text
    display_fill_rect(632, 188, 32, 22);
  }

  //Set trigger color for the box behind the selected text
  //display_set_fg_color(TRIGGER_COLOR);

  //Check if channel is 1 or 2
  if(scopesettings.triggerchannel == 0)
  {
    //Set channel color for the box behind the selected text
    display_set_fg_color(CHANNEL1_COLOR);
    //1 so trigger color box behind CH1 text
    display_fill_rect(632, 188, 32, 22);
  }
  else
  {
      display_set_fg_color(CHANNEL2_COLOR);
    //2 so trigger color box behind CH2 text
    display_fill_rect(680, 188, 32, 22);
  }

  //Check if channel is 1 or 2
  if(scopesettings.triggerchannel == 0)
  {
    //1 so black CH1 text
    display_set_fg_color(BLACK_COLOR);
  }
  else
  {
    //2 so white or grey CH1 text
    if((!scopesettings.waveviewmode) && scopesettings.channel1.enable)
    {
      //White text if in normal mode
      display_set_fg_color(WHITE_COLOR);
    }
    else
    {
      //Grey text when in waveform view mode
      display_set_fg_color(0x00606060);
    }
  }

  //Display the CH1 text
  display_text(635, 191, "CH1");

  //Check if channel is 1 or 2
  if(scopesettings.triggerchannel == 0)
  {
    //1 so white or grey CH2 text
    if((!scopesettings.waveviewmode) && scopesettings.channel2.enable)
    {
      //White text if in normal mode
      display_set_fg_color(WHITE_COLOR);
    }
    else
    {
      //Grey text when in waveform view mode
      display_set_fg_color(0x00606060);
    }
  }
  else
  {
    //2 so black CH2 text
    display_set_fg_color(BLACK_COLOR);
  }

  //Display the CH2 text
  display_text(683, 191, "CH2");
}

//----------------------------------------------------------------------------------------------------------------------------------

void scope_open_system_settings_menu(void)
{
  int y;

  //Setup the menu in a separate buffer to be able to slide it onto the screen
  display_set_screen_buffer(displaybuffertmp);

  //Draw the background in dark grey
  display_set_fg_color(DARKGREY_COLOR);

  //Fill the background
  display_fill_rect(150, 46, 244, 413);//353

  //Draw the edge in a lighter grey
  display_set_fg_color(LIGHTGREY_COLOR);

  //Draw the edge
  display_draw_rect(150, 46, 244, 413);//353

  //Five black lines between the settings
  display_set_fg_color(BLACK_COLOR);

  for(y=104;y<410;y+=59)//350
  {
    display_draw_horz_line(y, 159, 385);
  }

  //Display the menu items
  scope_system_settings_screen_brightness_item(0);
  scope_system_settings_grid_brightness_item(0);
  scope_system_settings_trigger_50_item();
  scope_system_settings_calibration_item(0);
  scope_system_settings_x_y_mode_item();
  scope_system_settings_confirmation_item();
  scope_system_settings_RTC_settings_item(0);

  //Set source and target for getting it on the actual screen
  display_set_source_buffer(displaybuffertmp);
  display_set_screen_buffer((uint16 *)maindisplaybuffer);

  //Slide the image onto the actual screen. The speed factor makes it start fast and end slow, Smaller value makes it slower.
  display_slide_left_rect_onto_screen(150, 46, 244, 413, 63039);//353
}

//----------------------------------------------------------------------------------------------------------------------------------

void scope_system_settings_screen_brightness_item(int mode)
{
  //Check if inactive or active
  if(mode == 0)
  {
    //Inactive so dark grey background
    display_set_fg_color(DARKGREY_COLOR);
  }
  else
  {
    //Active so yellow background
    display_set_fg_color(YELLOW_COLOR);
  }

  //Draw the background
  display_fill_rounded_rect(159, 58, 226, 36, 3);//159 59 226 36 3

  //Check if inactive or active
  if(mode == 0)
  {
    //Inactive so white foreground and grey background
    display_set_fg_color(WHITE_COLOR);
    display_set_bg_color(DARKGREY_COLOR);
  }
  else
  {    
    //Set the icon to white sun
    scope_maxlight_item(0);
    //switch from max light to user set value
    scope_set_maxlight(0);
    
    //Active so black foreground and yellow background
    display_set_fg_color(BLACK_COLOR);
    display_set_bg_color(YELLOW_COLOR);
  }

  //Display the icon with the set colors
  display_copy_icon_use_colors(screen_brightness_icon, 171, 64, 24, 24);

  //Display the text
  display_set_font(&font_3);
  display_text(231, 60, "Screen");
  display_text(220, 76, "brightness");

  //Show the actual setting
  scope_system_settings_screen_brightness_value();
}

//----------------------------------------------------------------------------------------------------------------------------------

void scope_system_settings_screen_brightness_value(void)
{
  //Draw the yellow background
  display_set_fg_color(YELLOW_COLOR);
  display_fill_rounded_rect(332, 67, 32, 15, 2);

  //Display the number with fixed width font and black color
  display_set_font(&font_0);
  display_set_fg_color(BLACK_COLOR);
  display_decimal(337, 68, scopesettings.screenbrightness);
}

//----------------------------------------------------------------------------------------------------------------------------------

void scope_system_settings_grid_brightness_item(int mode)
{
  //Check if inactive or active
  if(mode == 0)
  {
    //Inactive so dark grey background
    display_set_fg_color(DARKGREY_COLOR);
  }
  else
  {
    //Active so yellow background
    display_set_fg_color(YELLOW_COLOR);
  }

  //Draw the background
  display_fill_rounded_rect(159, 116, 226, 36, 3);

  //Check if inactive or active
  if(mode == 0)
  {
    //Inactive so white foreground and grey background
    display_set_fg_color(WHITE_COLOR);
    display_set_bg_color(DARKGREY_COLOR);
  }
  else
  {
    //Active so black foreground and yellow background
    display_set_fg_color(BLACK_COLOR);
    display_set_bg_color(YELLOW_COLOR);
  }

  //Display the icon with the set colors
  display_copy_icon_use_colors(grid_brightness_icon, 171, 122, 24, 24);

  //Display the text
  display_set_font(&font_3);
  display_text(240, 118, "Grid");
  display_text(220, 134, "brightness");

  //Show the actual setting
  scope_system_settings_grid_brightness_value();
}

//----------------------------------------------------------------------------------------------------------------------------------

void scope_system_settings_grid_brightness_value(void)
{
  //Draw the yellow background
  display_set_fg_color(YELLOW_COLOR);
  display_fill_rounded_rect(332, 124, 32, 15, 2);
  
   //Display the number with fixed width font and black color
  display_set_font(&font_0);
  display_set_fg_color(BLACK_COLOR);
  display_decimal(337, 125, scopesettings.gridbrightness);
}

//----------------------------------------------------------------------------------------------------------------------------------

void scope_system_settings_trigger_50_item(void)
{
  //Set the colors for white foreground and grey background
  display_set_fg_color(WHITE_COLOR);
  display_set_bg_color(DARKGREY_COLOR);

  //Display the icon with the set colors
  display_copy_icon_use_colors(trigger_50_percent_icon, 171, 181, 24, 24);

  //Display the text
  display_set_font(&font_3);
  display_text(229, 178, "Always");
  display_text(217, 194, "trigger 50%");

  //Show the state
  scope_display_slide_button(326, 183, scopesettings.alwaystrigger50, GREEN_COLOR);
}

//----------------------------------------------------------------------------------------------------------------------------------

void scope_system_settings_calibration_item(int mode)
{
  //Check if inactive or active
  if(mode == 0)
  {
    //Inactive so dark grey background
    display_set_fg_color(DARKGREY_COLOR);
  }
  else
  {
    //Active so yellow background
    display_set_fg_color(YELLOW_COLOR);
  }

  //Draw the background
  display_fill_rounded_rect(159, 235, 226, 36, 3);

  //Check if inactive or active
  if(mode == 0)
  {
    //Inactive so white foreground and grey background
    display_set_fg_color(WHITE_COLOR);
    display_set_bg_color(DARKGREY_COLOR);
  }
  else
  {
    //Active so black foreground and yellow background
    display_set_fg_color(BLACK_COLOR);
    display_set_bg_color(YELLOW_COLOR);
  }

  //Display the icon with the set colors
  display_copy_icon_use_colors(baseline_calibration_icon, 171, 239, 24, 25);

  //Display the text
  display_set_font(&font_3);
  display_text(225, 237, "Baseline");
  display_text(219, 253, "calibration");
}

//----------------------------------------------------------------------------------------------------------------------------------

void scope_system_settings_x_y_mode_item(void)
{
  //Set the colors for white foreground and grey background
  display_set_fg_color(WHITE_COLOR);
  display_set_bg_color(DARKGREY_COLOR);

  //Display the icon with the set colors
  display_copy_icon_use_colors(x_y_mode_display_icon, 171, 299, 24, 24);

  //Display the text
  display_set_font(&font_3);
  display_text(223, 295, "X-Y mode");
  display_text(231, 311, "display");

  //Show the state
  scope_display_slide_button(326, 299, scopesettings.xymodedisplay, GREEN_COLOR);
}

//----------------------------------------------------------------------------------------------------------------------------------

void scope_system_settings_confirmation_item(void)
{
  //Set the colors for white foreground and grey background
  display_set_fg_color(WHITE_COLOR);
  display_set_bg_color(DARKGREY_COLOR);

  //Display the icon with the set colors
  display_copy_icon_use_colors(confirmation_icon, 171, 358, 24, 24);//356

  //Display the text
  display_set_font(&font_3);
  display_text(217, 354, "Notification");
  display_text(213, 370, "confirmation");

  //Show the state
  scope_display_slide_button(326, 358, scopesettings.confirmationmode, GREEN_COLOR);
}

//----------------------------------------------------------------------------------------------------------------------------------
void scope_system_settings_RTC_settings_item(int mode)//set
{
  //Check if inactive or active
  if(mode == 0)
  {
    //Inactive so dark grey background
    display_set_fg_color(DARKGREY_COLOR);
  }
  else
  {
    //Active so yellow background
    display_set_fg_color(YELLOW_COLOR);
  }

  //Draw the background
  display_fill_rounded_rect(159, 410, 226, 38, 3);//401 36

  //Check if inactive or active
  if(mode == 0)
  {
    //Inactive so white foreground and grey background
    display_set_fg_color(WHITE_COLOR);
    display_set_bg_color(DARKGREY_COLOR);
  }
  else
  {
    //Active so black foreground and yellow background
    display_set_fg_color(BLACK_COLOR);
    display_set_bg_color(YELLOW_COLOR);
  }

  //Display the icon with the set colors
  display_copy_icon_use_colors(RTC_icon, 171, 417, 24, 24);

  //Display the text
  display_set_font(&font_3);
  display_text(241, 413, "RTC settings");
  display_text(220, 429, "Time & Date settings");
  
  //Read time for RTC
  get_fattime();
}

//----------------------------------------------------------------------------------------------------------------------------------

void scope_open_RTC_settings_menu(void)
{
  int y;

  //Setup the menu in a separate buffer to be able to slide it onto the screen
  display_set_screen_buffer(displaybuffertmp);

  //Draw the background in dark grey
  display_set_fg_color(DARKGREY_COLOR);

  //Fill the background
  display_fill_rect(395, 106, 200, 353);//150  46 244 353

  //Draw the edge in a lighter grey
  display_set_fg_color(LIGHTGREY_COLOR);

  //Draw the edge
  display_draw_rect(395, 106, 200, 353);//46 413 353

  //Five black lines between the settings
  display_set_fg_color(BLACK_COLOR);

  for(y=163;y<410;y+=59)//104 350
  {
    display_draw_horz_line(y, 404, 585);//159 385 630
  }

  //Display the menu items
  scope_system_settings_hour_item();
  scope_system_settings_minute_item();
  scope_system_settings_day_item();
  scope_system_settings_month_item();
  scope_system_settings_year_item();
  scope_system_settings_ONOFF_RTC_item();

  //Set source and target for getting it on the actual screen
  display_set_source_buffer(displaybuffertmp);
  display_set_screen_buffer((uint16 *)maindisplaybuffer);

  //Slide the image onto the actual screen. The speed factor makes it start fast and end slow, Smaller value makes it slower.
  display_slide_left_rect_onto_screen(395, 106, 200, 353, 63039);//46 244 353
}

//----------------------------------------------------------------------------------------------------------------------------------

void scope_system_settings_hour_item(void)
{
  //Grey color for inactive button
  display_set_fg_color(GREY_COLOR);
  
  //Draw the body of the button < and >
  display_fill_rounded_rect(415, 115, 35, 35, 2);//739 5 51 50
  display_fill_rounded_rect(540, 115, 35, 35, 2);//739 5 51 50

  //Draw the edge
  display_set_fg_color(0x00606060);
  display_draw_rounded_rect(415, 115, 35, 35, 2);
  display_draw_rounded_rect(540, 115, 35, 35, 2);

  display_set_fg_color(WHITE_COLOR);
  display_set_bg_color(GREY_COLOR);
  
  //Display the icon with the set colors
  display_copy_icon_use_colors(Left_icon, 423, 120, 24, 24);
  display_copy_icon_use_colors(Right_icon, 544, 120, 24, 24);
  
  //Set the colors for white foreground and gray background
   display_set_bg_color(DARKGREY_COLOR);
   
  //Display the text
  display_set_font(&font_3);
  display_text(476, 119, "Hours");//475

  //Show the state
  scope_system_settings_hour_item_value();
}
void scope_system_settings_hour_item_value(void)
{
  //Draw the gray background
  display_set_fg_color(DARKGREY_COLOR);
  display_fill_rect(488, 136, 32, 15);
  
  //Set the colors for white foreground and gray background
  display_set_fg_color(WHITE_COLOR);
  display_set_bg_color(DARKGREY_COLOR);

   //Display the text
  display_set_font(&font_3);
  readnameRTC();
  display_decimal(488, 136, hour);
}

//----------------------------------------------------------------------------------------------------------------------------------

void scope_system_settings_minute_item(void)
{
  //Grey color for inactive button
  display_set_fg_color(GREY_COLOR);
  
  //Draw the body of the button < and >
  display_fill_rounded_rect(415, 174, 35, 35, 2);//739 5 51 50
  display_fill_rounded_rect(540, 174, 35, 35, 2);//739 5 51 50

  //Draw the edge
  display_set_fg_color(0x00606060);
  display_draw_rounded_rect(415, 174, 35, 35, 2);
  display_draw_rounded_rect(540, 174, 35, 35, 2);

  display_set_fg_color(WHITE_COLOR);
  display_set_bg_color(GREY_COLOR);
  
  //Display the icon with the set colors
  display_copy_icon_use_colors(Left_icon, 423, 179, 24, 24);
  display_copy_icon_use_colors(Right_icon, 544, 179, 24, 24);
  
  //Set the colors for white foreground and gray background
   display_set_bg_color(DARKGREY_COLOR);
   
  //Display the text
  display_set_font(&font_3);
  display_text(471, 178, "Minutes");//475

  //Show the state
  scope_system_settings_minute_item_value();
}
void scope_system_settings_minute_item_value(void)
{
  //Draw the gray background
  display_set_fg_color(DARKGREY_COLOR);
  display_fill_rect(488, 195, 32, 15);
  
  //Set the colors for white foreground and gray background
  display_set_fg_color(WHITE_COLOR);
  display_set_bg_color(DARKGREY_COLOR);

   //Display the text
  display_set_font(&font_3);
  readnameRTC();
  display_decimal(488, 195, minute);
}

//----------------------------------------------------------------------------------------------------------------------------------

void scope_system_settings_day_item(void)
{
  //Grey color for inactive button
  display_set_fg_color(GREY_COLOR);
  
  //Draw the body of the button < and >
  display_fill_rounded_rect(415, 233, 35, 35, 2);//739 5 51 50
  display_fill_rounded_rect(540, 233, 35, 35, 2);//739 5 51 50

  //Draw the edge
  display_set_fg_color(0x00606060);
  display_draw_rounded_rect(415, 233, 35, 35, 2);
  display_draw_rounded_rect(540, 233, 35, 35, 2);

  display_set_fg_color(WHITE_COLOR);
  display_set_bg_color(GREY_COLOR);
  
  //Display the icon with the set colors
  display_copy_icon_use_colors(Left_icon, 423, 238, 24, 24);
  display_copy_icon_use_colors(Right_icon, 544, 238, 24, 24);
  
  //Set the colors for white foreground and gray background
   display_set_bg_color(DARKGREY_COLOR);
   
  //Display the text
  display_set_font(&font_3);
  display_text(483, 237, "Day");//475

  //Show the state
  scope_system_settings_day_item_value();
}
void scope_system_settings_day_item_value(void)
{
  //Draw the gray background
  display_set_fg_color(DARKGREY_COLOR);
  display_fill_rect(488, 254, 32, 15);
  
  //Set the colors for white foreground and gray background
  display_set_fg_color(WHITE_COLOR);
  display_set_bg_color(DARKGREY_COLOR);

   //Display the text
  display_set_font(&font_3);
  readnameRTC();
  display_decimal(488, 254, day);
}

//----------------------------------------------------------------------------------------------------------------------------------

void scope_system_settings_month_item(void)
{
  //Grey color for inactive button
  display_set_fg_color(GREY_COLOR);
  
  //Draw the body of the button < and >
  display_fill_rounded_rect(415, 291, 35, 35, 2);//739 5 51 50
  display_fill_rounded_rect(540, 291, 35, 35, 2);//739 5 51 50

  //Draw the edge
  display_set_fg_color(0x00606060);
  display_draw_rounded_rect(415, 291, 35, 35, 2);
  display_draw_rounded_rect(540, 291, 35, 35, 2);

  display_set_fg_color(WHITE_COLOR);
  display_set_bg_color(GREY_COLOR);
  
  //Display the icon with the set colors
  display_copy_icon_use_colors(Left_icon, 423, 296, 24, 24);
  display_copy_icon_use_colors(Right_icon, 544, 296, 24, 24);
  
  //Set the colors for white foreground and gray background
   display_set_bg_color(DARKGREY_COLOR);
 
  //Display the text
  display_set_font(&font_3);
  display_text(475, 295, "Month");//241

  //Show the state
  scope_system_settings_month_item_value();
}
void scope_system_settings_month_item_value(void)
{
  //Draw the gray background
  display_set_fg_color(DARKGREY_COLOR);
  display_fill_rect(488, 312, 32, 15);
  
  //Set the colors for white foreground and gray background
  display_set_fg_color(WHITE_COLOR);
  display_set_bg_color(DARKGREY_COLOR);

   //Display the text
  display_set_font(&font_3);
  readnameRTC();
  display_decimal(488, 312, month);
}

//----------------------------------------------------------------------------------------------------------------------------------

void scope_system_settings_year_item(void)
{
  //Grey color for inactive button
  display_set_fg_color(GREY_COLOR);
  
  //Draw the body of the button < and >
  display_fill_rounded_rect(415, 350, 35, 35, 2);//739 5 51 50
  display_fill_rounded_rect(540, 350, 35, 35, 2);//739 5 51 50

  //Draw the edge
  display_set_fg_color(0x00606060);
  display_draw_rounded_rect(415, 350, 35, 35, 2);
  display_draw_rounded_rect(540, 350, 35, 35, 2);

  display_set_fg_color(WHITE_COLOR);
  display_set_bg_color(GREY_COLOR);
  
  //Display the icon with the set colors
  display_copy_icon_use_colors(Left_icon, 423, 355, 24, 24);
  display_copy_icon_use_colors(Right_icon, 544, 355, 24, 24);
  
  //Set the colors for white foreground and gray background
  display_set_bg_color(DARKGREY_COLOR);

  //Display the text
  display_set_font(&font_3);
  display_text(481, 354, "Year");//241
  display_text(481, 371, "20");
  //Show the state
  scope_system_settings_year_item_value();
}
void scope_system_settings_year_item_value(void)
{
  //char buffer_time_date[]={'2','0','2','3',0}; 
     //Draw the gray background
  display_set_fg_color(DARKGREY_COLOR);
  display_fill_rect(495, 371, 32, 15);
  
  //Set the colors for white foreground and gray background
  display_set_fg_color(WHITE_COLOR);
  display_set_bg_color(DARKGREY_COLOR);

   //Display the text
  display_set_font(&font_3);
  readnameRTC();
  display_decimal(496, 371, year);
}

//----------------------------------------------------------------------------------------------------------------------------------

void scope_system_settings_ONOFF_RTC_item(void)
{
  //Set the colors for white foreground and grey background
  display_set_fg_color(WHITE_COLOR);
  display_set_bg_color(DARKGREY_COLOR);

  //Display the text
  display_set_font(&font_3);
  display_text(415, 420, "RTC On/Off");//410 241

  //Show the state
  scope_display_slide_button(530, 419, onoffRTC, GREEN_COLOR);
}

//----------------------------------------------------------------------------------------------------------------------------------

void scope_open_diagnostic_view(void)
{
  //Setup the menu in a separate buffer to be able to slide it onto the screen
  //display_set_screen_buffer(displaybuffertmp);
  //******************************************
//  uint32 *ptr = STARTUP_CONFIG_ADDRESS; //for diagnostic menu  
//  boot_menu_start = ptr[0] & 0x07;  //choice 1 for fnirsi firmware, 0 for peco firmware, 2 for FEL mode, 4 view choice menu
        
  const uint16    a=20;     //280
  const uint16    b=100;    //360
  const uint16    c=200;    //460
  const uint16    d=b+40;
  //y
  const uint16    i=65; //40
  const uint16    j=225;//200
  const uint16    k=328;//385
  
  uint16    b1;
  uint8    temp;
  uint8    tmp;
  
  //Clear the whole screen
  display_set_fg_color(BLACK_COLOR);
  //display_fill_rect(265, 0, 535, 480);//400
  display_fill_rect(0, 0, 800, 480);//400
  
  //Draw the edge
  display_set_fg_color(WHITE_COLOR);
  display_draw_rounded_rect(10, 10, 780, 460, 3);  //4
  display_draw_rounded_rect(11, 11, 778, 458, 3);  //4
  //  display_draw_rounded_rect(270, 10, 520, 460, 3);  //4
  //display_draw_rounded_rect(271, 11, 518, 458, 3);  //4
  //display_draw_vert_line(575, 10, 468);
  display_draw_vert_line(315, 10, 468);
  display_draw_vert_line(515, 10, 468);
  
  display_draw_horz_line(350, 10, 315);
  
  
  //Display the text with the green color
  display_set_fg_color(GREEN_COLOR);
  display_set_font(&font_3);
  display_text(a, 20, VERSION_STRING);  //c//460      //Show version information 
  
  //Display the temperature with darkgreen color
  //display_set_font(&font_3);
  display_set_fg_color(DARKGREEN_COLOR);
  display_text(b, 20, "Temperature");//j+135
  
  //Display the text with the white color
  display_set_fg_color(WHITE_COLOR);
  
  //display_text(85, 375, "Adjustment of calibration data"); //335 390
  //display_text(120, 375, "Reset or restore of calibration data"); //x70 //335 390
  display_text(37, 385, "Settings"); //x70 //335 390
  display_text(160, 370, "Reset or restore"); //x70 //335 390
  display_text(150, 385, "of calibration data"); //x70 //335 390
  
  
  display_text(k+57, 42,  "New autosetup");      //575 425
  display_text(k+57, 82,  "DC shift calibration");     
  display_text(k+57, 122, "Boot menu");     
  display_text(k+57, 162, "Default start"); 
  display_text(k+57, 202, "Lock move cursors"); 
    
  temp = readtemperature();
  
  //Display sign (-)
  if (!(temp & 0x80)) b1=c; else {b1=c+10; display_text(c, 20, "-");}
  
  display_decimal(b1, 20, temp & 0x7F);
  display_text(b1+20, 20, "C"); 
 /* 
  //Display the text with font 3 and the red color
  display_set_fg_color(RED_COLOR);
  display_set_font(&font_3);
  
  //display_text(435, 414, "Reset calibration values !!!");
  display_text(473, 390, "RESET");                      //670 260
  //display_text(320, 425, "calibration values !!!");     //630 220
  
  display_set_fg_color(DARKGREEN_COLOR);
  display_text(363, 390, "RESTORE"); 
 */ 
  
  //display_text(435, 414, "Reset calibration values !!!");
          //display_text(75, 390, "Please run BASE CALIBRATION !!!"); //335 390 
          //display_text(435, 414, "Reset calibration values !!!");
          //display_text(77, 380, "Adjustment of calibration data"); //335 390 
          
  //Display the text with the channel1 color
  display_set_fg_color(CHANNEL1_COLOR);
  display_text(a, i-20, "Channel 1");//b
  
  //Display the text with the channel2 color
  display_set_fg_color(CHANNEL2_COLOR);
  display_text(a, j-20, "Channel 2");//b
  
  
  display_set_fg_color(DARKGREEN_COLOR);
  display_text(a, i, "ADc comp"); 
  display_set_fg_color(RED_COLOR);
  display_text(a, i+30, "DC offset");
  display_set_fg_color(ORANGE_COLOR);
  display_text(a, i+60, "DC shift"); 
    
  //Display the text with the white color
  display_set_fg_color(WHITE_COLOR);
  display_decimal(a, i+15, scopesettings.channel1.adc1compensation);
  display_decimal(a+40, i+15, scopesettings.channel1.adc2compensation);
  display_decimal(a, i+45, scopesettings.channel1.dcoffset);
  display_decimal(a, i+75, scopesettings.channel1.dc_shift_center); 
  display_decimal(a, i+90, scopesettings.channel1.dc_shift_size); 
  display_decimal(a, i+105, scopesettings.channel1.dc_shift_value); 
  
  display_set_fg_color(DARKGREEN_COLOR);  
  display_text(a, j, "ADc comp"); 
  display_set_fg_color(RED_COLOR); 
  display_text(a, j+30, "DC offset");  
  display_set_fg_color(ORANGE_COLOR);
  display_text(a, j+60, "DC shift"); 
  
  //Display the text with the white color
  display_set_fg_color(WHITE_COLOR);    
  display_decimal(a, j+15, scopesettings.channel2.adc1compensation);
  display_decimal(a+40, j+15, scopesettings.channel2.adc2compensation);
  display_decimal(a, j+45, scopesettings.channel2.dcoffset);
  display_decimal(a, j+75, scopesettings.channel2.dc_shift_center); 
  display_decimal(a, j+90, scopesettings.channel2.dc_shift_size); 
  display_decimal(a, j+105, scopesettings.channel2.dc_shift_value);  
  
  display_set_fg_color(ORANGE_COLOR);
  display_text(b, i, "Offset");  
  display_text(c, i, "Calibration input"); 
  //Display the text with the white color
  display_set_fg_color(WHITE_COLOR);
  display_decimal(b, i+15, scopesettings.channel1.dc_calibration_offset[0]);
  display_text(d, i+15, "5V");     
  display_decimal(b, i+30, scopesettings.channel1.dc_calibration_offset[1]);
  display_decimal(b, i+45, scopesettings.channel1.dc_calibration_offset[2]);
  display_decimal(b, i+60, scopesettings.channel1.dc_calibration_offset[3]);
  display_decimal(b, i+75, scopesettings.channel1.dc_calibration_offset[4]);
  display_decimal(b, i+90, scopesettings.channel1.dc_calibration_offset[5]);
  display_decimal(b, i+105, scopesettings.channel1.dc_calibration_offset[6]);
  display_text(d, i+105, "50mV");
      
  
  display_decimal(c, i+15, scopesettings.channel1.input_calibration[0]);
  display_decimal(c, i+30, scopesettings.channel1.input_calibration[1]);
  display_decimal(c, i+45, scopesettings.channel1.input_calibration[2]);
  display_decimal(c, i+60, scopesettings.channel1.input_calibration[3]);
  display_decimal(c, i+75, scopesettings.channel1.input_calibration[4]);
  display_decimal(c, i+90, scopesettings.channel1.input_calibration[5]);
  display_decimal(c, i+105, scopesettings.channel1.input_calibration[6]);
      
  display_set_fg_color(ORANGE_COLOR);    
  display_text(b, j, "Offset");  
  display_text(c, j, "Calibration input");    //130 bolo
  //Display the text with the white color
  display_set_fg_color(WHITE_COLOR);
  display_decimal(b, j+15, scopesettings.channel2.dc_calibration_offset[0]);
  display_text(d, j+15, "5V"); 
  display_decimal(b, j+30, scopesettings.channel2.dc_calibration_offset[1]);
  display_decimal(b, j+45, scopesettings.channel2.dc_calibration_offset[2]);
  display_decimal(b, j+60, scopesettings.channel2.dc_calibration_offset[3]);
  display_decimal(b, j+75, scopesettings.channel2.dc_calibration_offset[4]);
  display_decimal(b, j+90, scopesettings.channel2.dc_calibration_offset[5]);
  display_decimal(b, j+105, scopesettings.channel2.dc_calibration_offset[6]);
  display_text(d, j+105, "50mV");
 
      
  display_decimal(c, j+15, scopesettings.channel2.input_calibration[0]);
  display_decimal(c, j+30, scopesettings.channel2.input_calibration[1]);
  display_decimal(c, j+45, scopesettings.channel2.input_calibration[2]);
  display_decimal(c, j+60, scopesettings.channel2.input_calibration[3]);
  display_decimal(c, j+75, scopesettings.channel2.input_calibration[4]);
  display_decimal(c, j+90, scopesettings.channel2.input_calibration[5]);
  display_decimal(c, j+105, scopesettings.channel2.input_calibration[6]);
/*
  //Display the text with the green color
  display_set_fg_color(GREEN_COLOR);
  display_text(c, j+135, "Xtouch");
  display_decimal(c+60, j+135, xtouch);
  display_text(c, j+150, "Ytouch");
  display_decimal(c+60, j+150, ytouch);
 */
  
  //Add the button for DEFAULT
  scope_default_button(30, 410, 0);   
  
  //Add the button for RESTORE
  scope_restore_button(130, 410, 0);   //b=100 //360 410 //350 410
          
  //Add the button for RESET
  scope_reset_button(230, 410, 0);    //c=200 //460 400
  
  //Show the state (autosetup button mode)
  scope_display_slide_button(k, 40, scopesettings.new_autosetup, GREEN_COLOR);//588
        
  //Show the state (dc_shift_cal button mode)
  scope_display_slide_button(k, 80, dc_shift_cal, GREEN_COLOR);//588
  
  //Show the state (boot menu button mode)
  tmp=((boot_menu_start & 0x04)>>2);
  scope_display_slide_button(k, 120, tmp, GREEN_COLOR);//588

  //Add the button for BOOT Default start
  scope_boot_button(k, 160, 0);//588
  
  //Show the state (lock cursors move)
  scope_display_slide_button(k, 200, scopesettings.lockcursors, GREEN_COLOR);//588
  
  //Add the button for EXIT
  scope_exit_button(710, 410, 0);
  
  //*********************************************************
  /*
  //Set source and target for getting it on the actual screen
  display_set_source_buffer(displaybuffertmp);
  display_set_screen_buffer((uint16 *)maindisplaybuffer);

  //Slide the image onto the actual screen. The speed factor makes it start fast and end slow, Smaller value makes it slower.
  //display_slide_left_rect_onto_screen(150, 46, 244, 413, 63039);//353
  display_slide_left_rect_onto_screen(265, 0, 535, 480, 63039);//353
*/
}

//----------------------------------------------------------------------------------------------------------------------------------

void scope_open_calibration_start_text(void)
{
  //Save the screen under the baseline calibration start text
  display_set_destination_buffer(displaybuffer3);//2
  display_copy_rect_from_screen(395, 222, 199, 59);

  //Setup the text in a separate buffer to be able to slide it onto the screen
  display_set_screen_buffer(displaybuffertmp);//1

  //Draw the background in yellow //white
  display_set_fg_color(YELLOW_COLOR);
  display_fill_rect(395, 222, 199, 59);

  //Draw the edge in a lighter grey
  display_set_fg_color(LIGHTGREY_COLOR);
  display_draw_rect(395, 222, 199, 59);

  //Display the text in white
  display_set_fg_color(RED_COLOR);
  display_set_font(&font_3);
  display_text(409, 227, "Please unplug");
  display_text(410, 243, "the probe and");
  display_text(424, 259, "USB first !");    //409

  //Add the ok button
  scope_display_ok_button(517, 230, 0);

  //Set source and target for getting it on the actual screen
  display_set_source_buffer(displaybuffertmp);//1
  display_set_screen_buffer((uint16 *)maindisplaybuffer);

  //Slide the image onto the actual screen. The speed factor makes it start fast and end slow, Smaller value makes it slower.
  display_slide_left_rect_onto_screen(395, 222, 199, 59, 63039);
}

//----------------------------------------------------------------------------------------------------------------------------------

void scope_show_calibrating_text(void)
{
  //Restore the screen from under the calibration start text to get rid of it
  display_set_source_buffer(displaybuffer3);//2
  display_copy_rect_to_screen(395, 222, 199, 59);

  //Draw the background in dark grey
  display_set_fg_color(DARKGREY_COLOR);
  display_fill_rect(395, 222, 109, 58);//110 59

  //Draw the edge in a lighter grey
  display_set_fg_color(LIGHTGREY_COLOR);
  display_draw_rect(395, 222, 110, 59);

  //Display the text in white
  display_set_fg_color(WHITE_COLOR);
  display_set_font(&font_3);
  display_text(409, 243, "Calibrating...");
}

//----------------------------------------------------------------------------------------------------------------------------------

void scope_show_calibration_done_text(void)
{
  //Draw the background in dark grey
  display_set_fg_color(DARKGREY_COLOR);
  display_fill_rect(395, 222, 109, 58);

  //Draw the edge in a lighter grey
  display_set_fg_color(LIGHTGREY_COLOR);
  display_draw_rect(395, 222, 110, 59);

  //Display the text in green
  display_set_fg_color(GREEN_COLOR);
  display_set_font(&font_3);
  
  //scope_save_input_calibration_data();
          
  display_text(414, 233, "Calibration");
  display_text(416, 255, "successful");
}

//----------------------------------------------------------------------------------------------------------------------------------

void scope_show_calibration_fail(void)
{
  //Draw the background in white
  display_set_fg_color(WHITE_COLOR);
  display_fill_rect(395, 222, 198, 58);//199 59

  //Draw the edge in a lighter grey
  display_set_fg_color(LIGHTGREY_COLOR);
  display_draw_rect(395, 222, 199, 59);

  //Display the text in red
  display_set_fg_color(RED_COLOR);
  display_set_font(&font_3);
  
  display_text(420, 233, "Offset calibration failed");
  display_text(445, 255, "Check hardware");
}

//-*********************************************************************************
void scope_show_Input_calibration(void)
{
  //Draw the background in dark grey
  display_set_fg_color(DARKGREY_COLOR);
  display_fill_rect(395, 222, 198, 58);//199 59

  //Draw the edge in a lighter grey
  display_set_fg_color(LIGHTGREY_COLOR);
  display_draw_rect(395, 222, 199, 59);

  //Display the text in white
  display_set_fg_color(WHITE_COLOR);
  display_set_font(&font_3);
  //display_text(435, 235, "Input");
  display_text(405, 233, "Input CH1 & CH2");
  display_text(421, 255, "calibration");
  
  //Add the ok button
  scope_display_ok_button(517, 230, 0);
}

//-----------------------------------------------------

void scope_show_calibrating_300mV_text(void)
{
  //Draw the background in dark grey
  display_set_fg_color(DARKGREY_COLOR);
  display_fill_rect(395, 222, 198, 58);

  //Draw the edge in a lighter grey
  display_set_fg_color(LIGHTGREY_COLOR);
  display_draw_rect(395, 222, 199, 59);

  //Display the text in white
  display_set_fg_color(WHITE_COLOR);
  display_set_font(&font_3);
  display_text(413, 233, "Set 300mV DC");//100mV/div 425 -12
  display_text(416, 255, "and press OK");
  
  //Add the ok button
  scope_display_ok_button(517, 230, 0);
}

//----------------------------------------------------------------------------------------------------------------------------------
void scope_show_calibrating_600mV_text(void)
{
  //Draw the background in dark grey
  display_set_fg_color(DARKGREY_COLOR);
  display_fill_rect(395, 222, 198, 58);

  //Draw the edge in a lighter grey
  display_set_fg_color(LIGHTGREY_COLOR);
  display_draw_rect(395, 222, 199, 59);

  //Display the text in white
  display_set_fg_color(WHITE_COLOR);
  display_set_font(&font_3);
  display_text(413, 233, "Set 600mV DC");//200mV/div 440
  display_text(416, 255, "and press OK");
  
  //Add the ok button
  scope_display_ok_button(517, 230, 0);
}

//----------------------------------------------------------------------------------------------------------------------------------
void scope_show_calibrating_1_5V_text(void)
{
  //Draw the background in dark grey
  display_set_fg_color(DARKGREY_COLOR);
  display_fill_rect(395, 222, 198, 58);

  //Draw the edge in a lighter grey
  display_set_fg_color(LIGHTGREY_COLOR);
  display_draw_rect(395, 222, 199, 59);

  //Display the text in white
  display_set_fg_color(WHITE_COLOR);
  display_set_font(&font_3);
  display_text(420, 233, "Set 1.5V DC");//500mV/div 430
  display_text(416, 255, "and press OK");
  
  //Add the ok button
  scope_display_ok_button(517, 230, 0);
}

//----------------------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------------------
void scope_show_calibrating_3V_text(void)
{
  //Draw the background in dark grey
  display_set_fg_color(DARKGREY_COLOR);
  display_fill_rect(395, 222, 198, 58);

  //Draw the edge in a lighter grey
  display_set_fg_color(LIGHTGREY_COLOR);
  display_draw_rect(395, 222, 199, 59);

  //Display the text in white
  display_set_fg_color(WHITE_COLOR);
  display_set_font(&font_3);
  display_text(425, 233, "Set 3V DC");  //3V/div
  display_text(416, 255, "and press OK");
  
  //Add the ok button
  scope_display_ok_button(517, 230, 0);
}

//----------------------------------------------------------------------------------------------------------------------------------

void scope_show_calibrating_7_5V_text(void)
{
  //Draw the background in dark grey
  display_set_fg_color(DARKGREY_COLOR);
  display_fill_rect(395, 222, 198, 58);

  //Draw the edge in a lighter grey
  display_set_fg_color(LIGHTGREY_COLOR);
  display_draw_rect(395, 222, 199, 59);

  //Display the text in white
  display_set_fg_color(WHITE_COLOR);
  display_set_font(&font_3);
  display_text(420, 233, "Set 7.5V DC");//2.5V/div 235
  display_text(416, 255, "and press OK");//252
  
  //Add the ok button
  scope_display_ok_button(517, 230, 0);
}

void scope_show_calibrating_15V_text(void)
{
  //Draw the background in dark grey
  display_set_fg_color(DARKGREY_COLOR);
  display_fill_rect(395, 222, 198, 58);

  //Draw the edge in a lighter grey
  display_set_fg_color(LIGHTGREY_COLOR);
  display_draw_rect(395, 222, 199, 59);

  //Display the text in white
  display_set_fg_color(WHITE_COLOR);
  display_set_font(&font_3);
  display_text(422, 233, "Set 15V DC");//5V/div
  display_text(416, 255, "and press OK");
  
  //Add the ok button
  scope_display_ok_button(517, 230, 0);
}

void scope_show_Input_calibration_done(void)
{
  //Draw the background in dark grey
  display_set_fg_color(DARKGREY_COLOR);
  display_fill_rect(395, 222, 198, 58);//199 59

  //Draw the edge in a lighter grey
  display_set_fg_color(LIGHTGREY_COLOR);
  display_draw_rect(395, 222, 199, 59);

  //Display the text in green
  display_set_fg_color(GREEN_COLOR);
  display_set_font(&font_3);
  
  display_text(425, 233, "Calibration successful");
  display_set_fg_color(RED_COLOR);
  display_text(455, 255, "Please restart");
}

void scope_show_Input_calibration_fail(void)
{
  //Draw the background in white
  display_set_fg_color(WHITE_COLOR);
  display_fill_rect(395, 222, 198, 58);//199 59

  //Draw the edge in a lighter grey
  display_set_fg_color(LIGHTGREY_COLOR);
  display_draw_rect(395, 222, 199, 59);

  //Display the text in red
  display_set_fg_color(RED_COLOR);
  display_set_font(&font_3);
  
  display_text(440, 233, "Calibration failed");//435
  //display_text(470, 255, "Try again");//475
  display_text(437, 255, "PLS Reset values !");//475
}

//-*******************************************************************************
//----------------------------------------------------------------------------------------------------------------------------------
/*
void scope_open_t_cursor_menu(void)
{
  //int item;
  //int channel;

  //Setup the menu in a separate buffer to be able to slide it onto the screen
  display_set_screen_buffer(displaybuffertmp);//1

  //Draw the background in dark grey
  display_set_fg_color(DARKGREY_COLOR);
  display_fill_rect(677, 185, 50, 50);

  //Draw the edge in black
  //display_set_fg_color(BLACK_COLOR);
  display_set_fg_color(WHITE_COLOR);
  display_draw_rect(677, 185, 50, 50);
  
  //Set source and target for getting it on the actual screen
  display_set_source_buffer(displaybuffertmp);
  display_set_screen_buffer((uint16 *)maindisplaybuffer);

  //Slide the image onto the actual screen. The speed factor makes it start fast and end slow, Smaller value makes it slower.
  display_slide_right_rect_onto_screen(677, 185, 50, 50, 46);//449
}
*/
//----------------------------------------------------------------------------------------------------------------------------------
/*
void scope_open_v_cursor_menu(void)
{
  //int item;
  //int channel;

  //Setup the menu in a separate buffer to be able to slide it onto the screen
  display_set_screen_buffer(displaybuffertmp);//1

  //Draw the background in dark grey
  display_set_fg_color(DARKGREY_COLOR);
  display_fill_rect(677, 245, 50, 50);

  //Draw the edge in black
  //display_set_fg_color(BLACK_COLOR);
  display_set_fg_color(WHITE_COLOR);
  display_draw_rect(677, 245, 50, 50);
  
  //Set source and target for getting it on the actual screen
  display_set_source_buffer(displaybuffertmp);
  display_set_screen_buffer((uint16 *)maindisplaybuffer);

  //Slide the image onto the actual screen. The speed factor makes it start fast and end slow, Smaller value makes it slower.
  display_slide_right_rect_onto_screen(677, 245, 50, 50, 46);//449
}
*/
//----------------------------------------------------------------------------------------------------------------------------------

void scope_open_measures_menu(void)
{
  int item;
  int channel;

  //Setup the menu in a separate buffer to be able to slide it onto the screen
  display_set_screen_buffer(displaybuffertmp);//1

  //Draw the background in dark grey
  display_set_fg_color(DARKGREY_COLOR);
  display_fill_rect(231, 263, 501, 215);//499

  //Draw the edge in black
  //display_set_fg_color(BLACK_COLOR);
  display_set_fg_color(WHITE_COLOR);
  display_draw_rect(231, 263, 501, 215);//499

  //Three horizontal black lines between the settings y, x, long
  display_set_fg_color(BLACK_COLOR);
  //display_set_fg_color(WHITE_COLOR);
  display_draw_horz_line(296, 232, 730);
  display_draw_horz_line(356, 232, 730);
  display_draw_horz_line(416, 232, 730);
  
  //Channel 1 top bar
  display_set_fg_color(CHANNEL1_COLOR);
  display_fill_rect(232, 264, 248, 31);

  //Channel 2 top bar
  display_set_fg_color(CHANNEL2_COLOR);
  display_fill_rect(482, 264, 248, 31);//246
  
  //If hide measures menu item is active change color Hide bar CH1 & CH2
  scope_hide_measures_menu_item();

  //Vertical separator between the channel sections
  //Draw the edge in white
  //display_set_fg_color(ORANGE_COLOR);
  display_set_fg_color(WHITE_COLOR);
  display_draw_vert_line(481, 264, 476);

  //Vertical separators between the items
  //Draw the edge in black
  display_set_fg_color(BLACK_COLOR);
  display_draw_vert_line(294, 264, 476);//294 289 476
  display_draw_vert_line(356, 264, 476);
  display_draw_vert_line(418, 264, 476);
  display_draw_vert_line(544, 264, 476);
  display_draw_vert_line(606, 264, 476);
  display_draw_vert_line(668, 264, 476);

  //Display the channel identifier text in black
  display_set_fg_color(BLACK_COLOR);
  display_set_font(&font_2);
  display_text(240, 273, "CH1");
  display_text(300, 273, "Select All");
  display_text(375, 273, "Clear");  //373
  
  display_text(490, 273, "CH2");
  display_text(550, 273, "Select All");
  display_text(625, 273, "Clear");  //623

  //Display the menu items
  for(channel=0;channel<2;channel++)
  {
    //For each channel 12 items
    for(item=0;item<12;item++)
    {
      //Draw the separate items
      scope_measures_menu_item(channel, item);
    }
  }

  //Set source and target for getting it on the actual screen
  display_set_source_buffer(displaybuffertmp);
  display_set_screen_buffer((uint16 *)maindisplaybuffer);

  //Slide the image onto the actual screen. The speed factor makes it start fast and end slow, Smaller value makes it slower.
  display_slide_right_rect_onto_screen(231, 263, 501, 215, 75646);//449
}

//----------------------------------------------------------------------------------------------------------------------------------

void scope_hide_measures_menu_item(void)
{
  //Channel 1 HIDE top bar
  if( scopesettings.hide_values_CH1) display_set_fg_color(RED_COLOR); else display_set_fg_color(CHANNEL1_COLOR);
  display_fill_rect(419, 264, 61, 31);

  //Channel 2 HIDE top bar
  if( scopesettings.hide_values_CH2) display_set_fg_color(RED_COLOR); else display_set_fg_color(CHANNEL2_COLOR);
  display_fill_rect(669, 264, 61, 31);   
  
  //Display text Hide in black
  display_set_fg_color(BLACK_COLOR);
  display_set_font(&font_2);
  display_text(440, 273, "Hide");  
  display_text(690, 273, "Hide");     
}

//----------------------------------------------------------------------------------------------------------------------------------

void scope_measures_menu_item(int channel, int item)
{
  uint16  xpos;
  uint16  ypos;
  char   *text;

  //Set the x position offset for the given channel
  if(channel == 0)
  {
    //Channel 1 is on the left side
    xpos = 232;
  }
  else
  {
    //Channel 2 is on the right side
    xpos = 482;
  }

  //Set the text and the position for the given item
  switch(item)
  {
    case 0:
      text  = "Vmax";
      xpos += 15;
      ypos  = 316;
      break;

    case 1:
      text  = "Vmin";
      xpos += 79;
      ypos  = 316;
      break;

    case 2:
      text  = "Vavg";
      xpos += 141;
      ypos  = 316;
      break;

    case 3:
      text  = "Vrms";
      xpos += 203;
      ypos  = 316;
      break;

    case 4:
      text  = "Vpp";
      xpos += 19;
      ypos  = 376;
      break;

    case 5:
      text  = "Vp";
      xpos += 86;
      ypos  = 376;
      break;

    case 6:
      text  = "Freq";
      xpos += 143;
      ypos  = 376;
      break;

    case 7:
      text  = "Cycle";
      xpos += 201;
      ypos  = 376;
      break;

    case 8:
      text  = "Tim+";
      xpos += 17;
      ypos  = 437;
      break;

    case 9:
      text  = "Tim-";
      xpos += 80;
      ypos  = 437;
      break;

    case 10:
      text  = "Duty+";
      xpos += 138;
      ypos  = 437;
      break;

    case 11:
      text  = "Duty-";
      xpos += 202;
      ypos  = 437;
      break;

    default:
      return;
  }

  //Check if item is on or off
  if(scopesettings.measuresstate[channel][item] == 0)
  {
    //Off so some dark grey text
    display_set_fg_color(GREY_COLOR);
  }
  else
  {
    //On so white text
    display_set_fg_color(WHITE_COLOR);
  }

  //Display the text
  display_set_font(&font_3);
  display_text(xpos, ypos, text);
}

//----------------------------------------------------------------------------------------------------------------------------------

void scope_open_slider(uint16 xpos, uint16 ypos, uint8 position)
{
  //Save the screen under the screen brightness slider
  display_set_destination_buffer(displaybuffer3);//2
  display_copy_rect_from_screen(xpos, ypos, 331, 58);

  //Setup the slider menu in a separate buffer to be able to slide it onto the screen
  display_set_screen_buffer(displaybuffertmp);

  //Draw the background in dark grey
  display_set_fg_color(DARKGREY_COLOR);

  //Fill the background
  display_fill_rect(xpos, ypos, 331, 58);

  //Draw the edge in a lighter grey
  display_set_fg_color(LIGHTGREY_COLOR);

  //Draw the edge
  display_draw_rect(xpos, ypos, 331, 58);

  //Display the actual slider
  scope_display_slider(xpos, ypos, position);

  //Set source and target for getting it on the actual screen
  display_set_source_buffer(displaybuffertmp);
  display_set_screen_buffer((uint16 *)maindisplaybuffer);

  //Slide the image onto the actual screen. The speed factor makes it start fast and end slow, Smaller value makes it slower.
  display_slide_left_rect_onto_screen(xpos, ypos, 331, 58, 63039);
}

//----------------------------------------------------------------------------------------------------------------------------------

void scope_display_slider(uint16 xpos, uint16 ypos, uint8 position)
{
  uint16 x = xpos + 20;
  uint16 y = ypos + 24;
  uint16 w = (291 * position) / 100;
  uint16 ys = ypos + 23;
  uint16 ye = ypos + 35;

  //Clear the background first
  display_set_fg_color(DARKGREY_COLOR);
  display_fill_rect(xpos + 8, ypos + 17, 315, 24);

  //Draw the first part of the slider bar in a yellow color
  display_set_fg_color(YELLOW_COLOR);
  display_fill_rounded_rect(x, y, w, 10, 2);

  //Adjust positions for the grey part
  x += w;
  w  = 291 - w;

  //Draw the last part of the slider bar in a light grey color
  display_set_fg_color(0x00666666);
  display_fill_rounded_rect(x, y, w, 10, 2);

  //Adjust positions for drawing the knob
  x -= 11;
  y -= 6;

  //Draw the knob
  display_set_fg_color(0x00AAAAAA);
  display_fill_rounded_rect(x, y, 22, 22, 2);

  //Draw the black lines on the knob
  display_set_fg_color(BLACK_COLOR);
  display_draw_vert_line(x +  6, ys, ye);
  display_draw_vert_line(x + 11, ys, ye);
  display_draw_vert_line(x + 16, ys, ye);
}

//----------------------------------------------------------------------------------------------------------------------------------

int32 scope_move_slider(uint16 xpos, uint16 ypos, uint8 *position)
{
  uint16 xs = xpos + 20;
  uint16 value;
  int16 filter = xtouch - prevxtouch;

  //Check if update needed
  if((filter > -3) && (filter < 3))
  {
    //When change in movement less the absolute 3 don't process
    return(0);
  }

  //Save for next filter check
  prevxtouch = xtouch;

  //Make sure it stays in allowed range
  if(xtouch <= xs)
  {
    //Below slider keep it on 0
    value = 0;
  }
  else if(xtouch >= (xpos + 311))
  {
    //Above slider keep it on max
    value = 100;
  }
  else
  {
    //Based on xtouch position calculate a new position from 0 to 100
    value = ((xtouch - xs) * 100) / 291;
  }

  //Update the position variable
  *position = value;

  //Show the new position on screen
  scope_display_slider(xpos, ypos, value);

  //Signal there is change
  return(1);
}

//----------------------------------------------------------------------------------------------------------------------------------

void scope_display_slide_button(uint16 xpos, uint16 ypos, uint8 state, uint32 color)
{
  uint16 linex      = xpos + 9;//8
  uint16 lineystart = ypos + 6;//6
  uint16 lineyend   = ypos + 14;//15
  uint16 buttonx    = xpos + 4;
  uint32 edgecolor  = GREY_COLOR;
  uint32 fillcolor  = 0x00888888;

  if(state == 1)
  {
    //Displace the lines and button by 19 pixels
    linex   += 17;//19
    buttonx += 17;//19

    //Set the enabled colors
    //edgecolor  = 0x00008800;
    //fillcolor  = GREEN_COLOR;
    edgecolor = color-0x7700;
    fillcolor  = color;
    
  }

  //Draw the background
  display_set_fg_color(fillcolor);
  display_fill_rounded_rect(xpos, ypos, 45, 21, 2);

  //Draw the edge
  display_set_fg_color(edgecolor);
  display_draw_rounded_rect(xpos, ypos, 45, 21, 2);

  //Draw button in dark grey
  display_set_fg_color(GREY_COLOR);
  display_fill_rect(buttonx, ypos + 4, 20, 13);//19 13

  //Draw lines in black
  display_set_fg_color(BLACK_COLOR);
  display_draw_vert_line(linex,     lineystart, lineyend);
  display_draw_vert_line(linex + 3, lineystart, lineyend);
  display_draw_vert_line(linex + 6, lineystart, lineyend);
  display_draw_vert_line(linex + 9, lineystart, lineyend);
}

//----------------------------------------------------------------------------------------------------------------------------------

void scope_display_ok_button(uint16 xpos, uint16 ypos, uint8 mode)
{
  //Check if inactive or active
  if(mode == 0)
  {
    //Inactive so light gray background
    display_set_fg_color(LIGHTGREY_COLOR);
  }
  else
  {
    //Active so light grey background
    display_set_fg_color(0x00CCCCCC);
  }

  //Draw the background
  display_fill_rect(xpos, ypos, 66, 44);

  //Check if inactive or active
  if(mode == 0)
  {
    //Inactive so white foreground
    display_set_fg_color(WHITE_COLOR);
  }
  else
  {
    //Active so black foreground
    display_set_fg_color(BLACK_COLOR);
  }

  //Display the text
  display_set_font(&font_3);
  display_text(xpos + 24, ypos + 14, "OK");
}

//----------------------------------------------------------------------------------------------------------------------------------

void scope_default_button(uint16 xpos, uint16 ypos, uint8 mode)
{
  //Check if inactive or active
  if(mode == 0)
  {
    //Inactive so dark orange background
    display_set_fg_color(ORANGE_COLOR);
  }
  else
  {
    //Active so light gray background
    display_set_fg_color(0x00CCCCCC);
  }

  //Draw the background
  display_fill_rect(xpos, ypos, 66, 44);

  //Black text
  display_set_fg_color(BLACK_COLOR);

  //Display the text
  display_set_font(&font_3);
  display_text(xpos + 3, ypos + 14, "DEFAULT");
  //display_text(xpos + 3, ypos + 14, "RESTORE");

}
//----------------------------------------------------------------------------------------------------------------------------------

void scope_restore_button(uint16 xpos, uint16 ypos, uint8 mode)
{
  //Check if inactive or active
  if(mode == 0)
  {
    //Inactive so dark dark green background
    display_set_fg_color(DARKGREEN_COLOR);
  }
  else
  {
    //Active so light gray background
    display_set_fg_color(0x00CCCCCC);
  }

  //Draw the background
  display_fill_rect(xpos, ypos, 66, 44);

  //Black text
  display_set_fg_color(BLACK_COLOR);

  //Display the text
  display_set_font(&font_3);
  display_text(xpos + 3, ypos + 14, "RESTORE");

}

//----------------------------------------------------------------------------------------------------------------------------------

void scope_reset_button(uint16 xpos, uint16 ypos, uint8 mode) //((xtouch >= 470) && (xtouch <= 520) && (ytouch >= 420) && (ytouch <= 460))
{
  //Check if inactive or active
  if(mode == 0)
  {
    //Inactive so red background
    display_set_fg_color(RED_COLOR);
  }
  else
  {
    //Active so light grey background
    display_set_fg_color(0x00CCCCCC);
  }

  //Draw the background
  display_fill_rect(xpos, ypos, 66, 44);

  //Black text
  display_set_fg_color(BLACK_COLOR);

  //Display the text
  display_set_font(&font_3);
  display_text(xpos + 13, ypos + 14, "RESET");

}

//----------------------------------------------------------------------------------------------------------------------------------

void scope_boot_button(uint16 xpos, uint16 ypos, uint8 mode)
{
  uint8 tmp;   
  
  //type firmware to view
  tmp = (boot_menu_start & 0x03);
  
  //Check if inactive or active
  if(mode == 0)
  {
    //Inactive so color
    if (tmp == 0) display_set_fg_color(GREEN_COLOR);    //Peco
    if (tmp == 1) display_set_fg_color(BLUE_COLOR);     //fnirsi    
    if (tmp == 2) display_set_fg_color(ORANGE_COLOR);   //FEL mode
  }
  else
  {
    //Active so light grey background
    display_set_fg_color(0x00CCCCCC);
  }

  //Draw the background
  display_fill_rect(xpos, ypos, 46, 22);

  //Black text
  display_set_fg_color(BLACK_COLOR);

  //Display the text
  display_set_font(&font_2);//3
  if (tmp == 0) display_text(xpos + 10, ypos + 4, "PECO"); else
  if (tmp == 1) display_text(xpos + 5,  ypos + 4, "FNIRSI"); else
  if (tmp == 2) display_text(xpos + 2,  ypos + 4, "FELmod");

}
//----------------------------------------------------------------------------------------------------------------------------------

void scope_exit_button(uint16 xpos, uint16 ypos, uint8 mode)
{
  //Check if inactive or active
  if(mode == 0)
  {
    //Inactive so white background
    display_set_fg_color(WHITE_COLOR);
  }
  else
  {
    //Active so green background
    display_set_fg_color(GREEN_COLOR);
  }

  //Draw the background
  display_fill_rect(xpos, ypos, 66, 44);

  //Black text
  display_set_fg_color(BLACK_COLOR);

  //Display the text
  display_set_font(&font_3);
  display_text(xpos + 20, ypos + 14, "EXIT");//13

}

//----------------------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------------------
/*
void scope_adjust_timebase(void)
{
  //Check if touch within the trace display region
  if((previousxtouch > 2) && (previousxtouch < 720) && (previousytouch > 50) && (previousytouch < 470))
  {
    //Check if touch on the left of the center line
    if(previousxtouch < 358)
    {
      //Check if not already on the highest setting (50S/div)
      if(scopesettings.timeperdiv > 0)
      {
        //Go up in time by taking one of the setting
        scopesettings.timeperdiv--;
      }
    }
    //Check if touch on the right of the center line
    else if(previousxtouch > 362)
    {
      //Check if not already on the lowest setting (10nS/div)
      if(scopesettings.timeperdiv < ((sizeof(time_div_texts) / sizeof(int8 *)) - 1))
      {
        //Go down in time by adding one to the setting
        scopesettings.timeperdiv++;
      }
    }

    //For time per div set with tapping on the screen the direct relation between the time per div and the sample rate is set
    //but only when the scope is running. Otherwise the sample rate of the acquired buffer still is valid.
    if(scopesettings.runstate)//ok
    {
      //Set the sample rate that belongs to the selected time per div setting
      scopesettings.samplerate = time_per_div_sample_rate[scopesettings.timeperdiv];
    }

    //Set the new setting in the FPGA
    fpga_set_sample_rate(scopesettings.samplerate);

    //Show he new setting on the display
    scope_acqusition_settings(0);
  }
}
*/
//----------------------------------------------------------------------------------------------------------------------------------

void scope_draw_grid(void)
{
  uint32 color;
  register uint32 i;
   
  uint32 kurzor_position;
  
  //Set the color for drawing
  display_set_fg_color(WHITE_COLOR);
  //draw stvorec pre posuvanie buffra
  //Draw the edge
  //display_draw_rect(2, 451, 726, 20);//46 404
  //display_draw_rect(2, 452, 726, 20);//46 404
  display_draw_rect(2, 452, 726, 20);//46 404
  
  if (disp_first_sample>2999)   disp_first_sample=2999;
  
  kurzor_position = ((disp_first_sample*100)/554)+4;
  //if (kurzor_position > 686) kurzor_position = 686; //600 ma byt
  //if (kurzor_position > 635) kurzor_position = 635; //600 ma byt
  //Draw the edge
  //display_draw_rect(kurzor_position, 455, 40, 14);
  display_draw_rect(kurzor_position, 455, 181, 14);//181
  
  /*
   
     kurzor_position = (disp_first_sample/6)+4;
  //if (kurzor_position > 686) kurzor_position = 686; //600 ma byt
  if (kurzor_position > 545) kurzor_position = 545; //600 ma byt
  //Draw the edge
  //display_draw_rect(kurzor_position, 455, 40, 14);
  display_draw_rect(kurzor_position, 455, 223, 14);//181
   */
  
  //Set the color for drawing
  display_set_fg_color(YELLOW_COLOR);
  
  //Display the text
  display_set_font(&font_3);//3
  
  //Draw the ticks on the x line
    for(i=4;i<722;i+=5)         //722 krok 5
    {
      //display_draw_vert_line(i, 247, 251);
      display_text(i, 453, "~");    //452
    }
    
    //-********************************************************
  
// */
  //Only draw the grid when something will show (not in the original code)
  if(scopesettings.gridbrightness > 3)
  {
    //Calculate a grey shade based on the grid brightness setting
    color = (scopesettings.gridbrightness * 255) / 100;
    color = (color << 16) | (color << 8) | color;

    //Set the color for drawing
    display_set_fg_color(color);

    //Draw the edge
    display_draw_rect(2, 48, 726, 403);//46 404   44-406

    //Draw the center lines
    display_draw_horz_line(249,  2, 726);
    display_draw_vert_line(364, 49, 449);//44-448

    //Draw the ticks on the x line
    for(i=4;i<726;i+=5)
    {
      display_draw_vert_line(i, 247, 251);
    }

    //Draw the ticks on the y line-ok
    for(i=54;i<448;i+=5)//49 448 5
    {
      display_draw_horz_line(i, 362, 366);
    }

    //Draw the horizontal dots-ok
    for(i=99;i<448;i+=50)
    {
      display_draw_horz_dots(i, 4, 726, 5);
    }

    //Draw the vertical dots-ok
    for(i=14;i<726;i+=50)
    {
      display_draw_vert_dots(i, 54, 448, 5);//49
    }
  }
}

//----------------------------------------------------------------------------------------------------------------------------------
/*
void scope_draw_pointers(void)
{
  int32 position;

  //Draw channel 1 pointer when it is enabled
  if(scopesettings.channel1.enable)
  {
    //Set the colors for drawing
    display_set_fg_color(CHANNEL1_COLOR);
    display_set_bg_color(BLACK_COLOR);

    //Select the font for this pointer id
    display_set_font(&font_0);

    //Check if in normal or x-y display mode
    if(scopesettings.xymodedisplay == 0)
    {
      //y position for the channel 1 trace center pointer.
      position = 441 - scopesettings.channel1.traceposition;

      //Limit on the top of the displayable region
      if(position < 44)
      {
        position = 44;
      }
      //Limit on the bottom of the displayable region
      else if(position > 441)
      {
        position = 441;
      }

      //Draw the pointer
      display_left_pointer(2, position, '1');
    }
    else
    {
      //y position for the channel 1 trace center pointer.
      position = 157 + scopesettings.channel1.traceposition;

      //Limit on the left of the active range
      if(position < 166)
      {
        position = 166;
      }
      //Limit on the right of the active range
      else if(position > 548)
      {
        position = 548;
      }

      //Draw the pointer
      display_top_pointer(position, 47, '1');
    }
  }

  //Draw channel 2 pointer when it is enabled
  if(scopesettings.channel2.enable)
  {
    //y position for the channel 2 trace center pointer
    position = 441 - scopesettings.channel2.traceposition;

    //Limit on the top of the displayable region
    if(position < 44)
    {
      position = 44;
    }
    //Limit on the bottom of the displayable region
    else if(position > 441)
    {
      position = 441;
    }

    //Set the colors for drawing
    display_set_fg_color(CHANNEL2_COLOR);
    display_set_bg_color(BLACK_COLOR);

    //Select the font for this pointer id
    display_set_font(&font_0);

    //Draw the pointer
    display_left_pointer(2, position, '2');
  }

  //Need to think about trigger position in 200mS - 20mS/div settings. Not sure if they work or need to be done in software
  //The original scope does not show them for 50mS and 20mS/div

  //Draw trigger position and level pointer when in normal display mode
  if(scopesettings.xymodedisplay == 0)
  {
    //Set the colors for drawing
    if(disp_have_trigger == 1) display_set_fg_color(RED_COLOR); 
    else display_set_fg_color(TRIGGER_COLOR);
    display_set_bg_color(BLACK_COLOR);

    //Select the font for this pointer id
    display_set_font(&font_3); 
    
    //x position for the trigger position pointer
    //position = scopesettings.triggerhorizontalposition + 2;
    position = scopesettings.triggerhorizontalposition;

    //Limit on the left of the displayable region
    if(position < 3)//3
    { //Out of limit on the left of the displayable region
      display_right_pointer(2, 45, 'H');
    }
    //Limit on the right of the displayable region
    else if(position > 713)
    {//Out of limit on the right of the displayable region
      display_left_pointer(706, 45, 'H');
    }
    else 
    //Draw the pointer
    display_top_pointer(position, 45, 'H');//47
    
    //-----------------------------------------------------
    //y position for the trigger level pointer
    position = 441 - scopesettings.triggerverticalposition;

    //Limit on the top of the displayable region
    if(position < 44)
    {
      position = 44;
    }
    //Limit on the bottom of the displayable region
    else if(position > 441)
    {
      position = 441;
    }

    //Need to reset the fore ground color
    //Select the channel based on the current trigger color
    if(scopesettings.triggerchannel == 0) display_set_fg_color(CHANNEL1_COLOR);
            else display_set_fg_color(CHANNEL2_COLOR);   
    

    //Draw the pointer
    display_right_pointer(707, position, 'T');
  }
}
*/
//----------------------------------------------------------------------------------------------------------------------------------

void scope_draw_time_cursors(void)
{
  //Only draw the lines when enabled
  if(scopesettings.timecursorsenable)
  {
    //Set the color for the dashed lines
    display_set_fg_color(CURSORS_COLOR);

    //Draw the lines
    display_draw_vert_dashes(scopesettings.timecursor1position, 51, 446, 3, 3);//49 444 3 3 //48 448//46 447
    display_draw_vert_dashes(scopesettings.timecursor2position, 51, 446, 3, 3);//48 448
  }
}

//----------------------------------------------------------------------------------------------------------------------------------

void scope_draw_volt_cursors(void)
{
  //Only draw the lines when enabled
  if(scopesettings.voltcursorsenable)
  {
    //Set the color for the dashed lines
    display_set_fg_color(CURSORS_COLOR);

    //Draw the lines
    display_draw_horz_dashes(scopesettings.voltcursor1position, 6, 725, 3, 3);//6 726
    display_draw_horz_dashes(scopesettings.voltcursor2position, 6, 725, 3, 3);//6 726
  }
}

//----------------------------------------------------------------------------------------------------------------------------------
