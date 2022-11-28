.. Overview of the Zynq timestamping solution

.. _overview:

Overview of the Zynq timestamping solution
==============================================

Introduction
************

An overview of the timestamping FPGA logic for the ADC and DAC chains can be observed below and
comprises the blocks listed below.

The main purpose of the Zynq FPGA timestamping slution is to enable a time-synchronized operation
between the FPGA and the ARM, which ultimately allows the precise timing of the packets to be
transmitted in the UL (i.e., DAC chain).

In the MPSoC case, the current design builds on top of the baseline ADI firmware for the AD936x
chipset. Towards that end, no modifications of the original ADI's HDL code are required; that is,
modifications only take place at system-level (i.e., block instantiation, interconnection and
configuration). As for the ZCU111 implementations, the design directly interfaces the RFdc IP core
and provides a signal adequation stage targeting the typical 4G/5G sampling rates (i.e.,
configurable ADC decimation and DAC interpolation logic). The logic managing the timestamping per
se is common to both architectures.

FPGA block details
******************

Details on the different FPGA blocks comprising the timestamping solution are provided next:

1. **Sample counter** (*timestamp_unit_lclk_count*): this simple block will just count the number
of samples that have passed since the last system reset. It must be noted that in some RF front-end
boards (e.g., FMCOMMS2/3/4) and as for ADI's firmware design, the FPGA receives a clock that is 2
times (1x1 antenna configuration; 4 times for 2x2 antenna configuration) the sampling-rate of the
ADC/DAC chains (which use a shared configuration); an input clock-enable port is used in those cases
to make sure that the sample-counting takes this into account (e.g., for zcu102 with 1x1 configuration
it will only count 1 out of each 2 clocks).

2. **ADC timestamping** (*adc_fifo_timestamp_enabler*): the I/Q samples travelling in the ADC chain
are organized in variable-length packets (e.g., length as requested from the srsRAN software
application when managing the DMA buffers interconnecting the ARM and the FPGA). Moreover, a header
will be added to each DMA packet in order to enable a time-synchronized operation between the FPGA
and the ARM; that is, samples are provided in an ordered manner (packetized, with no losses) and a
timestamp value is associated to the first sample of each packet. The timestamp is a 64-bit integer
value corresponding to the AD936x sample count. In those cases where the ADC chain is only employing
1 out of N clock cycles to forward data to the ARM, the timestamped ADC chain will exploit the
unused clock cycles to transmit the additional timestamp-related (i.e., non user-data) information;
otherwise, an early conversion to the AXI clock domain will take place.

3. **DMA packet size sniffer (ADC chain)** (*adc_dmac_xlength_sniffer*): in AD936x-based systems,
this block will intercept the communication between the PS and ADI's *axi_dmac* ADC-chain instance
with the aim to passively capturing the length of the current DMA packet requested by the ARM
(i.e.,
`x_length register value <https://wiki.analog.com/resources/fpga/docs/axi_dmac>`_
),
to provide the ADC timestamping logic with the required information. Taking into account the
interfacing requirements of the *adc_fifo_timestamp_enabler* block, the captured *x-length*
value will be translated to the destination ADC-clock domain before being forwarded.

4. **DMA packet controller (ADC chain)** (*ADC_DMA_packet_controller*): this block provides the
control functions to ensure a succesful packet-level forwarding of ADC data to the DMA. Besides
the sniffed DMA packet-length information, the block takes into account the status of the DMA FIFO
before starting the forwarding of a new packet.

5. **DMA packet size sniffer (DAC chain)** (*dac_dmac_xlength_sniffer*): in AD936x-based systems,
this block will intercept the communication between the PS and ADI's *axi_dmac* DAC-chain instance
with the aim to passively capturing the length of the current DMA packet requested by the ARM
(i.e.,
`x_length register value <https://wiki.analog.com/resources/fpga/docs/axi_dmac>`_
), to provide the DAC timestamping logic with the required information. In this case, no
clock-domain conversion will be required and the captured *x-length* parameter will be
forwarded at the origin AXI-clock.

