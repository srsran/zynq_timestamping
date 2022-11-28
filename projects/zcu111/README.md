# Requirements:
​
- Install Python3. E.g. for Ubuntu 20.04:

```
sudo apt update
sudo apt install -y python3.8
```

- Install other dependencies:

```
sudo apt update
sudo apt install -y python3-pip
sudo apt install -y git
```
​
- Install SRS tools:
​
```
cd *local_SRS_FPGA_repo_path*/lib/src/phy/ue/fpga_ue/project_scripts/python_tools/srs_tools_fpga
pip3 install .
```

**NOTE:** *local_SRS_FPGA_repo_path* is the path where the SRS FPGA repository is locally cloned.

# FPGA project

```
export SRS_VIVADO_PATH=*local_Vivado_installation_path*
cd *local_SRS_FPGA_repo_path*/lib/src/phy/ue/fpga_ue/project_scripts/tx_rx_rfsoc/
./create_project.sh bitstream
```

**NOTE:** *local_Vivado_installation_path* is the path where Vivado is locally installed (e.g., */opt/Xilinx/Vivado/2019.2/*).

# Boot files

```
export SRS_VIVADO_PATH=*local_Vivado_installation_path*
cd *local_SRS_FPGA_repo_path*/lib/src/phy/ue/fpga_ue/project_scripts/tx_rx_rfsoc/bootfiles
./create_boot.sh
```

# Software app

Bulding the test application - the commands below assume that the *Petalinux SDK* is installed in the build machine and, hence, the required dependencies are satisfied (e.g., *libmetal*, *librfdc*), otherwise have a look at the [Petalinux build instructions](https://github.com/softwareradiosystems/sonic-5g-fpga/blob/master/lib/src/phy/ue/fpga_ue/srsRAN_RFSoC.md#building-petalinux-20192):

```
. *local_Petalinux_SDK_installation_path*/environment-setup-aarch64-xilinx-linux
cd *local_SRS_FPGA_repo_path*/
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release -DUSE_LTE_RATES=ON -DCMAKE_TOOLCHAIN_FILE=../lib/src/phy/ue/fpga_ue/project_scripts/tx_rx_rfsoc/toolchain_zcu111.cmake ..
make -j4 rfsoc_txrx
```

**NOTE:** *local_Petalinux_SDK_installation_path* is the path where the Petalinux SDK is installed (e.g., */opt/Petalinux_SDK_RFSoC/*).

Running the application - the example command below sets the center RF frequency at 2.4 GHz, fixes a signal bandwidth of 5 MHz [25 PRB] and requests transmission in 4 ms (i.e., default tx-timestamp is *current_time + 4 ms*):

```
cd lib/examples
./rfsoc_txrx -f 2400000000 -a clock=external -p 25 -g 50 -o test_txrx_rfsoc.bin
```

Other transmission times can be requested by using the *-T* parameter (e.g., adding *-T 7680* would shift the transmission time to *current_time + 3 ms*).

# Test setup

As per FPGA design (i.e., fixed in the Vivado project), the ADC port in Tile 224, channel 1 (labelled as *ADC224_T0_CH1* in the XM500 daugtherboard plugged to the ZCU111 board) and the DAC port in Tile 229, channel 3 (labelled as *ADC224_T1_CH3*) need to be cabled together forming a closed loop.

Use of an external 10 MHz reference is advised (see the [5G SA UE application note](https://docs.srsran.com/en/rfsoc/app_notes/source/5g_sa_emb_ue/source/index.html) for more details).
