#define BUS_FREQUENCY 6     /* JB16 runs at 6 MHz */
#define ACKN_TIMEOUT  375   /* longest time after which the target should produce ACKN pulse (150 cycles @ 400kHz = 375us) */
#define BDM_SYNC_REQ  350   /* length of the longest possible SYNC REQUEST pulse (128 BDM cycles @ 400kHz = 320us plus some extra time) */
#define SOFT_RESET    1280  /* longest time needed for soft reset of the BDM interface (512 BDM cycles @ 400kHz = 1280us) */
#define RESET_WAIT    300   /* how long to wait for the reset pin to come high in *ms* */
#define RESET_SETTLE  3000  /* time to wait for signals to settle in us, this should certainly be longer than the soft reset time */
#define RESET_LENGTH  100   /* time of RESET assertion in *ms* */

/* please note: pin assignements cannot be changed freely */
/* RX AND TX ROUTINES ARE DEPENDANT ON THIS SPECIFIC ASSIGNEMENT */
#define BDM_IN        PTA_PTA6
#define BDM_IN_MASK   PTA_PTA6_MASK
#define BDM_IN_PORT   PTA
#define BDM_IN_DDR    DDRA

#define BDM_OUT       PTA_PTA7
#define BDM_OUT_MASK  PTA_PTA7_MASK
#define BDM_OUT_PORT  PTA
#define BDM_OUT_DDR   DDRA

#define BDM_DIR1      PTA_PTA4
#define BDM_DIR1_MASK PTA_PTA4_MASK
#define BDM_DIR1_PORT PTA
#define BDM_DIR1_DDR  DDRA

#define BDM_DIR2      PTC_PTC0
#define BDM_DIR2_MASK PTC_PTC0_MASK
#define BDM_DIR2_PORT PTC
#define BDM_DIR2_DDR  DDRC

#define RESET_OUT      PTA_PTA1
#define RESET_OUT_MASK PTA_PTA1_MASK
#define RESET_OUT_PORT PTA
#define RESET_OUT_DDR  DDRA

#define RESET_IN      PTA_PTA5
#define RESET_IN_MASK PTA_PTA5_MASK
#define RESET_IN_PORT PTA
#define RESET_IN_DDR  DDRA

void bdm_init(void);
unsigned char bdm_rx1(void);
unsigned char bdm_rx2(void);
unsigned char bdm_rx3(void);
unsigned char bdm_rx4(void);
unsigned char bdm_rx5(void);
unsigned char bdm_rx6(void);
unsigned char bdm_rx7(void);
unsigned char bdm_rx8(void);
unsigned char bdm_rx9(void);
unsigned char bdm_rx10(void);
unsigned char bdm_rx11(void);
unsigned char bdm_rx12(void);
unsigned char bdm_rx13(void);
unsigned char bdm_rx14(void);
unsigned char bdm_rx15(void);
void bdm_tx1(unsigned char data);
void bdm_tx2(unsigned char data);
void bdm_tx3(unsigned char data);
void bdm_tx4(unsigned char data);
void bdm_tx5(unsigned char data);
void bdm_tx6(unsigned char data);
void bdm_tx7(unsigned char data);
void bdm_tx8(unsigned char data);
void bdm_tx9(unsigned char data);
void bdm_tx10(unsigned char data);
void bdm_tx11(unsigned char data);
void bdm_tx12(unsigned char data);
void bdm_tx13(unsigned char data);
void bdm_tx14(unsigned char data);
void bdm_tx15(unsigned char data);
void bdm_tx16(unsigned char data);
void bdm_tx17(unsigned char data);
void bdm_tx18(unsigned char data);
void bdm_tx19(unsigned char data);
unsigned char bdm_empty_rx_tx(void);
unsigned char bdm_rx_tx_select(void);
void bdm_tx_prepare(void);
void rx_stack_decode(void);
void bdm_tx_finish(void);
void bdm_ackn(void);
void bdm_wait64(void);
void bdm_wait150(void);
void bdm_ackn_init(void);
unsigned char bdm_sync_meas(void);
unsigned char bdm_reset(unsigned char mode);
unsigned char bdm12_connect(void);

typedef enum {
  HC12=0,
  HCS08=1
} target_type_e;