6. **DAC timestamping** (*dac_fifo_timestamp_enabler*): the I/Q samples received from the ARM to be
forwarded to the DAC chip will be organized in variable-length packets (e.g., length as required
by the srsRAN software application when managing the DMA buffers interconnecting the ARM and the
FPGA). Moreover, each packet will include a header enabling a time-synchronized operation between
the FPGA and the ARM: that's it, samples are provided in an ordered manner (packetized, with no
losses) and a timestamp value is associated to the first sample of each packet. The timestamp is
a 64-bit integer value corresponding to the time at which the ARM expects the first sample of the
packet to enter the DAC chain (i.e., offset with respect to actual sample count or current FPGA
time). To this end, the block includes a circular buffer, providing intermediate storage of the
I/Q samples, and a control FSM that inspects the timestamp and manages the actual forwarding of the
stored data to the chip according to the ARM requirements. Hence, the following situations can take
place:
  - **Early** case: the samples have arrived to the FPGA before its transmission time. They will
    remain stored in the circular buffer until its transmission time.
  - **Late** case: the samples have arrived to the FPGA after its transmission time. The number of
    required samples will be discarded in order to realign the I/Q samples to the current FPGA time.
  - **Idle** case: when there is no actual I/Q samples to be transmitted then 0s will be forwarded
    to the DAC.
  - **Transmit now** case: when the **64-bit timestamp value received from the ARM is 0** then the
    I/Q samples will be directly forwarded to the DAC chip, thus effectively bypassing the FSM control.

Whereas typically the provision of samples to the DAC chip would operate at the DAC-clock domain
(e.g., in the original ADI firmware), the timestamped DAC chain will force the operation of the
timestamping logic and that interconnected to it (e.g., ADI DAC-related blocks) at the DMA-clock
domain (i.e., AXI clock). This design decision enables the creation of time-gaps during the provision
of DAC samples which is necessary to properly manage the *late* situations. This block interfaces
the DAC DMA engine in order to enable the inspection of the ARM-inserted timestamp and controls the
timed IQ-data forwarding to the DAC.

7. **DAC-chain AXI-based control-signal generation** (*dac_control_s_axi_aclk*): in AD936x-based
systems, taking into account the requirement of the customized timestamped DAC-chain to enable
operation of all DAC-related blocks at the DMA-clock domain (i.e., AXI/ARM-derived clock), this
block will generate the requried DAC enable/valid signals to get rid of the original 1/N timing
(*although not necessary, for those cases where the ratio between the FPGA-received clock and the
sampling one is 1 this block can be safely used*).

8. **Configurable decimation block** (*rfdc_adc_data_decim_and_depack*): in RFSoC-based systems,
this block will implement a configurable decimation between x8 (from *245.76 MHz* to *30.72 MHz*
[20 MHz BW in 4G/5G systems]) and x128 (from *245.76 MHz* to *1.92 MHz* [1.4 MHz BW in 4G
systems]). Moreover, the block will handle the unpacking of the I/Q data samples coming from the
RFdc IP core in order to provide a dedicated 16-bit output I and Q signals. Finally, an MMCM is
utilized by this block to generate the required baseband sampling frequencies, which will then be
shared with the DAC chain; nevertheless, it needs to be accounted that MMCMs can only synthesize
frequencies down to 6.25 MHz and, hence, a BUFGCE_DIV primitive will also be used to generate those
clocks below this value. Moreover, the locked status signal of the MMCM will be used as clock-enable
input to the BUFGCE_DIV in order to avoid forwarding a non-stable baseband clock.

  .. image:: images/rfdc_decim_IP_design.png
    :width: 800
    :alt: Configurable ADC-path decimation logic interfacing the RFdc IP core.

9. **Configurable interpolation block** (*rfdc_dac_data_interp_and_pack*): in RFSoC-based systems,
this block will implement a configurable interpolation between x8 (from *30.72 MHz* [20 MHz BW in
4G/5G systems] to *245.76 MHz*) and x128 (from *1.92 MHz* [1.4 MHz BW in 4G systems] to *245.76 MHz*);
additionally a final low-pass filter can be used to provide a cleaner signal to the DAC. Moreover,
the block will handle the packing of the dedicated 16-bit input I and Q signals as required by the
RFdc IP core.

  .. image:: images/rfdc_interp_IP_design.png
    :width: 800
    :alt: Configurable DAC-path interpolation logic interfacing the RFdc IP core.

