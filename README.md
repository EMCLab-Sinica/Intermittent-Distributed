# Intermittent-aware Distributed Concurrency Control Demo

<!-- ABOUT THE PROJECT -->
## Project Description

This project presents an intermittent-aware distributed concurrency control protocol, which leverages existing data copies inherently created in the network to improve the computation progressof concurrently executed tasks.
In particular, we propose a borrowing-based data management method to increase data availability, and an intermittent two-phase commit procedure incorporatedwith distributed backward validation to ensure data consistency in the network.
The proposed protocol was integrated into a FreeRTOS-extended intermittent operating system running on Texas Instruments devices

## Implementaton Description
To realize an operating system for intermittent networks, we integrated our borrowing-based distributed concurrency control protocol into an intermittent operating system, which was built upon FreeRTOS according to the [failure-resilient](https://www.citi.sinica.edu.tw/papers/pchsiu/7157-F.pdf) design intended for standalone intermittent systems.
Our protocol is realized on top of the task schedulerand memory manager, as shown in Figure 7, with around 4800 lines of C code scattered into 18 files. Our operating system is then deployed on several TI MSP430 LaunchPad devices,equipped with TI CC1101 Radio Frequency (RF) transceivers, to form an intermittentnetwork.
Currently the project suuports up to 10 tasks and 16 data object globally.

### Project Structure
  * The folder `DataManager/` is the implementation of Data Manager in paper Chapter 5.1
  * The folder `ValidationHandler/` is the implementation of Validation Handler in paper Chapter 5.2
  * The folder `Connectivity/` is the implementation of Node Communications in paper Chapter 5.3

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


## License

This project is licensed under the MIT License
