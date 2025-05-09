Infinite Keyboard is a portable musical scratchpad. It continously captures of every note played on the integrated keyboard as MIDI data, saved directly to an SD card. This allows you to freely play ideas as they occur, knowing they can always revisit the performance later. The system uses an ESP32-S3 microcontroller, with aPCM5102 DAC and TPA6130A2 headphone amplifier, and supports USB connectivity for potential MIDI output or other functions.

1. ESP32-S3 Module (ESP32-S3-WROOM-1-N8)
    [ ] Power (3.3V):
        [X] Connect module Pin 2 (3V3) to the main 3.3V power rail.
        [X] Connect module Pin 1 and Pin 40 (GND) and EPAD (Pin 41) to the main ground plane.
        [ ] Place bulk decoupling capacitor (10µF - 22µF) near the module between 3.3V and GND.
        [ ] Place local decoupling capacitor (0.1µF - 1µF) very close to Pin 2 (3V3) and GND.
    [X] Enable/Reset Circuit:
        [X] Connect an RC delay circuit to the EN pin (Pin 3): 10kΩ resistor to 3.3V, 1µF capacitor to GND.
        [X] Connect a momentary push button (Reset Switch) between EN (Pin 3) and GND.
    [X] Boot Mode Circuit:
        [X] Connect a 10kΩ pull-up resistor from GPIO0 (Pin 27) to 3.3V.
        [X] Connect a momentary push button (Boot Switch) between GPIO0 (Pin 27) and GND.
    [X] Crystal Oscillator: 40 MHz crystal is integrated within the module. No external crystal needed.
    [X] Antenna: Use onboard PCB antenna (WROOM-1). Ensure proper keep-out zones on your PCB as per layout guidelines. [cite: uploaded:esp32_s3_wroom_1_wroom_1u_datasheet_en-2930317.pdf]
2. USB Interface (Native OTG)
    [-] Connections:
        [X] Connect ESP32-S3 GPIO19 (Module Pin 13) to USB Connector D-.
        [X] Connect ESP32-S3 GPIO20 (Module Pin 14) to USB Connector D+.
        [X] Connect USB Connector VBUS to 5V supply (if host mode power required, consider power switching).
        [X] Connect USB Connector GND to main ground.
        [-] Connect USB Connector Shield to main ground.
    [-] Optional Components:
        [X] Place 0Ω series resistors/jumpers on D+/D- lines if desired.
        [-] Place ESD protection diodes (TVS array) near the connector on D+/D- lines.
    [] Layout: Route D+/D- as a 90Ω differential pair.
3. I2S Interface (ESP32-S3 to PCM5102)
    [X] Connections:
        [X] Connect ESP32-S3 GPIO (Select Pin) -> PCM5102 BCK (Pin 13).
        [X] Connect ESP32-S3 GPIO (Select Pin) -> PCM5102 LRCK (Pin 15).
        [X] Connect ESP32-S3 GPIO (Select Pin) -> PCM5102 DIN (Pin 14).
    [X] PCM5102 SCK: Ensure PCM5102 SCK (Pin 12) is connected to GND (to enable internal PLL). [cite: uploaded:pcm5102.pdf]
4. PCM5102 DAC Module
    [X] Power (3.3V):
        [X] Connect CPVDD (Pin 1), AVDD (Pin 8), DVDD (Pin 20) to 3.3V rail.
        [X] Connect CPGND (Pin 3), AGND (Pin 9), DGND (Pin 19) to ground plane (ensure proper analog/digital ground separation if needed, though often a single solid plane works well with careful layout).
        [X] Place 1µF decoupling capacitors close to pins 1, 8, 20 (to GND).
        [X] Place 1µF decoupling capacitor close to pin 18 (LDOO) (to DGND).
    [X] Charge Pump:
        [X] Connect 1µF ceramic capacitor between CAPP (Pin 2) and CAPM (Pin 4).
        [X] Connect 1µF ceramic capacitor from VNEG (Pin 5) to AGND.
    [X] Configuration Pins:
        [X] Connect FMT (Pin 16) to GND (I2S Format).
        [X] Connect FLT (Pin 11) to GND (Normal Latency Filter).
        [X] Connect DEMP (Pin 10) to GND (De-emphasis OFF).
        [X] Connect XSMT (Pin 17) to a selected ESP32-S3 GPIO pin for soft mute control.
    [X] Output Filter (Per Channel):
        [X] Connect 470Ω resistor in series with OUTL (Pin 6).
        [X] Connect 2.2nF capacitor from the output of the 470Ω resistor to AGND.
        [X] Repeat for OUTR (Pin 7).
5. I2C Interface (ESP32-S3 to TPA6130A2)
    [X] Connections:
        [X] Connect ESP32-S3 GPIO (Select SCL Pin) -> TPA6130A2 SCL (Pin 8 on QFN).
        [X] Connect ESP32-S3 GPIO (Select SDA Pin) -> TPA6130A2 SDA (Pin 7 on QFN).
    [X] Pull-up Resistors:
        [X] Connect 4.7kΩ pull-up resistor from SCL line to 3.3V rail.
        [X] Connect 4.7kΩ pull-up resistor from SDA line to 3.3V rail.
