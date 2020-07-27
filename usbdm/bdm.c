/*
    Turbo BDM Light - BDM communication
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

#include "MC68HC908JB8.h"
#include "hidef.h"
#include "bdm.h"
#include "commands.h"
#include "usb.h"

/* i, j & k are used as timing and general purpose variables in the Tx & Rx routines */
/* must be placed into the direct segment */
static unsigned char i;
static unsigned char j;
static unsigned char k;

/* pointers to Rx & Tx routines, tables for selection */
unsigned char (*bdm_rx_ptr)(void) = bdm_empty_rx_tx;
void (*bdm_tx_ptr)(unsigned char) = bdm_empty_rx_tx;

/* when SYNC length expressed in 60MHz ticks is ABOVE OR EQUAL to the value in the table, the correspnding pointer is selected */
/* if SYNC is shother than the first entry, the target runs too fast */
/* if SYNC is longer or equal to the last entry, the target runs too slow */

/*
const unsigned int bdm_tx_sel_tresholds[]=
  {914,     1129,    1335,    1541,    1747,    1952,    2157,    2465,    2877,    3288,
   3800,    4418,    5136,    6059,    7189,    8524,    10066,   11814,   13867,   16988};
*/
//new values for tics as the JB16 CPU is 2 times faster
const unsigned int bdm_tx_sel_tresholds[]=
  {914/2,     1129/2,    1335/2,    1541/2,    1747/2,    1952/2,    2157/2,    2465/2,    2877/2,    3288/2,
   3800/2,    4418/2,    5136/2,    6059/2,    7189/2,    8524/2,    10066/2,   11814/2,   13867/2,   16988/2};
void (* const bdm_tx_sel_ptrs[])(unsigned char)=
  {bdm_tx1, bdm_tx2, bdm_tx3, bdm_tx4, bdm_tx5, bdm_tx6, bdm_tx7, bdm_tx8, bdm_tx9, bdm_tx10,
   bdm_tx11,bdm_tx12,bdm_tx13,bdm_tx14,bdm_tx15,bdm_tx16,bdm_tx17,bdm_tx18,bdm_tx19,bdm_empty_rx_tx};
/*
const unsigned int bdm_rx_sel_tresholds[]=
  {853,     1101,    1347,    1592,    1837,    2202,    2694,    3303,    4042,    4897,
   5998,    7346,    9055,    11257,   13952,   17919};
*/
//new values for tics as the JB16 CPU is 2 times faster
const unsigned int bdm_rx_sel_tresholds[]=
  {853/2,     1101/2,    1347/2,    1592/2,    1837/2,    2202/2,    2694/2,    3303/2,    4042/2,    4897/2,
   5998/2,    7346/2,    9055/2,    11257/2,   13952/2,   17919/2};
unsigned char (* const bdm_rx_sel_ptrs[])(void)=
  {bdm_rx1, bdm_rx2, bdm_rx3, bdm_rx4, bdm_rx5, bdm_rx6, bdm_rx7, bdm_rx8, bdm_rx9, bdm_rx10,
   bdm_rx11,bdm_rx12,bdm_rx13,bdm_rx14,bdm_rx15,bdm_empty_rx_tx};

/* status of the BDM communication */


bdm_status_t bdm_status;

/* private macros */

#define ACKN_CLR   asm (BCLR TSC0_CH0F_BITNUM,TSC0); /* clear timer capture flag, in assembly to make sure the compiler does not mess it up... */

/* functions */

/* connect to HC12 or HCS12 target */
/* returns 0 on succes or 1 on failure */
unsigned char bdm12_connect(void) {
  unsigned char bdm_sts;
  bdm_status.ackn = WAIT;                                                           /* clear the ACKN feature */
  bdm_status.reset = NO_RESET_ACTIVITY;               /* clear the reset flag */
  /* first wait until both RESET and BDM are high */
  TMOD = RESET_WAIT * BUS_FREQUENCY * 16;             /* this is by 2.4% longer than (RESET_WAIT * BUS_FREQUENCY * 1000)/64, but cannot do that in 16-bit math */
  TSC = TSC_TRST_MASK | TSC_PS1_MASK | TSC_PS2_MASK;  /* reset the timer and start counting @ bus clock divided by 64 */
  TSC_TOF = 0;
  while(((RESET_IN==0)||(BDM_IN==0))&&(TSC_TOF==0));  /* wait for reset and bdm to rise or timeout */
  if (TSC_TOF) return(1);                             /* timeout */
  if (bdm_sync_meas()) {
    /* trying to measure SYNC was not successful */
      return(1);
  }
    if (bdm_rx_tx_select()) return(1); /* if at least one of the two methods succeeded, we can select the right Rx and Tx routines */
    bdm_ackn_init();    /* try the ACKN feature */
    BDM12_CMD_BDREADB(BDM12_STS_ADDR,&bdm_sts);
    if ((bdm_sts&0x80)==0) BDM12_CMD_BDWRITEB(BDM12_STS_ADDR,0x80|bdm_sts); /* if BDM not enabled yet, enable it so it can be made active */
  return(0);          /* connection established */
}

/* resets the target */
/* mode == 0 -> reset to special mode, mode == 1 -> reset to normal mode */
/* returns zero on success and non-zero if reset signal stuck to ground */
unsigned char bdm_reset(unsigned char mode) {
  BDM_DIR1 = 1;                                       /* stop driving the BDM */
  KBSCR_IMASKK = 1;                                   /* mask KBD interrupts */
  TMOD = RESET_WAIT * BUS_FREQUENCY * 16;             /* this is by 2.4% longer than (RESET_WAIT * BUS_FREQUENCY * 1000)/64, but cannot do that in 16-bit math */
  TSC = TSC_TRST_MASK | TSC_PS1_MASK | TSC_PS2_MASK;  /* reset the timer and start counting @ bus clock divided by 64 */
  TSC_TOF = 0;
  while((RESET_IN==0)&&(TSC_TOF==0));                 /* wait for reset to rise or timeout */
  if (TSC_TOF) return(1);
  if (mode==0) {
    BDM_OUT = 0;                                      /* drive BDM low */
    BDM_DIR1 = 0;
    TMOD = RESET_SETTLE * BUS_FREQUENCY;              /* time to wait for signals to settle */
    TSC = TSC_TRST_MASK;                              /* reset the timer and start counting @ bus clock */
    TSC_TOF = 0;
    while(TSC_TOF==0);                                /* wait for timeout */
  }
  RESET_OUT = 0;                                      /* start driving RESET */
  RESET_OUT_DDR |= RESET_OUT_MASK;
  TMOD = RESET_LENGTH * BUS_FREQUENCY * 16;           /* time of the RESET pulse */
  TSC = TSC_TRST_MASK | TSC_PS1_MASK | TSC_PS2_MASK;  /* reset the timer and start counting @ bus clock divided by 64 */
  TSC_TOF = 0;
  while(TSC_TOF==0);                                  /* wait for timeout */
  RESET_OUT = 1;                                                                            /* stop driving the RESET */
  RESET_OUT_DDR &= ~RESET_OUT_MASK;                   /* and make the pin input again so nothing interferes with it */
  TMOD = RESET_WAIT * BUS_FREQUENCY * 16;             /* time to wait for reset to rise */
  TSC = TSC_TRST_MASK | TSC_PS1_MASK | TSC_PS2_MASK;  /* reset the timer and start counting @ bus clock divided by 64 */
  TSC_TOF = 0;
  while((RESET_IN==0)&&(TSC_TOF==0));                 /* wait for reset to rise or timeout */
  if (TSC_TOF) return(1);
  if (mode==0) {
    TMOD = RESET_SETTLE * BUS_FREQUENCY;              /* time to wait for signals to settle */
    TSC = TSC_TRST_MASK;                              /* reset the timer and start counting @ bus clock */
    TSC_TOF = 0;
    while(TSC_TOF==0);                                /* wait for timeout */
    asm {
      CLRX                                                        /* point to PTA */
      CLRH
      LDA   #BDM_OUT_MASK + RESET_OUT_MASK
      STA   ,X                                        /* bring BDM high */
      ORA   #BDM_DIR1_MASK
      STA   ,X                                        /* stop driving the BDM */
      /* it took 4 cycles from bringing BDM high to stop driving it and that is fast enough up to 16*3/4 = 12 MHz of BDM frequency on JB8 */
    }
  }
  /* wait one more settling time before allowing anythig else to happen on the BDM */
  TMOD = RESET_SETTLE * BUS_FREQUENCY;                /* time to wait for signals to settle */
  TSC = TSC_TRST_MASK;                                /* reset the timer and start counting @ bus clock */
  TSC_TOF = 0;
  while(TSC_TOF==0);                                  /* wait for timeout */
  KBSCR_ACKK=1;                                       /* acknowledge KBD interrupt */
  KBSCR_IMASKK = 0;                                   /* enable KBD interrupts again */
  return(0);
}

/* interrupt function servicing the KBD interrupt from RESET_IN assertion */
void interrupt bdm_reset_sense(void) {
  KBSCR_ACKK=1;                    /* acknowledge the interrupt */
  bdm_status.reset=RESET_DETECTED; /* record the fact that reset was asserted */
}

/* measures the SYNC length and writes the result into bdm_status structure */
/* returns 0 on succes and non-zero on timeout */
unsigned char bdm_sync_meas(void) {
  unsigned int time;
  bdm_status.speed = NO_INFO; /* indicate that we do not have a clue about target speed at the moment... */
  TMOD = BDM_SYNC_REQ * BUS_FREQUENCY;  /* load TMOD with the longest SYNC REQUEST possible */
  BDM_DIR1 = 1;         /* stop driving the BDM */
  TSC = TSC_TRST_MASK;  /* restart the timer */
  TSC_TOF = 0;          /* clear TOF */
  while((TSC_TOF==0)&&(BDM_IN==0)); /* wait for the BDM to come high or timeout */
  if (TSC_TOF) return(1);           /* timeout ! */
  BDM_OUT = 0;
  BDM_DIR1 = 0;         /* bring BDM low */
  TSC = TSC_TRST_MASK;  /* restart the timer */
  TSC_TOF = 0;          /* clear TOF */
  while(TSC_TOF==0);    /* wait for timeout */
  TSC_TOF = 0;          /* clear the TOF flag */
  TSC0 = TSC0_ELS0B_MASK; /* capture falling edges */
  TSC0_CH0F=0;          /* clear capture flag */
  //this is not fast enough, the target will start driving 16 BDM cycles after the pin comes high
  //BDM_OUT_PORT = BDM_OUT_MASK;  /* bring BDM high */
  //BDM_DIR1_PORT = BDM_OUT_MASK | BDM_DIR1_MASK; /* stop driving it */
  asm {
    CLRX                          /* point to PTA */
    CLRH
    LDA   #BDM_OUT_MASK + RESET_OUT_MASK
    STA   ,X            /* bring BDM high */
    ORA   #BDM_DIR1_MASK
    STA   ,X            /* stop driving the BDM */
    /* it took 4 cycles from bringing BDM high to stop driving it and that is fast enough up to 16*3/4 = 12 MHz of BDM frequency on JB8 */
  }
  while ((TSC0_CH0F==0)&&(TSC_TOF==0));     /* wait for capture or timeout */
  time=TCH0;                                /* capture start of the SYNC */
  TSC0 = TSC0_ELS0A_MASK;                   /* capture rising edge, clear capture flag */
  /* it takes 32 cycles to reenable capture (worst case) which is good enough up to 128*3/32 = 12 MHz again on JB8 */
  while ((TSC0_CH0F==0)&&(TSC_TOF==0));     /* wait for capture or timeout */
  time=TCH0-time;                           /* calculate length of the SYNC */
  if (TSC_TOF) return(2);                   /* timeout ! */
  #if (BUS_FREQUENCY==3)
    bdm_status.sync_length=(time<<2)+(time<<4);   /* multiply by 20 to get the time in 60MHz ticks */
  #elif (BUS_FREQUENCY==6)
    bdm_status.sync_length=(time<<1)+(time<<3);   /* multiply by 10 to get the time in 60MHz ticks */
  #else
    bdm_status.sync_length=time*(60/BUS_FREQUENCY); /* if not 3 or 6 then do it the stupid way... */
  #endif
  bdm_status.speed = SYNC_SUPPORTED; /* SYNC feature is supported by the target */
  return(0);
}

