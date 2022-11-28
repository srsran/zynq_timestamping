.. Application Note: Petalinux build, software cross-compilation and Tx-Rx testing with the ZCU102 and ZCU111 platforms

.. _zcu:

Application Note: Petalinux build, software cross-compilation and Tx-Rx testing with the ZCU102 and ZCU111 platforms
====================================================================================================================

Overview
********

.. image:: images/app_note_zcu.png
  :alt: System



Hardware and Software Requirements
**********************************

For this application note, the following hardware and software will be used:

        1. Dell XPS13 with Ubuntu 20.04.4
        2. Xilinx Zynq UltraScale+ MPSoC ZCU102/RFSoC ZCU111 Evaluation Kit with custom SRS bitstream and Petalinux 2019.2
        3. srsRAN
        4. SRS Zynq timestamping
        5. Analog Devices libiio and libad9361 software libraries (ZCU102 only)


Prerequisites
*************

1. You need to generate the custom SRS timestamp bitstream and to load it in the board (see the
:ref:`zcu102_project`
or the
:ref:`zcu111_project`
section for more details).


Building the binaires for ARM and preparing a bootable SD card
**************************************************************

Before you can build srsRAN or your custom SDR application binaries targeting the ARM embedded in
the MPSoC/RFSoC device, you'll need to setup a toolchain and a minimal sysroot suitable for the
cross-compilation of the Zynq timestamping software.

In this Application Note it is described the build and utilization of Petalinux, as it provides an
easy and convenient command-line interface which allows building bootfiles, a rootfilesystem and a
complete SDK (i.e., toolchain + target sysroot) suitable for cross-compilation. It should also be
perfectly possible to use Yocto tools directly, or another build-system (e.g., buildroot) if you
should prefer so, but it is out of the scope of this Application Note.

Building Petalinux 2019.2
-------------------------

The Petalinux project is based on the exported hardware configuration files of the Vivado project
implemented as part of the prerequisites. Once the project is implemented, make sure to export the
hardware configuration and bitstream as described below:

1. From the lateral *IMPLEMENTATION* menu select *Open Implemented Design*
2. From the *File* select *Export -> Export Hardware...*
3. Check *Include bistream*
4. In *Export to* select the *sdk* subfolder of the Vivado project (_VIVADO_PROJECT_PATH_/_VIVADO_PROJECT_NAME_.sdk)

The following steps are required to build Petalinux:

1. Download the BSP for
`ZCU102 <https://www.xilinx.com/member/forms/download/xef.html?filename=xilinx-zcu102-v2019.2-final.bsp>`_
or
`ZCU111 <https://www.xilinx.com/member/forms/download/xef.html?filename=xilinx-zcu111-v2019.2-final.bsp>`_
for  for Vivado 2019.2.

2. Download the
`Petalinux SDK for Vivado 2019.2 <https://www.xilinx.com/member/forms/download/xef.html?filename=petalinux-v2019.2-final-installer.run>`_.


3. Check the documentation and verify that all dependencies are installed. The following command
can be used to install dependencies in Ubuntu 20.04:

.. code-block:: bash

   sudo apt-get update & sudo apt-get install -y python tofrodos iproute2 gawk xvfb gcc git make net-tools libncurses5-dev tftpd zlib1g-dev libssl-dev flex bison libselinux1 gnupg wget diffstat chrpath socat xterm autoconf libtool tar unzip texinfo zlib1g-dev gcc-multilib build-essential libsdl1.2-dev libglib2.0-dev zlib1g:i386 screen pax


4. Install the Petalinux Tools:

.. code-block:: bash

   ./petalinux-v2019.2-final-installer.run _INSTALL_PATH_/petalinux_sdk_2019.2


5. Create a folder to build Petalinux 2019.2 (e.g., *_PETALINUX_*).


6. Run the following commands (from *_PETALINUX_* build folder):

.. code-block:: bash

   source _INSTALL_PATH_/petalinux_sdk_2019.2/settings.sh


In case of problems with bash see the following
`Xilinx forum post <https://forums.xilinx.com/t5/Embedded-Linux/Petalinux-settings-sh-problem/td-p/567202>`_
.

.. code-block:: bash

   petalinux-create -t project -s xilinx-zcu111-v2019.2-final.bsp
   cd xilinx-zcu111-2019.2/


7. Load the hardware description (generated from Vivado):

.. code-block:: bash

   petalinux-config --get-hw-description _VIVADO_PROJECT_PATH_/_VIVADO_PROJECT_NAME_.sdk


