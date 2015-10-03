#include "common.h"
#include "arm_cm4.h"
#include "usb.h"
#include "string.h"

#define LED_ON  GPIOC_PSOR = (1<<5)
#define LED_OFF GPIOC_PCOR = (1<<5)

void usb_endpoint_15_transmit(char *out)
{
	char *str = "HELLO FROM ENDPOINT 15";
	int len = strlen(str);
        memcpy(out,str,len);
}

void usb_endpoint_13_transmit(char *out)
{
	char *str = "HELLO FROM ENDPOINT 13";
	int len = strlen(str);
        memcpy(out,str,len);
}

void usb_endpoint_1_receive(char *in)
{
	if (in[0] == 'Y')
	{
		LED_ON;
	} else {
		LED_OFF;
	}
}

int main(void)
{
   
    PORTC_PCR5 = PORT_PCR_MUX(0x1); // LED is on PC5 (pin 13), config as GPIO (alt = 1)
    GPIOC_PDDR = (1<<5);   	// make this an output pin pin 13
    LED_OFF;                     // start with LED off

    usb_set_endpoint_on_receive(usb_endpoint_1_receive, 1);
    usb_set_endpoint_on_transmit(usb_endpoint_13_transmit, 13);
    usb_set_endpoint_on_transmit(usb_endpoint_15_transmit, 15);
    usb_init();

    EnableInterrupts

    while(1)
    {
    }

    return  0;
}
