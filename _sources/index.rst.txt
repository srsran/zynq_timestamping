.. SRS Zynq timestamping solution

.. _main:

SRS Zynq timestamping solution
==============================

Introduction to the SRS Zynq timestamping solution
**************************************************

.. image:: _images/libiio_timestamping_overview.png
  :width: 800
  :alt: Overview of the Zynq timestamping solution for AD936x-based systems

.. raw:: html

   <br><br/>

 The Zynq timestamping solution provides a sample-aligning mechanism, which is based in counting
 internal FPGA clock ticks (e.g., in case of reusing ADI's firmware design, the clock-tick counting
 is derived from the clock signal driving the AD936x chipset; i.e., we are thus counting samples).
 Based on this sample-counting, SRS solution has the objective of determining the exact clock cycle
 at which one sample was received from the ADC or needs to hit the DAC interface.

 The solution is targeting a typical SDR implementation in which the transmission and reception of
 I/Q samples is triggered by a call to a software function. SRS solution comprises both RTL code
 and C code and enables exchanging the timestamping information between the CPU and the FPGA
 as meta-data (i.e., appended to each group of samples written to/read from the related DMA engine).
 In more detail, from the software side a function call requests a transmission at time instant
 *N+1ms*, which in the FPGA side will be finally seen as a request to ensure that the samples
 received through the DMA do arrive at the DAC at a certain  timestamp (i.e., clock-count value).
 In the opposite direction, the FPGA will append a timestamp to each DMA packet (i.e., group of samples
 forwarded to the CPU). When using AD936x RF chipsets the interfacing to the ADCs/DACs is based on
 libiio. Then, a libiio wrapper will sit between the high-level software function requesting a
 reception/transmission of samples and the low-level libiio calls to enable the use of timestamps
 (as shown in the above diagram). For ZCU111 a custom FPGA and driver implementation is used.

.. toctree::
   :maxdepth: 1
   :caption: Main page
   :hidden:

   self

.. toctree::
   :maxdepth: 1
   :caption: FPGA projects
   :hidden:

   bitstream/overview
   bitstream/plutosdr_project
   bitstream/antsdr_project
   bitstream/zcu102_project
   bitstream/zcu111_project

.. toctree::
   :maxdepth: 1
   :caption: Application notes
   :hidden:

   app/antsdr
   app/plutosdr
   app/zcu

.. toctree::
   :maxdepth: 1
   :caption: RTL modules
   :hidden:

   rtl_module/adc_dma_packet_controller
   rtl_module/adc_dmac_xlength_sniffer
   rtl_module/adc_fifo_timestamp_enabler
   rtl_module/adc_timestamp_enabler_packetizer
   rtl_module/dac_dmac_xlength_sniffer
   rtl_module/dac_fifo_timestamp_enabler
   rtl_module/dma_depack_channels
   rtl_module/timestamp_unit_lclk_count
   rtl_module/rfdc_adc_data_decim_and_depack
   rtl_module/rfdc_dac_data_interp_and_pack
