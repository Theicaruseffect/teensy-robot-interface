/**
 * Header file for my implementation of using the USB peripheral
 * Kevin Cuzner - Edited by Phillip Nash
 */
#ifndef _USB_H_
#define _USB_H_

#include "arm_cm4.h"

#define PID_OUT   0x1
#define PID_IN    0x9
#define PID_SOF   0x5
#define PID_SETUP 0xd
#define ENDP_SIZE 64
#define BDT_BC_SHIFT   16
#define BDT_OWN_MASK   0x80
#define BDT_DATA1_MASK 0x40
#define BDT_KEEP_MASK  0x20
#define BDT_NINC_MASK  0x10
#define BDT_DTS_MASK   0x08
#define BDT_STALL_MASK 0x04
#define BDT_DESC(count, data) ((count << BDT_BC_SHIFT) | BDT_OWN_MASK | (data ? BDT_DATA1_MASK : 0x00) | BDT_DTS_MASK)
#define BDT_PID(desc) ((desc >> 2) & 0xF)

// we enforce a max of 15 endpoints (15 + 1 control = 16)
#define USB_N_ENDPOINTS 15

//determines an appropriate BDT index for the given conditions (see fig. 41-3)
#define RX 0
#define TX 1
#define EVEN 0
#define ODD  1
#define BDT_INDEX(endpoint, tx, odd) ((endpoint << 2) | (tx << 1) | odd)

/**
 * USB structures
 */
typedef struct {
        union {
            struct {
                uint8_t bmRequestType;
                uint8_t bRequest;
            };
            uint16_t wRequestAndType;
        };
        uint16_t wValue;
        uint16_t wIndex;
        uint16_t wLength;
} setup_t;

typedef struct {
        uint8_t bLength;
        uint8_t bDescriptorType;
        uint16_t wString[];
} str_descriptor_t;

typedef struct {
        uint16_t wValue;
        uint16_t wIndex;
        const void* addr;
        uint8_t length;
} descriptor_entry_t;


/**
 * Buffer Descriptor Table entry
 * There are two entries per direction per endpoint:
 *   In  Even/Odd
 *   Out Even/Odd
 * A bidirectional endpoint would then need 4 entries
 */
typedef struct {
        uint32_t desc;
        void* addr;
} bdt_t;


/**
 * Buffer descriptor table, aligned to a 512-byte boundary (see linker file)
 */
__attribute__ ((aligned(512), used))
static bdt_t table[(USB_N_ENDPOINTS + 1)*4]; //max endpoints is 15 + 1 control


/**
 * Initializes the USB module
 */
void usb_init(void);

void usb_endp0_handler(uint8_t);
void usb_endp1_handler(uint8_t);
void usb_endp2_handler(uint8_t);
void usb_endp3_handler(uint8_t);
void usb_endp4_handler(uint8_t);
void usb_endp5_handler(uint8_t);
void usb_endp6_handler(uint8_t);
void usb_endp7_handler(uint8_t);
void usb_endp8_handler(uint8_t);
void usb_endp9_handler(uint8_t);
void usb_endp10_handler(uint8_t);
void usb_endp11_handler(uint8_t);
void usb_endp12_handler(uint8_t);
void usb_endp13_handler(uint8_t);
void usb_endp14_handler(uint8_t);
void usb_endp15_handler(uint8_t);

void usb_set_endpoint_1_receive(void (*handler) (char *in));
void usb_set_endpoint_15_transmit(void (*handler) (char *out));

#endif // _USB_H_
