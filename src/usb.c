#include "usb.h"
#include "arm_cm4.h"
#include "common.h"

/**
 * Endpoint 0 receive buffers (2x64 bytes)
 */
static uint8_t endp0_rx[2][ENDP_SIZE];
static uint8_t endp1_rx[2][ENDP_SIZE];
static uint8_t endp15_tx[2][ENDP_SIZE];

static const uint8_t* endp0_tx_dataptr = NULL; //pointer to current transmit chunk
static uint16_t endp0_tx_datalen = 0; //length of data remaining to send

/**
 * Device descriptor
 * NOTE: This cannot be const because without additional attributes, it will
 * not be placed in a part of memory that the usb subsystem can access. I
 * have a suspicion that this location is somewhere in flash, but not copied
 * to RAM.
 */
static uint8_t dev_descriptor[] = {
        18, //bLength
        1, //bDescriptorType
        0x00, 0x02, //bcdUSB
        0xff, //bDeviceClass
        0x00, //bDeviceSubClass
        0x00, //bDeviceProtocl
        ENDP_SIZE, //bMaxPacketSize0
        0xc0, 0x16, //idVendor
        0xdc, 0x05, //idProduct
        0x01, 0x00, //bcdDevice
        1, //iManufacturer
        2, //iProduct
        0, //iSerialNumber,
        1, //bNumConfigurations
};

/**
 * Configuration descriptor
 * NOTE: Same thing about const applies here
 */
static uint8_t cfg_descriptor[] = {
        9, //bLength
        2, //bDescriptorType
        9 + 9 + 7 + 7, 0x00, //wTotalLength
        1, //bNumInterfaces
        1, //bConfigurationValue,
        0, //iConfiguration
        0x80, //bmAttributes
        250, //bMaxPower

        /* INTERFACE 0 BEGIN */
        9, //bLength
        4, //bDescriptorType
        0, //bInterfaceNumber
        0, //bAlternateSetting
        2, //bNumEndpoints
        0xff, //bInterfaceClass
        0x00, //bInterfaceSubClass,
        0x00, //bInterfaceProtocol
        0, //iInterface
            /* INTERFACE 0, ENDPOINT 1 BEGIN */
            7, //bLength
            5, //bDescriptorType,
            0x01, //bEndpointAddress,
            0x02, //bmAttributes, bulk endpoint
            ENDP_SIZE, 0x00, //wMaxPacketSize,
            0, //bInterval
            /* INTERFACE 0, ENDPOINT 1 END */

            /* INTERFACE 0, ENDPOINT 15 BEGIN */
            7, //bLength
            5, //bDescriptorType,
            0x8F, //bEndpointAddress,
            0x02, //bmAttributes, bulk endpoint
            ENDP_SIZE, 0x00, //wMaxPacketSize,
            0 //bInterval
            /* INTERFACE 0, ENDPOINT 1 END */
        /* INTERFACE 0 END */

};

static str_descriptor_t lang_descriptor = {
        .bLength = 4,
        .bDescriptorType = 3,
        .wString = { 0x0409 } //english (US)
};

static str_descriptor_t manuf_descriptor = {
        .bLength = 2 + 15 * 2,
        .bDescriptorType = 3,
        .wString = { 'P','h','i','l','l','i','p',' ','N','a','s','h' }
};

static str_descriptor_t product_descriptor = {
        .bLength = 2 + 15 * 2,
        .bDescriptorType = 3,
        .wString = { 'R','o','b','o','t','-','I','n','t','e','r','f','a','c','e' }
};

static const descriptor_entry_t descriptors[] = {
        { 0x0100, 0x0000, dev_descriptor, sizeof(dev_descriptor) },
        { 0x0200, 0x0000, &cfg_descriptor, sizeof(cfg_descriptor) },
        { 0x0300, 0x0000, &lang_descriptor, 4 },
        { 0x0301, 0x0409, &manuf_descriptor, 2 + 15 * 2 },
        { 0x0302, 0x0409, &product_descriptor, 2 + 15 * 2 },
        { 0x0000, 0x0000, NULL, 0 }
};


static void (*usb_endpoint_1_receive) 	(char *in);
static void (*usb_endpoint_15_transmit) (char * out);


