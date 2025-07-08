//----------------------------------------------------------------------------------------------------------------------------------

#include"types.h"
#include "ch340_class.h"

//----------------------------------------------------------------------------------------------------------------------------------

//Global memory assignment for USB device
uint8 usb_rx[1024];                      //Buffer for USB receive data
uint32 volatile usb_rx_in_idx = 0;       //Index for putting data into the USB receive buffer
uint32 volatile usb_rx_out_idx = 0;      //Index for taking data from the USB receive buffer. Set volatile since it changes in interrupt routine.

uint8 usb_tx[1024];                      //Buffer for USB transmit data
uint32 volatile usb_tx_in_idx = 0;       //Index for putting data into the USB transmit buffer
uint32 volatile usb_tx_out_idx = 0;      //Index for taking data from the USB transmit buffer. Set volatile since it changes in interrupt routine.

uint32 volatile fifobuffer[128];         //Buffer to read FIFO data in on EP2 out

uint32 volatile EP2DisableTX = 0;        //Flag to disable the endpoint 2 transmission function.

//----------------------------------------------------------------------------------------------------------------------------------

const USB_DeviceDescriptor CH340_DevDesc =
{
  sizeof (USB_DeviceDescriptor),
  DEVICE_DESCRIPTOR,
  0x0200,   //0x0110,//0x0200,                 //Version 2.0
  0x02,
  0x00,
  0x00,
  USB_EP0_FIFO_SIZE,      //Ep0 FIFO size
  0x0483,   //0x067b,//0x1A86,                 //Vendor id
  0x5740,   //0x2303,//0x7523,                 //Device id
  0x0310,   //0x0300,//0x0263,                 //Release version
  0x01,                   //iManufacturer
  0x02,                   //iProduct
  0x00,                   //ISerial
  0x01                    //Number of configurations
};



//******************
/*
const CH340_Descriptor CH340_CommDesc =
{
    {
      //sizeof (USB_CommunicationsDescriptor), //COMMUNICATIONS_DESCRIPTOR
      //COMMUNICATIONS_DESCRIPTOR,
      //Header Functional Descriptor
      0x05,   // bLength: Endpoint Descriptor size 
      0x24,   // bDescriptorType: CS_INTERFACE 
      0x00,   // bDescriptorSubtype: Header Func Desc 
      0x10,   // bcdCDC: spec release number 
      0x01
    },
           
    {
      //sizeof (USB_CommunicationsDescriptor), //COMMUNICATIONS_DESCRIPTOR
      //COMMUNICATIONS_DESCRIPTOR,  
      //Call Management Functional Descriptor
      0x05,   //bFunctionLength 
      0x24,   // bDescriptorType: CS_INTERFACE 
      0x01,   // bDescriptorSubtype: Call Management Func Desc 
      0x00,   // bmCapabilities: D0+D1 
      0x01   // bDataInterface: 1 
    },
    {
      //sizeof (USB_CommunicationsDescriptor), //COMMUNICATIONS_DESCRIPTOR
      //COMMUNICATIONS_DESCRIPTOR, 
      //ACM Functional Descriptor
      0x04,   // bFunctionLength 
      0x24,   //bDescriptorType: CS_INTERFACE 
      0x02,   // bDescriptorSubtype: Abstract Control Management desc 
      0x02   // bmCapabilities 
    },
    {
      //sizeof (USB_CommunicationsDescriptor), //COMMUNICATIONS_DESCRIPTOR
      //COMMUNICATIONS_DESCRIPTOR, 
      //Union Functional Descriptor
      0x05,   // bFunctionLength 
      0x24,   // bDescriptorType: CS_INTERFACE 
      0x06,   // bDescriptorSubtype: Union func desc 
      0x00,   // bMasterInterface: Communication class interface 
      0x01   // bSlaveInterface0: Data Class Interface 
    }
}; 
 * */
//******************