typedef enum {
  WAIT=0,                 /* use WAIT instead */
  ACKN=1									/* ACKN feature available and enabled */
} ackn_e;

typedef enum {
  NO_RESET_ACTIVITY=0,
  RESET_DETECTED=1
} reset_e;

typedef enum {
  NO_INFO=0,
  SYNC_SUPPORTED=1,
  SPEED_GUESSED=2,
  SPEED_USER_SUPPLIED=3
} speed_e;

typedef struct {
  unsigned char target_type:2;  /*  target_type_e */
  unsigned char ackn:1;         /*  ackn_e */
  unsigned char reset:1;        /*  reset_e */
  unsigned char speed:2;			  /*  speed_e */
  unsigned char wait150_cnt;    /* time of 150 BDM cycles in bus cycles of the MCU divided by 3 */
  unsigned char wait64_cnt;
  unsigned int sync_length;     /* length of the target SYNC pulse in 60MHz ticks */
} bdm_status_t;

extern bdm_status_t bdm_status;
extern unsigned char (*bdm_rx_ptr)(void);
extern void (*bdm_tx_ptr)(unsigned char);

#define bdm_rx()      (*bdm_rx_ptr)()
#define bdm_tx(data)  (*bdm_tx_ptr)(data)

/* hardware commands */
#define _BDM_BACKGROUND		    0x90
#define _BDM_ACK_ENABLE		    0xD5
#define _BDM_ACK_DISABLE	    0xD6
#define _BDM_READ_BYTE		    0xE0
#define _BDM_WRITE_BYTE		    0xC0
/* HC/S12(x) hardware commands */
#define _BDM12_READ_BD_BYTE	  0xE4
#define _BDM12_READ_BD_WORD	  0xEC
#define _BDM12_READ_WORD	    0xE8
#define _BDM12_WRITE_BD_BYTE  0xC4
#define _BDM12_WRITE_BD_WORD  0xCC
#define _BDM12_WRITE_WORD	    0xC8
/* HCS08 'hardware'(non-intrusive) commands */
#define _BDM08_READ_STATUS	  0xE4
#define _BDM08_WRITE_CONTROL  0xC4
#define _BDM08_READ_BYTE_WS	  0xE1
#define _BDM08_READ_LAST	    0xE8
#define _BDM08_WRITE_BYTE_WS  0xC1
#define _BDM08_READ_BKPT	    0xE2
#define _BDM08_WRITE_BKPT     0xC2

/* firmware commands */
#define _BDM_GO				        0x08
#define _BDM_TRACE1           0x10
#define _BDM_TAGGO			      0x18

/* HCS08 'firmware' (active background mode) commands */
#define _BDM08_READ_A		      0x68
#define _BDM08_READ_CCR		    0x69
#define _BDM08_READ_PC		    0x6B
#define _BDM08_READ_HX		    0x6C
#define _BDM08_READ_SP		    0x6F
#define _BDM08_READ_NEXT	    0x70
#define _BDM08_READ_NEXT_WS	  0x71
#define _BDM08_WRITE_A		    0x48
#define _BDM08_WRITE_CCR	    0x49
#define _BDM08_WRITE_PC		    0x4B
#define _BDM08_WRITE_HX		    0x4C
#define _BDM08_WRITE_SP	    	0x4F
#define _BDM08_NEXT			      0x50
#define _BDM08_NEXT_WS		    0x51

/* HC/S12(x) firmware commands */
#define _BDM12_READ_NEXT	    0x62
#define _BDM12_READ_PC		    0x63
#define _BDM12_READ_D		      0x64
#define _BDM12_READ_X		      0x65
#define _BDM12_READ_Y		      0x66
#define _BDM12_READ_SP		    0x67
#define _BDM12_WRITE_NEXT	    0x42
#define _BDM12_WRITE_PC		    0x43
#define _BDM12_WRITE_D		    0x44
#define _BDM12_WRITE_X		    0x45
#define _BDM12_WRITE_Y		    0x46
#define _BDM12_WRITE_SP		    0x47
#define _BDM12_GO_UNTIL		    0x0C

#define BDM12_CCR_ADDR		    0xFF06	/* address of CCR register in BDM memory space */
#define BDM12_STS_ADDR	      0xFF01	/* location of BDM Status register */
#define BDM12_GUESS_ADDR      0xFF00  /* location of register to be read when trying to guess speed of the target */

