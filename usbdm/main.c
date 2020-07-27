/*
    Turbo BDM Light - main program loop
    Copyright (C) 2005  Daniel Malik

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include "hidef.h"
#include "MC68HC908JB8.h"
#include "commands.h"
#include "usb.h"
#include "bdm.h"
#include "led.h"
#include "main.h"

/* basic program flow:

1. all BDM activity is driven by commands received from the USB
2. all BDM commands are executed from within the ISRs servicing the USB IRQ (i.e. with interrupts disabled)
3. suspend timekeeping is performed in the main loop
4. the only asynchronous activity is detection of external resets on RESET_IN pin through KBD interrupts

*/

/* timer counts up and the part is suspended when it reaches treshold value */
/* the USB interrupt resets the timer */
volatile signed char suspend_timer;

/* initialise the CPU */
void init(void) {
  CONFIG = CONFIG_URSTD_MASK | CONFIG_STOP_MASK | CONFIG_COPD_MASK; /* disable part reset on USB reset, enable STOP instruction & disable COP */
  ISCR = ISCR_ACK_MASK | ISCR_IMASK_MASK;         /* acknowledge IRQ interrupt and disable it */
  bdm_init();
  usb_init();
  EnableInterrupts;                               /* enable interrupts */
}

/* wait 100us */
void wait100us(void) {
  asm {
    LDA   #((BUS_FREQUENCY*100/3)-4-2-4)  /* minus cycles needed for BSR, LDA and RTS */
  loop:
    DBNZA loop  /* 3 cycles per iteration */
  }
}

/* main function */
void main(void) {
//FOR USB LOAD DELETE IF NOT NEEDED
  POCR_PAP=1;                                         //pull up on port A
  DDRA = 0x00;                                                                      //direction of the port A used to enter ICP mode
  if (!(PTA&0x01))                                                                      //if port A0 is high then ICP mode
  {

    UCR3 = 0x04;                                  //internal pull up resistors enable
    asm{                                                                                    //enter ICP mode
    jmp 0xfa19;
    };
  }

//STOP DELETING!!!
  init();
  /* testing */
#if 0
//  bdm12_connect();
  bdm_status.sync_length=4693;
  bdm_rx_tx_select();

  while(1) {
    unsigned char i;

    BDM12_CMD_BDREADB(BDM12_STS_ADDR,&i);
    for (i=0;i<120;i++) asm(nop);

  }
#endif

  while(1) {
    wait100us();
    suspend_timer++;
    if (suspend_timer>=SUSPEND_TIME) {
      /* host is not sending keepalive signals, time to suspend all operation */
      /* BDM is in idle mode when not communicating, so nothing to do there */
      unsigned int i;
      KBSCR = KBSCR_IMASKK_MASK | KBSCR_ACKK_MASK; /* acknowledge any pending interrupt and disable KBD interrupts; this will prevent RESET activity waking us up out of stop */
      led_state=LED_OFF;  /* switch the LED off */
      LED_SW_OFF;         /* do it now, the interrupt which would do it normally is not going to come */
      UIR0_SUSPND=1;      /* suspend USB */
      while (suspend_timer) asm(STOP);  /* go to sleep, wait for USB resume or reset */
      for (i=0;i<RESUME_RECOVERY;i++) wait100us();  /* wait for host to recover */
      led_state=LED_ON;   /* switch the LED back on */
      bdm_init();         /* reinitialise the BDM after wake-up as the device might have been disconected in the meantime; assume nothing */
    }
  }
}   