8. A configuration menu pops up, verify the following configs and do any required changes, then save & exit:

.. code-block:: bash

   . Subsystem AUTO Hardware Settings ->
       .. Advanced bootable images storage Settings ->
           * boot image settings    -> image storage media -> primary sd
           * kernel image settings  -> image storage media -> primary sd
                                    -> image name -> Image
           * dtb image settings     -> image storage media -> primary sd
       .. Flash settings -> *** partition 3 *** -> set name 'spare'
   . Image Packaging Configuration ->
       .. Root filesystem type -> SD card
       .. Device node of SD device -> /dev/mmcblk0p2
       .. name for bootable kernel image -> Image
   . Yocto Settings ->
       .. Enable Debug Tweaks
       .. Parallel thread execution -> set number of bb trheads -> 4
                                    -> set number of parallel make -j -> 4


9. Configure the kernel:

.. code-block:: bash

   petalinux-config -c kernel


**Important note:** in the Zynq timestamping solution for the ZCU111 board a customized kernel is
built from source code, while adding a custom kernel module. The full instructions for this are
provided below. We advise you to first always build the kernel through the Petalinux tools, and to
validate that it works well and is properly configured (e.g., it has all necessary modules), before
proceeding to build a customized kernel.

10. A configuration menu pops up, verify the following configs and do any required changes, then save & exit:

.. code-block:: bash

   . CPU Power Management -> CPU Idle -> disable CPU idle PM support


11. Configure the rootfs:

.. code-block:: bash

   petalinux-config -c rootfs


12. A configuration menu pops up, add the following modules, then save & exit:

.. code-block:: bash

   . Filesystem Packages
       .. base -> select i2c-tools
       .. console -> network -> dropbear -> select dropbear
       .. devel -> python -> python -> select all modules
                             python-numpy -> select python-numpy
       .. libs -> libmetal -> select all modules
                  libgcrypt -> select all modules
                  network -> openssl -> select all modules
       .. misc -> gdb -> select all modules
                  python3 -> select all modules
                  python3-async -> select python3-async
                  python3-setuptools -> select python3-setuptools
   . Petalinux Package Groups
       .. packagegroup-petalinux -> select all modules


For the ZCU111 build case, no extra dependencies should be needed to cross-compile the Zynq timestamping
software and txrx example application; for ZCU102 the libiio libraries are required and, hence, they need
to be included in the rootfs and SDK (this is out of the scope of this Application note, but instructions
can be found in the
`META-ADI-XILINX <https://github.com/analogdevicesinc/meta-adi/tree/2019_R2/meta-adi-xilinx>`_
repository). In case you want to cross-compile the entire srsRAN software suite and test more complex
applications, you would also need to add the extra dependencies into *project-spec/meta-user/recipes-core/images/petalinux-user-image.bbappend*
as detailed:

.. code-block:: bash

   IMAGE_INSTALL_append = "\
    		          boost \
    		          boost-dev \
    		          mbedtls \
    		          mbedtls-dev \
    		          libfftw \
    		          libfftwf \
    		          pcsc-lite \
    		          pcsc-lite-dev \
    		          lksctp-tools \
    		          lksctp-tools-withsctp \
    		          lksctp-tools-dev \
	                "


13. Finally, to build Petalinux and package the generated rootsystem:

.. code-block:: bash

   petalinux-build
   petalinux-build --sdk
   petalinux-package --sysroot


In case of problems see the following
`Xilinx forum post <https://forums.xilinx.com/t5/Embedded-Linux/PetaLinux-build-fails-with-locale-errors-How-to-disable-locale/m-p/894431/highlight/false#M28960>`_
.

The files resulting from the commands above will be left at *_PETALINUX_/xilinx-zcu111-2019.2/images/linux*

.. code-block:: bash

   petalinux-package --boot --format BIN --fsbl images/linux/zynqmp_fsbl.elf --u-boot images/linux/u-boot.elf --pmufw images/linux/pmufw.elf --fpga _VIVADO_PROJECT_PATH_/_VIVADO_PROJECT_NAME_.runs/impl_1/design_1_wrapper.bit --force


Preparing the SD card
----------------------

1. Create 2 partitions in the SD card:
  - **BOOT**, 1 GB, FAT32, primary
  - **rootfs**, remaining space, ext4, primary