static uint8_t endp0_odd, endp0_data = 0;
static void usb_endp0_transmit(const void* data, uint8_t length)
{
        table[BDT_INDEX(0, TX, endp0_odd)].addr = (void *)data;
        table[BDT_INDEX(0, TX, endp0_odd)].desc = BDT_DESC(length, endp0_data);
        //toggle the odd and data bits
        endp0_odd ^= 1;
        endp0_data ^= 1;
}

static uint8_t endp1_odd, endp1_data = 0;
static void usb_endp1_receive(uint8_t length)
{
        char *data = (char *) table[BDT_INDEX(1, RX, endp1_odd)].addr;
	usb_endpoint_1_receive(data);
        endp1_odd ^= 1;
        endp1_data ^= 1;
}


static uint8_t endp15_odd, endp15_data = 0;
static void usb_endp15_transmit(uint8_t length)
{
        char *data = (char *) table[BDT_INDEX(15, TX, endp1_odd)].addr;
	usb_endpoint_15_transmit(data);
        endp15_odd ^= 1;
        endp15_data ^= 1;
}

void usb_set_endpoint_1_receive(void (*callback) (char *data))
{
        usb_endpoint_1_receive = callback;
}

void usb_set_endpoint_15_transmit(void (*callback) (char *data))
{
	usb_endpoint_15_transmit = callback;
}

/**
 * Endpoint 0 setup handler
 */
static void usb_endp0_handle_setup(setup_t* packet)
{
        const descriptor_entry_t* entry;
        const uint8_t* data = NULL;
        uint8_t data_length = 0;
        uint32_t size = 0;


        switch(packet->wRequestAndType)
        {
                case 0x0500: //set address (wait for IN packet)
                    break;
                case 0x0900: //set configuration
                    //we only have one configuration at this time
                    break;
                case 0x0680: //get descriptor
                case 0x0681:
                    
                    for (entry = descriptors; 1; entry++)
                    {
                        if (entry->addr == NULL)
                            break;

                        if (packet->wValue == entry->wValue && packet->wIndex == entry->wIndex)
                        {
                            //this is the descriptor to send
                            data = entry->addr;
                            data_length = entry->length;
                            goto send;
                        }
                    }
                    
                    goto stall;
                    break;
                default:
                    goto stall;
        }

        //if we are sent here, we need to send some data
        send:
            //truncate the data length to whatever the setup packet is expecting
            if (data_length > packet->wLength)
                data_length = packet->wLength;

            //transmit 1st chunk
            size = data_length;
            
            if (size > ENDP_SIZE)
                size = ENDP_SIZE;
            
            usb_endp0_transmit(data, size);
            
            data += size; //move the pointer down
            data_length -= size; //move the size down
            
            if (data_length == 0 && size < ENDP_SIZE)
                return; //all done!

            //transmit 2nd chunk
            size = data_length;
            
            if (size > ENDP_SIZE)
                size = ENDP_SIZE;
            
            usb_endp0_transmit(data, size);
            
            data += size; //move the pointer down
            data_length -= size; //move the size down
            
            if (data_length == 0 && size < ENDP_SIZE)
                return; //all done!

            //if any data remains to be transmitted, we need to store it
            endp0_tx_dataptr = data;
            endp0_tx_datalen = data_length;
            
            return;

        //if we make it here, we are not able to send data and have stalled
        stall:
            USB0_ENDPT0 = USB_ENDPT_EPSTALL_MASK | USB_ENDPT_EPRXEN_MASK | USB_ENDPT_EPTXEN_MASK | USB_ENDPT_EPHSHK_MASK;
}

/**
 * Endpoint 0 handler
 */
