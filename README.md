# Zynq timestamping solution

This repository contains the source code of a timestamping mechanism developed by [SRS](http://www.srs.io) for both Zynq MPSoC and RFSoC devices, including RTL and C code, project generation scripts and extensive documentation. The solution is targeting a typical SDR implementation in which the transmission and reception of I/Q samples is triggered by a call to a software function. Two different approaches are supported towards this end:

1. **Use the Zynq-based board as an SDR front-end**: that is, the Zynq-board directly interfaces the RF and implements the timestamping solution, whereas the SDR application runs in a host that is connected to it via Ethernet/USB. For this use case, the following platforms are explicitly supported:

  - [MicroPhase AntSDR](/projects/antsdr/)
  - [ADALM-PLUTO](/projects/pluto/)

2. **Use the Zynq-based board as a fully embedded SDR solution**: that is, the Zynq-board directly interfaces the RF (or has it embedded it in the SoC; e.g., RFSoC), implements the timestamping solution (in the FPGA, where you could also accelerate other DSP functions) and also hosts the SDR application (in the embedded ARM). For this use case, the following platforms are explicitly supported:

  - [Xilinx Zynq UltraScale+ MPSoC ZCU102 Evaluation Kit](/projects/zcu102/)
  - [Xilinx Zynq UltraScale+ RFSoC ZCU111 Evaluation Kit](/projects/zcu111/)

For the sake of convenience this repository includes the code which is specific to the Zynq timestamping solution and uses submodules for the related code that is external to it, including the [srsRAN 4G/5G software radio suite](https://www.srsran.com) and [Analog Devices HDL library](https://wiki.analog.com/resources/fpga/docs/hdl). The latter is used because the timestamping solution is targeting AD936x-based front-ends for MPSoC architectures.

The full details of the Zynq timestamping solution can be found in the [documentation page](https://srsran.github.io/zynq_timestamping/). Additionally, dedicated application notes are covering all required steps from build to test:

- End-to-end 4G testing with the [AntSDR](https://srsran.github.io/zynq_timestamping/app/antsdr.html).
- Tx-Rx testing with the [ADALM-PLUTO](https://srsran.github.io/zynq_timestamping/app/plutosdr.html).
- Petalinux build, software cross-compilation and Tx-Rx testing with [ZCU102/ZCU111](https://srsran.github.io/zynq_timestamping/app/zcu.html) boards.

We recommend you to go through the application notes, as the detailed steps can be (often easily) modified/reused to target different boards and/or SDR applications.

# Requirements

- The solution has been developed, validated and tested using:

  * Vivado 2019.2
  * SRS Python Tools:

    ```
    cd python_tools
    sudo pip3 install -U pip
    pip3 install .
    ```
  * [optional] For documentation:
    ```
    npm install teroshdl
    ```

- To clone the repository and the utilized submodules:

  ```
  git clone --recursive
  ```

# Pre-built images

Pre-built images for all supported boards can be found attached as an asset to the [released code](https://github.com/srsran/zynq_timestamping/releases).
