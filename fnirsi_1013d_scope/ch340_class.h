//----------------------------------------------------------------------------------------------------------------------------------

#ifndef CH340_CLASS_H
#define CH340_CLASS_H

//----------------------------------------------------------------------------------------------------------------------------------

#include "types.h"
#include "usb_interface.h"

//----------------------------------------------------------------------------------------------------------------------------------

//#define CONFIG_CH340_DESCRIPTOR_LEN   (sizeof(USB_ConfigDescriptor) + sizeof(USB_InterfaceDescriptor) + (sizeof(USB_CommunicationsDescriptor)*2) + (sizeof(USB_EndPointDescriptor) * 3))
//#define CONFIG_CH340_DESCRIPTOR_LEN   (sizeof(USB_ConfigDescriptor) + (sizeof(USB_InterfaceDescriptor)) + (sizeof(USB_EndPointDescriptor) * 3))

#define	CONFIG_CH340_DESCRIPTOR_LEN	(sizeof(USB_ConfigDescriptor) + \
				 sizeof(USB_InterfaceDescriptor) + \
				 sizeof(USB_CDC_HeaderDescriptor) + \
				 sizeof(USB_CDC_CallMgmtDescriptor) + \
				 sizeof(USB_CDC_AcmDescriptor) + \
				 sizeof(USB_CDC_UnionDescriptor) + \
                                 sizeof(USB_EndPointDescriptor) + \
				 sizeof(USB_InterfaceDescriptor) + \
				 sizeof(USB_EndPointDescriptor) * 2)
//----------------------------------------------------------------------------------------------------------------------------------

extern const USB_DeviceDescriptor CH340_DevDesc;

extern const CH340_Descriptor CH340_ConfDesc;

//extern const CH340_Descriptor CH340_CommDesc;

 extern const uint8 str_ret[38];
 extern const uint8 str_isernum[32];
extern const uint8 str_config[16];
extern const uint8 str_interface1[34];
extern const uint8 str_interface2[14];


extern const uint8 StringLangID[4];
extern const uint8 StringVendorCH340[38];
extern const uint8 StringProductCH340[28];
extern const uint8 StringSerialCH340[28];

extern const uint8 vendorVersion[2];
extern const uint8 vendorAttach[2];
extern const uint8 vendorStatus[2];

//----------------------------------------------------------------------------------------------------------------------------------

int16 ch340Receive(void);
void  ch340Send(uint8 c);
/*
// Structure to hold line coding parameters
typedef struct {
    uint32 baudRate;    // Baud rate (32-bit)
    uint8  stopBits;    // Stop bits (1, 1.5, 2)
    uint8  parity;      // Parity (None, Odd, Even)
    uint8  dataBits;    // Data bits (5, 6, 7, 8)
} LineCoding;
*/





//----------------------------------------------------------------------------------------------------------------------------------

void usb_ch340_out_ep_callback(void *fifo, int length);

void usb_ch340_in_ep_callback(void);

//void set_baud_rate(void);

//----------------------------------------------------------------------------------------------------------------------------------

#endif /* CH340_CLASS_H */