/* waits 64 BDM cycles of the target MCU */
void bdm_wait64(void) {
  asm {
    LDA  bdm_status.wait64_cnt  /* number of loop iterations to wait */
  loop:
    DBNZA loop                  /* 3 cycles per iteration */
  }
}

/* waits 150 BDM cycles of the target MCU */
void bdm_wait150(void) {
  asm {
    LDA  bdm_status.wait150_cnt  /* number of loop iterations to wait */
  loop:
    DBNZA loop                  /* 3 cycles per iteration */
  }
}

/* enables ACKN and prepares the timer for easy ACKN timeout use */
void bdm_ackn_init(void) {
  TMOD = ACKN_TIMEOUT * BUS_FREQUENCY;  /* the timer will set the TOF flag as soon as the timeout time is reached */
  TSC = TSC_TRST_MASK;    /* start the timer, prescaler = 1 */
  TSC0 = TSC0_ELS0A_MASK;   /* capture rising edges */
  bdm_status.ackn = ACKN; /* switch ACKN on */
  BDM_CMD_ACK_ENABLE();   /* send the enable command to the target */
}

/* waits for ACKN pulse from the target */
void bdm_ackn(void) {
  TSC = TSC_TRST_MASK;                      /* clear the TOF flag if set and restart the timer */
  TSC_TOF = 0;                              /* clear TOF */
  while ((TSC0_CH0F==0)&&(TSC_TOF==0));     /* wait for capture or timeout */
  TSC0 = TSC0_ELS0A_MASK;                   /* capture rising edge, clear capture flag */
  if (TSC_TOF) {
    /* timeout */
    bdm_status.ackn = WAIT;                 /* switch the ackn feature off */
  }
}

/* selects Rx and Tx routine to be used according to SYNC length in bdm_status structure */
/* returns 0 on success and 1 when no appropriate function can be found */
unsigned char bdm_rx_tx_select(void) {
  signed char i;
  bdm_rx_ptr = bdm_empty_rx_tx;                         /* clear the pointers */
  bdm_tx_ptr = bdm_empty_rx_tx;
  for (i=(sizeof(bdm_tx_sel_tresholds)/2)-1;i>=0;i--) { /* search through the table */
    if (bdm_status.sync_length>=bdm_tx_sel_tresholds[i]) {
      bdm_tx_ptr = bdm_tx_sel_ptrs[i];                  /* is SYNC is >=, select this routine */
      break;                                            /* and finish the search */
    }
  }
  if (bdm_tx_ptr==bdm_empty_rx_tx) return(1);           /* check if valid routine has been found */
  for (i=(sizeof(bdm_rx_sel_tresholds)/2)-1;i>=0;i--) { /* do the same for Rx as well */
    if (bdm_status.sync_length>=bdm_rx_sel_tresholds[i]) {
      bdm_rx_ptr = bdm_rx_sel_ptrs[i];
      break;
    }
  }
  if (bdm_rx_ptr==bdm_empty_rx_tx) return(1);
  /* there is plenty of overhead: JSR, LDA and RTS of the WAIT, RTS from the previous routine, JSR to the next routine: at least 21 cycles */
  /* cannot subtract more than the smallest possible result -1 as the number must be > 0 */
  bdm_status.wait64_cnt = bdm_status.sync_length/(3*(60/BUS_FREQUENCY)*128/64)-6;
  bdm_status.wait150_cnt = bdm_status.sync_length/(3*(60/BUS_FREQUENCY)*128/150)-7;
  return(0);
}

/* when no function appropriate for the target speed can be found the following routine is selected */
/* this is just to make things safe and to make sure there is a place to jump to in such a case */
unsigned char bdm_empty_rx_tx(void) {
  command_buffer[0]=CMD_FAILED; /* if BDM command is executed with this routine set as either Tx or Rx it has failed... */
  return(0);
}

/* initialises I/O pins and internal variables of the module */
void bdm_init() {
  bdm_status.ackn = WAIT;                 /* select default behaviour & values */
  bdm_status.target_type = HC12;
  bdm_status.reset = NO_RESET_ACTIVITY;
  bdm_status.speed = NO_INFO;
  POCR_PAP=1;                     /* enable pullups on RESET_OUT, RESET_IN and BDM_DRIVE */
  RESET_IN_DDR &= ~RESET_IN_MASK; /* RESET_IN is input */
  RESET_OUT = 1;                  /* RESET_OUT inactive */
  RESET_OUT_DDR &= ~RESET_OUT_MASK;/* RESET_OUT is input for the moment, only turn it out when needed as Rx routines interfere with RESET_OUT */
  BDM_IN_DDR &= ~BDM_IN_MASK;     /* BDM_IN is input */
  BDM_OUT = 1;                    /* idle state of BDM line is high */
  BDM_OUT_DDR |= BDM_OUT_MASK;    /* BDM_OUT is output */
  BDM_DIR2 = 0;                   /* prepare to drive the BDM once DIR2 is turned to output */
  BDM_DIR2_DDR &= ~BDM_DIR2_MASK; /* turn BDM_DIR2 to input */
  BDM_DIR1 = 1;                   /* do not drive the BDM */
  BDM_DIR1_DDR |= BDM_DIR1_MASK;  /* turn BDM_DIR1 to output */
  /* now give the BDM interface enough time to soft reset in case it was doing something before */
  TMOD = SOFT_RESET * BUS_FREQUENCY;  /* load TMOD with the longest SOFT RESET time possible */
  TSC = TSC_TRST_MASK;                /* restart the timer */
  TSC_TOF = 0;                        /* clear TOF */
  while(TSC_TOF==0);                  /* wait for timeout */
  KBIER = RESET_IN_MASK;          /* enable RESET_IN as keyboard pin */
  KBSCR = KBSCR_ACKK_MASK;        /* acknowledge any pending interrupt and enable KBD to cause IRQ (once enabled) */
}

/* prepares transmit of BDM data */
/* assumes that BDM_OUT is 1 */
/* interrupts need to be disabled for the duration of all BDM commands */
/* it is up to the caller to see to this */
void bdm_tx_prepare(void) {
  BDM_OUT_PORT = BDM_DIR1_MASK | BDM_OUT_MASK | RESET_OUT_MASK; /* make sure BDM will be driven high once the driver is enabled */
  BDM_OUT_DDR = BDM_OUT_MASK;     /* turn DIR1 & RESET_OUT to input (now the only output in PTA register is BDM_OUT) */
  BDM_DIR2_DDR = BDM_DIR2_MASK;   /* bring DIR low (start driving the BDM) */
}

/* finishes transmit of BDM data */
void bdm_tx_finish(void) {
  BDM_OUT_PORT = BDM_DIR1_MASK | BDM_OUT_MASK | RESET_OUT_MASK;  /* do not drive RESET nor BDM once DIR1 is output again */
  BDM_DIR2_DDR = 0;                                              /* stop driving DIR2 low */
  BDM_DIR1_DDR = BDM_DIR1_MASK | BDM_OUT_MASK;                   /* enable DIR1 again, leave RESET_OUT as input */
}

