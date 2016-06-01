Grove
=====

Mailing list: grove-electronics@googlegroups.com

# Boards

1) Main board - Teensy shield
    - 4 x 3 pins for 144/m 5m strips on trunk
    - 4 x 5 pins (power + rgb) for dispatcher board
    - 3 pins (power + data) for breathing sensor
    - Power switch
    - PTC fuse + zener diode
    - 4 pins for buck converter, 12V -> 5V
    
2) Sensor board
    - 3 pins from mainboard (power, data)
    - Proximity sensor (SI1143x)
    - IR motion detector
    - "breathing" sensor
    - active LEDs

3) Dispatcher board
    - 4 x 5 pins input (power + rgb)
    - 4 PicoBucks (six pins in, six pins out)
    - 16 x 6 pin RGB LEDs (r±, g±, b±)

# Concerns

1) Power
    - 12V deep cycle marine batteries
    - Charging circuit
    - Solar array
    - Delivering 12V to each tree (voltage drop)
2) Firmware
    - Performance over time
    - Running 4x trunks vs 1 trunk 4 times
3) Lighting
    - Soldering high current LEDs
    - 144/m 5m vs splicing power

# Timeline

- May 31: Initial meeting
- June 8th: Dispatcher board rev #1 sent to printer
- June 9th: Main board rev #1 sent to printer
- June 10th: Sensor board rev #1 sent to printer
- July 1st: Boards return from printer, soldering begins
- July 6th: Final board revisions sent to printer
- July 27th: Final boards return from printer, soldering begins
- August 20th: Installation begins on playa
- August 28th: Burning Man begins

# Useful links:

Grove proposal: https://docs.google.com/presentation/d/1Oe51Et6uq_moFoTw9EoB07Vlh8WwSTc89PW6PQpZ1io/edit#slide=id.g12e5fa7d1f_0_1
Grove budget: https://docs.google.com/spreadsheets/d/1ltcCgIae5ANJpvHL2A5ckWDxvADcpXeD_jZIF5hPCvg/edit#gid=0
Prototypes and earlier installation: https://cambridge.nuvustudio.com/studios/grove#tab-proposal-url