const CH340_Descriptor CH340_ConfDesc =
{
  {
    sizeof (USB_ConfigDescriptor),
    CONFIGURATION_DESCRIPTOR,
    CONFIG_CH340_DESCRIPTOR_LEN, //Total length of the Configuration descriptor
    0x02, //2//NumInterfaces   //1 bolo origo
    0x01, //Configuration Value
    0x00, //0x04, //Configuration Description String Index
    0xc0, //Self Powered, no remote wakeup
    0x32  //Maximum power consumption 500 mA
  },
  /*
    {
    sizeof (USB_InterfaceAssocDescriptor),
    INTERFACE_ASSOC_DESCRIPTOR,
    0x00, //bInterfaceNumber
    0x02, //bAlternateSetting
    0x02, //ep number
    0x02,   //0x02,    //0xFF, //Interface Class
    //0x02, //0x01   //0x00,//0x01, //Interface Subclass
    0x01,//0x02, //Interface Protocol
    0x00  //Interface Description String Index
            
      	},
  */
  {
    sizeof (USB_InterfaceDescriptor),
    INTERFACE_DESCRIPTOR,
    0x00, //bInterfaceNumber
    0x00, //bAlternateSetting
    0x01, //ep number
    0x02,   //0x02,    //0xFF, //Interface Class
    0x02, //0x01   //0x00,//0x01, //Interface Subclass
    0x01,//0x02, //Interface Protocol
    0x00  //Interface Description String Index
            
      	},
	{
		sizeof(USB_CDC_HeaderDescriptor),
		USB_DT_CS_INTERFACE,
		0x00,
		0x0110
                
	},
	{
		sizeof(USB_CDC_CallMgmtDescriptor),
		USB_DT_CS_INTERFACE,
		0x01,
		0x00,
		0x01
	},
	{
		sizeof(USB_CDC_AcmDescriptor),
		USB_DT_CS_INTERFACE,
		0x02,
		0x02
	},
	{
		sizeof(USB_CDC_UnionDescriptor),
		USB_DT_CS_INTERFACE,
		0x06,
		0x00,
		0x01
	},
        
        	{
		sizeof(USB_EndPointDescriptor),
		ENDPOINT_DESCRIPTOR,
		(1 << 7) | 1,	// endpoint 2 IN //1
		3,	/* interrupt */
		8,	/* IN EP FIFO size */
		16	/* Interval */
	},
	
        {
		sizeof(USB_InterfaceDescriptor),
		INTERFACE_DESCRIPTOR,
		0x01,
		0x00,
		0x02,
		0x0a,
		0x00,
		0x00,
		0x00
	},
            
            
            
  //},
  {
      {
			sizeof(USB_EndPointDescriptor),
			ENDPOINT_DESCRIPTOR,
			(1 << 7) | 2,   // endpoint 1 IN //2
			2, /* bulk */
			/* Transfer Type: Bulk;
			 * Synchronization Type: No Synchronization;
			 * Usage Type: Data endpoint
			 */
           	512,
			0
		},
		{
			sizeof(USB_EndPointDescriptor),
			ENDPOINT_DESCRIPTOR,
			(0 << 7) | 2,// endpoint 1 OUT  //2
			2, /* bulk */
			/* Transfer Type: Bulk;
			 * Synchronization Type: No Synchronization;
			 * Usage Type: Data endpoint
			 */
           	512,
			0
		}
 /*   {
      sizeof (USB_EndPointDescriptor),
      ENDPOINT_DESCRIPTOR,
      0x82,//0x82, //endpoint 2 IN
      2,    //bulk
      512,  //IN EP FIFO size  512 bytes
      0
    },
    {
      sizeof (USB_EndPointDescriptor),
      ENDPOINT_DESCRIPTOR,
      0x02, //endpoint 1 OUT
      2,    //bulk
      512,  //OUT EP FIFO size  512 bytes
      0
    },
    {
      sizeof (USB_EndPointDescriptor),
      ENDPOINT_DESCRIPTOR,
      0x81, //endpoint 1 IN
      3,    //interrupt
      10,    //IN EP FIFO size  8 bytes
      9
    }
  * */
  }
};
/*
//USB String Descriptors
const uint8 StringLangID[4] =
{
  0x04,
  0x03,
  0x09,
  0x04 // LangID = 0x0409: U.S. English
};
*/



const uint8 str_ret[38] = {
		0x26,
                0x03,
		'H', 0, '2', 0, '7', 0, '5', 0, '0', 0, ' ', 0, ' ', 0, 'U', 0,
                's', 0, 'b', 0, ' ', 0, 'D', 0, 'e', 0, 'v', 0, 'i', 0, 'c', 0,
                'e', 0, 's', 0
};

const uint8 str_isernum[32] = {
		0x20,
                0x03,
		'H','e','r','o','J','e',' ','S','c','a','n','n','e','r','s',' ','D','r','i','v','e','s',' ','-',' ','H','2','7','5','0'
};

const uint8 str_config[16] = {
		0x10,
                0x03,
		'C','D','C',' ','A','C','M',' ','c','o','n','f','i','g'
};

const uint8 str_interface1[34] = {
		0x22,
                0x03,
		'C','D','C',' ','A','b','s','t','r','a','c','t',' ','C','o','n','t','r','o','l',' ','M','o','d','e','l',' ','(','A','C','M',')'
};

const uint8 str_interface2[14] = {
		0x0e,
                0x03,
		'C','D','C',' ','A','C','M',' ','D','a','t','a'
};

const uint8 StringVendorCH340[38] =
{
  0x26, // Size of Vendor string
  0x03, // bDescriptorType
  // Manufacturer: "STMicroelectronics"
  'S', 0, 'T', 0, 'M', 0, 'i', 0, 'c', 0, 'r', 0, 'o', 0, 'e', 0,
  'l', 0, 'e', 0, 'c', 0, 't', 0, 'r', 0, 'o', 0, 'n', 0, 'i', 0,
  'c', 0, 's', 0
};


const uint8 StringProductCH340[28] =
{
  0x1C, // bLength
  0x03, // bDescriptorType
  // Serial name: "USB 2.0-Serial"
  'U', 0, 'S', 0, 'B', 0, '2', 0, '.', 0, '0', 0, '-', 0,
  'S', 0, 'e', 0, 'r', 0, 'i', 0, 'a', 0, 'l', 0
};