void usb_endp0_handler(uint8_t stat)
{
        static setup_t last_setup;

        const uint8_t* data = NULL;
        uint32_t size = 0;


        //determine which bdt we are looking at here
        bdt_t* bdt = &table[BDT_INDEX(0, (stat & USB_STAT_TX_MASK) >> USB_STAT_TX_SHIFT, (stat & USB_STAT_ODD_MASK) >> USB_STAT_ODD_SHIFT)];

        switch (BDT_PID(bdt->desc))
        {
                case PID_SETUP:
                    //extract the setup token
                            last_setup = *((setup_t*)(bdt->addr));

                            //we are now done with the buffer
                    bdt->desc = BDT_DESC(ENDP_SIZE, 1);

                    //clear any pending IN stuff
                    table[BDT_INDEX(0, TX, EVEN)].desc = 0;
                    table[BDT_INDEX(0, TX, ODD)].desc = 0;
                    endp0_data = 1;

                    //cast the data into our setup type and run the setup
                    usb_endp0_handle_setup(&last_setup);//&last_setup);

                    //unfreeze this endpoint
                    USB0_CTL = USB_CTL_USBENSOFEN_MASK;
                    break;
                case PID_IN:
                    //continue sending any pending transmit data
                    data = endp0_tx_dataptr;
                    
                    if (data)
                    {
                            size = endp0_tx_datalen;
                            
                            if (size > ENDP_SIZE)
                            {
                                size = ENDP_SIZE;
                                
                                usb_endp0_transmit(data, size);
                                
                                data += size;
                                endp0_tx_datalen -= size;
                                endp0_tx_dataptr = (endp0_tx_datalen > 0 || size == ENDP_SIZE) ? data : NULL;
                            }
                    }

                    if (last_setup.wRequestAndType == 0x0500)
                    {
                        USB0_ADDR = last_setup.wValue;
                    }
                    
                    break;
                case PID_OUT:
                    //nothing to do here..just give the buffer back
                    bdt->desc = BDT_DESC(ENDP_SIZE, 1);
                    break;
                case PID_SOF:
                    break;
        }

        USB0_CTL = USB_CTL_USBENSOFEN_MASK;
}

/**
 * Endpoint 1 handler
 */
void usb_endp1_handler(uint8_t stat)
{
        //determine which bdt we are looking at here
        bdt_t* bdt = &table[BDT_INDEX(1, (stat & USB_STAT_TX_MASK) >> USB_STAT_TX_SHIFT, (stat & USB_STAT_ODD_MASK) >> USB_STAT_ODD_SHIFT)];

        DisableInterrupts;

        switch (BDT_PID(bdt->desc))
        {
            case PID_OUT:
	    usb_endp1_receive(8);
            break;
        }

        //Give the buffer back.
        bdt->desc = BDT_DESC(ENDP_SIZE, 1);
        EnableInterrupts;

        table[BDT_INDEX(1, RX, EVEN)].addr = bdt->addr;
        table[BDT_INDEX(1, RX, EVEN)].desc = BDT_DESC(ENDP_SIZE, 0);
}

/**
 * Endpoint 15 handler
 */
void usb_endp15_handler(uint8_t stat)
{
        //determine which bdt we are looking at here
        bdt_t* bdt = &table[BDT_INDEX(15, (stat & USB_STAT_TX_MASK) >> USB_STAT_TX_SHIFT, (stat & USB_STAT_ODD_MASK) >> USB_STAT_ODD_SHIFT)];

        DisableInterrupts;

        switch (BDT_PID(bdt->desc))
        {
            case PID_IN:
		usb_endp15_transmit(8);
            break;
        }

        //Give the buffer back.
        bdt->desc = BDT_DESC(ENDP_SIZE, 1);
        EnableInterrupts;

        table[BDT_INDEX(15, TX, EVEN)].addr = bdt->addr;
        table[BDT_INDEX(15, TX, EVEN)].desc = BDT_DESC(ENDP_SIZE, 0);
}

static void (*handlers[16])(uint8_t) = {
        usb_endp0_handler,
        usb_endp1_handler,
        usb_endp2_handler,
        usb_endp3_handler,
        usb_endp4_handler,
        usb_endp5_handler,
        usb_endp6_handler,
        usb_endp7_handler,
        usb_endp8_handler,
        usb_endp9_handler,
        usb_endp10_handler,
        usb_endp11_handler,
        usb_endp12_handler,
        usb_endp13_handler,
        usb_endp14_handler,
        usb_endp15_handler,
};

/**
 * Default handler for USB endpoints that does nothing
 */
static void usb_endp_default_handler(uint8_t stat) { }

