/* command format:

1   byte : command number (see below)
n   bytes: command parameters (data)

changes from original TBDM:
  speed has higher resolution
  go and step commands cannot have optional parameters because of different command format
  block commands have smaller size limit and trnsfer data as bytes rather than words for the HC12 platform
  write block commands now have data count parameter in addition to address
  version command now returns differently formated number
  string descriptor is now returned by USB directly and the command does not exist anymore
  echo commands have been removed, use GET_VER instead
  get status has slightly extended format

Commands not implemented compared to original TBDM:

CMD_WRITE_REGS        - debuggers typically need write access to individual registers and thus having an agregate command is not of a substantial benefit (how often are the registers written anyway...)
CMD_WRITE_BLOCK_FAST  - fast block operations are only available in active background mode. Their use for debuggers is therefore limited as the debugger needs to have access to memory independant of the target state
CMD_READ_BLOCK_FAST     moreover the BDM transfer time is comparable to low speed USB on faster targets and the faster BDM operation does not neccessaily bring a huge advantage here
												at 2MHz BDM frequency the saving for 128 byte block would be slightly less than 64 transfers * 16 bit of address * bit time = 64 * 16 * 16/2 us = 8192us
												while the overall transfer length with HW word writes is at least 64 tfrs * 40 bits * bit time + USB transfer time = 64 * 40 * 16/2 us + 128/8 * 1000us = 36480us
												i.e. the saving would be less than 22% and smaller with crystals > 4MHz

   data format:

all 16-bit word data is transferred in intel (little) endian, i.e. LSB on lower and MSB on higher address

*/

/* if command fails, the device responds with command code CMD_FAILED */
/* if command succeeds, the device responds with the same command number followed by any results as appropriate */

#define MAX_DATA_SIZE        129 /* this is to make sure that response of READ_BLOCK plus the command status fit into 16 frames exactly */

/* System related commands */
#define CMD_OK                0  /* deprecated */
#undef  CMD_ERR_SUM              /* deprecated, USB has CRC16 in place */
#define CMD_FAILED            2  /* command execution failed (incorrect parameters, target not responding, etc.) */
#define CMD_UNKNOWN           3  /* unknown command */

/* TurboBdmLight related commands */
#undef  CMD_GET_DESC		         /* deprecated */
#undef  CMD_GET_SWSER		  	     /* deprecated */
#define CMD_GET_VER           12 /* returns 16 bit HW/SW version number, (major & minor revision in BCD in each byte - HW in MSB, SW in LSB; intel endianism) */
#define CMD_GET_LAST_STATUS   13 /* returns status of the previous command */

/* BDM related commands */
#define CMD_SET_TARGET        30 /* set target, 8bit parameter: 00=HC12/HCS12(default), 01=HCS08 */
#define CMD_CONNECT           31 /* try to connect to the target */
#undef  CMD_READ_SPEED           /* deprecated, speed has higher resolution now */
#define CMD_RESET             33 /* 8bit parameter: 0=reset to Special Mode, 1=reset to Normal mode */
#define CMD_GET_STATUS        34 /* returns 16bit status word: bit0 - ACKN, bit1 - target was reset (this bit is cleared after reading), bit2 - always 0, bit4-3 - comm status: 00=NOT CONNECTED, 01=SYNC, 10=GUESS, 11=USER SUPPLIED */
#define CMD_READ_BD           35 /* parameter: 16-bit address, returns 8-bit value read from address; on HCS08 it returns the 8-bit BDCSCR register, parameters are none */
#define CMD_WRITE_BD          36 /* parameter: 16-bit address, 8-bit value to write; on HCS08 it writes the BDCSCR register, parameter is only the 8-bit value to write */
#undef  CMD_GO                   /* deprecated */
#undef  CMD_STEP                 /* deprecated */
#define CMD_HALT              39 /* stop the CPU and bring it into background mode */
#define CMD_SET_SPEED         40 /* deprecated */
#define CMD_READ_SPEED1       41 /* read speed of the target: returns 16-bit tick count */
#define CMD_GO1               42 /* start code execution */
#define CMD_STEP1             44 /* perform single step */
#define CMD_SET_SPEED1        46 /* sets-up the BDM interface for a new bit rate & tries to enable ackn feature, parameter: 16-bit word = number of 16.6ns ticks per SYNC */

/* CPU related commands */
#define CMD_READ_8            50 /* parameter 16bit address, returns 8bit value read from address */
#define CMD_READ_16           51 /* parameter 16bit address, returns 16bit value read from address */
#undef  CMD_READ_BLOCK		       /* deprecated */
#define CMD_READ_REGS         53 /* reads registers, returns 16bit values: HC/S12(X): PC, SP, IX, IY, D(B:A), CCR; HCS08: PC, SP, H:X, A, CCR; upper bytes are 0 when not used */
#undef  CMD_READ_BLOCK_FAST	  	 /* deprecated */
#define CMD_READ_BLOCK1       55 /* parameter 16bit address, 8bit count of bytes to read, returns block of bytes from given address, count MUST be <=MAX_DATA_SIZE */

#define CMD_WRITE_8           60 /* parameter 16bit address, 8bit value to write */
#define CMD_WRITE_16          61 /* parameter 16bit address, 16bit value to write */
#undef  CMD_WRITE_BLOCK          /* deprecated */
#undef  CMD_WRITE_REGS           /* deprecated */
#undef  CMD_WRITE_BLOCK_FAST     /* deprecated */
#define CMD_WRITE_BLOCK1      65 /* parameters: 16bit address, 8bit count of bytes, block of bytes to write, size of data block can be up to MAX_DATA_SIZE */

/* HC12 CPU individual register write commands (added for debugger interfaces which need access to individial registers rather than whole block) */
#define CMD_WRITE_REG_PC      80 /* parameter: 16-bit PC value */
#define CMD_WRITE_REG_SP      81 /* parameter: 16-bit SP value */
#define CMD_WRITE_REG_X       82 /* parameter: 16-bit IX (H:X) value */
#define CMD_WRITE_REG_Y       83 /* parameter: 16-bit IY value */
#define CMD_WRITE_REG_D       84 /* parameter: 16-bit B:A (x:A) value */
#define CMD_WRITE_REG_CCR     85 /* parameter: 16-bit CCR (x:CCR) value */

/* remarks:

1. WRITE_16 and WRITE_16 work on HC/S12(X) only

 TBD: special commands specific to HCS08 (breakpoints)
        may need separate commands for writing X and H on S08

   block commands may need to be changed to include a checksum to protect the data in case they prove to be unreliable

   then an additional improvement would be to only clear a command exec faulure after a special command (CMD_CLEAR_ERROR).
   This would enable the user to execute several commands and only then check whether any of them has failed.
   It does not make sense at this moment as the commands do not check their parameters very much...

*/ 
