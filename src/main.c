#include "common.h"
#include "arm_cm4.h"
#include "usb.h"

#define LED_ON  GPIOC_PSOR=(1<<5)
#define LED_OFF GPIOC_PCOR=(1<<5)

int main(void)
{
   
    PORTC_PCR5 = PORT_PCR_MUX(0x1); // LED is on PC5 (pin 13), config as GPIO (alt = 1)
    GPIOC_PDDR = (1<<5);   	// make this an output pin pin 13
    LED_OFF;                     // start with LED off

    usb_init();

    EnableInterrupts

    while(1)
    {
    }

    return  0;
}
