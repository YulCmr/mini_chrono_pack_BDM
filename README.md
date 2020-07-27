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