Timestamping DMA-packet headers
-------------------------------

Essential control information is exchanged between the FPGA and the ARM through metadata inserted
in the headers of the DMA packets travelling the ADC and DAC channels. Specifically, each DMA packet
generated by the timestamping logic in the ADC and DAC chains will be preceded by a 8-sample header
organized as follows (e.g., if a 1000-sample buffer is created for the ADC DMA channel, then each DMA
packet will contain 992 I/Q samples and a 8-sample header):

1. **1st 32-bit word:** known PS-FPGA synchronization word *0xbbbbaaaa*.
2. **2nd 32-bit word:** known PS-FPGA synchronization word *0xddddcccc*.
3. **3rd 32-bit word:** known PS-FPGA synchronization word *0xffffeeee*.
4. **4th 32-bit word:** known PS-FPGA synchronization word *0xabcddcba*.
5. **5th 32-bit word:** known PS-FPGA synchronization word *0xfedccdef*.
6. **6th 32-bit word:** known PS-FPGA synchronization word *0xdfcbaefd*.
7. **7th 32-bit word:** 32 LSBs of the timestamp associated with the current DMA packet; lower 32-bits of the 64-bit timestamp value providing: i) information forwarded to the ARM regarding the current time at the FPGA for the first sample in the DMA packet for the ADC case (i.e., number of ADC samples since the last reset) or ii) the requested time of transmission for the first sample of the packet for the DAC case (i.e., offset with respect to the current FPGA time when the ARM expects the samples to arrive at the DAC chip).
8. **8th 32-bit word:** 32 MSBs of the timestamp associated with the current DMA packet; upper 32-bits of the 64-bit timestamp value from/to the ARM processor.

User-definable parameters
-------------------------

A set of parameters can be defined/modified by the user (setting the desired value either by manually instantiating
the IPs in the HDL top-level file or through the related Vivado GUI in the block design view) and are essential to the
correct behavior of the timestamping solution:

1 . **ADC timestamping** (*adc_fifo_timestamp_enabler*): the following parameters are available
  - **PARAM_DMA_LENGTH_WIDTH** *[integer; default '24']* defines the width in bits of the transfer length control register in
    `High-Speed DMA Controller Peripheral <https://wiki.analog.com/resources/fpga/docs/axi_dmac>`_
    ; this value effectively limits the maximum length of the transfers to 2^DMA_LENGTH_WIDTH (e.g., 2^24 = 16M). It is
    essential that this value coincides with the one in *adc_dmac_xlength_sniffer* and *ADC_DMA_packet_controller*.
  - **BYPASS** *[boolean; default 'false']* indicates whether the block is bypassed or not. It is included to facilitate the implementation of an (functionally-wise) unmodified version of the baseline ADI firmware
  - **PARAM_TWO_ANTENNA_SUPPORT** *[boolean; default 'false']* defines wether *adc_X_2/3* ports are active or not.
  - **PARAM_x1_FPGA_SAMPLING_RATIO** *[boolean; default 'false']* efines whether the baseband FPGA clock (ADCxN_clk) has an actual x1 ratio to the sampling clock or not. **IMPORTANT:** both pluto and antsdr require this parameter to be set to true, whereas zcu102 needs it to be set to false.

2 . **DMA packet size sniffer (ADC chain)** (*adc_dmac_xlength_sniffer*): the following parameters are available
  - **DMA_LENGTH_WIDTH** *[integer; default '24']* defines the width in bits of the transfer length control register in
  `High-Speed DMA Controller Peripheral <https://wiki.analog.com/resources/fpga/docs/axi_dmac>`_
  ; this value effectively limits the maximum length of the transfers to 2^DMA_LENGTH_WIDTH (e.g., 2^24 = 16M). It is
  essential that this value coincides with the one in *adc_fifo_timestamp_enabler* and *ADC_DMA_packet_controller*