/* following macros perform individual BDM command */
/* naming convention: bdm_cmd_x_y, where x in number of input parameters, y is number of output paramreters */
/* suffxes indicate Word or Byte width */
/* HC/S12(X) wait times (in case ACKN is not used) are longer than those required for HCS08, so HCS08 can use the same macros */
/* HCS08 should support ACKN anyway */

/* no parameter */
#define BDM_CMD_0_0(cmd) 			                  bdm_tx_prepare(),\
                                                bdm_tx(cmd),\
                                                bdm_tx_finish(),\
									                              (bdm_status.ackn==ACKN)?bdm_ackn():bdm_wait64()

/* write word */
#define BDM_CMD_1W_0(cmd,parameter)	            bdm_tx_prepare(),\
                                                bdm_tx(cmd),\
									                              bdm_tx(((parameter)>>8)&0x00ff),\
									                              bdm_tx(((parameter)&0x00ff)),\
									                              bdm_tx_finish(),\
									                              (bdm_status.ackn==ACKN)?bdm_ackn():bdm_wait64()

/* read word */
#define BDM_CMD_0_1W(cmd,parameter)	            bdm_tx_prepare(),\
                                                bdm_tx(cmd),\
                                                bdm_tx_finish(),\
									                              (bdm_status.ackn==ACKN)?bdm_ackn():bdm_wait64(),\
                                                *((unsigned char * far)((void * far)(parameter)))=bdm_rx(),\
									                              *((unsigned char * far)((void * far)(parameter))+1)=bdm_rx()

/* write word & read byte (read word but return byte - HC/S12(x)) */
#define BDM_CMD_1W_1WB(cmd, parameter, result)  bdm_tx_prepare(),\
                                                bdm_tx(cmd),\
									                              bdm_tx(((parameter)>>8)&0x00ff),\
									                              bdm_tx(((parameter)&0x00ff)),\
									                              bdm_tx_finish(),\
  																							(bdm_status.ackn==ACKN)?bdm_ackn():bdm_wait150();\
                              									if ((parameter)&0x0001) {\
                              									  bdm_rx();\
                              									  *((unsigned char * far)((void * far)(result)))=bdm_rx();\
                              									} else {\
                              									  *((unsigned char * far)((void * far)(result)))=bdm_rx();\
                              									  bdm_rx();\
                              									}

/* write 2 words */
#define BDM_CMD_2W_0(cmd, parameter1, parameter2)   bdm_tx_prepare(),\
                                                    bdm_tx(cmd),\
									                                  bdm_tx(((parameter1)>>8)&0x00ff),\
									                                  bdm_tx(((parameter1)&0x00ff)),\
									                                  bdm_tx(((parameter2)>>8)&0x00ff),\
									                                  bdm_tx(((parameter2)&0x00ff)),\
									                                  bdm_tx_finish(),\
									                                  (bdm_status.ackn==ACKN)?bdm_ackn():bdm_wait150()

/* write word and a byte (sends 2 words, the byte in both high and low byte of the 16-bit value) */
#define BDM_CMD_2WB_0(cmd, parameter1, parameter2)  bdm_tx_prepare(),\
                                                    bdm_tx(cmd),\
									                                  bdm_tx(((parameter1)>>8)&0x00ff),\
									                                  bdm_tx(((parameter1)&0x00ff)),\
									                                  bdm_tx(((parameter2)&0x00ff)),\
									                                  bdm_tx(((parameter2)&0x00ff)),\
									                                  bdm_tx_finish(),\
									                                  (bdm_status.ackn==ACKN)?bdm_ackn():bdm_wait150()

/* write word & read word */
#define BDM_CMD_1W_1W(cmd, parameter, result)   bdm_tx_prepare(),\
                                                bdm_tx(cmd),\
									                              bdm_tx(((parameter)>>8)&0x00ff),\
									                              bdm_tx(((parameter)&0x00ff)),\
									                              bdm_tx_finish(),\
  																							(bdm_status.ackn==ACKN)?bdm_ackn():bdm_wait150(),\
                             									  *((unsigned char * far) ((void * far)(result)))=bdm_rx(),\
                             									  *((unsigned char * far) ((void * far)(result))+1)=bdm_rx()

