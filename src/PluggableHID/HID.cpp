/* Copyright (c) 2015, Arduino LLC
**
** Original code (pre-library): Copyright (c) 2011, Peter Barrett
**
** Permission to use, copy, modify, and/or distribute this software for  
** any purpose with or without fee is hereby granted, provided that the  
** above copyright notice and this permission notice appear in all copies.  
** 
** THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL  
** WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED  
** WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR  
** BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES  
** OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,  
** WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,  
** ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS  
** SOFTWARE.  
*/

#include "PluggableUSB.h"
#include "HID.h"
#include "HIDDevice.h"
#include "HID-Project.h" // Only used for the BootKeyboard setting

#if defined(USBCON)

HID_ HID;

static u8 HID_ENDPOINT_INT;

//================================================================================
//================================================================================

//	HID report descriptor

#define LSB(_x) ((_x) & 0xFF)
#define MSB(_x) ((_x) >> 8)

#define RAWHID_USAGE_PAGE	0xFFC0
#define RAWHID_USAGE		0x0C00
#define RAWHID_TX_SIZE 64
#define RAWHID_RX_SIZE 64

static u8 HID_INTERFACE;

HIDDescriptor _hidInterface;

static HIDDevice* rootDevice = NULL;
static uint16_t sizeof_hidReportDescriptor = 0;
static uint8_t modules_count = 0;
//================================================================================
//================================================================================
//	Driver

u8 _hid_protocol = 1;
u8 _hid_idle = 1;

int HID_GetInterface(u8* interfaceNum)
{
	interfaceNum[0] += 1;	// uses 1
	_hidInterface =
	{
#if defined(USE_BOOT_KEYBOARD_PROTOCOL)
		D_INTERFACE(HID_INTERFACE,1,3,1,1),
#else
		D_INTERFACE(HID_INTERFACE,1,3,0,0),
#endif
		D_HIDREPORT(sizeof_hidReportDescriptor),
		D_ENDPOINT(USB_ENDPOINT_IN (HID_ENDPOINT_INT),USB_ENDPOINT_TYPE_INTERRUPT,USB_EP_SIZE,0x01)
	};
	return USB_SendControl(0,&_hidInterface,sizeof(_hidInterface));
}

int HID_GetDescriptor(int8_t t)
{
	if (HID_REPORT_DESCRIPTOR_TYPE == t) {
		HIDDevice* current = rootDevice;
		int total = 0;
		while(current != NULL) {
			total += USB_SendControl(TRANSFER_PGM,current->descriptorData,current->descriptorLength);
			current = current->next;
		}
		return total;
	} else {
		return 0;
	}
}

void HID_::AppendDescriptor(HIDDevice *device)
{
	if (modules_count == 0) {
		rootDevice = device;
	} else {
		HIDDevice *current = rootDevice;
		while(current->next != NULL) {
			current = current->next;
		}
		current->next = device;
	}
	modules_count++;
	sizeof_hidReportDescriptor += (uint16_t)device->descriptorLength;
}

void HID_::SendReport(u8 id, const void* data, int len)
{
	// Only send report ID if it exists
	if(id){
		USB_Send(HID_TX, &id, 1);
	}
	USB_Send(HID_TX | TRANSFER_RELEASE,data,len);
}

bool HID_::HID_Setup(USBSetup& setup, u8 i)
{
	if (HID_INTERFACE != i) {
		return false;
	} else {
		u8 r = setup.bRequest;
		u8 requestType = setup.bmRequestType;
		if (REQUEST_DEVICETOHOST_CLASS_INTERFACE == requestType)
		{
			if (HID_GET_REPORT == r)
			{
			//HID_GetReport();
				return true;
			}
			if (HID_GET_PROTOCOL == r)
			{
			//Send8(_hid_protocol);	// TODO
				return true;
			}
		}
		
		if (REQUEST_HOSTTODEVICE_CLASS_INTERFACE == requestType)
		{
			if (HID_SET_PROTOCOL == r)
			{
				_hid_protocol = setup.wValueL;
				return true;
			}

			if (HID_SET_IDLE == r)
			{
				_hid_idle = setup.wValueL;
				return true;
			}
			
			if (HID_SET_REPORT == r)
			{
				// Get reportID and search for the suited HIDDevice
				uint8_t ID = setup.wIndex;
				HIDDevice *current = rootDevice;
				while(current != NULL) 
				{
					// Search HIDDevice for report ID
					if(current->reportID == ID)
					{
						// Get the data length information and the corresponding bytes
						uint8_t length = setup.wLength;
						uint8_t data[length];
						USB_RecvControl(data, length);
							
						// Skip report ID data, if any
						if(ID){
							current->setReportData(data+1, length-1);
						}
						// TODO test this case
						else{
							current->setReportData(data, length);
						}
						
						// Dont search any further
						break;
					}
					current = current->next;
				}
			}
		}
		return false;
	}
}

HID_::HID_(void)
{
	static uint8_t endpointType[1];

	endpointType[0] = EP_TYPE_INTERRUPT_IN;

	static PUSBCallbacks cb = {
		.setup = HID_Setup,
		.getInterface = &HID_GetInterface,
		.getDescriptor = &HID_GetDescriptor,
		.numEndpoints = 1,
		.numInterfaces = 1,
		.endpointType = endpointType,
	};

	static PUSBListNode node(&cb);

	HID_ENDPOINT_INT = PUSB_AddFunction(&node, &HID_INTERFACE);
}

int HID_::begin(void)
{
}

#endif /* if defined(USBCON) */