2. Copy the following files to the BOOT partition of the SD card:
  - BOOT.BIN (from *_PETALINUX_/xilinx-zcu111-2019.2/images/linux*)
  - Image (from *_PETALINUX_/xilinx-zcu111-2019.2/images/linux*)
  - system.dtb (from *_PETALINUX_/xilinx-zcu111-2019.2/images/linux*)
  - uEnv.txt (you can find an example in the repository under */projects/zcu1XX/bootfiles/*)

3. Deploy rootfs in the ext4 partition of the SD card (the compressed rootfs file is located at
*_PETALINUX_/xilinx-zcu111-2019.2/images/linux*):

.. code-block:: bash

   sudo tar xvf rootfs.tar.gz -C _PATH_TO_MOUNTED_SD_CARD_/rootfs


Compiling a customized Linux kernel from source code
----------------------------------------------------

Once you have verified that you can successfully boot the board using an SD card prepared as detailed
above, you can replace the Linux kernel with a customized one compiled from source code (this approach
seems more convenient when compilation of out-of-tree Linux kernel modules is required). Follow the
instructions below for two different kernel sources.

Default Xilinx kernel
.....................

Get the source code from Xilinx GitHub:

.. code-block:: bash

   git clone https://github.com/Xilinx/linux-xlnx.git
   cd linux-xlnx
   git checkout -b xilinx-v2019.2.01 tags/xilinx-v2019.2.01
   export ARCH=arm64 && export CROSS_COMPILE=aarch64-linux-gnu-


Take the configuration from a running kernel:

.. code-block:: bash

   scp -r root@<zcu111-ip-address>:/proc/config.gz .
   zcat config.gz > .config


Configure the kernel:

.. code-block:: bash

   make ARCH=arm64 oldconfig


Cross-compile the kernel and modules:

.. code-block:: bash

   make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- -j8 Image UIMAGE_LOADADDR=0x8000
   mkdir compiled_modules
   make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- -j8 INSTALL_MOD_PATH=./compiled_modules modules
   make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- -j8 INSTALL_MOD_PATH=./compiled_modules modules_install


Copy the *Image* file from *arch/arm64/boot/* to the BOOT partition of the SD card. Copy the modules
to */lib/modules/* in the rootfs partition of the card.

Analog Devices kernel
.....................

You can find conveniently detailed instructions, especially for the ZCU102 and FMCOMMS2 pairing, on the
`Analog Devices Wiki <https://wiki.analog.com/resources/eval/user-guides/ad-fmcomms2-ebz/software/linux/zynqmp>`_
.

Compiling a custom Linux kernel driver
--------------------------------------

The source code for the driver required to implement timestamping support in the ZCU111 board can
be found in a dedicated folder in the Zynq timestamping repository (under
*/sw/lib/src/phy/rf/xrfdc/kernel_module*), jointly with its associated *Makefile*. Note that this
specific driver is not required for any other of the supported boards.

Before compiling the custom driver, make sure that you first have compiled the Linux kernel by
following the instructions provided above. Once all is set, a *KDIR* variable needs to be defined
in the *Makefile* to ensure that it points to the kernel source code directory. The following
commands must then be executed:


.. code-block:: bash

   export CROSS_COMPILE=aarch64-linux-gnu-
   export ARCH=arm64
   make ARCH=arm64 -j4


As a result of its successful compilation a *srs_dma_driver.ko* file will be generated. This file
can be used with the previously compiled Linux kernel after booting up the board.

In order to install the driver in the Linux running in the ZCU111 the *srs_dma_driver.ko* file needs
to be transferred to the board. Then execute:

.. code-block:: bash

   insmod srs_dma_driver.ko

Modifying evicetree
-------------------

The custom *srs_dma_driver* driver obtains information regarding the DMA IPs from the devicetree.
Hence, the latter must include additional information for this. Below you can see how to define
the *srs_rx_dma* and *srs_tx_dma* nodes that do refer to the specific DMAs used by the Zynq
timestamping solution (you may also check this in the Vivado project built in earlier steps):

