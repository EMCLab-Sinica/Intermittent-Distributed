# An Intermittent Operating System for Intermittent Networks

<!-- ABOUT THE PROJECT -->
## Project Description

This project develops an intermittent operating system for an intermittent network formed by multiple intermittent devices, facilitating programmers in developing intermittent-aware IoT applications, regardless of power stability.

Internet-of-Things (IoT) devices are gradually adopting battery-less, energy harvesting solutions, thereby driving the development of an intermittent computing paradigm to accumulate computation progress across multiple power cycles. However, the computation progress improved by distributed task concurrency in an intermittent network can be significantly offset by data unavailability due to frequent power failures. We propose an intermittent-aware distributed concurrency control protocol which leverages existing data copies inherently created in the network to improve the computation progress of concurrently executed tasks. The protocol uses a borrowing-based data management method to increase data availability and an intermittent two-phase commit procedure incorporated with distributed backward validation to ensure data consistency in the network.

## Implementaton Description

To realize an operating system for intermittent networks, we integrated our borrowing-based distributed concurrency control protocol into an [intermittent operating system](https://github.com/EMCLab-Sinica/Intermittent-OS), which was built upon FreeRTOS for standalone intermittent systems.
Our protocol is realized on top of the task schedulerand memory manager, as shown in the figure below, with around 4800 lines of C code divided into 18 files.
Our operating system can be deployed on several TI MSP430 LaunchPad devices, equipped with TI CC1101 RF transceivers to form an intermittent network. The used low-power transceiver supports neither time-division multiple access (TDMA) nor frequency division multiple access (FDMA), resulting in severe channel collision when a network comprises more than four nodes. The intermittent OS when applied to a larger-scale intermittent network require another low-power wireless module that support multiple access and multi-hop relay routing.


![System Design](https://i.imgur.com/mttLGuu.png)

### Project Structure

Refer to the system design figure above

  * The folder `DataManager/` is the implementation of Data manager
  * The folder `ValidationHandler/` is the implementation of Validation handler
  * The folder `Connectivity/` is the implementation of Node communications with RF

<!-- TABLE OF CONTENTS -->
## Table of Contents
* [Getting Started](#getting-started)
  * [Prerequisites](#prerequisites)
  * [Setup and Build](#setup-and-build)
* [License](#license)
* [Contact](#contact)
<!--* [Contributing](#contributing)-->
<!-- GETTING STARTED -->
## Getting Started

### Prerequisites

Here are the basic software and hardware you need to build the applications running on this OS.

* [Code Composer Studio](https://www.ti.com/tool/CCSTUDIO) (tested version: 10.0.0)
* [TI MSP-EXP430FR5994](https://www.ti.com/tool/MSP-EXP430FR5994)
* [TI CC1101 RF Transceiver](https://www.ti.com/product/CC1101) or other low-power wireless module
* [DHT11 Humidity Sensor](https://www.mouser.com/datasheet/2/758/DHT11-Technical-Data-Sheet-Translated-Version-1143054.pdf) or other equivalent sensors.

### Setup and Build

1. Download/clone this repository

  * Download [TI MSP driverlib](https://www.ti.com/tool/MSPDRIVERLIB) and put in the root dir of the project (used version: 2.91.13.01)

2. Import this project to your workspace in Code Composer Studio:

3. Setup `node_addr` in `config.h` (from 1 to 4)

  * 1: Monitoring node
  * 2 and above: Sprinking node

4. Build and flash to your device

#### Note
* Please refer to `Connectivity/pins.h` to setup the the RF module, the pinout may be different
* Please refer to `Peripheral/dht11.c` to setup the the humidity sensor

## License

This project is licensed under the MIT License
