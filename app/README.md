# Preparation

Set the board IP, the frequency offset and the RX gain for the UE:

```
export BOARD_IP="10.12.1.201"
export FREQ_OFFSET="-4100"
export RX_GAIN=50
```

Execute the initialization script. It will download srsRAN repository, will compile it and it will modify
the default srsenb, srsue configuration with the previous parameters.
The default command builds rf driver based on libIIO library, it is suitable for zcu102, plutoSDR and antSDR.

```
./prepare.sh
```

In case you want to build rf driver for the ZCU111 based board, you need to pass an extra parameter as follows
```
./prepare.sh zcu111
```
Depending on your environment setup, the script will compile software for your host computer or cross-compile for aarch64 (namely for zcu102 and zcu111)

# Execution

There are different examples for each of the supported boards under scripts directory, for example one may run txrx_test for the plutoSDR as follows:

```
cd scripts
./run_txrx_plutosdr.sh
```

# Problems

When antSDR board is rebooted you need to configure the IP using serial device. The serialcom device used to be ttyUSB8 or ttyUSB21 in our case, but it can change.

```
sudo screen /dev/ttyUSB8 115200
```

user: root, pass: analog

```
ifconfig eth0 10.12.1.201
```
