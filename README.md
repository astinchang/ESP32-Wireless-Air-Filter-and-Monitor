# ESP32 Air Filter and Monitor
<img width="2160" height="2880" alt="ESP32AirFilterPic (1)" src="https://github.com/user-attachments/assets/9915909a-748c-4199-995c-b214147d9786" />

## **Project Report**

### **Project Overview**

This project is an automatic air filtration system built around an ESP32 microcontroller. The goal of the project is to improve indoor air quality while saving energy by only increasing fan speed when the air quality gets worse. The system uses a particulate matter sensor to measure the amount of particles in the air, a temperature and humidity sensor to monitor environmental conditions, PWM-controlled fans to move air through filters, a MicroSD card module to log air quality data over time, and WiFi tracking so that air quality trends could be monitored from a phone or laptop.

The basic idea is that the ESP32 reads sensor data, decides how dirty the air is, and adjusts the speed of the fans accordingly. If the particulate matter level is low, the fans can run at a lower speed to save power. If the particulate matter level rises, the fans ramp up to clean the air more quickly. This makes the system more efficient than having the fans run constantly. I also included a push button that can interrupt the system to force the fans off, for safety.

### **Components and Their Purpose**

- The main MCU for the project is the ESP32C6-DevKitC-1. It implements the basic features I need to get this project running: I2C, SPI, and UART communication, with WiFi and PWM capabilities. The ESP32 is also low power.
- The PMS5003 particulate matter sensor measures the amount of particulate matter in the air. This component is polled to check air quality at the PM2.5 (particles <2.5um diameter) level, although it does estimate other particle sizes, useful for data reporting. We use this sensor to control the fans' PWM level.
- The HDC3022 temperature and humidity sensor measures the surrounding temperature and humidity. This sensor really helps the node be a more complete source of information, because air quality readings can be affected by humidity. Also useful for more complete, environmental monitoring.
- The PWM PC fans are used to pull air through the filter. They are controlled using PWM output from the ESP32. This is necessary for saving energy and making the air filter respond gradually to different air quality levels.
- The air filters physically remove particles from the air as the fans pull air through the system. These are the part of the project that actually performs the cleaning.
- The MicroSD card module is used for non-volatile data logging. This allows the system to save particulate matter, temperature, and humidity data over time. The MicroSD module is important because it lets the user look back at how air quality changed instead of only seeing the current reading.
- The foam board was used as the physical body of the air filter. It held the fans, filters, sensors, and wiring together. While it worked for a prototype, it was not as sturdy or polished as a real product enclosure.

CREDIT TO JOEY FOX FOR THE AIR FILTER DESIGN: https://itsairborne.com/building-a-pc-fan-corsi-rosenthal-box-68e7cd1ca570 

### **Problems Encountered**