/* definitions of the actual commands - these should be used in the C-code */
/* all BDM commands need to be called with interrupts disabled (ither by hand or for example from within an ISR) */
/* value is 8 or 16 bit value, addr is 16 bit address, value_p is pointer to 8 or 16 bit variable */
/* FIRMWARE COMMANDS */
#define BDM_CMD_GO()					        BDM_CMD_0_0(_BDM_GO)
#define BDM12_CMD_GO_UNTIL()			    BDM_CMD_0_0(_BDM12_GO_UNTIL)
#define BDM_CMD_TRACE1()				      BDM_CMD_0_0(_BDM_TRACE1)
#define BDM_CMD_TAGGO()					      BDM_CMD_0_0(_BDM_TAGGO)
/* write memory using X as a pointer with automatic pre-increment */
#define BDM12_CMD_WRITE_NEXT(value)		BDM_CMD_1W_0(_BDM12_WRITE_NEXT,value)
/* write register commands */
#define BDM12_CMD_WRITE_PC(value)		  BDM_CMD_1W_0(_BDM12_WRITE_PC,value)
#define BDM12_CMD_WRITE_D(value)		  BDM_CMD_1W_0(_BDM12_WRITE_D,value)
#define BDM12_CMD_WRITE_X(value)		  BDM_CMD_1W_0(_BDM12_WRITE_X,value)
#define BDM12_CMD_WRITE_Y(value)		  BDM_CMD_1W_0(_BDM12_WRITE_Y,value)
#define BDM12_CMD_WRITE_SP(value)		  BDM_CMD_1W_0(_BDM12_WRITE_SP,value)
/* read memory using X as a pointer with automatic pre-increment */
#define BDM12_CMD_READ_NEXT(value_p)	BDM_CMD_0_1W(_BDM12_READ_NEXT,value_p)
/* read register commands */
#define BDM12_CMD_READ_PC(value_p)		BDM_CMD_0_1W(_BDM12_READ_PC,value_p)
#define BDM12_CMD_READ_D(value_p)		  BDM_CMD_0_1W(_BDM12_READ_D,value_p)
#define BDM12_CMD_READ_X(value_p)		  BDM_CMD_0_1W(_BDM12_READ_X,value_p)
#define BDM12_CMD_READ_Y(value_p)		  BDM_CMD_0_1W(_BDM12_READ_Y,value_p)
#define BDM12_CMD_READ_SP(value_p)		BDM_CMD_0_1W(_BDM12_READ_SP,value_p)
/* HARDWARE COMMANDS */
#define BDM_CMD_ACK_ENABLE()			    BDM_CMD_0_0(_BDM_ACK_ENABLE)
#define BDM_CMD_ACK_DISABLE() 			  BDM_CMD_0_0(_BDM_ACK_DISABLE)
#define BDM_CMD_BACKGROUND()			    BDM_CMD_0_0(_BDM_BACKGROUND)
/* read and write commands */
#define BDM12_CMD_WRITEW(addr,value)	BDM_CMD_2W_0(_BDM12_WRITE_WORD,addr,value)
#define BDM12_CMD_WRITEB(addr,value)	BDM_CMD_2WB_0(_BDM_WRITE_BYTE,addr,value)
#define BDM12_CMD_READW(addr,value_p)	BDM_CMD_1W_1W(_BDM12_READ_WORD,addr,value_p)
#define BDM12_CMD_READB(addr,value_p)	BDM_CMD_1W_1WB(_BDM_READ_BYTE,addr,value_p)
/* read and writes from/to the BDM memory space */
#define BDM12_CMD_BDWRITEW(addr,value)	BDM_CMD_2W_0(_BDM12_WRITE_BD_WORD,addr,value)
#define BDM12_CMD_BDWRITEB(addr,value)  BDM_CMD_2WB_0(_BDM12_WRITE_BD_BYTE,addr,value)
#define BDM12_CMD_BDREADW(addr,value_p)	BDM_CMD_1W_1W(_BDM12_READ_BD_WORD,addr,value_p)
#define BDM12_CMD_BDREADB(addr,value_p) BDM_CMD_1W_1WB(_BDM12_READ_BD_BYTE,addr,value_p) 