6. TPA6130A2 Headphone Amplifier Module
    [X] Power (5V):
        [X] Connect TPA6130A2 pins (e.g., Pin 12, Pin 20 on QFN) to 5V rail.
        [X] Connect TPA6130A2 GND pins (e.g., Pin 3, 9, 10, 13, 19 on QFN) and Thermal Pad (if QFN) to ground plane.
        [X] Place 1µF decoupling capacitors close to pins (to GND).
    [X] Charge Pump:
        [X] Connect 1µF low-ESR ceramic capacitor between CPP (Pin 18 on QFN) and CPN (Pin 17 on QFN).
        [X] Connect 1µF low-ESR ceramic capacitor from (Pin 15/16 on QFN) to GND.
    [X] Input Coupling (Single-Ended):
        [X] Connect PCM5102 Left Output (after filter) -> 0.47µF capacitor -> TPA6130A2 LEFTINM (Pin 1 on QFN).
        [X] Connect TPA6130A2 LEFTINP (Pin 2 on QFN) -> 0.47µF capacitor -> GND.
        [X] Connect PCM5102 Right Output (after filter) -> 0.47µF capacitor -> TPA6130A2 RIGHTINM (Pin 5 on QFN).
        [X] Connect TPA6130A2 RIGHTINP (Pin 4 on QFN) -> 0.47µF capacitor -> GND.
    [ ] Output:
        [X] Connect TPA6130A2 HPLEFT (Pin 14 on QFN) -> 3.5mm Jack Tip.
        [X] Connect TPA6130A2 HPRIGHT (Pin 11 on QFN) -> 3.5mm Jack Ring.
        [X] Connect 3.5mm Jack Sleeve -> GND.
    [X] Control Pins:
        [X] Connect SD (Shutdown) pin (Pin 6 on QFN) -> 10kΩ pull-up resistor -> 5V rail (Hardware Enabled).
7. SD Card Interface (SPI)
    [ ] Connections:
        [X] Connect ESP32-S3 GPIO (Select MOSI Pin) -> MicroSD Socket CMD (Pin 3).
        [X] Connect ESP32-S3 GPIO (Select MISO Pin) -> MicroSD Socket DAT0 (Pin 7).
        [X] Connect ESP32-S3 GPIO (Select SCK Pin) -> MicroSD Socket CLK (Pin 5).
        [X] Connect ESP32-S3 GPIO (Select CS Pin) -> MicroSD Socket DAT3/CS (Pin 2).
        [X] Connect MicroSD Socket VDD (Pin 4) -> 3.3V rail.
        [X] Connect MicroSD Socket VSS (Pin 6) -> GND.
    [ ] Supporting Components:
        [X] Place 0.1µF - 1µF decoupling capacitor near the socket VDD pin (to GND).
        [X] Connect 10kΩ - 50kΩ pull-up resistors from CS, MOSI, MISO, SCK lines to 3.3V rail.
    [ ] Optional: Connect Card Detect (CD) pin from socket (e.g., Pin Cd on MSD-1-A) to an ESP32-S3 GPIO if detection is desired. [cite: uploaded:msd_1_a-3510386.pdf]
8. User Interface
    [X] Keyboard Matrix (6x4):
        [X] Connect 6 Rows to 6 selected ESP32-S3 GPIOs.
        [X] Connect 4 Columns to 4 selected ESP32-S3 GPIOs.
        [X] Ensure diodes are included in the matrix design if required for ghosting prevention.
    [X] Push Buttons (x2):
        [X] Connect each button between a selected ESP32-S3 GPIO and GND.
        [X] Enable internal pull-up resistors on the ESP32-S3 GPIOs or add external pull-up resistors (e.g., 10kΩ to 3.3V).
    [X] LEDs (x2):
        [X] Connect each LED's anode to 3.3V (or other suitable supply).
        [X] Connect each LED's cathode through a current-limiting resistor (e.g., 220Ω - 1kΩ, value depends on LED specs and desired brightness) to a selected ESP32-S3 GPIO.
9. Power Supplies
    [ ] Input: Assume 5V input (e.g., from USB VBUS or external supply).
    [ ] 3.3V Regulator (LDO):
        [ ] Use MIC5504-3.3YM5-TR (SOT23-5 package).
        [ ] Connect LDO VIN pin to 5V input rail.
        [ ] Connect LDO GND pin to main ground plane.
        [ ] Connect LDO EN (Enable) pin to VIN (for always-on operation) or to an ESP32-S3 GPIO for software control.
        [ ] Connect LDO VOUT pin to the 3.3V power rail (supplying ESP32-S3, PCM5102, SD Card, UI pull-ups, etc.).
        [ ] Place input decoupling capacitor (e.g., 1µF ceramic) close to LDO VIN pin (to GND).
        [ ] Place output decoupling capacitor (e.g., 1µF ceramic) close to LDO VOUT pin (to GND). Check MIC5504 datasheet for specific capacitor requirements.
    [ ] 5V Rail: Ensure 5V rail is available for TPA6130A2 and LDO input.
    [ ] Layout: Employ good power supply layout practices (e.g., separate power planes or wide traces, proper decoupling placement).