3 . **ADC DMA packet forwarding controller** (*ADC_DMA_packet_controller*): the following parameters are available
  **DMA_LENGTH_WIDTH** *[integer; default '24']* defines the width in bits of the transfer length control register in
  `High-Speed DMA Controller Peripheral <https://wiki.analog.com/resources/fpga/docs/axi_dmac>`_
  ; this value effectively limits the maximum length of the transfers to 2^DMA_LENGTH_WIDTH (e.g., 2^24 = 16M). It is
  essential that this value coincides with the one in *adc_fifo_timestamp_enabler* and *adc_dmac_xlength_sniffer*.

4 . **DAC timestamping** (*dac_fifo_timestamp_enabler*): the following parameters are available
  - **DMA_LENGTH_WIDTH** *[integer; default '24']* defines the width in bits of the transfer length control register in
  `High-Speed DMA Controller Peripheral <https://wiki.analog.com/resources/fpga/docs/axi_dmac>`_
  ; this value effectively limits the maximum length of the transfers to 2^DMA_LENGTH_WIDTH (e.g., 2^24 = 16M). It is
  essential that this value coincides with the one in *dac_dmac_xlength_sniffer*.
  - **BYPASS** *[boolean; default 'false']* indicates whether the block is bypassed or not. It is included to facilitate the implementation of an (functionally-wise) unmodified version of the baseline ADI firmware
  - **BUFFER_LENGTH** *[integer; default '8']* defines the length of the internal circular buffer. The length is defined in number of subframes *@1.92 MHz* (i.e., number of 2048x32 memories that will be implemented); the *valid values are in the range [4..10]* (i.e., values below 4 will still generate 4 memories and values above 10 will still generate 10 memories).
  - **MAX_DMA_PACKET_LENGTH** *[integer; default '16000']* defines the maximum supported DMA packet length, which can be stored in a single element of the circular buffer (i.e. it effectively defines the number of RAM blocks implemented per each element of the circular buffer).
  - **PARAM_DMA_LENGTH_IN_HEADER** *[boolean; default 'false']* indicates whether the DMA packet size is received as part of a packet header itself ('true') or received on the input ports ('false'). **IMPORTANT:** must be disabled when ADI DMA engines are used (in this repo, this applies to all supported boards).
  - **PARAM_TWO_ANTENNA_SUPPORT** *[boolean; default 'false']* indicates whether the design has to be synthesized to support two transmit antennas or not.
  - **PARAM_x1_FPGA_SAMPLING_RATIO** *[boolean; default 'false']* defines whether the baseband FPGA clock (DACxN_clk) has an actual x1 ratio to the sampling clock or not. *NOTE: both pluto and antsdr require this parameter to be set to true, whereas zcu102 needs it to be set to false.*
  - **PARAM_MEM_TYPE** *[string; default 'ramb36e2]* indicates the memory instantiation type; supported types *ramb36e2* (Ultrascale), *ramb36e1* (7 series).

5 . **DMA packet size sniffer (DAC chain)** (*dac_dmac_xlength_sniffer*): the following parameters are available
  - **DMA_LENGTH_WIDTH** *[integer; default '24']* defines the width in bits of the transfer length control register in
    `High-Speed DMA Controller Peripheral <https://wiki.analog.com/resources/fpga/docs/axi_dmac>`_
    ; this value effectively limits the maximum length of the transfers to 2^DMA_LENGTH_WIDTH (e.g., 2^24 = 16M). It is
    essential that this value coincides with the one in *dac_fifo_timestamp_enabler*.

6 . **Configurable decimation block** (*rfdc_adc_data_decim_and_depack*): the following parameters are available

7 . **Configurable interpolation block** (*rfdc_dac_data_interp_and_pack*): the following parameters are available
  - **PARAM_OUT_CLEANING_FIR** *[boolean; default 'false']* indicates if a low-pass filter is added at the end to clean the output signal ('true') or not ('false').