const uint8 StringSerialCH340[28] =
{
  0x1C, // bLength
  0x03, // bDescriptorType
  // Serial name: "USB 2.0-Serial"
  'U', 0, 'S', 0, 'B', 0, '2', 0, '.', 0, '0', 0, '-', 0,
  'S', 0, 'e', 0, 'r', 0, 'i', 0, 'a', 0, 'l', 0
};

//Vendor specifics
const uint8 vendorVersion[2] = { 0x31, 0x00 };
const uint8 vendorAttach[2]  = { 0xC3, 0x00 };
const uint8 vendorStatus[2]  = { 0xFF, 0xEE };

//----------------------------------------------------------------------------------------------------------------------------------

//Function for getting a character from the USB receive buffer
int16 ch340Receive(void)
{
  uint8 c;

  //See if there is any data in the buffer
  if(usb_rx_out_idx == usb_rx_in_idx)
    return -1;

  //Get available character
  c = usb_rx[usb_rx_out_idx++];

  //Keep index in valid range
  usb_rx_out_idx %= sizeof(usb_rx);

  return c;
}

//----------------------------------------------------------------------------------------------------------------------------------

//Function for putting a character in the USB transmit buffer
void ch340Send(uint8 c)
{
  uint32 charsfree;

  //Wait until there is room in the transmit buffer
  do
  {
    //Calculate the number of free bytes minus one. Need one byte free because indexes being the same means empty buffer.
    charsfree = usb_tx_out_idx - usb_tx_in_idx - 1;

    //When the in index is higher than the out index the result is negative so add the size of the buffer to get the
    //number of free bytes. Otherwise the result is already positive
    if(usb_tx_in_idx >= usb_tx_out_idx)
      charsfree += sizeof(usb_tx);

    //When charsfree is 0 the buffer is full so calculate again till space comes available
  } while(charsfree == 0);

  //Disable transmission while putting character in the buffer
  EP2DisableTX = 1;

  //Put the character in the transmit buffer and move to next free location
  usb_tx[usb_tx_in_idx++] = c;

  //Keep index in range of buffer size
  usb_tx_in_idx %= sizeof(usb_tx);

  //Enable transmission when done
  EP2DisableTX = 0;
}

//----------------------------------------------------------------------------------------------------------------------------------

void usb_ch340_out_ep_callback(void *fifo, int length)
{
  uint8 *ptr;
  
  //Not the most elegant solution but will do for now.
  
  //Read the data from the USB FIFO into the DRAM FIFO buffer
  usb_read_from_fifo(fifo, (void *)fifobuffer, length);
  
  //Put the bytes in the receive buffer
  ptr = (uint8 *)fifobuffer;
  
  //Copy the data from the FIFO buffer to the receive buffer
  while(length--)
  {
    //Put the byte in the receive buffer
    usb_rx[usb_rx_in_idx++] = *ptr++;
    usb_rx_in_idx %= sizeof(usb_rx);
  }
}

//----------------------------------------------------------------------------------------------------------------------------------

void usb_ch340_in_ep_callback(void)
{
  uint32 count;
  uint32 bytes;
  uint8 *ptr;
  
  //Check if transmission has been disabled
  if(EP2DisableTX == 1)
    return;

  //Check if there is data to send to the host
  //Calculate the number of bytes available in the buffer
  if((count = usb_tx_in_idx - usb_tx_out_idx) != 0)
  {
    //When out index bigger then in index data has a rollover in the buffer so need to add the size of the buffer to get the right number
    if(usb_tx_out_idx > usb_tx_in_idx)
      count += sizeof(usb_tx);

    //Check if available number of characters more then the USB buffer size. If so limit the number to be copied
    if(count > 512)
      count = 512;
    
    //Not the most elegant solution but will do for now.
    //Direct to USB FIFO writing would be quicker, but needs lower level code.

    //Set number of bytes to copy to the FIFO buffer
    bytes = count;

    //Put the bytes in the transmit buffer
    ptr = (uint8 *)fifobuffer;
    
    //Copy all the needed bytes to the FIFO buffer
    while(bytes--)
    {
      //Put first byte in low part of the temporary storage for making 16 bits data
      *ptr++ = (usb_tx[usb_tx_out_idx++]);

      //Check if out index needs to rollover back to start of buffer
      usb_tx_out_idx %= sizeof(usb_tx);
    }
    
    //Write the data to the USB device
    usb_write_ep2_data((void *)fifobuffer, count);
  }
}

//----------------------------------------------------------------------------------------------------------------------------------
/*
// Example of setting the baud rate to 9600
void set_baud_rate() {
    LineCoding lineCoding;
    
    lineCoding.baudRate = 9600;    // Set the baud rate to 9600
    lineCoding.stopBits = 1;       // 1 stop bit
    lineCoding.parity = 0;         // No parity
    lineCoding.dataBits = 8;       // 8 data bits

    // Call the function to set the line coding (baud rate)
    //usb_cdc_set_line_coding(&lineCoding);
    //Write the data to the USB device
    usb_write_ep2_data(&lineCoding, 4);
}
  */