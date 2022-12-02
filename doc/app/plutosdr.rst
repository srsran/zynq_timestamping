.. Application Note: Tx-Rx testing with ADALM-PLUTO

.. _plutosdr:

Application Note: Tx-Rx testing with ADALM-PLUTO
================================================

Overview
********

.. image:: images/app_note_plutosdr.png
  :alt: System



Hardware and Software Requirements
**********************************

For this application note, the following hardware and software will be used:

        1. Dell XPS13 with Ubuntu 20.04.4
        2. USRP B200 mini
        3. PlutoSDR with custom SRS bitstream
        4. srsRAN
        5. SRS Zynq timestamping
        6. Analog Devices libiio and libad9361 software libraries
        7. (Optional) Python-3 Numpy and Matplotlib


Prerequisites
*************

1. You need to generate the custom SRS timestamp bitstream and to load it in the board (see the
:ref:`plutosdr_project`
section for more details).

2. Install srsRAN dependencies:

.. code-block:: bash

    sudo apt-get install build-essential cmake libfftw3-dev libmbedtls-dev libboost-program-options-dev libconfig++-dev libsctp-dev


3. Install the libiio software library (see Analog Devices
`libiio wiki <https://wiki.analog.com/resources/tools-software/linux-software/libiio>`_
and
`libad9361-iio repository <https://github.com/analogdevicesinc/libad9361-iio>`_
for the full instructions).

4. (Optional) Install Python Numpy and Matplotlib if you want to be able to get a plot of the
data capture generated as a result of the Tx-Rx test:

.. code-block:: bash

    sudo pip3 install -U pip
    sudo pip3 install numpy matplotlib


Building and configuring srsRAN
*******************************

1. Execute the initialization script. It will compile the srsRAN stack as well as the RF drivers
utilized by the Zynq timestamping solution.

.. code-block:: bash

    git clone https://github.com/srsran/zynq_timestamping.git --recursive
    cd app
    ./prepare.sh

Running
*******

First of all, you need to make sure that the ADALM-PLUTO board is set up to implement a Tx-Rx loopback
(e.g., cable the Tx and Rx ports together).

A customized *txrx* application (aimed at demonstrating the basic capabilities of the Zynq timestamping
solution) has been compiled in the previous step. It will transmit three tones with a separation of
4 ms between them, while generating a capture file signal as well. A script is also provided to execute
it:

.. code-block:: bash

    cd scripts
    ./run_txrx_plutosdr.sh

Note that all the scripts located under '/app/scripts' are meant to help the user understand how to
use the test applications and which parameters need to be provided.

(Optional) You can plot the data capture generated during the execution of the previous script with
the following command:

.. code-block:: bash

    python3 show.py test_txrx_pluto.bin
