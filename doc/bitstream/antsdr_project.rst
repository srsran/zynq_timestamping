.. MicroPhase AntSDR project build

.. _antsdr_project:

MicroPhase AntSDR project build
===============================

System diagram
**************

.. image:: images/system_antsdr.png
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

   cd projects/antsdr
   ./create_project.sh bitstream


Bootgen
*******

1. Generate the boot file:

.. code-block:: bash

   make gen-boot


2. Configure the antsdr parameters:

.. code-block:: bash

    export BOARD_USER="root"
    export BOARD_PASS="analog"
    export BOARD_IP="192.168.1.10"


3. Load boot files to antsdr:

.. code-block:: bash

    make load-boot


4. Reboot the board.

Board usage
***********

See the
:ref:`antsdr`
for full details on building and running an SDR software application in this platform.


Tips and tricks
***************

Configure antsdr with a custom IP:

.. code-block:: bash

    sudo screen /dev/ttyUSB8 115200
    ifconfig eth0 10.12.1.201


In case you want to manually load the bootfiles, just use the following commands (as used in the
*board-specific Makefile*):

.. code-block:: bash

    export BOARD_USER="root"
    export BOARD_PASS="analog"
    export BOARD_IP="10.12.1.201"

    sshpass -p $BOARD_PASS ssh -o StrictHostKeyChecking=no $BOARD_USER@$BOARD_IP "mkdir /mnt/data"
    sshpass -p $BOARD_PASS ssh -o StrictHostKeyChecking=no $BOARD_USER@$BOARD_IP "mount /dev/mmcblk0p1 /mnt/data"
    sshpass -p $BOARD_PASS scp ./bootgen/BOOT.bin $BOARD_USER@$BOARD_IP:/mnt/data/BOOT.bin
    sshpass -p $BOARD_PASS ssh -o StrictHostKeyChecking=no $BOARD_USER@$BOARD_IP "sync"
    sshpass -p $BOARD_PASS ssh -o StrictHostKeyChecking=no $BOARD_USER@$BOARD_IP "umount /mnt/data"