.. code-block:: bash

               dma@a0060000 {
                       #dma-cells = <0x01>;
                       clock-names = "s_axi_lite_aclk\0m_axi_s2mm_aclk";
                       clocks = <0x03 0x47 0x03 0x47>;
                       compatible = "xlnx,axi-dma-7.1\0xlnx,axi-dma-1.00.a";
                       interrupt-names = "s2mm_introut";
                       interrupt-parent = <0x04>;
                       interrupts = <0x00 0x5a 0x04>;
                       reg = <0x00 0xa0060000 0x00 0x10000>;
                       xlnx,addrwidth = <0x40>;
                       phandle = <0x37>;

                       dma-channel@a0060030 {
                               compatible = "xlnx,axi-dma-s2mm-channel";
                               dma-channels = <0x01>;
                               interrupts = <0x00 0x5a 0x04>;
                               xlnx,datawidth = <0x40>;
                               xlnx,device-id = <0x00>;
                       };
               };

               srs_rx_dma {
                       compatible = "srs,txrx_dma";
                       dmas = <0x37 0x00>;
                       dma-names = "rx";
                       dma-direction = "rx";
                       dma-coherent;
               };

               dma@a0070000 {
                       #dma-cells = <0x01>;
                       clock-names = "s_axi_lite_aclk\0m_axi_mm2s_aclk";
                       clocks = <0x03 0x47 0x03 0x47>;
                       compatible = "xlnx,axi-dma-7.1\0xlnx,axi-dma-1.00.a";
                       interrupt-names = "mm2s_introut";
                       interrupt-parent = <0x04>;
                       interrupts = <0x00 0x5b 0x04>;
                       reg = <0x00 0xa0070000 0x00 0x10000>;
                       xlnx,addrwidth = <0x40>;
                       phandle = <0x38>;

                       dma-channel@a0070000 {
                               compatible = "xlnx,axi-dma-mm2s-channel";
                               dma-channels = <0x01>;
                               interrupts = <0x00 0x5b 0x04>;
                               xlnx,datawidth = <0x20>;
                               xlnx,device-id = <0x01>;
                       };
               };

               srs_tx_dma {
                       compatible = "srs,txrx_dma";
                       dmas = <0x38 0x00>;
                       dma-names = "tx";
                       dma-direction = "tx";
               };


The following commands will be useful for altering the default devicetree file, either built by
Petalinux or by follwing the instructions from Analog Devices wiki, ensuring that the board can
successfully boot with it.

1. Generate an editable devicetree file:

.. code-block:: bash

   dtc -I dtb -O dts -o system.dts system.dtb


2. Modify it as needed and recompile:

.. code-block:: bash

  dtc -I dts -O dtb -o system.dtb system.dts


Cross-compiling the Zynq timestamping library and Tx-Rx example application
---------------------------------------------------------------------------

The first step is to install the SDK that was built via petalinux-tools in your host PC. This file
is located at */PETALINUX_BUILD_PATH/xilinx-zcu111-2019.2/images/linux*. To install it, use the
following command:

.. code-block:: bash

   ./sdk.sh


You will be prompted to specify the toolchain installation path (for instance, use
*/opt/plnx_sdk_rfsoc*). When the installation finishes, set up the following environment variables:

.. code-block:: bash

   . /opt/plnx_sdk_rfsoc/environment-setup-aarch64-xilinx-linux


Then, go to the */app* subfolder in the path where the Zynq timestamping repository is cloned locally
and execute the initialization script. It will download all necessary git submodules and compile the
RF drivers and example Tx-Rx application. The default command builds the RF driver based on the Analog
Devices libiio library (i.e, it is suitable for the ZCU102, plutoSDR and antSDR boards), using the
following command:

.. code-block:: bash

   ./prepare.sh


For the ZCU111 board, which uses an RFSoC device, you will need to pass an extra parameter to the
initialization script call as follows, so that it uses the Xilinx librfdc library:

.. code-block:: bash

   ./prepare.sh rfsoc


When the build finishes, you will find the application under the *bin_app/* subfolder. The
binary needs then to be transferred to the board (e.g., in */home/srs/bin*).

Running
*******

First of all, you need to make sure that the board is set up to implement a Tx-Rx loopback
(e.g., cable the Tx and Rx ports together).

A customized *txrx* application (aimed at demonstrating the basic capabilities of the Zynq timestamping
solution) has been compiled and transferred to the board in the previous step. It will transmit three
tones with a separation of 4 ms between them, while generating a capture file signal as well. A script
is also provided to execute it. After transferring the script to the same path containing the *txrx*
application binary in the board (e.g. */home/srs/bin*), run the follwing command (from that path):

.. code-block:: bash

    ./run_txrx_zcu.sh


Note that all the scripts located under '/app/scripts' are meant to help the understand what applications
can be used with each board and what parameters need to be provided.

(Optional) After transferring back the data capture generated in the board to your computer, you
can plot the captured signal with the following command:

.. code-block:: bash

    python3 show.py test_txrx_zcu.bin