/* receive 8 bit of data, MSB first */
/* expects DIR1 active and DIR2 inactive */
/* 6.75 - 9.75 MHz */
unsigned char bdm_rx1(void) {
  #pragma NO_RETURN
  asm {
    LDA   #BDM_DIR1_MASK+RESET_OUT_MASK /* contents of A will be driven to PTA in order to switch the driver off */
    CLRX                    /* prepare HX to point to PTA */
    CLRH
    /* bit 7 (MSB) */
    STX   ,X                /* drive BDM low */
    STA   ,X                /* switch BDM to high impedance */
    LDX   ,X                /* load X with value on the PTA port (including BDM_IN) */
    PSHX                    /* store the value on the stack */
    CLRX                    /* clear X again */
    /* bit 6 */
    STX   ,X                /* drive BDM low */
    STA   ,X                /* switch BDM to high impedance */
    LDX   ,X                /* load X with value on the PTA port (including BDM_IN) */
    PSHX                    /* store the value on the stack */
    CLRX                    /* clear X again */
    /* bit 5 */
    STX   ,X                /* drive BDM low */
    STA   ,X                /* switch BDM to high impedance */
    LDX   ,X                /* load X with value on the PTA port (including BDM_IN) */
    PSHX                    /* store the value on the stack */
    CLRX                    /* clear X again */
    /* bit 4 */
    STX   ,X                /* drive BDM low */
    STA   ,X                /* switch BDM to high impedance */
    LDX   ,X                /* load X with value on the PTA port (including BDM_IN) */
    PSHX                    /* store the value on the stack */
    CLRX                    /* clear X again */
    /* bit 3 */
    STX   ,X                /* drive BDM low */
    STA   ,X                /* switch BDM to high impedance */
    LDX   ,X                /* load X with value on the PTA port (including BDM_IN) */
    PSHX                    /* store the value on the stack */
    CLRX                    /* clear X again */
    /* bit 2 */
    STX   ,X                /* drive BDM low */
    STA   ,X                /* switch BDM to high impedance */
    LDX   ,X                /* load X with value on the PTA port (including BDM_IN) */
    PSHX                    /* store the value on the stack */
    CLRX                    /* clear X again */
    /* bit 1 */
    STX   ,X                /* drive BDM low */
    STA   ,X                /* switch BDM to high impedance */
    LDX   ,X                /* load X with value on the PTA port (including BDM_IN) */
    PSHX                    /* store the value on the stack */
    CLRX                    /* clear X again */
    /* bit 0 */
    STX   ,X                /* drive BDM low */
    STA   ,X                /* switch BDM to high impedance */
    LDX   ,X                /* load X with value on the PTA port (including BDM_IN) */
    /* now get the bit values (last value is in X, previous 7 on stack) */
    JMP   rx_stack_decode
  }
}
/* 5.4 - 7.8 MHz */
unsigned char bdm_rx2(void) {
  #pragma NO_RETURN
  asm {
    LDA   #BDM_DIR1_MASK+RESET_OUT_MASK /* contents of A will be driven to PTA in order to switch the driver off */
    CLRX                    /* prepare HX to point to PTA */
    CLRH
    /* bit 7 (MSB) */
    STX   ,X                /* drive BDM low */
    STA   ,X                /* switch BDM to high impedance */
    NOP                     /* wait 1 cycle */
    LDX   ,X                /* load X with value on the PTA port (including BDM_IN) */
    PSHX                    /* store the value on the stack */
    CLRX                    /* clear X again */
    /* bit 6 */
    STX   ,X                /* drive BDM low */
    STA   ,X                /* switch BDM to high impedance */
    NOP                     /* wait 1 cycle */
    LDX   ,X                /* load X with value on the PTA port (including BDM_IN) */
    PSHX                    /* store the value on the stack */
    CLRX                    /* clear X again */
    /* bit 5 */
    STX   ,X                /* drive BDM low */
    STA   ,X                /* switch BDM to high impedance */
    NOP                     /* wait 1 cycle */
    LDX   ,X                /* load X with value on the PTA port (including BDM_IN) */
    PSHX                    /* store the value on the stack */
    CLRX                    /* clear X again */
    /* bit 4 */
    STX   ,X                /* drive BDM low */
    STA   ,X                /* switch BDM to high impedance */
    NOP                     /* wait 1 cycle */
    LDX   ,X                /* load X with value on the PTA port (including BDM_IN) */
    PSHX                    /* store the value on the stack */
    CLRX                    /* clear X again */
    /* bit 3 */
    STX   ,X                /* drive BDM low */
    STA   ,X                /* switch BDM to high impedance */
    NOP                     /* wait 1 cycle */
    LDX   ,X                /* load X with value on the PTA port (including BDM_IN) */
    PSHX                    /* store the value on the stack */
    CLRX                    /* clear X again */
    /* bit 2 */
    STX   ,X                /* drive BDM low */
    STA   ,X                /* switch BDM to high impedance */
    NOP                     /* wait 1 cycle */
    LDX   ,X                /* load X with value on the PTA port (including BDM_IN) */
    PSHX                    /* store the value on the stack */
    CLRX                    /* clear X again */
    /* bit 1 */
    STX   ,X                /* drive BDM low */
    STA   ,X                /* switch BDM to high impedance */
    NOP                     /* wait 1 cycle */
    LDX   ,X                /* load X with value on the PTA port (including BDM_IN) */
    PSHX                    /* store the value on the stack */
    CLRX                    /* clear X again */
    /* bit 0 */
    STX   ,X                /* drive BDM low */
    STA   ,X                /* switch BDM to high impedance */
    NOP                     /* wait 1 cycle */
    LDX   ,X                /* load X with value on the PTA port (including BDM_IN) */
    /* now get the bit values (last value is in X, previous 7 on stack) */
    JMP   rx_stack_decode
  }
}
/* 4.5 - 6.5 MHz */
unsigned char bdm_rx3(void) {
  #pragma NO_RETURN
  asm {
    LDA   #BDM_DIR1_MASK+RESET_OUT_MASK /* contents of A will be driven to PTA in order to switch the driver off */
    CLRX                    /* prepare HX to point to PTA */
    CLRH
    /* bit 7 (MSB) */
    STX   ,X                /* drive BDM low */
    STA   ,X                /* switch BDM to high impedance */
    BIT   ,X                /* wait 2 cycles */
    LDX   ,X                /* load X with value on the PTA port (including BDM_IN) */
    PSHX                    /* store the value on the stack */
    CLRX                    /* clear X again */
    /* bit 6 */
    STX   ,X                /* drive BDM low */
    STA   ,X                /* switch BDM to high impedance */
    BIT   ,X                /* wait 2 cycles */
    LDX   ,X                /* load X with value on the PTA port (including BDM_IN) */
    PSHX                    /* store the value on the stack */
    CLRX                    /* clear X again */
    /* bit 5 */
    STX   ,X                /* drive BDM low */
    STA   ,X                /* switch BDM to high impedance */
    BIT   ,X                /* wait 2 cycles */
    LDX   ,X                /* load X with value on the PTA port (including BDM_IN) */
    PSHX                    /* store the value on the stack */
    CLRX                    /* clear X again */
    /* bit 4 */
    STX   ,X                /* drive BDM low */
    STA   ,X                /* switch BDM to high impedance */
    BIT   ,X                /* wait 2 cycles */
    LDX   ,X                /* load X with value on the PTA port (including BDM_IN) */
    PSHX                    /* store the value on the stack */
    CLRX                    /* clear X again */
    /* bit 3 */
    STX   ,X                /* drive BDM low */
    STA   ,X                /* switch BDM to high impedance */
    BIT   ,X                /* wait 2 cycles */
    LDX   ,X                /* load X with value on the PTA port (including BDM_IN) */
    PSHX                    /* store the value on the stack */
    CLRX                    /* clear X again */
    /* bit 2 */
    STX   ,X                /* drive BDM low */
    STA   ,X                /* switch BDM to high impedance */
    BIT   ,X                /* wait 2 cycles */
    LDX   ,X                /* load X with value on the PTA port (including BDM_IN) */
    PSHX                    /* store the value on the stack */
    CLRX                    /* clear X again */
    /* bit 1 */
    STX   ,X                /* drive BDM low */
    STA   ,X                /* switch BDM to high impedance */
    BIT   ,X                /* wait 2 cycles */
    LDX   ,X                /* load X with value on the PTA port (including BDM_IN) */
    PSHX                    /* store the value on the stack */
    CLRX                    /* clear X again */
    /* bit 0 */
    STX   ,X                /* drive BDM low */
    STA   ,X                /* switch BDM to high impedance */
    BIT   ,X                /* wait 2 cycles */
    LDX   ,X                /* load X with value on the PTA port (including BDM_IN) */
    /* now get the bit values (last value is in X, previous 7 on stack) */
    JMP   rx_stack_decode
  }
}
/* 3.86 - 5.57 MHz */
unsigned char bdm_rx4(void) {
  #pragma NO_RETURN
  asm {
    LDA   #BDM_DIR1_MASK+RESET_OUT_MASK /* contents of A will be driven to PTA in order to switch the driver off */
    CLRX                    /* prepare HX to point to PTA */
    CLRH
    /* bit 7 (MSB) */
    STX   ,X                /* drive BDM low */
    STA   ,X                /* switch BDM to high impedance */
    BRN   0                 /* wait 3 cycles */
    LDX   ,X                /* load X with value on the PTA port (including BDM_IN) */
    PSHX                    /* store the value on the stack */
    CLRX                    /* clear X again */
    NOP                     /* wait 1 cycle */
    /* bit 6 */
    STX   ,X                /* drive BDM low */
    STA   ,X                /* switch BDM to high impedance */
    BRN   0                 /* wait 3 cycles */
    LDX   ,X                /* load X with value on the PTA port (including BDM_IN) */
    PSHX                    /* store the value on the stack */
    CLRX                    /* clear X again */
    NOP                     /* wait 1 cycle */
    /* bit 5 */
    STX   ,X                /* drive BDM low */
    STA   ,X                /* switch BDM to high impedance */
    BRN   0                 /* wait 3 cycles */
    LDX   ,X                /* load X with value on the PTA port (including BDM_IN) */
    PSHX                    /* store the value on the stack */
    CLRX                    /* clear X again */
    NOP                     /* wait 1 cycle */
    /* bit 4 */
    STX   ,X                /* drive BDM low */
    STA   ,X                /* switch BDM to high impedance */
    BRN   0                 /* wait 3 cycles */
    LDX   ,X                /* load X with value on the PTA port (including BDM_IN) */
    PSHX                    /* store the value on the stack */
    CLRX                    /* clear X again */
    NOP                     /* wait 1 cycle */
    /* bit 3 */
    STX   ,X                /* drive BDM low */
    STA   ,X                /* switch BDM to high impedance */
    BRN   0                 /* wait 3 cycles */
    LDX   ,X                /* load X with value on the PTA port (including BDM_IN) */
    PSHX                    /* store the value on the stack */
    CLRX                    /* clear X again */
    NOP                     /* wait 1 cycle */
    /* bit 2 */
    STX   ,X                /* drive BDM low */
    STA   ,X                /* switch BDM to high impedance */
    BRN   0                 /* wait 3 cycles */
    LDX   ,X                /* load X with value on the PTA port (including BDM_IN) */
    PSHX                    /* store the value on the stack */
    CLRX                    /* clear X again */
    NOP                     /* wait 1 cycle */
    /* bit 1 */
    STX   ,X                /* drive BDM low */
    STA   ,X                /* switch BDM to high impedance */
    BRN   0                 /* wait 3 cycles */
    LDX   ,X                /* load X with value on the PTA port (including BDM_IN) */
    PSHX                    /* store the value on the stack */
    CLRX                    /* clear X again */
    NOP                     /* wait 1 cycle */
    /* bit 0 */
    STX   ,X                /* drive BDM low */
    STA   ,X                /* switch BDM to high impedance */
    BRN   0                 /* wait 3 cycles */
    LDX   ,X                /* load X with value on the PTA port (including BDM_IN) */
    /* now get the bit values (last value is in X, previous 7 on stack) */
    JMP   rx_stack_decode
  }
}
/* 3.38 - 4.48 MHz */
unsigned char bdm_rx5(void) {
  #pragma NO_RETURN
  asm {
    LDA   #BDM_DIR1_MASK+RESET_OUT_MASK /* contents of A will be driven to PTA in order to switch the driver off */
    CLRX                    /* prepare HX to point to PTA */
    CLRH
    /* bit 7 (MSB) */
    STX   ,X                /* drive BDM low */
    NOP                     /* wait 1 cycle */
    STA   ,X                /* switch BDM to high impedance */
    BRN   0                 /* wait 3 cycles */
    LDX   ,X                /* load X with value on the PTA port (including BDM_IN) */
    PSHX                    /* store the value on the stack */
    CLRX                    /* clear X again */
    BIT   ,X                /* wait 2 cycles */
    /* bit 6 */
    STX   ,X                /* drive BDM low */
    NOP                     /* wait 1 cycle */
    STA   ,X                /* switch BDM to high impedance */
    BRN   0                 /* wait 3 cycles */
    LDX   ,X                /* load X with value on the PTA port (including BDM_IN) */
    PSHX                    /* store the value on the stack */
    CLRX                    /* clear X again */
    BIT   ,X                /* wait 2 cycles */
    /* bit 5 */
    STX   ,X                /* drive BDM low */
    NOP                     /* wait 1 cycle */
    STA   ,X                /* switch BDM to high impedance */
    BRN   0                 /* wait 3 cycles */
    LDX   ,X                /* load X with value on the PTA port (including BDM_IN) */
    PSHX                    /* store the value on the stack */
    CLRX                    /* clear X again */
    BIT   ,X                /* wait 2 cycles */
    /* bit 4 */
    STX   ,X                /* drive BDM low */
    NOP                     /* wait 1 cycle */
    STA   ,X                /* switch BDM to high impedance */
    BRN   0                 /* wait 3 cycles */
    LDX   ,X                /* load X with value on the PTA port (including BDM_IN) */
    PSHX                    /* store the value on the stack */
    CLRX                    /* clear X again */
    BIT   ,X                /* wait 2 cycles */
    /* bit 3 */
    STX   ,X                /* drive BDM low */
    NOP                     /* wait 1 cycle */
    STA   ,X                /* switch BDM to high impedance */
    BRN   0                 /* wait 3 cycles */
    LDX   ,X                /* load X with value on the PTA port (including BDM_IN) */
    PSHX                    /* store the value on the stack */
    CLRX                    /* clear X again */
    BIT   ,X                /* wait 2 cycles */
    /* bit 2 */
    STX   ,X                /* drive BDM low */
    NOP                     /* wait 1 cycle */
    STA   ,X                /* switch BDM to high impedance */
    BRN   0                 /* wait 3 cycles */
    LDX   ,X                /* load X with value on the PTA port (including BDM_IN) */
    PSHX                    /* store the value on the stack */
    CLRX                    /* clear X again */
    BIT   ,X                /* wait 2 cycles */
    /* bit 1 */
    STX   ,X                /* drive BDM low */
    NOP                     /* wait 1 cycle */
    STA   ,X                /* switch BDM to high impedance */
    BRN   0                 /* wait 3 cycles */
    LDX   ,X                /* load X with value on the PTA port (including BDM_IN) */
    PSHX                    /* store the value on the stack */
    CLRX                    /* clear X again */
    BIT   ,X                /* wait 2 cycles */
    /* bit 0 */
    STX   ,X                /* drive BDM low */
    NOP                     /* wait 1 cycle */
    STA   ,X                /* switch BDM to high impedance */
    BRN   0                 /* wait 3 cycles */
    LDX   ,X                /* load X with value on the PTA port (including BDM_IN) */
    /* now get the bit values (last value is in X, previous 7 on stack) */
    JMP   rx_stack_decode
  }
}
/* 2.7 - 3.9 MHz */
unsigned char bdm_rx6(void) {
  asm {
    LDA   #BDM_DIR1_MASK+RESET_OUT_MASK /* contents of A will be driven to PTA in order to switch the driver off */
    CLRX                    /* prepare HX to point to PTA */
    CLRH
    /* bit 7 (MSB) */
    STX   ,X                /* drive BDM low */
    NOP                     /* wait 1 cycle */
    STA   ,X                /* switch BDM to high impedance */
    BRSET 0,DDRA,0          /* wait 5 cycles */
    LDX   ,X                /* load X with value on the PTA port (including BDM_IN) */
        LSLX                    /* shift BDM_IN into C */
        LSLX
    CLRX                    /* clear X again */
    BRN   0                 /* wait 3 cycles */
    /* bit 6 */
    STX   ,X                /* drive BDM low */
    NOP                     /* wait 1 cycle */
    STA   ,X                /* switch BDM to high impedance */
    ROL   i                 /* shift C into i (from the bottom) */
        NOP                     /* wait 1 more cycle (5 cycles in total together with ROL) */
    LDX   ,X                /* load X with value on the PTA port (including BDM_IN) */
        LSLX                    /* shift BDM_IN into C */
        LSLX
    CLRX                    /* clear X again */
    BRN   0                 /* wait 3 cycles */
    /* bit 5 */
    STX   ,X                /* drive BDM low */
    NOP                     /* wait 1 cycle */
    STA   ,X                /* switch BDM to high impedance */
    ROL   i                 /* shift C into i (from the bottom) */
        NOP                     /* wait 1 more cycle */
    LDX   ,X                /* load X with value on the PTA port (including BDM_IN) */
        LSLX                    /* shift BDM_IN into C */
        LSLX
    CLRX                    /* clear X again */
    BRN   0                 /* wait 3 cycles */
    /* bit 4 */
    STX   ,X                /* drive BDM low */
    NOP                     /* wait 1 cycle */
    STA   ,X                /* switch BDM to high impedance */
    ROL   i                 /* shift C into i (from the bottom) */
        NOP                     /* wait 1 more cycle */
    LDX   ,X                /* load X with value on the PTA port (including BDM_IN) */
        LSLX                    /* shift BDM_IN into C */
        LSLX
    CLRX                    /* clear X again */
    BRN   0                 /* wait 3 cycles */
    /* bit 3 */
    STX   ,X                /* drive BDM low */
    NOP                     /* wait 1 cycle */
    STA   ,X                /* switch BDM to high impedance */
    ROL   i                 /* shift C into i (from the bottom) */
        NOP                     /* wait 1 more cycle */
    LDX   ,X                /* load X with value on the PTA port (including BDM_IN) */
        LSLX                    /* shift BDM_IN into C */
        LSLX
    CLRX                    /* clear X again */
    BRN   0                 /* wait 3 cycles */
    /* bit 2 */
    STX   ,X                /* drive BDM low */
    NOP                     /* wait 1 cycle */
    STA   ,X                /* switch BDM to high impedance */
    ROL   i                 /* shift C into i (from the bottom) */
        NOP                     /* wait 1 more cycle */
    LDX   ,X                /* load X with value on the PTA port (including BDM_IN) */
        LSLX                    /* shift BDM_IN into C */
        LSLX
    CLRX                    /* clear X again */
    BRN   0                 /* wait 3 cycles */
    /* bit 1 */
    STX   ,X                /* drive BDM low */
    NOP                     /* wait 1 cycle */
    STA   ,X                /* switch BDM to high impedance */
    ROL   i                 /* shift C into i (from the bottom) */
        NOP                     /* wait 1 more cycle */
    LDX   ,X                /* load X with value on the PTA port (including BDM_IN) */
        LSLX                    /* shift BDM_IN into C */
        LSLX
    CLRX                    /* clear X again */
    BRN   0                 /* wait 3 cycles */
    /* bit 0 */
    STX   ,X                /* drive BDM low */
    NOP                     /* wait 1 cycle */
    STA   ,X                /* switch BDM to high impedance */
    ROL   i                 /* shift C into i (from the bottom) */
        NOP                     /* wait 1 more cycle */
    LDX   ,X                /* load X with value on the PTA port (including BDM_IN) */
        LSLX                    /* shift BDM_IN into C */
        LSLX
    ROL   i                 /* shift C into i (from the bottom) */
    LDA   i                 /* load the result into A */
  }
}
/* 2.25 - 3.25 MHz */
unsigned char bdm_rx7(void) {
  asm {
    LDA   #BDM_DIR1_MASK+RESET_OUT_MASK /* contents of A will be driven to PTA in order to switch the driver off */
    MOV   #8,j              /* store number of iterations into j */
    CLRH                    /* prepare H */
  loop:
    /* the following construction is a bit awkward cause by the fact that there is time for loop iteration in the middle of the routine, but not at its end... */
    /* this is the end of bit processing, it is done 1 extra time before bit 7 (but that does not matter) */
    LDX   ,X                /* load X with value on the PTA port (including BDM_IN) */
        LSLX                    /* shift BDM_IN into C */
        LSLX
    ROL   i                 /* shift C into i (from the bottom) */
    NOP                     /* wait 1 cycle (5 in total with ROL) */
    CLRX                    /* clear X for HX to point to PTA */
    /* here the bit begins */
    STX   ,X                /* drive BDM low */
    NOP                     /* wait 1 cycle */
    STA   ,X                /* switch BDM to high impedance */
    DEC   j                   /* iterate the loop (this takes 7 cycles) */
    BNE   loop
    /* finish processing of bit 0 (after exit from the loop) */
    LDX   ,X                /* load X with value on the PTA port (including BDM_IN) */
        LSLX                    /* shift BDM_IN into C */
        LSLX
    ROL   i                 /* shift C into i (from the bottom) */
    LDA   i                 /* load the result into A */
  }
}
/* 1.8 - 2.6 MHz */
unsigned char bdm_rx8(void) {
  asm {
    LDA   #BDM_DIR1_MASK+RESET_OUT_MASK /* contents of A will be driven to PTA in order to switch the driver off */
    MOV   #8,j              /* store number of iterations into j */
    CLRH                    /* prepare H */
  loop:
    CLRX                    /* clear X to point to PTA, CLRX has been moved to the beginning of the algorithm here to prevent its execution for the last bit */
    STX   ,X                /* drive BDM low */
    NOP                     /* wait 2 cycles, cannot use BIT here as it changes C */
    NOP
    STA   ,X                /* switch BDM to high impedance */
    ROL   i                 /* shift C into i (from the bottom) */
    BRSET 0,DDRA,0          /* wait 5 cycles (9 in total with ROL) */
    LDX   ,X                /* load X with value on the PTA port (including BDM_IN) */
        LSLX                    /* shift BDM_IN into C */
        LSLX
    DEC   j                   /* iterate the loop (this takes 7 cycles) */
    BNE   loop
    ROL   i                 /* shift C into i (from the bottom) */
    LDA   i                 /* load the result into A */
  }
}
/* 1.5 - 2.166 MHz */
unsigned char bdm_rx9(void) {
  asm {
    LDA   #BDM_DIR1_MASK+RESET_OUT_MASK /* contents of A will be driven to PTA in order to switch the driver off */
    MOV   #8,j              /* store number of iterations into j */
    CLRH                    /* prepare H */
  loop:
    CLRX                    /* clear X to point to PTA, CLRX has been moved to the beginning of the algorithm here to prevent its execution for the last bit */
    STX   ,X                /* drive BDM low */
    NOP                     /* wait 2 cycles, cannot use BIT here as it changes C */
    NOP
    STA   ,X                /* switch BDM to high impedance */
    ROL   i                 /* shift C into i (from the bottom) */
    BIT   ,X                /* wait 8 cycles (12 in total with ROL) */
    BIT   ,X
    BIT   ,X
    BIT   ,X
    LDX   ,X                /* load X with value on the PTA port (including BDM_IN) */
        BIT   ,X                /* wait 9 cycles, iterate the loop while waiting (7 cycles) */
        LSLX                    /* shift BDM_IN into C */
        LSLX                    /* LSLX needs to be after the BIT as it chages C */
    DEC   j
    BNE   loop
    ROL   i                 /* shift C into i (from the bottom) */
    LDA   i                 /* load the result into A */
  }
}
/* 1.227 - 1.772 MHz */
unsigned char bdm_rx10(void) {
  asm {
    LDA   #BDM_DIR1_MASK+RESET_OUT_MASK /* contents of A will be driven to PTA in order to switch the driver off */
    MOV   #8,j              /* store number of iterations into j */
    CLRH                    /* prepare H */
  loop:
    CLRX                    /* clear X to point to PTA, CLRX has been moved to the beginning of the algorithm here to prevent its execution for the last bit */
    STX   ,X                /* drive BDM low */
    BRN   0                 /* wait 3 cycles */
    STA   ,X                /* switch BDM to high impedance */
    BIT   ,X                /* wait 15 cycles */
    BIT   ,X
    BIT   ,X
    BIT   ,X
    BIT   ,X
    BIT   ,X
    BRN   0
    LDX   ,X                /* load X with value on the PTA port (including BDM_IN) */
        LSLX                    /* shift BDM_IN into C */
        LSLX
    ROL   i                 /* shift C into i (from the bottom) */
    BIT   ,X                /* wait 13 cycles, iterate the loop while waiting (7 cycles), ROL takes 4 cycles */
    DEC   j
    BNE   loop
    LDA   i                 /* load the result into A */
  }
}
/* 1 - 1.444 MHz */
unsigned char bdm_rx11(void) {
  asm {
    LDA   #BDM_DIR1_MASK+RESET_OUT_MASK /* contents of A will be driven to PTA in order to switch the driver off */
    MOV   #8,j              /* store number of iterations into j */
    CLRH                    /* prepare H */
  loop:
    CLRX                    /* clear X to point to PTA, CLRX has been moved to the beginning of the algorithm here to prevent its execution for the last bit */
    STX   ,X                /* drive BDM low */
    BIT   ,X                /* wait 4 cycles */
    BIT   ,X
    STA   ,X                /* switch BDM to high impedance */
    BIT   ,X                /* wait 19 cycles */
    BIT   ,X
    BIT   ,X
    BIT   ,X
    BIT   ,X
    BIT   ,X
    BIT   ,X
    BIT   ,X
    BRN   0
    LDX   ,X                /* load X with value on the PTA port (including BDM_IN) */
        LSLX                    /* shift BDM_IN into C */
        LSLX
    ROL   i                 /* shift C into i (from the bottom) */
    BIT   ,X                /* wait 16 cycles, iterate the loop while waiting (7 cycles), ROL takes 4 cycles */
    BRN   0
    DEC   j
    BNE   loop
    LDA   i                 /* load the result into A */
  }
}
/* 0.818 - 1.181 MHz */
unsigned char bdm_rx12(void) {
  asm {
    LDA   #BDM_DIR1_MASK+RESET_OUT_MASK /* contents of A will be driven to PTA in order to switch the driver off */
    MOV   #8,j              /* store number of iterations into j */
    CLRH                    /* prepare H */
  loop:
    CLRX                    /* clear X to point to PTA, CLRX has been moved to the beginning of the algorithm here to prevent its execution for the last bit */
    STX   ,X                /* drive BDM low */
    BIT   ,X                /* wait 6 cycles */
    BIT   ,X
    BIT   ,X
    STA   ,X                /* switch BDM to high impedance */
    MOV   #2,k              /* wait 23 cycles (= 4 + 2*9 + 1) */
  wait1:
    BIT   ,X
    DEC   k
    BNE   wait1
    NOP
    LDX   ,X                /* load X with value on the PTA port (including BDM_IN) */
        LSLX                    /* shift BDM_IN into C */
        LSLX
    ROL   i                 /* shift C into i (from the bottom) */
    BIT   ,X                /* wait 21 cycles, iterate the loop while waiting (7 cycles), ROL takes 4 cycles */
    BIT   ,X
    BIT   ,X
    BIT   ,X
    BIT   ,X
    DEC   j
    BNE   loop
    LDA   i                 /* load the result into A */
  }
}
/* 0.658 - 0.951 MHz */
unsigned char bdm_rx13(void) {
  asm {
    LDA   #BDM_DIR1_MASK+RESET_OUT_MASK /* contents of A will be driven to PTA in order to switch the driver off */
    MOV   #8,j              /* store number of iterations into j */
    CLRH                    /* prepare H */
  loop:
    CLRX                    /* clear X to point to PTA, CLRX has been moved to the beginning of the algorithm here to prevent its execution for the last bit */
    STX   ,X                /* drive BDM low */
    BIT   ,X                /* wait 8 cycles */
    BIT   ,X
    BIT   ,X
    BIT   ,X
    STA   ,X                /* switch BDM to high impedance */
    MOV   #3,k              /* wait 29 cycles (= 4 + 3*8 + 1) */
  wait1:
    NOP
    DEC   k
    BNE   wait1
    NOP
    LDX   ,X                /* load X with value on the PTA port (including BDM_IN) */
        LSLX                    /* shift BDM_IN into C */
        LSLX
    ROL   i                 /* shift C into i (from the bottom) */
    BIT   ,X                /* wait 27 cycles, iterate the loop while waiting (7 cycles), ROL takes 4 cycles */
    BIT   ,X
    BIT   ,X
    BIT   ,X
    BIT   ,X
    BIT   ,X
    BIT   ,X
    BIT   ,X
    DEC   j
    BNE   loop
    LDA   i                 /* load the result into A */
  }
}
/* 0.529 - 0.764 MHz */
unsigned char bdm_rx14(void) {
  asm {
    LDA   #BDM_DIR1_MASK+RESET_OUT_MASK /* contents of A will be driven to PTA in order to switch the driver off */
    MOV   #8,j              /* store number of iterations into j */
    CLRH                    /* prepare H */
  loop:
    CLRX                    /* clear X to point to PTA, CLRX has been moved to the beginning of the algorithm here to prevent its execution for the last bit */
    STX   ,X                /* drive BDM low */
    BIT   ,X                /* wait 10 cycles */
    BIT   ,X
    BIT   ,X
    BIT   ,X
    BIT   ,X
    STA   ,X                /* switch BDM to high impedance */
    MOV   #4,k              /* wait 37 cycles (= 4 + 4*8 + 1) */
  wait1:
    NOP
    DEC   k
    BNE   wait1
    NOP
    LDX   ,X                /* load X with value on the PTA port (including BDM_IN) */
        LSLX                    /* shift BDM_IN into C */
        LSLX
    ROL   i                 /* shift C into i (from the bottom) */
    MOV   #2,k              /* wait 35 cycles, iterate the loop while waiting (7 cycles), ROL takes 4 cycles */
  wait2:                                    /* 35 = 4 + 2*10 + 4 + 7 */
    BRN   0
    DEC   k
    BNE   wait2
    DEC   j
    BNE   loop
    LDA   i                 /* load the result into A */
  }
}
/* 0.421 - 0.609 MHz */
unsigned char bdm_rx15(void) {
  asm {
    LDA   #BDM_DIR1_MASK+RESET_OUT_MASK /* contents of A will be driven to PTA in order to switch the driver off */
    MOV   #8,j              /* store number of iterations into j */
    CLRH                    /* prepare H */
  loop:
    CLRX                    /* clear X to point to PTA, CLRX has been moved to the beginning of the algorithm here to prevent its execution for the last bit */
    STX   ,X                /* drive BDM low */
    BIT   ,X                /* wait 13 cycles */
    BIT   ,X
    BIT   ,X
    BIT   ,X
    BIT   ,X
    BRN   0
    STA   ,X                /* switch BDM to high impedance */
    MOV   #6,k              /* wait 46 cycles (= 4 + 6*7) */ /* wait 47 cycles (= 4 + 6*7 + 1) */
  wait1:
    DEC   k
    BNE   wait1
/*  NOP  */
    LDX   ,X                /* load X with value on the PTA port (including BDM_IN) */
        LSLX                    /* shift BDM_IN into C */
        LSLX
    ROL   i                 /* shift C into i (from the bottom) */
    MOV   #3,k              /* wait 45 cycles, iterate the loop while waiting (7 cycles), ROL takes 4 cycles */
  wait2:                                    /* 45 = 4 + 3*10 + 4 + 7 */
    BRN   0
    DEC   k
    BNE   wait2
    DEC   j
    BNE   loop
    LDA   i                 /* load the result into A */
  }
}