//weak aliases as "defaults" for the usb endpoint handlers
//void usb_endp1_handler(uint8_t) __attribute__((weak, alias("usb_endp_default_handler")));
void usb_endp2_handler(uint8_t) __attribute__((weak, alias("usb_endp_default_handler")));
void usb_endp3_handler(uint8_t) __attribute__((weak, alias("usb_endp_default_handler")));
void usb_endp4_handler(uint8_t) __attribute__((weak, alias("usb_endp_default_handler")));
void usb_endp5_handler(uint8_t) __attribute__((weak, alias("usb_endp_default_handler")));
void usb_endp6_handler(uint8_t) __attribute__((weak, alias("usb_endp_default_handler")));
void usb_endp7_handler(uint8_t) __attribute__((weak, alias("usb_endp_default_handler")));
void usb_endp8_handler(uint8_t) __attribute__((weak, alias("usb_endp_default_handler")));
void usb_endp9_handler(uint8_t) __attribute__((weak, alias("usb_endp_default_handler")));
void usb_endp10_handler(uint8_t) __attribute__((weak, alias("usb_endp_default_handler")));
void usb_endp11_handler(uint8_t) __attribute__((weak, alias("usb_endp_default_handler")));
void usb_endp12_handler(uint8_t) __attribute__((weak, alias("usb_endp_default_handler")));
void usb_endp13_handler(uint8_t) __attribute__((weak, alias("usb_endp_default_handler")));
void usb_endp14_handler(uint8_t) __attribute__((weak, alias("usb_endp_default_handler")));
//void usb_endp15_handler(uint8_t) __attribute__((weak, alias("usb_endp_default_handler")));

void usb_init(void)
{
        uint32_t i;

        //reset the buffer descriptors
        for (i = 0; i < (USB_N_ENDPOINTS + 1) * 4; i++)
        {
            table[i].desc = 0;
            table[i].addr = 0;
        }

        //1: Select clock source
        SIM_SOPT2 |= SIM_SOPT2_USBSRC_MASK | SIM_SOPT2_PLLFLLSEL_MASK; //we use MCGPLLCLK divided by USB fractional divider
        SIM_CLKDIV2 = SIM_CLKDIV2_USBDIV(1);// SIM_CLKDIV2_USBDIV(2) | SIM_CLKDIV2_USBFRAC_MASK; //(USBFRAC + 1)/(USBDIV + 1) = (1 + 1)/(2 + 1) = 2/3 for 72Mhz clock

        //2: Gate USB clock
        SIM_SCGC4 |= SIM_SCGC4_USBOTG_MASK;

        //3: Software USB module reset
        USB0_USBTRC0 |= USB_USBTRC0_USBRESET_MASK;
        while (USB0_USBTRC0 & USB_USBTRC0_USBRESET_MASK);

        //4: Set BDT base registers
        USB0_BDTPAGE1 = ((uint32_t)table) >> 8;  //bits 15-9
        USB0_BDTPAGE2 = ((uint32_t)table) >> 16; //bits 23-16
        USB0_BDTPAGE3 = ((uint32_t)table) >> 24; //bits 31-24

        //5: Clear all ISR flags and enable weak pull downs
        USB0_ISTAT = 0xFF;
        USB0_ERRSTAT = 0xFF;
        USB0_OTGISTAT = 0xFF;
        USB0_USBTRC0 |= 0x40; //a hint was given that this is an undocumented interrupt bit

        //6: Enable USB reset interrupt
        USB0_CTL = USB_CTL_USBENSOFEN_MASK;
        USB0_USBCTRL = 0;

        USB0_INTEN |= USB_INTEN_USBRSTEN_MASK;
        //NVIC_SET_PRIORITY(IRQ(INT_USB0), 112);
        enable_irq(IRQ(INT_USB0));

        //7: Enable pull-up resistor on D+ (Full speed, 12Mbit/s)
        USB0_CONTROL = USB_CONTROL_DPPULLUPNONOTG_MASK;
}

