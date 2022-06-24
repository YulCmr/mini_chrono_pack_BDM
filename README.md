# Mini R52/53 Chrono Pack reverse engineering, Mileage & VIN Coding

### Current status :

1. IC identification : **OK**
2. Tracing : **OK**
3. CPU BDM learning : **OK**
4. BDM code writing : **Aborted, needs new dev board which costs at least the price of a R270 programmer in china.... Not worth the effort**
5. Testing : TODO
6. Binary modification (need to find where info are and how to encode/decode them) : TODO
7. Step by Step guide writing : TODO

### PCB chip identification

![pcb](/pictures/reverse_pcb.jpeg)

[CPU datasheet](/pictures/cpu_datasheet.pdf)

[Flash programming](/pictures/flash_programming.pdf)

[BDM app note](/pictures/BDM_for_M68HC12.pdf)

### CPU Pinout

![pinout](/pictures/pinout.JPG)

### 10 pins program header

![10_pin](/pictures/10_pin_header.jpg)

### USBDM 6 pins BDM Pinout

![6_pin](/pictures/bdm_pinout.jpg)

### How to go in BDM Mode ...

Not worth the effort, R270 programmers are like 20 bucks on aliexpress, the price of a dev board ....

http://blog.obdii365.com/2017/08/17/read-write-bmw-cas-9s12-mercedes-ezs-9s12/

Wire color :

- YELLOW = RESET
- PURPLE = BKGD/BDM
- BLACK = VSS/-
- RED = VDD/+
- GREEN = PE7 / xclks

R260 - R270
- Black / GND / 26
- Red / 5v / 1
- Yellow / RST / 7
- Purple / BKGD /19
- Green / XCLKS / 5

Coaxial
- White / ECLK / 8
- Black / ECLK_REF / 24

### Nav & Chronopack pinout
**NAV**
![nav](/pictures/nav_pinout.png)

**CHRONO**
![chrono](/pictures/chrono_pinout.png)

### Tutorial + infos

Thx to theses guys : https://www.northamericanmotoring.com/forums/r50-r53-hatch-talk-2002-2006/344533-chrono-pack-retrofit-all-the-info-you-need-hopefully.html

HW for chrono gauges is 30, for nav its 08. Flash and EEPROM files are sensitive to HW version so be careful which one you flash.

KM & VIN are stored in the 2Kb Eeprom (not the 128Kb one)

According to "AutoMutt" on namotoring.com :

VIN is stored from 0x600 to 0x670, and Mileage is stored from 0x400 to 0x470

Best practice seems to be : copy theses blocks from original cluster, then paste in new cluster binary.

![VIN](/pictures/VIN_add.jpg)

If you use a R270, you most likely corrupted your flash while trying to read it (seems very common and occured to me aswell... Needles were completly off as soon as I plugged Kombi in car...)
To solve this, reflash theses files :

HW30:
- [Flash](HW30_SW23_R270_FLASH.bin)
- [Eeprom with 0km on odometer](HW30_SW23_R270_EEPROM_0km.bin) <-- You still need to write you VIN in this file (or see TL;DR at the bottom of the page)

HW08:
- [Flash](HW08_SW23_R270_FLASH.bin)
- [Eeprom with 0km on odometer](HW08_SW23_R270_EEPROM_0km.bin) <-- You still need to write you VIN in this file (or see TL;DR at the bottom of the page)

**Update ZCS :**
- Plug cable into OBD2 port and connect
- Open NCS Expert
- Load Revtor profile
- Press F1 (VIN/ZCS/FA)
- Press F3 (VIN/FA f. ECU)
- Choose: Chasis R50 (for R50/R52/R53)
- Choose: EWS (To get current data from your auto)
- Now you will see the GM/SA/VN. Take a pic or write this down.
- Press F3 (VIN/FA f. ECU)
- Choose: Chasis R50 (for R50/R52/R53)
- Choose: KMB (To get data from replacement kombi, You will notice the GM/SA/VN is different)
- Press F1 (Enter ZCS)
- Enter the data you took a pic of or wrote down from earlier step
- Press F6 (Back)
- Press F4 (Process ECU ***do NOT press Process Car***)
- Choose: KMB
- Press F2 (Change Job)
- Choose: ZCS-SCHREIBEN (This means ZCS_WRITE)
- Press F3 (Execute Job)
- Close NCS Expert

TL;DR
Using R270, upload flash, then eeprom to the cluster.
Connect the cluster to the car (if the odometer reads something like 250km, your flashing was successful).
Using NCSExpert read vehicle data from EWS (VIN/FA f. ECU).
Then do a ZCS_LESEN on EWS and a ZCS_SCHREIBEN on KMB.
Then do a FGNR_LESEN on EWS and a FGNR_SCHREIBEN on KMB.
Now turn the ignition off an on and you should see your mileage appear in the instrument cluster.
Optional step is to code the KBM module using NCSExpert/NCSDummy.
