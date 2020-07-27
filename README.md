# Mini R52/53 Chrono Pack reverse engineering, Mileage & VIN Coding

### Current status :

1. IC identification : **OK**
2. Tracing : **OK**
3. CPU BDM learning : **Currently working on it**
4. BDM code writing : TODO
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
- Black 26
- Red 1
- Yellow 7
- Purple 19
- XCLKS 5

Coaxial
- White 8
- Black 24

### Nav & Chronopack pinout
**NAV**
![nav](/pictures/nav_pinout.png)

**CHRONO**
![chrono](/pictures/chrono_pinout.png)

### Tutorial + infos

Thx to theses guys : https://www.northamericanmotoring.com/forums/r50-r53-hatch-talk-2002-2006/344533-chrono-pack-retrofit-all-the-info-you-need-hopefully.html

HW for chrono gauges is 30, for nav its 08. Currently don't know what to do of this info ¯\_(ツ)_/¯

KM & VIN are stored in the 2Kb Eeprom (not the 128Kb one)

According to "AutoMutt" on namotoring.com :

VIN is stored from 0x600 to 0x670, and Mileage is stored from 0x400 to 0x470

Best practice seems to be : copy theses blocks from original cluster, then paste in new cluster binary.

![VIN](/pictures/VIN_add.jpg)


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