One of the biggest issues was the physical wiring and construction. Since the project was built as a prototype, wiring parts from the wall and connecting everything cleanly was more difficult than expected. The system had several separate parts that needed power, especially with needing to split the 12V power from the wall adapter 3 ways: the 2 sets of fans and buck converter to the breadboard (I split the 6 fans into 2 sets because I didn't feel comfortable daisy chaining 6 together). I also realized midway that using the flimsy breadboard wire was probably not safe and causing overheating, so buying new wire and using wire nuts was necessary, although it made keeping the wiring organized a challenge.

Another issue was that the project was built using a breadboard and foam board instead of a custom PCB and enclosure. This made the system easier to prototype and change, but it also made it more fragile and less clean-looking. Wires were mostly loose, connections were less secure, and the final build was not as sturdy as a real embedded product.

### **Comparison to Real-World Embedded Systems**

This project is similar to real-world embedded systems because it uses sensors, a microcontroller, actuator control, data logging, and low-power design ideas. Like many real embedded systems, it reads information from the environment, processes that data, and controls hardware based on the result. It also uses common embedded communication methods such as serial communication, I2C, SPI, PWM, and WiFi.

The project is different from a real-world embedded system in its physical construction. A commercial air quality monitor or smart air purifier would likely use a mounted PCB, proper connectors, a stronger enclosure, and more fixed wiring. Real products also usually go through more testing for safety, durability, airflow efficiency, and long-term reliability. In comparison, this project was more of a functional prototype built with available class materials, breadboard wiring, and foam board.

### **Future Improvements**

With more time and resources, the biggest improvement would be moving the circuit off the breadboard and onto a custom PCB. This would make the system much more reliable, compact, and easier to mount inside an enclosure. Another improvement would be using 3D printing to create a better electrical box and outer enclosure. A 3D-printed enclosure would protect the electronics, organize the wiring, and make the project look closer to a real consumer product.

Overall, this project successfully implements an ESP32-controlled smart air filter. It senses air quality, adjusts fan speed, and logs environmental data, while also showing how embedded systems can be used to make everyday devices more customizable and energy efficient.


## **Wiring Diagrams**

<img width="681" height="315" alt="AirFilterWallPowerDiagram" src="https://github.com/user-attachments/assets/a91af10e-a892-4f6c-8f8b-96b3b342ef41" />
<br>
The diagram above shows how the system is powered from the wall outlet. As I said before, I separated the PC fans into 2 sets of 3 daisy chained together, because I didn't want to daisy chain all of them together. Starting from the wall outlet, I used an AC to DC plug and barrel jack to two terminal connector to separate the voltage and ground wire. Afterwards, took voltage wire and split it into 3 wires: two for the aforementioned fans, one leading to 12V to 5V buck converter for the ESP32 input voltage. All grounds in this chain are tied to a universal ground inside of a screw terminal bus bar. The breadboard/ESP32 logic is described in the next diagram:
<br>
<br>

<img width="1100" height="671" alt="AirFilterESP32PinoutDiagram" src="https://github.com/user-attachments/assets/15377f6a-f80a-4416-a495-1f70c18995e5" />
<br>
Diagram above shows the ESP32 pinout I used to connect all peripherals using the ESP32 and 2 breadboards. As for the power, the 3.3V power was supplied by the ESP32, the 5V power was used as the input for the MCU, and all grounds were tied to the screw terminal bus bar. For the peripherals, pins 4-5 are used for UART to the particle sensor, pin 6 is used for PWM to control the fan speed, pin 0 is used for digital input (input pullup) for the fan shut off button, pins 22-23 are used for I2C to the temp/humidity sensor, and finally pins 18-21 are used for SPI to the microSD card module. 
<br>
<br>

## Libraries Used
The following libraries/APIs were used:

- Arduino.h: Core Arduino/ESP32 functions such as pinMode, delay, millis, Serial, and basic sketch structure.
- Wire.h: Arduino I2C library used to communicate with the HDC3022 temperature/humidity sensor.
- WiFi.h: ESP32 WiFi library used to create the ESP32 access point and host the web dashboard.
- Adafruit_PM25AQI: Adafruit library used to read data from the PMS5003 particulate matter sensor over UART.
- Adafruit_HDC302x: Adafruit library used to read temperature and humidity from the HDC3022 sensor.
- SPI.h: Arduino SPI library used for communication with the microSD card module.
- SD.h: Arduino SD card library used to create and append data to airlog.csv on the microSD card.

## KiCAD Build
*Implemented hardware designed above in dedicated PCB with slight differences (FIRST SOLO BUILD DON'T JUDGE LOL):
- ESP32-C6-WROOM-1 microcontroller
- USB-C programming through CP2102N
- 5 V to 3.3 V regulation
- I2C, SPI, and UART expansion headers
<br>
IF TRYING TO GET THIS TO WORK FROM MY FILES MAKE SURE TO DOWNLOAD THESE LIBRARIES:
- 10171746-00021LF
- ESD122DMYR
- espressifLibraries

<br>
<img width="835" height="691" alt="image" src="https://github.com/user-attachments/assets/c4f4d555-60f0-48c0-a344-a557fffa9235" />
<br>