/* decodes values recorded by RX functions */
/* expects LSB data in X and remaining 7 bytes on stack */
/* it is expected that caller will JUMP into this routine */
void rx_stack_decode(void) {
  asm {
    MOV   #8,i
  decode:
    ROLX                    /* get the interesting bit into C (it is bit 6) */
    ROLX
    RORA                    /* and rotate it into A from the top */
    PULX                    /* get the next value from stack */
    DEC   i
    BNE   decode
    PSHX                    /* that was one pop too many, so push the value back */
  }
}

/* transmit 8 bits of data, MSB first */
/* expects DIR2 active and DIR1 inactive (call bdm_tx_prepare) */
/* target frequency 6.6 - 8.4 MHz (this is BDM frequency (=crystal/2)) */
void bdm_tx1(unsigned char data) {
  asm {
    CLRX                    /* HX points to PTA */
    CLRH
    MOV   #0,BDM_DIR2_PORT    /* start driving the BDM */
    /* bit 7 (MSB) */
    SEC           /* set C */
    CLR   ,X      /* bring BDM low */
    STA   ,X      /* assign value from MSB of A to BDM */
    RORA          /* set MSB of A */
    STA   ,X      /* bring BDM high */
    ROLA          /* advance to next bit */
    ROLA
    /* bit 6 */
    SEC           /* set C */
    CLR   ,X      /* bring BDM low */
    STA   ,X      /* assign value from MSB of A to BDM */
    RORA          /* set MSB of A */
    STA   ,X      /* bring BDM high */
    ROLA          /* advance to next bit */
    ROLA
    /* bit 5 */
    SEC           /* set C */
    CLR   ,X      /* bring BDM low */
    STA   ,X      /* assign value from MSB of A to BDM */
    RORA          /* set MSB of A */
    STA   ,X      /* bring BDM high */
    ROLA          /* advance to next bit */
    ROLA
    /* bit 4 */
    SEC           /* set C */
    CLR   ,X      /* bring BDM low */
    STA   ,X      /* assign value from MSB of A to BDM */
    RORA          /* set MSB of A */
    STA   ,X      /* bring BDM high */
    ROLA          /* advance to next bit */
    ROLA
    /* bit 3 */
    SEC           /* set C */
    CLR   ,X      /* bring BDM low */
    STA   ,X      /* assign value from MSB of A to BDM */
    RORA          /* set MSB of A */
    STA   ,X      /* bring BDM high */
    ROLA          /* advance to next bit */
    ROLA
    /* bit 2 */
    SEC           /* set C */
    CLR   ,X      /* bring BDM low */
    STA   ,X      /* assign value from MSB of A to BDM */
    RORA          /* set MSB of A */
    STA   ,X      /* bring BDM high */
    ROLA          /* advance to next bit */
    ROLA
    /* bit 1 */
    SEC           /* set C */
    CLR   ,X      /* bring BDM low */
    STA   ,X      /* assign value from MSB of A to BDM */
    RORA          /* set MSB of A */
    STA   ,X      /* bring BDM high */
    ROLA          /* advance to next bit */
    ROLA
    /* bit 0 */
    SEC           /* set C */
    CLR   ,X      /* bring BDM low */
    STA   ,X      /* assign value from MSB of A to BDM */
    RORA          /* set MSB of A */
    STA   ,X      /* bring BDM high */
    MOV   #BDM_DIR2_MASK,BDM_DIR2_PORT              /* stop driving the BDM */
  }
  ACKN_CLR;    /* clear ACKN flag */
  /* it takes 8 cycles from end of the last bit till enable of the ACKN capture */
  /* that is short enough for BDM freq of: (32+16)*3/8 = 18 MHz */
  /* 32+16 comes from minimum delay between command and ACKN (32 BDM cycles) and 16 cycles of the ACKN pulse (capturing its rising edge) */
}
/* target frequency 5.5 - 7 MHz (this is BDM frequency (=crystal/2)) */
void bdm_tx2(unsigned char data) {
  asm {
    CLRX                    /* HX points to PTA */
    CLRH
    MOV   #0,BDM_DIR2_PORT    /* start driving the BDM */
    /* bit 7 (MSB) */
    CLR   ,X      /* bring BDM low */
    STA   ,X      /* assign value from MSB of A to BDM */
    ORA   #BDM_OUT_MASK /* set MSB of A */
    STA   ,X      /* bring BDM high */
    LSLA          /* advance to next bit */
    /* bit 6 */
    CLR   ,X      /* bring BDM low */
    STA   ,X      /* assign value from MSB of A to BDM */
    ORA   #BDM_OUT_MASK /* set MSB of A */
    STA   ,X      /* bring BDM high */
    LSLA          /* advance to next bit */
    /* bit 5 */
    CLR   ,X      /* bring BDM low */
    STA   ,X      /* assign value from MSB of A to BDM */
    ORA   #BDM_OUT_MASK /* set MSB of A */
    STA   ,X      /* bring BDM high */
    LSLA          /* advance to next bit */
    /* bit 4 */
    CLR   ,X      /* bring BDM low */
    STA   ,X      /* assign value from MSB of A to BDM */
    ORA   #BDM_OUT_MASK /* set MSB of A */
    STA   ,X      /* bring BDM high */
    LSLA          /* advance to next bit */
    /* bit 3 */
    CLR   ,X      /* bring BDM low */
    STA   ,X      /* assign value from MSB of A to BDM */
    ORA   #BDM_OUT_MASK /* set MSB of A */
    STA   ,X      /* bring BDM high */
    LSLA          /* advance to next bit */
    /* bit 2 */
    CLR   ,X      /* bring BDM low */
    STA   ,X      /* assign value from MSB of A to BDM */
    ORA   #BDM_OUT_MASK /* set MSB of A */
    STA   ,X      /* bring BDM high */
    LSLA          /* advance to next bit */
    /* bit 1 */
    CLR   ,X      /* bring BDM low */
    STA   ,X      /* assign value from MSB of A to BDM */
    ORA   #BDM_OUT_MASK /* set MSB of A */
    STA   ,X      /* bring BDM high */
    LSLA          /* advance to next bit */
    /* bit 0 */
    CLR   ,X      /* bring BDM low */
    STA   ,X      /* assign value from MSB of A to BDM */
    ORA   #BDM_OUT_MASK /* set MSB of A */
    STA   ,X      /* bring BDM high */
    MOV   #BDM_DIR2_MASK,BDM_DIR2_PORT              /* stop driving the BDM */
  }
  ACKN_CLR;    /* clear ACKN flag */
}
/* target frequency 4.714 - 6 MHz (this is BDM frequency (=crystal/2)) */
void bdm_tx3(unsigned char data) {
  asm {
    CLRX                    /* HX points to PTA */
    CLRH
    MOV   #0,BDM_DIR2_PORT    /* start driving the BDM */
    /* bit 7 (MSB) */
    CLR   ,X      /* bring BDM low */
    STA   ,X      /* assign value from MSB of A to BDM */
    ORA   #BDM_OUT_MASK /* set MSB of A */
    NOP           /* wait 1 cycle */
    STA   ,X      /* bring BDM high */
    LSLA          /* advance to next bit */
    NOP           /* wait 1 cycle */
    /* bit 6 */
    CLR   ,X      /* bring BDM low */
    STA   ,X      /* assign value from MSB of A to BDM */
    ORA   #BDM_OUT_MASK /* set MSB of A */
    NOP           /* wait 1 cycle */
    STA   ,X      /* bring BDM high */
    LSLA          /* advance to next bit */
    NOP           /* wait 1 cycle */
    /* bit 5 */
    CLR   ,X      /* bring BDM low */
    STA   ,X      /* assign value from MSB of A to BDM */
    ORA   #BDM_OUT_MASK /* set MSB of A */
    NOP           /* wait 1 cycle */
    STA   ,X      /* bring BDM high */
    LSLA          /* advance to next bit */
    NOP           /* wait 1 cycle */
    /* bit 4 */
    CLR   ,X      /* bring BDM low */
    STA   ,X      /* assign value from MSB of A to BDM */
    ORA   #BDM_OUT_MASK /* set MSB of A */
    NOP           /* wait 1 cycle */
    STA   ,X      /* bring BDM high */
    LSLA          /* advance to next bit */
    NOP           /* wait 1 cycle */
    /* bit 3 */
    CLR   ,X      /* bring BDM low */
    STA   ,X      /* assign value from MSB of A to BDM */
    ORA   #BDM_OUT_MASK /* set MSB of A */
    NOP           /* wait 1 cycle */
    STA   ,X      /* bring BDM high */
    LSLA          /* advance to next bit */
    NOP           /* wait 1 cycle */
    /* bit 2 */
    CLR   ,X      /* bring BDM low */
    STA   ,X      /* assign value from MSB of A to BDM */
    ORA   #BDM_OUT_MASK /* set MSB of A */
    NOP           /* wait 1 cycle */
    STA   ,X      /* bring BDM high */
    LSLA          /* advance to next bit */
    NOP           /* wait 1 cycle */
    /* bit 1 */
    CLR   ,X      /* bring BDM low */
    STA   ,X      /* assign value from MSB of A to BDM */
    ORA   #BDM_OUT_MASK /* set MSB of A */
    NOP           /* wait 1 cycle */
    STA   ,X      /* bring BDM high */
    LSLA          /* advance to next bit */
    NOP           /* wait 1 cycle */
    /* bit 0 */
    CLR   ,X      /* bring BDM low */
    STA   ,X      /* assign value from MSB of A to BDM */
    ORA   #BDM_OUT_MASK /* set MSB of A */
    NOP           /* wait 1 cycle */
    STA   ,X      /* bring BDM high */
    MOV   #BDM_DIR2_MASK,BDM_DIR2_PORT              /* stop driving the BDM */
  }
  ACKN_CLR;    /* clear ACKN flag */
}
/* target frequency 4.125 - 5.25 MHz (this is BDM frequency (=crystal/2)) */
void bdm_tx4(unsigned char data) {
  asm {
    CLRX                    /* HX points to PTA */
    CLRH
    MOV   #0,BDM_DIR2_PORT    /* start driving the BDM */
    /* bit 7 (MSB) */
    CLR   ,X      /* bring BDM low */
    STA   ,X      /* assign value from MSB of A to BDM */
    ORA   #BDM_OUT_MASK /* set MSB of A */
    BIT   ,X      /* wait 2 cycles */
    STA   ,X      /* bring BDM high */
    LSLA          /* advance to next bit */
    NOP           /* wait 1 cycle */
    /* bit 6 */
    CLR   ,X      /* bring BDM low */
    STA   ,X      /* assign value from MSB of A to BDM */
    ORA   #BDM_OUT_MASK /* set MSB of A */
    BIT   ,X      /* wait 2 cycles */
    STA   ,X      /* bring BDM high */
    LSLA          /* advance to next bit */
    NOP           /* wait 1 cycle */
    /* bit 5 */
    CLR   ,X      /* bring BDM low */
    STA   ,X      /* assign value from MSB of A to BDM */
    ORA   #BDM_OUT_MASK /* set MSB of A */
    BIT   ,X      /* wait 2 cycles */
    STA   ,X      /* bring BDM high */
    LSLA          /* advance to next bit */
    NOP           /* wait 1 cycle */
    /* bit 4 */
    CLR   ,X      /* bring BDM low */
    STA   ,X      /* assign value from MSB of A to BDM */
    ORA   #BDM_OUT_MASK /* set MSB of A */
    BIT   ,X      /* wait 2 cycles */
    STA   ,X      /* bring BDM high */
    LSLA          /* advance to next bit */
    NOP           /* wait 1 cycle */
    /* bit 3 */
    CLR   ,X      /* bring BDM low */
    STA   ,X      /* assign value from MSB of A to BDM */
    ORA   #BDM_OUT_MASK /* set MSB of A */
    BIT   ,X      /* wait 2 cycles */
    STA   ,X      /* bring BDM high */
    LSLA          /* advance to next bit */
    NOP           /* wait 1 cycle */
    /* bit 2 */
    CLR   ,X      /* bring BDM low */
    STA   ,X      /* assign value from MSB of A to BDM */
    ORA   #BDM_OUT_MASK /* set MSB of A */
    BIT   ,X      /* wait 2 cycles */
    STA   ,X      /* bring BDM high */
    LSLA          /* advance to next bit */
    NOP           /* wait 1 cycle */
    /* bit 1 */
    CLR   ,X      /* bring BDM low */
    STA   ,X      /* assign value from MSB of A to BDM */
    ORA   #BDM_OUT_MASK /* set MSB of A */
    BIT   ,X      /* wait 2 cycles */
    STA   ,X      /* bring BDM high */
    LSLA          /* advance to next bit */
    NOP           /* wait 1 cycle */
    /* bit 0 */
    CLR   ,X      /* bring BDM low */
    STA   ,X      /* assign value from MSB of A to BDM */
    ORA   #BDM_OUT_MASK /* set MSB of A */
    BIT   ,X      /* wait 2 cycles */
    STA   ,X      /* bring BDM high */
    MOV   #BDM_DIR2_MASK,BDM_DIR2_PORT              /* stop driving the BDM */
  }
  ACKN_CLR;    /* clear ACKN flag */
}
/* target frequency 3.667 - 4.667 MHz (this is BDM frequency (=crystal/2)) */
void bdm_tx5(unsigned char data) {
  asm {
    CLRX                    /* HX points to PTA */
    CLRH
    MOV   #0,BDM_DIR2_PORT    /* start driving the BDM */
    /* bit 7 (MSB) */
    CLR   ,X      /* bring BDM low */
    STA   ,X      /* assign value from MSB of A to BDM */
    ORA   #BDM_OUT_MASK /* set MSB of A */
    BRN   0       /* wait 3 cycles */
    STA   ,X      /* bring BDM high */
    LSLA          /* advance to next bit */
    BIT   ,X      /* wait 2 cycles */
    /* bit 6 */
    CLR   ,X      /* bring BDM low */
    STA   ,X      /* assign value from MSB of A to BDM */
    ORA   #BDM_OUT_MASK /* set MSB of A */
    BRN   0       /* wait 3 cycles */
    STA   ,X      /* bring BDM high */
    LSLA          /* advance to next bit */
    BIT   ,X      /* wait 2 cycles */
    /* bit 5 */
    CLR   ,X      /* bring BDM low */
    STA   ,X      /* assign value from MSB of A to BDM */
    ORA   #BDM_OUT_MASK /* set MSB of A */
    BRN   0       /* wait 3 cycles */
    STA   ,X      /* bring BDM high */
    LSLA          /* advance to next bit */
    BIT   ,X      /* wait 2 cycles */
    /* bit 4 */
    CLR   ,X      /* bring BDM low */
    STA   ,X      /* assign value from MSB of A to BDM */
    ORA   #BDM_OUT_MASK /* set MSB of A */
    BRN   0       /* wait 3 cycles */
    STA   ,X      /* bring BDM high */
    LSLA          /* advance to next bit */
    BIT   ,X      /* wait 2 cycles */
    /* bit 3 */
    CLR   ,X      /* bring BDM low */
    STA   ,X      /* assign value from MSB of A to BDM */
    ORA   #BDM_OUT_MASK /* set MSB of A */
    BRN   0       /* wait 3 cycles */
    STA   ,X      /* bring BDM high */
    LSLA          /* advance to next bit */
    BIT   ,X      /* wait 2 cycles */
    /* bit 2 */
    CLR   ,X      /* bring BDM low */
    STA   ,X      /* assign value from MSB of A to BDM */
    ORA   #BDM_OUT_MASK /* set MSB of A */
    BRN   0       /* wait 3 cycles */
    STA   ,X      /* bring BDM high */
    LSLA          /* advance to next bit */
    BIT   ,X      /* wait 2 cycles */
    /* bit 1 */
    CLR   ,X      /* bring BDM low */
    STA   ,X      /* assign value from MSB of A to BDM */
    ORA   #BDM_OUT_MASK /* set MSB of A */
    BRN   0       /* wait 3 cycles */
    STA   ,X      /* bring BDM high */
    LSLA          /* advance to next bit */
    BIT   ,X      /* wait 2 cycles */
    /* bit 0 */
    CLR   ,X      /* bring BDM low */
    STA   ,X      /* assign value from MSB of A to BDM */
    ORA   #BDM_OUT_MASK /* set MSB of A */
    BRN   0       /* wait 3 cycles */
    STA   ,X      /* bring BDM high */
    MOV   #BDM_DIR2_MASK,BDM_DIR2_PORT              /* stop driving the BDM */
  }
  ACKN_CLR;    /* clear ACKN flag */
}
/* target frequency 3.3 - 4.2 MHz (this is BDM frequency (=crystal/2)) */
void bdm_tx6(unsigned char data) {
  asm {
    CLRX                    /* HX points to PTA */
    CLRH
    MOV   #0,BDM_DIR2_PORT    /* start driving the BDM */
    /* bit 7 (MSB) */
    CLR   ,X      /* bring BDM low */
    STA   ,X      /* assign value from MSB of A to BDM */
    ORA   #BDM_OUT_MASK /* set MSB of A */
    BIT   ,X      /* wait 4 cycles */
    BIT   ,X
    STA   ,X      /* bring BDM high */
    LSLA          /* advance to next bit */
    BIT   ,X      /* wait 2 cycles */
    /* bit 6 */
    CLR   ,X      /* bring BDM low */
    STA   ,X      /* assign value from MSB of A to BDM */
    ORA   #BDM_OUT_MASK /* set MSB of A */
    BIT   ,X      /* wait 4 cycles */
    BIT   ,X
    STA   ,X      /* bring BDM high */
    LSLA          /* advance to next bit */
    BIT   ,X      /* wait 2 cycles */
    /* bit 5 */
    CLR   ,X      /* bring BDM low */
    STA   ,X      /* assign value from MSB of A to BDM */
    ORA   #BDM_OUT_MASK /* set MSB of A */
    BIT   ,X      /* wait 4 cycles */
    BIT   ,X
    STA   ,X      /* bring BDM high */
    LSLA          /* advance to next bit */
    BIT   ,X      /* wait 2 cycles */
    /* bit 4 */
    CLR   ,X      /* bring BDM low */
    STA   ,X      /* assign value from MSB of A to BDM */
    ORA   #BDM_OUT_MASK /* set MSB of A */
    BIT   ,X      /* wait 4 cycles */
    BIT   ,X
    STA   ,X      /* bring BDM high */
    LSLA          /* advance to next bit */
    BIT   ,X      /* wait 2 cycles */
    /* bit 3 */
    CLR   ,X      /* bring BDM low */
    STA   ,X      /* assign value from MSB of A to BDM */
    ORA   #BDM_OUT_MASK /* set MSB of A */
    BIT   ,X      /* wait 4 cycles */
    BIT   ,X
    STA   ,X      /* bring BDM high */
    LSLA          /* advance to next bit */
    BIT   ,X      /* wait 2 cycles */
    /* bit 2 */
    CLR   ,X      /* bring BDM low */
    STA   ,X      /* assign value from MSB of A to BDM */
    ORA   #BDM_OUT_MASK /* set MSB of A */
    BIT   ,X      /* wait 4 cycles */
    BIT   ,X
    STA   ,X      /* bring BDM high */
    LSLA          /* advance to next bit */
    BIT   ,X      /* wait 2 cycles */
    /* bit 1 */
    CLR   ,X      /* bring BDM low */
    STA   ,X      /* assign value from MSB of A to BDM */
    ORA   #BDM_OUT_MASK /* set MSB of A */
    BIT   ,X      /* wait 4 cycles */
    BIT   ,X
    STA   ,X      /* bring BDM high */
    LSLA          /* advance to next bit */
    BIT   ,X      /* wait 2 cycles */
    /* bit 0 */
    CLR   ,X      /* bring BDM low */
    STA   ,X      /* assign value from MSB of A to BDM */
    ORA   #BDM_OUT_MASK /* set MSB of A */
    BIT   ,X      /* wait 4 cycles */
    BIT   ,X
    STA   ,X      /* bring BDM high */
    MOV   #BDM_DIR2_MASK,BDM_DIR2_PORT              /* stop driving the BDM */
  }
  ACKN_CLR;    /* clear ACKN flag */
}
/* target frequency 3 - 3.818 MHz (this is BDM frequency (=crystal/2)) */
void bdm_tx7(unsigned char data) {
  asm {
    CLRX                    /* HX points to PTA */
    CLRH
    MOV   #0,BDM_DIR2_PORT    /* start driving the BDM */
    /* bit 7 (MSB) */
    CLR   ,X      /* bring BDM low */
    STA   ,X      /* assign value from MSB of A to BDM */
    ORA   #BDM_OUT_MASK /* set MSB of A */
    BIT   ,X      /* wait 5 cycles */
    BRN   0
    STA   ,X      /* bring BDM high */
    LSLA          /* advance to next bit */
    BIT   ,X      /* wait 2 cycles */
    /* bit 6 */
    CLR   ,X      /* bring BDM low */
    STA   ,X      /* assign value from MSB of A to BDM */
    ORA   #BDM_OUT_MASK /* set MSB of A */
    BIT   ,X      /* wait 5 cycles */
    BRN   0
    STA   ,X      /* bring BDM high */
    LSLA          /* advance to next bit */
    BIT   ,X      /* wait 2 cycles */
    /* bit 5 */
    CLR   ,X      /* bring BDM low */
    STA   ,X      /* assign value from MSB of A to BDM */
    ORA   #BDM_OUT_MASK /* set MSB of A */
    BIT   ,X      /* wait 5 cycles */
    BRN   0
    STA   ,X      /* bring BDM high */
    LSLA          /* advance to next bit */
    BIT   ,X      /* wait 2 cycles */
    /* bit 4 */
    CLR   ,X      /* bring BDM low */
    STA   ,X      /* assign value from MSB of A to BDM */
    ORA   #BDM_OUT_MASK /* set MSB of A */
    BIT   ,X      /* wait 5 cycles */
    BRN   0
    STA   ,X      /* bring BDM high */
    LSLA          /* advance to next bit */
    BIT   ,X      /* wait 2 cycles */
    /* bit 3 */
    CLR   ,X      /* bring BDM low */
    STA   ,X      /* assign value from MSB of A to BDM */
    ORA   #BDM_OUT_MASK /* set MSB of A */
    BIT   ,X      /* wait 5 cycles */
    BRN   0
    STA   ,X      /* bring BDM high */
    LSLA          /* advance to next bit */
    BIT   ,X      /* wait 2 cycles */
    /* bit 2 */
    CLR   ,X      /* bring BDM low */
    STA   ,X      /* assign value from MSB of A to BDM */
    ORA   #BDM_OUT_MASK /* set MSB of A */
    BIT   ,X      /* wait 5 cycles */
    BRN   0
    STA   ,X      /* bring BDM high */
    LSLA          /* advance to next bit */
    BIT   ,X      /* wait 2 cycles */
    /* bit 1 */
    CLR   ,X      /* bring BDM low */
    STA   ,X      /* assign value from MSB of A to BDM */
    ORA   #BDM_OUT_MASK /* set MSB of A */
    BIT   ,X      /* wait 5 cycles */
    BRN   0
    STA   ,X      /* bring BDM high */
    LSLA          /* advance to next bit */
    BIT   ,X      /* wait 2 cycles */
    /* bit 0 */
    CLR   ,X      /* bring BDM low */
    STA   ,X      /* assign value from MSB of A to BDM */
    ORA   #BDM_OUT_MASK /* set MSB of A */
    BIT   ,X      /* wait 5 cycles */
    BRN   0
    STA   ,X      /* bring BDM high */
    MOV   #BDM_DIR2_MASK,BDM_DIR2_PORT              /* stop driving the BDM */
  }
  ACKN_CLR;    /* clear ACKN flag */
}
/* target frequency 2.538 - 3.231 MHz (this is BDM frequency (=crystal/2)) */
void bdm_tx8(unsigned char data) {
  asm {
    CLRX                    /* HX points to PTA */
    CLRH
    MOV   #0,BDM_DIR2_PORT    /* start driving the BDM */
    /* bit 7 (MSB) */
    CLR   ,X      /* bring BDM low */
    NOP           /* wait 1 cycle */
    STA   ,X      /* assign value from MSB of A to BDM */
    ORA   #BDM_OUT_MASK /* set MSB of A */
    BRN   0       /* wait 6 cycles */
    BRN   0
    STA   ,X      /* bring BDM high */
    LSLA          /* advance to next bit */
    BRN   0       /* wait 3 cycles */
    /* bit 6 */
    CLR   ,X      /* bring BDM low */
    NOP           /* wait 1 cycle */
    STA   ,X      /* assign value from MSB of A to BDM */
    ORA   #BDM_OUT_MASK /* set MSB of A */
    BRN   0       /* wait 6 cycles */
    BRN   0
    STA   ,X      /* bring BDM high */
    LSLA          /* advance to next bit */
    BRN   0       /* wait 3 cycles */
    /* bit 5 */
    CLR   ,X      /* bring BDM low */
    NOP           /* wait 1 cycle */
    STA   ,X      /* assign value from MSB of A to BDM */
    ORA   #BDM_OUT_MASK /* set MSB of A */
    BRN   0       /* wait 6 cycles */
    BRN   0
    STA   ,X      /* bring BDM high */
    LSLA          /* advance to next bit */
    BRN   0       /* wait 3 cycles */
    /* bit 4 */
    CLR   ,X      /* bring BDM low */
    NOP           /* wait 1 cycle */
    STA   ,X      /* assign value from MSB of A to BDM */
    ORA   #BDM_OUT_MASK /* set MSB of A */
    BRN   0       /* wait 6 cycles */
    BRN   0
    STA   ,X      /* bring BDM high */
    LSLA          /* advance to next bit */
    BRN   0       /* wait 3 cycles */
    /* bit 3 */
    CLR   ,X      /* bring BDM low */
    NOP           /* wait 1 cycle */
    STA   ,X      /* assign value from MSB of A to BDM */
    ORA   #BDM_OUT_MASK /* set MSB of A */
    BRN   0       /* wait 6 cycles */
    BRN   0
    STA   ,X      /* bring BDM high */
    LSLA          /* advance to next bit */
    BRN   0       /* wait 3 cycles */
    /* bit 2 */
    CLR   ,X      /* bring BDM low */
    NOP           /* wait 1 cycle */
    STA   ,X      /* assign value from MSB of A to BDM */
    ORA   #BDM_OUT_MASK /* set MSB of A */
    BRN   0       /* wait 6 cycles */
    BRN   0
    STA   ,X      /* bring BDM high */
    LSLA          /* advance to next bit */
    BRN   0       /* wait 3 cycles */
    /* bit 1 */
    CLR   ,X      /* bring BDM low */
    NOP           /* wait 1 cycle */
    STA   ,X      /* assign value from MSB of A to BDM */
    ORA   #BDM_OUT_MASK /* set MSB of A */
    BRN   0       /* wait 6 cycles */
    BRN   0
    STA   ,X      /* bring BDM high */
    LSLA          /* advance to next bit */
    BRN   0       /* wait 3 cycles */
    /* bit 0 */
    CLR   ,X      /* bring BDM low */
    NOP           /* wait 1 cycle */
    STA   ,X      /* assign value from MSB of A to BDM */
    ORA   #BDM_OUT_MASK /* set MSB of A */
    BRN   0       /* wait 6 cycles */
    BRN   0
    STA   ,X      /* bring BDM high */
    MOV   #BDM_DIR2_MASK,BDM_DIR2_PORT              /* stop driving the BDM */
  }
  ACKN_CLR;    /* clear ACKN flag */
}
/* target frequency 2.2 - 2.8 MHz (this is BDM frequency (=crystal/2)) */
void bdm_tx9(unsigned char data) {
  asm {
    CLRX                    /* HX points to PTA */
    CLRH
    MOV   #8,i    /* store number of iterations to go through */
    SEC           /* set C */
    RORA          /* pre-rotate A, A now has '1' in MSB, so the extra assignement in first loop iteration does not matter */
    MOV   #0,BDM_DIR2_PORT /* start driving the BDM */
  loop:
    /* this is the end of the algorithm */
    /* the halves are back to front because there is more time in the middle to iterate the loop */
    STA   ,X      /* bring BDM high */
    ROLA          /* advance to next bit */
    BIT   ,X      /* wait 4 cycles */
    BIT   ,X
    /* this is beginning of the algorithm */
    CLR   ,X      /* bring BDM low */
    NOP           /* wait 1 cycle */
    STA   ,X      /* assign value from MSB of A to BDM */
    ORA   #BDM_OUT_MASK /* set MSB of A */
    NOP           /* wait 8 cycles, iterate the loop while waiting */
        DEC   i
    BNE   loop
    /* finish the last bit */
    STA   ,X      /* bring BDM high */
    MOV   #BDM_DIR2_MASK,BDM_DIR2_PORT /* stop driving the BDM */
  }
  ACKN_CLR;    /* clear ACKN flag */
}
/* target frequency 1.941 - 2.471 MHz (this is BDM frequency (=crystal/2)) */
void bdm_tx10(unsigned char data) {
  asm {
    CLRX                    /* HX points to PTA */
    CLRH
    MOV   #8,i    /* store number of iterations to go through */
    SEC           /* set C */
    RORA          /* pre-rotate A, A now has '1' in MSB, so the extra assignement in first loop iteration does not matter */
    MOV   #0,BDM_DIR2_PORT /* start driving the BDM */
  loop:
    /* this is the end of the algorithm */
    /* the halves are back to front because there is more time in the middle to iterate the loop */
    STA   ,X      /* bring BDM high */
    ROLA          /* advance to next bit */
    BRSET 0,DDRA,0 /* wait 5 cycles */
    /* this is beginning of the algorithm */
    CLR   ,X      /* bring BDM low */
    BIT   ,X      /* wait 2 cycles */
    STA   ,X      /* assign value from MSB of A to BDM */
    ORA   #BDM_OUT_MASK /* set MSB of A */
    BIT   ,X      /* wait 9 cycles, iterate the loop while waiting */
        DEC   i
    BNE   loop
    /* finish the last bit */
    STA   ,X      /* bring BDM high */
    MOV   #BDM_DIR2_MASK,BDM_DIR2_PORT /* stop driving the BDM */
  }
  ACKN_CLR;    /* clear ACKN flag */
}
/* target frequency 1.65 - 2.1 MHz (this is BDM frequency (=crystal/2)) */
void bdm_tx11(unsigned char data) {
  asm {
    CLRX                    /* HX points to PTA */
    CLRH
    MOV   #8,i    /* store number of iterations to go through */
    MOV   #0,BDM_DIR2_PORT /* start driving the BDM */
  loop:
    /* this is beginning of the algorithm */
    CLR   ,X      /* bring BDM low */
    BIT   ,X      /* wait 2 cycles */
    STA   ,X      /* assign value from MSB of A to BDM */
    ORA   #BDM_OUT_MASK /* set MSB of A */
    BIT   ,X       /* wait 12 cycles */
    BIT   ,X
    BIT   ,X
    BIT   ,X
    BIT   ,X
    BIT   ,X
    STA   ,X      /* bring BDM high */
    LSLA          /* advance to next bit */
        DEC   i       /* wait 7 cycles, iterate the loop while waiting */
    BNE   loop
    MOV   #BDM_DIR2_MASK,BDM_DIR2_PORT /* stop driving the BDM */
  }
  ACKN_CLR;    /* clear ACKN flag */
}
/* target frequency 1.435 - 1.826 MHz (this is BDM frequency (=crystal/2)) */
void bdm_tx12(unsigned char data) {
  asm {
    CLRX                    /* HX points to PTA */
    CLRH
    MOV   #8,i    /* store number of iterations to go through */
    MOV   #0,BDM_DIR2_PORT /* start driving the BDM */
  loop:
    /* this is beginning of the algorithm */
    CLR   ,X      /* bring BDM low */
    BRN   0       /* wait 3 cycles */
    STA   ,X      /* assign value from MSB of A to BDM */
    ORA   #BDM_OUT_MASK /* set MSB of A */
    BIT   ,X      /* wait 14 cycles */
    BIT   ,X
    BIT   ,X
    BIT   ,X
    BIT   ,X
    BIT   ,X
    BIT   ,X
    STA   ,X      /* bring BDM high */
    LSLA          /* advance to next bit */
        NOP           /* wait 8 cycles, iterate the loop while waiting */
        DEC   i
    BNE   loop
    MOV   #BDM_DIR2_MASK,BDM_DIR2_PORT /* stop driving the BDM */
  }
  ACKN_CLR;    /* clear ACKN flag */
}
/* target frequency 1.222 - 1.556 MHz (this is BDM frequency (=crystal/2)) */
void bdm_tx13(unsigned char data) {
  asm {
    CLRX                    /* HX points to PTA */
    CLRH
    MOV   #8,i    /* store number of iterations to go through */
    MOV   #0,BDM_DIR2_PORT /* start driving the BDM */
  loop:
    /* this is beginning of the algorithm */
    CLR   ,X      /* bring BDM low */
    BRN   0       /* wait 3 cycles */
    STA   ,X      /* assign value from MSB of A to BDM */
    ORA   #BDM_OUT_MASK /* set MSB of A */
    BIT   ,X      /* wait 18 cycles */
    BIT   ,X
    BIT   ,X
    BIT   ,X
    BIT   ,X
    BIT   ,X
    BIT   ,X
    BIT   ,X
    BIT   ,X
    STA   ,X      /* bring BDM high */
    LSLA          /* advance to next bit */
        BRN   0       /* wait 10 cycles, iterate the loop while waiting */
        DEC   i
    BNE   loop
    MOV   #BDM_DIR2_MASK,BDM_DIR2_PORT /* stop driving the BDM */
  }
  ACKN_CLR;    /* clear ACKN flag */
}
/* target frequency 1.031 - 1.313 MHz (this is BDM frequency (=crystal/2)) */
void bdm_tx14(unsigned char data) {
  asm {
    CLRX                    /* HX points to PTA */
    CLRH
    MOV   #8,i    /* store number of iterations to go through */
    MOV   #0,BDM_DIR2_PORT /* start driving the BDM */
  loop:
    /* this is beginning of the algorithm */
    CLR   ,X      /* bring BDM low */
    BIT   ,X      /* wait 4 cycles */
    BIT   ,X
    STA   ,X      /* assign value from MSB of A to BDM */
    ORA   #BDM_OUT_MASK /* set MSB of A */
    BIT   ,X      /* wait 22 cycles */
    BIT   ,X
    BIT   ,X
    BIT   ,X
    BIT   ,X
    BIT   ,X
    BIT   ,X
    BIT   ,X
    BIT   ,X
    BIT   ,X
    BIT   ,X
    STA   ,X      /* bring BDM high */
    LSLA          /* advance to next bit */
    BRSET 0,DDRA,0 /* wait 12 cycles, iterate the loop while waiting */
        DEC   i
    BNE   loop
    MOV   #BDM_DIR2_MASK,BDM_DIR2_PORT /* stop driving the BDM */
  }
  ACKN_CLR;    /* clear ACKN flag */
}
/* target frequency 0.868 - 1.105 MHz (this is BDM frequency (=crystal/2)) */
void bdm_tx15(unsigned char data) {
  asm {
    CLRX                    /* HX points to PTA */
    CLRH
    MOV   #8,i    /* store number of iterations to go through */
    MOV   #0,BDM_DIR2_PORT /* start driving the BDM */
  loop:
    /* this is beginning of the algorithm */
    CLR   ,X      /* bring BDM low */
    BRSET 0,DDRA,0 /* wait 5 cycles */
    STA   ,X      /* assign value from MSB of A to BDM */
    ORA   #BDM_OUT_MASK /* set MSB of A */
    MOV   #3,j     /* wait 27 cycles = 4 + 3*7 + 2 */
  wait1:
    DEC   j
    BNE   wait1
    BIT   ,X
    STA   ,X      /* bring BDM high */
    LSLA          /* advance to next bit */
    BIT   ,X      /* wait 12 cycles, iterate the loop while waiting */
    BIT   ,X
    BIT   ,X
    BIT   ,X
    DEC   i
    BNE   loop
    MOV   #BDM_DIR2_MASK,BDM_DIR2_PORT /* stop driving the BDM */
  }
  ACKN_CLR;    /* clear ACKN flag */
}
/* target frequency 0.733 - 0.933 MHz (this is BDM frequency (=crystal/2)) */
void bdm_tx16(unsigned char data) {
  asm {
    CLRX                    /* HX points to PTA */
    CLRH
    MOV   #8,i    /* store number of iterations to go through */
    MOV   #0,BDM_DIR2_PORT /* start driving the BDM */
  loop:
    /* this is beginning of the algorithm */
    CLR   ,X      /* bring BDM low */
    BRN   0       /* wait 7 cycles */
    BIT   ,X
    BIT   ,X
    STA   ,X      /* assign value from MSB of A to BDM */
    ORA   #BDM_OUT_MASK /* set MSB of A */
    MOV   #4,j     /* wait 32 cycles = 4 + 4*7 */
  wait1:
    DEC   j
    BNE   wait1
    STA   ,X      /* bring BDM high */
    LSLA          /* advance to next bit */
    BIT   ,X      /* wait 18 cycles, iterate the loop while waiting */
    BIT   ,X
    BIT   ,X
    BIT   ,X
    BRN   0
    DEC   i
    BNE   loop
    MOV   #BDM_DIR2_MASK,BDM_DIR2_PORT /* stop driving the BDM */
  }
  ACKN_CLR;    /* clear ACKN flag */
}
/* target frequency 0.623 - 0.792 MHz (this is BDM frequency (=crystal/2)) */
void bdm_tx17(unsigned char data) {
  asm {
    CLRX                    /* HX points to PTA */
    CLRH
    MOV   #8,i    /* store number of iterations to go through */
    MOV   #0,BDM_DIR2_PORT /* start driving the BDM */
  loop:
    /* this is beginning of the algorithm */
    CLR   ,X      /* bring BDM low */
    BIT   ,X      /* wait 8 cycles */
    BIT   ,X
    BIT   ,X
    BIT   ,X
    STA   ,X      /* assign value from MSB of A to BDM */
    ORA   #BDM_OUT_MASK /* set MSB of A */
    MOV   #5,j     /* wait 39 cycles = 4 + 5*7 */
  wait1:
    DEC   j
    BNE   wait1
    STA   ,X      /* bring BDM high */
    LSLA          /* advance to next bit */
    BIT   ,X      /* wait 22 cycles, iterate the loop while waiting */
    BIT   ,X
    BIT   ,X
    BIT   ,X
    BIT   ,X
    BIT   ,X
    BRN   0
    DEC   i
    BNE   loop
    MOV   #BDM_DIR2_MASK,BDM_DIR2_PORT /* stop driving the BDM */
  }
  ACKN_CLR;    /* clear ACKN flag */
}
/* target frequency 0.532 - 0.677 MHz (this is BDM frequency (=crystal/2)) */
void bdm_tx18(unsigned char data) {
  asm {
    CLRX                    /* HX points to PTA */
    CLRH
    MOV   #8,i    /* store number of iterations to go through */
    MOV   #0,BDM_DIR2_PORT /* start driving the BDM */
  loop:
    /* this is beginning of the algorithm */
    CLR   ,X      /* bring BDM low */
    BIT   ,X      /* wait 10 cycles */
    BIT   ,X
    BIT   ,X
    BIT   ,X
    BIT   ,X
    STA   ,X      /* assign value from MSB of A to BDM */
    ORA   #BDM_OUT_MASK /* set MSB of A */
    MOV   #6,j     /* wait 46 cycles = 4 + 6*7 */
  wait1:
    DEC   j
    BNE   wait1
    STA   ,X      /* bring BDM high */
    LSLA          /* advance to next bit */
    BIT   ,X      /* wait 26 cycles, iterate the loop while waiting */
    BIT   ,X
    BIT   ,X
    BIT   ,X
    BIT   ,X
    BIT   ,X
    BIT   ,X
    BIT   ,X
    BRN   0
    DEC   i
    BNE   loop
    MOV   #BDM_DIR2_MASK,BDM_DIR2_PORT /* stop driving the BDM */
  }
  ACKN_CLR;    /* clear ACKN flag */
}
/* target frequency 0.452 - 0.575 MHz (this is BDM frequency (=crystal/2)) */
void bdm_tx19(unsigned char data) {
  asm {
    CLRX                    /* HX points to PTA */
    CLRH
    MOV   #8,i    /* store number of iterations to go through */
    MOV   #0,BDM_DIR2_PORT /* start driving the BDM */
  loop:
    /* this is beginning of the algorithm */
    CLR   ,X      /* bring BDM low */
    BIT   ,X      /* wait 12 cycles */
    BIT   ,X
    BIT   ,X
    BIT   ,X
    BIT   ,X
    BIT   ,X
    STA   ,X      /* assign value from MSB of A to BDM */
    ORA   #BDM_OUT_MASK /* set MSB of A */
    MOV   #7,j    /* wait 55 cycles = 4 + 7*7 + 2 */
  wait1:
    DEC   j
    BNE   wait1
    BIT   ,X
    STA   ,X      /* bring BDM high */
    LSLA          /* advance to next bit */
    MOV   #2,j    /* wait 31 cycles = 4 + 2*7 + 6 + 7, iterate the loop while waiting */
  wait2:
    DEC   j
    BNE   wait2
    BIT   ,X
    BIT   ,X
    BIT   ,X
    DEC   i
    BNE   loop
    MOV   #BDM_DIR2_MASK,BDM_DIR2_PORT /* stop driving the BDM */
  }
  ACKN_CLR;    /* clear ACKN flag */
}   