void USBOTG_IRQHandler(void)
{
        uint8_t status;
        uint8_t stat, endpoint;

        status = USB0_ISTAT;

        if (status & USB_ISTAT_USBRST_MASK)
        {
            //handle USB reset

            //initialize endpoint 0 ping-pong buffers
            USB0_CTL |= USB_CTL_ODDRST_MASK;
            endp0_odd = 0;
            table[BDT_INDEX(0, RX, EVEN)].desc = BDT_DESC(ENDP_SIZE, 0);
            table[BDT_INDEX(0, RX, EVEN)].addr = endp0_rx[0];
            table[BDT_INDEX(0, RX, ODD)].desc = BDT_DESC(ENDP_SIZE, 0);
            table[BDT_INDEX(0, RX, ODD)].addr = endp0_rx[1];
            table[BDT_INDEX(0, TX, EVEN)].desc = 0;
            table[BDT_INDEX(0, TX, ODD)].desc = 0;

            //initialize endpoint0 to 0x0d (41.5.23)
            //transmit, receive, and handshake
            USB0_ENDPT0 = USB_ENDPT_EPRXEN_MASK | USB_ENDPT_EPTXEN_MASK | USB_ENDPT_EPHSHK_MASK;

            table[BDT_INDEX(1, RX, EVEN)].desc = BDT_DESC(ENDP_SIZE, 0);
            table[BDT_INDEX(1, RX, EVEN)].addr = endp1_rx[0];
            table[BDT_INDEX(1, RX, ODD)].desc = BDT_DESC(ENDP_SIZE, 0);
            table[BDT_INDEX(1, RX, ODD)].addr = endp1_rx[1];
            table[BDT_INDEX(1, TX, EVEN)].desc = 0;
            table[BDT_INDEX(1, TX, ODD)].desc = 0;

            USB0_ENDPT1 = USB_ENDPT_EPRXEN_MASK | USB_ENDPT_EPHSHK_MASK;

            table[BDT_INDEX(15, TX, EVEN)].desc = BDT_DESC(ENDP_SIZE, 0);
            table[BDT_INDEX(15, TX, EVEN)].addr = endp15_tx[0];
            table[BDT_INDEX(15, TX, ODD)].desc = BDT_DESC(ENDP_SIZE, 0);
            table[BDT_INDEX(15, TX, ODD)].addr = endp15_tx[1];
            table[BDT_INDEX(15, RX, EVEN)].desc = 0;
            table[BDT_INDEX(15, RX, ODD)].desc = 0;

            USB0_ENDPT15 = USB_ENDPT_EPTXEN_MASK | USB_ENDPT_EPHSHK_MASK;


            //clear all interrupts...this is a reset
            USB0_ERRSTAT = 0xff;
            USB0_ISTAT = 0xff;

            //after reset, we are address 0, per USB spec
            USB0_ADDR = 0;

            //all necessary interrupts are now active
            USB0_ERREN = 0xFF;
            USB0_INTEN = USB_INTEN_USBRSTEN_MASK | USB_INTEN_ERROREN_MASK |
                                   USB_INTEN_SOFTOKEN_MASK | USB_INTEN_TOKDNEEN_MASK |
                                   USB_INTEN_SLEEPEN_MASK | USB_INTEN_STALLEN_MASK;
            return;
        }
        
        if (status & USB_ISTAT_ERROR_MASK)
        {
            //handle error
            USB0_ERRSTAT = USB0_ERRSTAT;
            USB0_ISTAT = USB_ISTAT_ERROR_MASK;
        }
        
        if (status & USB_ISTAT_SOFTOK_MASK)
        {
            //handle start of frame token
            USB0_ISTAT = USB_ISTAT_SOFTOK_MASK;
        }
        
        if (status & USB_ISTAT_TOKDNE_MASK)
        {
            //handle completion of current token being processed
            stat = USB0_STAT;
            endpoint = stat >> 4;
            handlers[endpoint & 0xf](stat);
            USB0_ISTAT = USB_ISTAT_TOKDNE_MASK;
        }
        
        if (status & USB_ISTAT_SLEEP_MASK)
        {
            //handle USB sleep
            USB0_ISTAT = USB_ISTAT_SLEEP_MASK;
        }
        
        if (status & USB_ISTAT_STALL_MASK)
        {
            //handle usb stall
            USB0_ISTAT = USB_ISTAT_STALL_MASK;
        }
}
