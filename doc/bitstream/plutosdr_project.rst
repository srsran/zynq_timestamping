.. ADALM-PLUTO project build

.. _plutosdr_project:

ADALM-PLUTO project build
=========================

System diagram
**************

.. image:: images/system_plutosdr.png
  :width: 800
  :alt: System

Prerequisites
*************

* Vivado 2019.2
* SRS Python tools:

.. code-block:: bash

    cd python_tools
    sudo pip3 install -U pip
    sudo pip3 install .


Bitstream generation
*********************

1. Set Vivado path:

.. code-block:: bash

   export SRS_VIVADO_PATH=/opt/Xilinx/Vivado/2019.2/


2. Generate SRS IPs, ADI IPs and bitstream:

.. code-block:: bash

   cd projects/plutosdr
   ./create_project.sh bitstream


Bootgen
*******

1. Configure the PlutoSDR parameters:

.. code-block:: bash

    export BOARD_USER="root"
    export BOARD_PASS="analog"
    export BOARD_IP="192.168.1.10"


2. Generate the boot file, load it in the board and reset it:

.. code-block:: bash

   make gen-boot-load


Board usage
***********

See the
:ref:`plutosdr`
for full details on building and running an SDR software application in this platform.

Tips and tricks
***************

In case you want to manually load the bootfiles, just use the following commands (as used in the
*board-specific Makefile*):

.. code-block:: bash

   export BOARD_USER="root"
   export BOARD_PASS="analog"
   export BOARD_IP="192.168.1.10"

   sshpass -p $BOARD_PASS scp ./bootfiles/system_top.bit.bin $BOARD_USER@$BOARD_IP:/lib/firmware
   sshpass -p $BOARD_PASS ssh $BOARD_USER@$BOARD_IP "cd /lib/firmware; echo system_top.bit.bin > /sys/class/fpga_manager/fpga0/firmware"
   sshpass -p $BOARD_PASS ssh $BOARD_USER@$BOARD_IP "echo 79024000.cf-ad9361-dds-core-lpc > /sys/bus/platform/drivers/cf_axi_dds/unbind"
   sshpass -p $BOARD_PASS ssh $BOARD_USER@$BOARD_IP "echo 79020000.cf-ad9361-lpc > /sys/bus/platform/drivers/cf_axi_adc/unbind"
   sshpass -p $BOARD_PASS ssh $BOARD_USER@$BOARD_IP "echo 7c400000.dma > /sys/bus/platform/drivers/dma-axi-dmac/unbind"
   sshpass -p $BOARD_PASS ssh $BOARD_USER@$BOARD_IP "echo 7c420000.dma > /sys/bus/platform/drivers/dma-axi-dmac/unbind"
   sshpass -p $BOARD_PASS ssh $BOARD_USER@$BOARD_IP "echo 7c420000.dma > /sys/bus/platform/drivers/dma-axi-dmac/bind"
   sshpass -p $BOARD_PASS ssh $BOARD_USER@$BOARD_IP "echo 7c400000.dma > /sys/bus/platform/drivers/dma-axi-dmac/bind"
   sshpass -p $BOARD_PASS ssh $BOARD_USER@$BOARD_IP "echo 79024000.cf-ad9361-dds-core-lpc > /sys/bus/platform/drivers/cf_axi_dds/bind"
   sshpass -p $BOARD_PASS ssh $BOARD_USER@$BOARD_IP "echo 79020000.cf-ad9361-lpc > /sys/bus/platform/drivers/cf_axi_adc/bind"
