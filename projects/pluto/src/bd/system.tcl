
################################################################
# This is a generated script based on design: system
#
# Though there are limitations about the generated script,
# the main purpose of this utility is to make learning
# IP Integrator Tcl commands easier.
################################################################

namespace eval _tcl {
proc get_script_folder {} {
   set script_path [file normalize [info script]]
   set script_folder [file dirname $script_path]
   return $script_folder
}
}
variable script_folder
set script_folder [_tcl::get_script_folder]

################################################################
# Check if script is running in correct Vivado version.
################################################################
set scripts_vivado_version 2019.2
set current_vivado_version [version -short]

if { [string first $scripts_vivado_version $current_vivado_version] == -1 } {
   puts ""
   catch {common::send_msg_id "BD_TCL-109" "ERROR" "This script was generated using Vivado <$scripts_vivado_version> and is being run in <$current_vivado_version> of Vivado. Please run the script in Vivado <$scripts_vivado_version> then open the design in Vivado <$current_vivado_version>. Upgrade the design by running \"Tools => Report => Report IP Status...\", then run write_bd_tcl to create an updated script."}

   return 1
}

################################################################
# START
################################################################

# To test this script, run the following commands from Vivado Tcl console:
# source system_script.tcl

# If there is no project opened, this script will create a
# project, but make sure you do not have an existing project
# <./myproj/project_1.xpr> in the current working folder.

set list_projs [get_projects -quiet]
if { $list_projs eq "" } {
   create_project project_1 myproj -part xc7z010clg225-1
}


# CHANGE DESIGN NAME HERE
variable design_name
set design_name system

# If you do not already have an existing IP Integrator design open,
# you can create a design using the following command:
#    create_bd_design $design_name

# Creating design if needed
set errMsg ""
set nRet 0

set cur_design [current_bd_design -quiet]
set list_cells [get_bd_cells -quiet]

if { ${design_name} eq "" } {
   # USE CASES:
   #    1) Design_name not set

   set errMsg "Please set the variable <design_name> to a non-empty value."
   set nRet 1

} elseif { ${cur_design} ne "" && ${list_cells} eq "" } {
   # USE CASES:
   #    2): Current design opened AND is empty AND names same.
   #    3): Current design opened AND is empty AND names diff; design_name NOT in project.
   #    4): Current design opened AND is empty AND names diff; design_name exists in project.

   if { $cur_design ne $design_name } {
      common::send_msg_id "BD_TCL-001" "INFO" "Changing value of <design_name> from <$design_name> to <$cur_design> since current design is empty."
      set design_name [get_property NAME $cur_design]
   }
   common::send_msg_id "BD_TCL-002" "INFO" "Constructing design in IPI design <$cur_design>..."

} elseif { ${cur_design} ne "" && $list_cells ne "" && $cur_design eq $design_name } {
   # USE CASES:
   #    5) Current design opened AND has components AND same names.

   set errMsg "Design <$design_name> already exists in your project, please set the variable <design_name> to another value."
   set nRet 1
} elseif { [get_files -quiet ${design_name}.bd] ne "" } {
   # USE CASES: 
   #    6) Current opened design, has components, but diff names, design_name exists in project.
   #    7) No opened design, design_name exists in project.

   set errMsg "Design <$design_name> already exists in your project, please set the variable <design_name> to another value."
   set nRet 2

} else {
   # USE CASES:
   #    8) No opened design, design_name not in project.
   #    9) Current opened design, has components, but diff names, design_name not in project.

   common::send_msg_id "BD_TCL-003" "INFO" "Currently there is no design <$design_name> in project, so creating one..."

   create_bd_design $design_name

   common::send_msg_id "BD_TCL-004" "INFO" "Making design <$design_name> as current_bd_design."
   current_bd_design $design_name

}

common::send_msg_id "BD_TCL-005" "INFO" "Currently the variable <design_name> is equal to \"$design_name\"."

if { $nRet != 0 } {
   catch {common::send_msg_id "BD_TCL-114" "ERROR" $errMsg}
   return $nRet
}

set bCheckIPsPassed 1
##################################################################
# CHECK IPs
##################################################################
set bCheckIPs 1
if { $bCheckIPs == 1 } {
   set list_check_ips "\ 
xilinx.com:ip:xlconstant:1.1\
softwareradiosystems.com:user:adc_dma_packet_controller:1.0\
softwareradiosystems.com:user:adc_dmac_xlength_sniffer:1.0\
softwareradiosystems.com:user:adc_fifo_timestamp_enabler:1.0\
analog.com:user:axi_ad9361:1.0\
analog.com:user:axi_dmac:1.0\
xilinx.com:ip:axi_iic:2.0\
xilinx.com:ip:axi_quad_spi:3.2\
analog.com:user:util_cpack2:1.0\
softwareradiosystems.com:user:dac_control_s_axi_aclk:1.0\
softwareradiosystems.com:user:dac_dmac_xlength_sniffer:1.0\
softwareradiosystems.com:user:dac_fifo_timestamp_enabler:1.0\
xilinx.com:ip:util_vector_logic:2.0\
xilinx.com:ip:proc_sys_reset:5.0\
xilinx.com:ip:xlconcat:2.1\
xilinx.com:ip:processing_system7:5.5\
softwareradiosystems.com:user:timestamp_unit_lclk_count:1.0\
analog.com:user:util_upack2:1.0\
"

   set list_ips_missing ""
   common::send_msg_id "BD_TCL-006" "INFO" "Checking if the following IPs exist in the project's IP catalog: $list_check_ips ."

   foreach ip_vlnv $list_check_ips {
      set ip_obj [get_ipdefs -all $ip_vlnv]
      if { $ip_obj eq "" } {
         lappend list_ips_missing $ip_vlnv
      }
   }

   if { $list_ips_missing ne "" } {
      catch {common::send_msg_id "BD_TCL-115" "ERROR" "The following IPs are not found in the IP Catalog:\n  $list_ips_missing\n\nResolution: Please add the repository containing the IP(s) to the project." }
      set bCheckIPsPassed 0
   }

}

if { $bCheckIPsPassed != 1 } {
  common::send_msg_id "BD_TCL-1003" "WARNING" "Will not continue with creation of design due to the error(s) above."
  return 3
}

##################################################################
# DESIGN PROCs
##################################################################



# Procedure to create entire design; Provide argument to make
# procedure reusable. If parentCell is "", will use root.
proc create_root_design { parentCell } {

  variable script_folder
  variable design_name

  if { $parentCell eq "" } {
     set parentCell [get_bd_cells /]
  }

  # Get object for parentCell
  set parentObj [get_bd_cells $parentCell]
  if { $parentObj == "" } {
     catch {common::send_msg_id "BD_TCL-100" "ERROR" "Unable to find parent cell <$parentCell>!"}
     return
  }

  # Make sure parentObj is hier blk
  set parentType [get_property TYPE $parentObj]
  if { $parentType ne "hier" } {
     catch {common::send_msg_id "BD_TCL-101" "ERROR" "Parent <$parentObj> has TYPE = <$parentType>. Expected to be <hier>."}
     return
  }

  # Save current instance; Restore later
  set oldCurInst [current_bd_instance .]

  # Set parent object as current
  current_bd_instance $parentObj


  # Create interface ports
  set ddr [ create_bd_intf_port -mode Master -vlnv xilinx.com:interface:ddrx_rtl:1.0 ddr ]

  set fixed_io [ create_bd_intf_port -mode Master -vlnv xilinx.com:display_processing_system7:fixedio_rtl:1.0 fixed_io ]

  set iic_main [ create_bd_intf_port -mode Master -vlnv xilinx.com:interface:iic_rtl:1.0 iic_main ]


  # Create ports
  set enable [ create_bd_port -dir O enable ]
  set gpio_i [ create_bd_port -dir I -from 16 -to 0 gpio_i ]
  set gpio_o [ create_bd_port -dir O -from 16 -to 0 gpio_o ]
  set gpio_t [ create_bd_port -dir O -from 16 -to 0 gpio_t ]
  set rx_clk_in [ create_bd_port -dir I rx_clk_in ]
  set rx_data_in [ create_bd_port -dir I -from 11 -to 0 rx_data_in ]
  set rx_frame_in [ create_bd_port -dir I rx_frame_in ]
  set spi0_clk_i [ create_bd_port -dir I spi0_clk_i ]
  set spi0_clk_o [ create_bd_port -dir O spi0_clk_o ]
  set spi0_csn_0_o [ create_bd_port -dir O spi0_csn_0_o ]
  set spi0_csn_1_o [ create_bd_port -dir O spi0_csn_1_o ]
  set spi0_csn_2_o [ create_bd_port -dir O spi0_csn_2_o ]
  set spi0_csn_i [ create_bd_port -dir I spi0_csn_i ]
  set spi0_sdi_i [ create_bd_port -dir I spi0_sdi_i ]
  set spi0_sdo_i [ create_bd_port -dir I spi0_sdo_i ]
  set spi0_sdo_o [ create_bd_port -dir O spi0_sdo_o ]
  set spi_clk_i [ create_bd_port -dir I spi_clk_i ]
  set spi_clk_o [ create_bd_port -dir O spi_clk_o ]
  set spi_csn_i [ create_bd_port -dir I spi_csn_i ]
  set spi_csn_o [ create_bd_port -dir O -from 0 -to 0 spi_csn_o ]
  set spi_sdi_i [ create_bd_port -dir I spi_sdi_i ]
  set spi_sdo_i [ create_bd_port -dir I spi_sdo_i ]
  set spi_sdo_o [ create_bd_port -dir O spi_sdo_o ]
  set tx_clk_out [ create_bd_port -dir O tx_clk_out ]
  set tx_data_out [ create_bd_port -dir O -from 11 -to 0 tx_data_out ]
  set tx_frame_out [ create_bd_port -dir O tx_frame_out ]
  set txnrx [ create_bd_port -dir O txnrx ]
  set up_enable [ create_bd_port -dir I up_enable ]
  set up_txnrx [ create_bd_port -dir I up_txnrx ]

  # Create instance: GND_1, and set properties
  set GND_1 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlconstant:1.1 GND_1 ]
  set_property -dict [ list \
   CONFIG.CONST_VAL {0} \
   CONFIG.CONST_WIDTH {1} \
 ] $GND_1

  # Create instance: RF_configuration_control, and set properties
  set RF_configuration_control [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlconstant:1.1 RF_configuration_control ]

  # Create instance: adc_dma_packet_contr_0, and set properties
  set adc_dma_packet_contr_0 [ create_bd_cell -type ip -vlnv softwareradiosystems.com:user:adc_dma_packet_controller:1.0 adc_dma_packet_contr_0 ]
  set_property -dict [ list \
   CONFIG.DMA_LENGTH_WIDTH {24} \
 ] $adc_dma_packet_contr_0

  # Create instance: adc_dmac_xlength_sni_0, and set properties
  set adc_dmac_xlength_sni_0 [ create_bd_cell -type ip -vlnv softwareradiosystems.com:user:adc_dmac_xlength_sniffer:1.0 adc_dmac_xlength_sni_0 ]
  set_property -dict [ list \
   CONFIG.DMA_LENGTH_WIDTH {24} \
 ] $adc_dmac_xlength_sni_0

  # Create instance: adc_fifo_timestamp_e_1, and set properties
  set adc_fifo_timestamp_e_1 [ create_bd_cell -type ip -vlnv softwareradiosystems.com:user:adc_fifo_timestamp_enabler:1.0 adc_fifo_timestamp_e_1 ]
  set_property -dict [ list \
   CONFIG.PARAM_x1_FPGA_SAMPLING_RATIO {true} \
 ] $adc_fifo_timestamp_e_1

  # Create instance: axi_ad9361, and set properties
  set axi_ad9361 [ create_bd_cell -type ip -vlnv analog.com:user:axi_ad9361:1.0 axi_ad9361 ]
  set_property -dict [ list \
   CONFIG.ADC_INIT_DELAY {21} \
   CONFIG.CMOS_OR_LVDS_N {1} \
   CONFIG.ID {0} \
   CONFIG.MODE_1R1T {1} \
 ] $axi_ad9361

  # Create instance: axi_ad9361_adc_dma, and set properties
  set axi_ad9361_adc_dma [ create_bd_cell -type ip -vlnv analog.com:user:axi_dmac:1.0 axi_ad9361_adc_dma ]
  set_property -dict [ list \
   CONFIG.AXI_SLICE_DEST {false} \
   CONFIG.AXI_SLICE_SRC {false} \
   CONFIG.CYCLIC {true} \
   CONFIG.DMA_2D_TRANSFER {false} \
   CONFIG.DMA_DATA_WIDTH_SRC {64} \
   CONFIG.DMA_TYPE_DEST {0} \
   CONFIG.DMA_TYPE_SRC {2} \
   CONFIG.FIFO_SIZE {32} \
   CONFIG.MAX_BYTES_PER_BURST {512} \
   CONFIG.SYNC_TRANSFER_START {true} \
 ] $axi_ad9361_adc_dma

  # Create instance: axi_ad9361_dac_dma, and set properties
  set axi_ad9361_dac_dma [ create_bd_cell -type ip -vlnv analog.com:user:axi_dmac:1.0 axi_ad9361_dac_dma ]
  set_property -dict [ list \
   CONFIG.AXI_SLICE_DEST {false} \
   CONFIG.AXI_SLICE_SRC {false} \
   CONFIG.CYCLIC {true} \
   CONFIG.DMA_2D_TRANSFER {false} \
   CONFIG.DMA_DATA_WIDTH_DEST {64} \
   CONFIG.DMA_TYPE_DEST {1} \
   CONFIG.DMA_TYPE_SRC {0} \
 ] $axi_ad9361_dac_dma

  # Create instance: axi_cpu_interconnect, and set properties
  set axi_cpu_interconnect [ create_bd_cell -type ip -vlnv xilinx.com:ip:axi_interconnect:2.1 axi_cpu_interconnect ]
  set_property -dict [ list \
   CONFIG.NUM_MI {5} \
 ] $axi_cpu_interconnect

  # Create instance: axi_iic_main, and set properties
  set axi_iic_main [ create_bd_cell -type ip -vlnv xilinx.com:ip:axi_iic:2.0 axi_iic_main ]

  # Create instance: axi_spi, and set properties
  set axi_spi [ create_bd_cell -type ip -vlnv xilinx.com:ip:axi_quad_spi:3.2 axi_spi ]
  set_property -dict [ list \
   CONFIG.C_NUM_SS_BITS {1} \
   CONFIG.C_SCK_RATIO {8} \
   CONFIG.C_USE_STARTUP {0} \
 ] $axi_spi

  # Create instance: cpack, and set properties
  set cpack [ create_bd_cell -type ip -vlnv analog.com:user:util_cpack2:1.0 cpack ]
  set_property -dict [ list \
   CONFIG.NUM_OF_CHANNELS {4} \
 ] $cpack

  # Create instance: dac_control_s_axi_ac_0, and set properties
  set dac_control_s_axi_ac_0 [ create_bd_cell -type ip -vlnv softwareradiosystems.com:user:dac_control_s_axi_aclk:1.0 dac_control_s_axi_ac_0 ]

  # Create instance: dac_dmac_xlength_sni_0, and set properties
  set dac_dmac_xlength_sni_0 [ create_bd_cell -type ip -vlnv softwareradiosystems.com:user:dac_dmac_xlength_sniffer:1.0 dac_dmac_xlength_sni_0 ]
  set_property -dict [ list \
   CONFIG.DMA_LENGTH_WIDTH {24} \
 ] $dac_dmac_xlength_sni_0

  # Create instance: dac_dmac_xlength_sni_1, and set properties
  set dac_dmac_xlength_sni_1 [ create_bd_cell -type ip -vlnv softwareradiosystems.com:user:dac_dmac_xlength_sniffer:1.0 dac_dmac_xlength_sni_1 ]

  # Create instance: dac_fifo_timestamp_e_0, and set properties
  set dac_fifo_timestamp_e_0 [ create_bd_cell -type ip -vlnv softwareradiosystems.com:user:dac_fifo_timestamp_enabler:1.0 dac_fifo_timestamp_e_0 ]
  set_property -dict [ list \
   CONFIG.PARAM_BUFFER_LENGTH {4} \
   CONFIG.PARAM_BYPASS {false} \
   CONFIG.PARAM_DMA_LENGTH_WIDTH {24} \
   CONFIG.PARAM_MAX_DMA_PACKET_LENGTH {2000} \
   CONFIG.PARAM_MEM_TYPE {ramb36e1} \
   CONFIG.PARAM_x1_FPGA_SAMPLING_RATIO {true} \
 ] $dac_fifo_timestamp_e_0

  # Create instance: logic_or, and set properties
  set logic_or [ create_bd_cell -type ip -vlnv xilinx.com:ip:util_vector_logic:2.0 logic_or ]
  set_property -dict [ list \
   CONFIG.C_OPERATION {or} \
   CONFIG.C_SIZE {1} \
 ] $logic_or

  # Create instance: proc_sys_reset_0, and set properties
  set proc_sys_reset_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:proc_sys_reset:5.0 proc_sys_reset_0 ]

  # Create instance: sys_concat_intc, and set properties
  set sys_concat_intc [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlconcat:2.1 sys_concat_intc ]
  set_property -dict [ list \
   CONFIG.NUM_PORTS {16} \
 ] $sys_concat_intc

  # Create instance: sys_ps7, and set properties
  set sys_ps7 [ create_bd_cell -type ip -vlnv xilinx.com:ip:processing_system7:5.5 sys_ps7 ]
  set_property -dict [ list \
   CONFIG.PCW_ACT_APU_PERIPHERAL_FREQMHZ {666.666687} \
   CONFIG.PCW_ACT_CAN_PERIPHERAL_FREQMHZ {10.000000} \
   CONFIG.PCW_ACT_DCI_PERIPHERAL_FREQMHZ {10.158730} \
   CONFIG.PCW_ACT_ENET0_PERIPHERAL_FREQMHZ {10.000000} \
   CONFIG.PCW_ACT_ENET1_PERIPHERAL_FREQMHZ {10.000000} \
   CONFIG.PCW_ACT_FPGA0_PERIPHERAL_FREQMHZ {100.000000} \
   CONFIG.PCW_ACT_FPGA1_PERIPHERAL_FREQMHZ {200.000000} \
   CONFIG.PCW_ACT_FPGA2_PERIPHERAL_FREQMHZ {10.000000} \
   CONFIG.PCW_ACT_FPGA3_PERIPHERAL_FREQMHZ {10.000000} \
   CONFIG.PCW_ACT_PCAP_PERIPHERAL_FREQMHZ {200.000000} \
   CONFIG.PCW_ACT_QSPI_PERIPHERAL_FREQMHZ {200.000000} \
   CONFIG.PCW_ACT_SDIO_PERIPHERAL_FREQMHZ {10.000000} \
   CONFIG.PCW_ACT_SMC_PERIPHERAL_FREQMHZ {10.000000} \
   CONFIG.PCW_ACT_SPI_PERIPHERAL_FREQMHZ {166.666672} \
   CONFIG.PCW_ACT_TPIU_PERIPHERAL_FREQMHZ {200.000000} \
   CONFIG.PCW_ACT_TTC0_CLK0_PERIPHERAL_FREQMHZ {111.111115} \
   CONFIG.PCW_ACT_TTC0_CLK1_PERIPHERAL_FREQMHZ {111.111115} \
   CONFIG.PCW_ACT_TTC0_CLK2_PERIPHERAL_FREQMHZ {111.111115} \
   CONFIG.PCW_ACT_TTC1_CLK0_PERIPHERAL_FREQMHZ {111.111115} \
   CONFIG.PCW_ACT_TTC1_CLK1_PERIPHERAL_FREQMHZ {111.111115} \
   CONFIG.PCW_ACT_TTC1_CLK2_PERIPHERAL_FREQMHZ {111.111115} \
   CONFIG.PCW_ACT_UART_PERIPHERAL_FREQMHZ {100.000000} \
   CONFIG.PCW_ACT_WDT_PERIPHERAL_FREQMHZ {111.111115} \
   CONFIG.PCW_ARMPLL_CTRL_FBDIV {40} \
   CONFIG.PCW_CAN_PERIPHERAL_DIVISOR0 {1} \
   CONFIG.PCW_CAN_PERIPHERAL_DIVISOR1 {1} \
   CONFIG.PCW_CLK0_FREQ {100000000} \
   CONFIG.PCW_CLK1_FREQ {200000000} \
   CONFIG.PCW_CLK2_FREQ {10000000} \
   CONFIG.PCW_CLK3_FREQ {10000000} \
   CONFIG.PCW_CPU_CPU_PLL_FREQMHZ {1333.333} \
   CONFIG.PCW_CPU_PERIPHERAL_DIVISOR0 {2} \
   CONFIG.PCW_DCI_PERIPHERAL_DIVISOR0 {15} \
   CONFIG.PCW_DCI_PERIPHERAL_DIVISOR1 {7} \
   CONFIG.PCW_DDRPLL_CTRL_FBDIV {32} \
   CONFIG.PCW_DDR_DDR_PLL_FREQMHZ {1066.667} \
   CONFIG.PCW_DDR_PERIPHERAL_DIVISOR0 {2} \
   CONFIG.PCW_DDR_RAM_HIGHADDR {0x1FFFFFFF} \
   CONFIG.PCW_DM_WIDTH {2} \
   CONFIG.PCW_DQS_WIDTH {2} \
   CONFIG.PCW_DQ_WIDTH {16} \
   CONFIG.PCW_ENET0_PERIPHERAL_DIVISOR0 {1} \
   CONFIG.PCW_ENET0_PERIPHERAL_DIVISOR1 {1} \
   CONFIG.PCW_ENET0_RESET_ENABLE {0} \
   CONFIG.PCW_ENET1_PERIPHERAL_DIVISOR0 {1} \
   CONFIG.PCW_ENET1_PERIPHERAL_DIVISOR1 {1} \
   CONFIG.PCW_ENET1_RESET_ENABLE {0} \
   CONFIG.PCW_ENET_RESET_ENABLE {0} \
   CONFIG.PCW_EN_CLK1_PORT {1} \
   CONFIG.PCW_EN_EMIO_GPIO {1} \
   CONFIG.PCW_EN_EMIO_SPI0 {1} \
   CONFIG.PCW_EN_GPIO {1} \
   CONFIG.PCW_EN_QSPI {1} \
   CONFIG.PCW_EN_RST1_PORT {1} \
   CONFIG.PCW_EN_SPI0 {1} \
   CONFIG.PCW_EN_UART1 {1} \
   CONFIG.PCW_EN_USB0 {1} \
   CONFIG.PCW_FCLK0_PERIPHERAL_DIVISOR0 {5} \
   CONFIG.PCW_FCLK0_PERIPHERAL_DIVISOR1 {2} \
   CONFIG.PCW_FCLK1_PERIPHERAL_DIVISOR0 {5} \
   CONFIG.PCW_FCLK1_PERIPHERAL_DIVISOR1 {1} \
   CONFIG.PCW_FCLK2_PERIPHERAL_DIVISOR0 {1} \
   CONFIG.PCW_FCLK2_PERIPHERAL_DIVISOR1 {1} \
   CONFIG.PCW_FCLK3_PERIPHERAL_DIVISOR0 {1} \
   CONFIG.PCW_FCLK3_PERIPHERAL_DIVISOR1 {1} \
   CONFIG.PCW_FCLK_CLK1_BUF {TRUE} \
   CONFIG.PCW_FPGA0_PERIPHERAL_FREQMHZ {100.0} \
   CONFIG.PCW_FPGA1_PERIPHERAL_FREQMHZ {200.0} \
   CONFIG.PCW_FPGA_FCLK0_ENABLE {1} \
   CONFIG.PCW_FPGA_FCLK1_ENABLE {1} \
   CONFIG.PCW_FPGA_FCLK2_ENABLE {0} \
   CONFIG.PCW_FPGA_FCLK3_ENABLE {0} \
   CONFIG.PCW_GPIO_EMIO_GPIO_ENABLE {1} \
   CONFIG.PCW_GPIO_EMIO_GPIO_IO {17} \
   CONFIG.PCW_GPIO_EMIO_GPIO_WIDTH {17} \
   CONFIG.PCW_GPIO_MIO_GPIO_ENABLE {1} \
   CONFIG.PCW_GPIO_MIO_GPIO_IO {MIO} \
   CONFIG.PCW_I2C0_GRP_INT_ENABLE {0} \
   CONFIG.PCW_I2C0_PERIPHERAL_ENABLE {0} \
   CONFIG.PCW_I2C0_RESET_ENABLE {0} \
   CONFIG.PCW_I2C1_GRP_INT_ENABLE {0} \
   CONFIG.PCW_I2C1_PERIPHERAL_ENABLE {0} \
   CONFIG.PCW_I2C1_RESET_ENABLE {0} \
   CONFIG.PCW_I2C_PERIPHERAL_FREQMHZ {25} \
   CONFIG.PCW_I2C_RESET_ENABLE {1} \
   CONFIG.PCW_IOPLL_CTRL_FBDIV {30} \
   CONFIG.PCW_IO_IO_PLL_FREQMHZ {1000.000} \
   CONFIG.PCW_IRQ_F2P_INTR {1} \
   CONFIG.PCW_IRQ_F2P_MODE {REVERSE} \
   CONFIG.PCW_MIO_0_DIRECTION {inout} \
   CONFIG.PCW_MIO_0_IOTYPE {LVCMOS 1.8V} \
   CONFIG.PCW_MIO_0_PULLUP {enabled} \
   CONFIG.PCW_MIO_0_SLEW {slow} \
   CONFIG.PCW_MIO_10_DIRECTION {inout} \
   CONFIG.PCW_MIO_10_IOTYPE {LVCMOS 1.8V} \
   CONFIG.PCW_MIO_10_PULLUP {enabled} \
   CONFIG.PCW_MIO_10_SLEW {slow} \
   CONFIG.PCW_MIO_11_DIRECTION {inout} \
   CONFIG.PCW_MIO_11_IOTYPE {LVCMOS 1.8V} \
   CONFIG.PCW_MIO_11_PULLUP {enabled} \
   CONFIG.PCW_MIO_11_SLEW {slow} \
   CONFIG.PCW_MIO_12_DIRECTION {out} \
   CONFIG.PCW_MIO_12_IOTYPE {LVCMOS 1.8V} \
   CONFIG.PCW_MIO_12_PULLUP {enabled} \
   CONFIG.PCW_MIO_12_SLEW {slow} \
   CONFIG.PCW_MIO_13_DIRECTION {in} \
   CONFIG.PCW_MIO_13_IOTYPE {LVCMOS 1.8V} \
   CONFIG.PCW_MIO_13_PULLUP {enabled} \
   CONFIG.PCW_MIO_13_SLEW {slow} \
   CONFIG.PCW_MIO_14_DIRECTION {inout} \
   CONFIG.PCW_MIO_14_IOTYPE {LVCMOS 1.8V} \
   CONFIG.PCW_MIO_14_PULLUP {enabled} \
   CONFIG.PCW_MIO_14_SLEW {slow} \
   CONFIG.PCW_MIO_15_DIRECTION {inout} \
   CONFIG.PCW_MIO_15_IOTYPE {LVCMOS 1.8V} \
   CONFIG.PCW_MIO_15_PULLUP {enabled} \
   CONFIG.PCW_MIO_15_SLEW {slow} \
   CONFIG.PCW_MIO_1_DIRECTION {out} \
   CONFIG.PCW_MIO_1_IOTYPE {LVCMOS 1.8V} \
   CONFIG.PCW_MIO_1_PULLUP {enabled} \
   CONFIG.PCW_MIO_1_SLEW {slow} \
   CONFIG.PCW_MIO_28_DIRECTION {inout} \
   CONFIG.PCW_MIO_28_IOTYPE {LVCMOS 1.8V} \
   CONFIG.PCW_MIO_28_PULLUP {enabled} \
   CONFIG.PCW_MIO_28_SLEW {slow} \
   CONFIG.PCW_MIO_29_DIRECTION {in} \
   CONFIG.PCW_MIO_29_IOTYPE {LVCMOS 1.8V} \
   CONFIG.PCW_MIO_29_PULLUP {enabled} \
   CONFIG.PCW_MIO_29_SLEW {slow} \
   CONFIG.PCW_MIO_2_DIRECTION {inout} \
   CONFIG.PCW_MIO_2_IOTYPE {LVCMOS 1.8V} \
   CONFIG.PCW_MIO_2_PULLUP {disabled} \
   CONFIG.PCW_MIO_2_SLEW {slow} \
   CONFIG.PCW_MIO_30_DIRECTION {out} \
   CONFIG.PCW_MIO_30_IOTYPE {LVCMOS 1.8V} \
   CONFIG.PCW_MIO_30_PULLUP {enabled} \
   CONFIG.PCW_MIO_30_SLEW {slow} \
   CONFIG.PCW_MIO_31_DIRECTION {in} \
   CONFIG.PCW_MIO_31_IOTYPE {LVCMOS 1.8V} \
   CONFIG.PCW_MIO_31_PULLUP {enabled} \
   CONFIG.PCW_MIO_31_SLEW {slow} \
   CONFIG.PCW_MIO_32_DIRECTION {inout} \
   CONFIG.PCW_MIO_32_IOTYPE {LVCMOS 1.8V} \
   CONFIG.PCW_MIO_32_PULLUP {enabled} \
   CONFIG.PCW_MIO_32_SLEW {slow} \
   CONFIG.PCW_MIO_33_DIRECTION {inout} \
   CONFIG.PCW_MIO_33_IOTYPE {LVCMOS 1.8V} \
   CONFIG.PCW_MIO_33_PULLUP {enabled} \
   CONFIG.PCW_MIO_33_SLEW {slow} \
   CONFIG.PCW_MIO_34_DIRECTION {inout} \
   CONFIG.PCW_MIO_34_IOTYPE {LVCMOS 1.8V} \
   CONFIG.PCW_MIO_34_PULLUP {enabled} \
   CONFIG.PCW_MIO_34_SLEW {slow} \
   CONFIG.PCW_MIO_35_DIRECTION {inout} \
   CONFIG.PCW_MIO_35_IOTYPE {LVCMOS 1.8V} \
   CONFIG.PCW_MIO_35_PULLUP {enabled} \
   CONFIG.PCW_MIO_35_SLEW {slow} \
   CONFIG.PCW_MIO_36_DIRECTION {in} \
   CONFIG.PCW_MIO_36_IOTYPE {LVCMOS 1.8V} \
   CONFIG.PCW_MIO_36_PULLUP {enabled} \
   CONFIG.PCW_MIO_36_SLEW {slow} \
   CONFIG.PCW_MIO_37_DIRECTION {inout} \
   CONFIG.PCW_MIO_37_IOTYPE {LVCMOS 1.8V} \
   CONFIG.PCW_MIO_37_PULLUP {enabled} \
   CONFIG.PCW_MIO_37_SLEW {slow} \
   CONFIG.PCW_MIO_38_DIRECTION {inout} \
   CONFIG.PCW_MIO_38_IOTYPE {LVCMOS 1.8V} \
   CONFIG.PCW_MIO_38_PULLUP {enabled} \
   CONFIG.PCW_MIO_38_SLEW {slow} \
   CONFIG.PCW_MIO_39_DIRECTION {inout} \
   CONFIG.PCW_MIO_39_IOTYPE {LVCMOS 1.8V} \
   CONFIG.PCW_MIO_39_PULLUP {enabled} \
   CONFIG.PCW_MIO_39_SLEW {slow} \
   CONFIG.PCW_MIO_3_DIRECTION {inout} \
   CONFIG.PCW_MIO_3_IOTYPE {LVCMOS 1.8V} \
   CONFIG.PCW_MIO_3_PULLUP {disabled} \
   CONFIG.PCW_MIO_3_SLEW {slow} \
   CONFIG.PCW_MIO_48_DIRECTION {inout} \
   CONFIG.PCW_MIO_48_IOTYPE {LVCMOS 1.8V} \
   CONFIG.PCW_MIO_48_PULLUP {enabled} \
   CONFIG.PCW_MIO_48_SLEW {slow} \
   CONFIG.PCW_MIO_49_DIRECTION {inout} \
   CONFIG.PCW_MIO_49_IOTYPE {LVCMOS 1.8V} \
   CONFIG.PCW_MIO_49_PULLUP {disabled} \
   CONFIG.PCW_MIO_49_SLEW {slow} \
   CONFIG.PCW_MIO_4_DIRECTION {inout} \
   CONFIG.PCW_MIO_4_IOTYPE {LVCMOS 1.8V} \
   CONFIG.PCW_MIO_4_PULLUP {disabled} \
   CONFIG.PCW_MIO_4_SLEW {slow} \
   CONFIG.PCW_MIO_52_DIRECTION {out} \
   CONFIG.PCW_MIO_52_IOTYPE {LVCMOS 1.8V} \
   CONFIG.PCW_MIO_52_PULLUP {enabled} \
   CONFIG.PCW_MIO_52_SLEW {slow} \
   CONFIG.PCW_MIO_53_DIRECTION {inout} \
   CONFIG.PCW_MIO_53_IOTYPE {LVCMOS 1.8V} \
   CONFIG.PCW_MIO_53_PULLUP {enabled} \
   CONFIG.PCW_MIO_53_SLEW {slow} \
   CONFIG.PCW_MIO_5_DIRECTION {inout} \
   CONFIG.PCW_MIO_5_IOTYPE {LVCMOS 1.8V} \
   CONFIG.PCW_MIO_5_PULLUP {disabled} \
   CONFIG.PCW_MIO_5_SLEW {slow} \
   CONFIG.PCW_MIO_6_DIRECTION {out} \
   CONFIG.PCW_MIO_6_IOTYPE {LVCMOS 1.8V} \
   CONFIG.PCW_MIO_6_PULLUP {disabled} \
   CONFIG.PCW_MIO_6_SLEW {slow} \
   CONFIG.PCW_MIO_7_DIRECTION {out} \
   CONFIG.PCW_MIO_7_IOTYPE {LVCMOS 1.8V} \
   CONFIG.PCW_MIO_7_PULLUP {disabled} \
   CONFIG.PCW_MIO_7_SLEW {slow} \
   CONFIG.PCW_MIO_8_DIRECTION {out} \
   CONFIG.PCW_MIO_8_IOTYPE {LVCMOS 1.8V} \
   CONFIG.PCW_MIO_8_PULLUP {disabled} \
   CONFIG.PCW_MIO_8_SLEW {slow} \
   CONFIG.PCW_MIO_9_DIRECTION {inout} \
   CONFIG.PCW_MIO_9_IOTYPE {LVCMOS 1.8V} \
   CONFIG.PCW_MIO_9_PULLUP {enabled} \
   CONFIG.PCW_MIO_9_SLEW {slow} \
   CONFIG.PCW_MIO_PRIMITIVE {32} \
   CONFIG.PCW_MIO_TREE_PERIPHERALS {GPIO#Quad SPI Flash#Quad SPI Flash#Quad SPI Flash#Quad SPI Flash#Quad SPI Flash#Quad SPI Flash#GPIO#GPIO#GPIO#GPIO#GPIO#UART 1#UART 1#GPIO#GPIO#Unbonded#Unbonded#Unbonded#Unbonded#Unbonded#Unbonded#Unbonded#Unbonded#Unbonded#Unbonded#Unbonded#Unbonded#USB 0#USB 0#USB 0#USB 0#USB 0#USB 0#USB 0#USB 0#USB 0#USB 0#USB 0#USB 0#Unbonded#Unbonded#Unbonded#Unbonded#Unbonded#Unbonded#Unbonded#Unbonded#GPIO#GPIO#Unbonded#Unbonded#USB Reset#GPIO} \
   CONFIG.PCW_MIO_TREE_SIGNALS {gpio[0]#qspi0_ss_b#qspi0_io[0]#qspi0_io[1]#qspi0_io[2]#qspi0_io[3]/HOLD_B#qspi0_sclk#gpio[7]#gpio[8]#gpio[9]#gpio[10]#gpio[11]#tx#rx#gpio[14]#gpio[15]#Unbonded#Unbonded#Unbonded#Unbonded#Unbonded#Unbonded#Unbonded#Unbonded#Unbonded#Unbonded#Unbonded#Unbonded#data[4]#dir#stp#nxt#data[0]#data[1]#data[2]#data[3]#clk#data[5]#data[6]#data[7]#Unbonded#Unbonded#Unbonded#Unbonded#Unbonded#Unbonded#Unbonded#Unbonded#gpio[48]#gpio[49]#Unbonded#Unbonded#reset#gpio[53]} \
   CONFIG.PCW_NAND_GRP_D8_ENABLE {0} \
   CONFIG.PCW_NAND_PERIPHERAL_ENABLE {0} \
   CONFIG.PCW_NOR_GRP_A25_ENABLE {0} \
   CONFIG.PCW_NOR_GRP_CS0_ENABLE {0} \
   CONFIG.PCW_NOR_GRP_CS1_ENABLE {0} \
   CONFIG.PCW_NOR_GRP_SRAM_CS0_ENABLE {0} \
   CONFIG.PCW_NOR_GRP_SRAM_CS1_ENABLE {0} \
   CONFIG.PCW_NOR_GRP_SRAM_INT_ENABLE {0} \
   CONFIG.PCW_NOR_PERIPHERAL_ENABLE {0} \
   CONFIG.PCW_PACKAGE_NAME {clg225} \
   CONFIG.PCW_PCAP_PERIPHERAL_DIVISOR0 {5} \
   CONFIG.PCW_PRESET_BANK0_VOLTAGE {LVCMOS 1.8V} \
   CONFIG.PCW_PRESET_BANK1_VOLTAGE {LVCMOS 1.8V} \
   CONFIG.PCW_QSPI_GRP_FBCLK_ENABLE {0} \
   CONFIG.PCW_QSPI_GRP_IO1_ENABLE {0} \
   CONFIG.PCW_QSPI_GRP_SINGLE_SS_ENABLE {1} \
   CONFIG.PCW_QSPI_GRP_SINGLE_SS_IO {MIO 1 .. 6} \
   CONFIG.PCW_QSPI_GRP_SS1_ENABLE {0} \
   CONFIG.PCW_QSPI_PERIPHERAL_DIVISOR0 {5} \
   CONFIG.PCW_QSPI_PERIPHERAL_ENABLE {1} \
   CONFIG.PCW_QSPI_PERIPHERAL_FREQMHZ {200} \
   CONFIG.PCW_QSPI_QSPI_IO {MIO 1 .. 6} \
   CONFIG.PCW_SD0_GRP_CD_ENABLE {0} \
   CONFIG.PCW_SD0_GRP_POW_ENABLE {0} \
   CONFIG.PCW_SD0_GRP_WP_ENABLE {0} \
   CONFIG.PCW_SD0_PERIPHERAL_ENABLE {0} \
   CONFIG.PCW_SDIO_PERIPHERAL_DIVISOR0 {1} \
   CONFIG.PCW_SINGLE_QSPI_DATA_MODE {x4} \
   CONFIG.PCW_SMC_PERIPHERAL_DIVISOR0 {1} \
   CONFIG.PCW_SPI0_GRP_SS0_ENABLE {1} \
   CONFIG.PCW_SPI0_GRP_SS0_IO {EMIO} \
   CONFIG.PCW_SPI0_GRP_SS1_ENABLE {1} \
   CONFIG.PCW_SPI0_GRP_SS1_IO {EMIO} \
   CONFIG.PCW_SPI0_GRP_SS2_ENABLE {1} \
   CONFIG.PCW_SPI0_GRP_SS2_IO {EMIO} \
   CONFIG.PCW_SPI0_PERIPHERAL_ENABLE {1} \
   CONFIG.PCW_SPI0_SPI0_IO {EMIO} \
   CONFIG.PCW_SPI1_GRP_SS0_ENABLE {0} \
   CONFIG.PCW_SPI1_GRP_SS1_ENABLE {0} \
   CONFIG.PCW_SPI1_GRP_SS2_ENABLE {0} \
   CONFIG.PCW_SPI1_PERIPHERAL_ENABLE {0} \
   CONFIG.PCW_SPI_PERIPHERAL_DIVISOR0 {6} \
   CONFIG.PCW_SPI_PERIPHERAL_FREQMHZ {166.666666} \
   CONFIG.PCW_SPI_PERIPHERAL_VALID {1} \
   CONFIG.PCW_TPIU_PERIPHERAL_DIVISOR0 {1} \
   CONFIG.PCW_TTC0_CLK0_PERIPHERAL_FREQMHZ {133.333333} \
   CONFIG.PCW_TTC0_CLK1_PERIPHERAL_FREQMHZ {133.333333} \
   CONFIG.PCW_TTC0_CLK2_PERIPHERAL_FREQMHZ {133.333333} \
   CONFIG.PCW_TTC0_PERIPHERAL_ENABLE {0} \
   CONFIG.PCW_UART1_GRP_FULL_ENABLE {0} \
   CONFIG.PCW_UART1_PERIPHERAL_ENABLE {1} \
   CONFIG.PCW_UART1_UART1_IO {MIO 12 .. 13} \
   CONFIG.PCW_UART_PERIPHERAL_DIVISOR0 {10} \
   CONFIG.PCW_UART_PERIPHERAL_FREQMHZ {100} \
   CONFIG.PCW_UART_PERIPHERAL_VALID {1} \
   CONFIG.PCW_UIPARAM_ACT_DDR_FREQ_MHZ {533.333374} \
   CONFIG.PCW_UIPARAM_DDR_BANK_ADDR_COUNT {3} \
   CONFIG.PCW_UIPARAM_DDR_BOARD_DELAY0 {0.241} \
   CONFIG.PCW_UIPARAM_DDR_BOARD_DELAY1 {0.240} \
   CONFIG.PCW_UIPARAM_DDR_BUS_WIDTH {16 Bit} \
   CONFIG.PCW_UIPARAM_DDR_CL {7} \
   CONFIG.PCW_UIPARAM_DDR_COL_ADDR_COUNT {10} \
   CONFIG.PCW_UIPARAM_DDR_CWL {6} \
   CONFIG.PCW_UIPARAM_DDR_DEVICE_CAPACITY {4096 MBits} \
   CONFIG.PCW_UIPARAM_DDR_DQS_TO_CLK_DELAY_0 {0.048} \
   CONFIG.PCW_UIPARAM_DDR_DQS_TO_CLK_DELAY_1 {0.050} \
   CONFIG.PCW_UIPARAM_DDR_DRAM_WIDTH {16 Bits} \
   CONFIG.PCW_UIPARAM_DDR_ECC {Disabled} \
   CONFIG.PCW_UIPARAM_DDR_PARTNO {MT41K256M16 RE-125} \
   CONFIG.PCW_UIPARAM_DDR_ROW_ADDR_COUNT {15} \
   CONFIG.PCW_UIPARAM_DDR_SPEED_BIN {DDR3_1066F} \
   CONFIG.PCW_UIPARAM_DDR_TRAIN_DATA_EYE {1} \
   CONFIG.PCW_UIPARAM_DDR_TRAIN_READ_GATE {1} \
   CONFIG.PCW_UIPARAM_DDR_TRAIN_WRITE_LEVEL {1} \
   CONFIG.PCW_UIPARAM_DDR_T_FAW {40.0} \
   CONFIG.PCW_UIPARAM_DDR_T_RAS_MIN {35.0} \
   CONFIG.PCW_UIPARAM_DDR_T_RC {48.75} \
   CONFIG.PCW_UIPARAM_DDR_T_RCD {7} \
   CONFIG.PCW_UIPARAM_DDR_T_RP {7} \
   CONFIG.PCW_UIPARAM_DDR_USE_INTERNAL_VREF {0} \
   CONFIG.PCW_USB0_PERIPHERAL_ENABLE {1} \
   CONFIG.PCW_USB0_PERIPHERAL_FREQMHZ {60} \
   CONFIG.PCW_USB0_RESET_ENABLE {1} \
   CONFIG.PCW_USB0_RESET_IO {MIO 52} \
   CONFIG.PCW_USB0_USB0_IO {MIO 28 .. 39} \
   CONFIG.PCW_USB1_RESET_ENABLE {0} \
   CONFIG.PCW_USB_RESET_ENABLE {1} \
   CONFIG.PCW_USB_RESET_SELECT {Share reset pin} \
   CONFIG.PCW_USE_FABRIC_INTERRUPT {1} \
   CONFIG.PCW_USE_S_AXI_HP1 {1} \
   CONFIG.PCW_USE_S_AXI_HP2 {1} \
 ] $sys_ps7

  # Create instance: sys_rstgen, and set properties
  set sys_rstgen [ create_bd_cell -type ip -vlnv xilinx.com:ip:proc_sys_reset:5.0 sys_rstgen ]
  set_property -dict [ list \
   CONFIG.C_EXT_RST_WIDTH {1} \
 ] $sys_rstgen

  # Create instance: timestamp_unit_lclk_0, and set properties
  set timestamp_unit_lclk_0 [ create_bd_cell -type ip -vlnv softwareradiosystems.com:user:timestamp_unit_lclk_count:1.0 timestamp_unit_lclk_0 ]
  set_property -dict [ list \
   CONFIG.PARAM_CLOCK_RATIO {1} \
 ] $timestamp_unit_lclk_0

  # Create instance: tx_upack, and set properties
  set tx_upack [ create_bd_cell -type ip -vlnv analog.com:user:util_upack2:1.0 tx_upack ]

  # Create instance: util_vector_logic_0, and set properties
  set util_vector_logic_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:util_vector_logic:2.0 util_vector_logic_0 ]
  set_property -dict [ list \
   CONFIG.C_OPERATION {not} \
   CONFIG.C_SIZE {1} \
   CONFIG.LOGO_FILE {data/sym_notgate.png} \
 ] $util_vector_logic_0

  # Create interface connections
  connect_bd_intf_net -intf_net S00_AXI_1 [get_bd_intf_pins axi_cpu_interconnect/S00_AXI] [get_bd_intf_pins sys_ps7/M_AXI_GP0]
  connect_bd_intf_net -intf_net axi_ad9361_adc_dma_m_dest_axi [get_bd_intf_pins axi_ad9361_adc_dma/m_dest_axi] [get_bd_intf_pins sys_ps7/S_AXI_HP1]
  connect_bd_intf_net -intf_net axi_ad9361_dac_dma_m_axis [get_bd_intf_pins axi_ad9361_dac_dma/m_axis] [get_bd_intf_pins tx_upack/s_axis]
  connect_bd_intf_net -intf_net axi_ad9361_dac_dma_m_src_axi [get_bd_intf_pins axi_ad9361_dac_dma/m_src_axi] [get_bd_intf_pins sys_ps7/S_AXI_HP2]
  connect_bd_intf_net -intf_net axi_cpu_interconnect_M00_AXI [get_bd_intf_pins axi_cpu_interconnect/M00_AXI] [get_bd_intf_pins axi_iic_main/S_AXI]
  connect_bd_intf_net -intf_net axi_cpu_interconnect_M01_AXI [get_bd_intf_pins axi_ad9361/s_axi] [get_bd_intf_pins axi_cpu_interconnect/M01_AXI]
  connect_bd_intf_net -intf_net axi_cpu_interconnect_M02_AXI [get_bd_intf_pins axi_ad9361_adc_dma/s_axi] [get_bd_intf_pins axi_cpu_interconnect/M02_AXI]
  connect_bd_intf_net -intf_net axi_cpu_interconnect_M03_AXI [get_bd_intf_pins axi_ad9361_dac_dma/s_axi] [get_bd_intf_pins axi_cpu_interconnect/M03_AXI]
  connect_bd_intf_net -intf_net axi_cpu_interconnect_M04_AXI [get_bd_intf_pins axi_cpu_interconnect/M04_AXI] [get_bd_intf_pins axi_spi/AXI_LITE]
  connect_bd_intf_net -intf_net axi_iic_main_IIC [get_bd_intf_ports iic_main] [get_bd_intf_pins axi_iic_main/IIC]
  connect_bd_intf_net -intf_net sys_ps7_DDR [get_bd_intf_ports ddr] [get_bd_intf_pins sys_ps7/DDR]
  connect_bd_intf_net -intf_net sys_ps7_FIXED_IO [get_bd_intf_ports fixed_io] [get_bd_intf_pins sys_ps7/FIXED_IO]

  # Create port connections
  connect_bd_net -net GND_1_dout [get_bd_pins GND_1/dout] [get_bd_pins axi_ad9361/tdd_sync] [get_bd_pins sys_concat_intc/In0] [get_bd_pins sys_concat_intc/In1] [get_bd_pins sys_concat_intc/In2] [get_bd_pins sys_concat_intc/In3] [get_bd_pins sys_concat_intc/In4] [get_bd_pins sys_concat_intc/In5] [get_bd_pins sys_concat_intc/In6] [get_bd_pins sys_concat_intc/In7] [get_bd_pins sys_concat_intc/In8] [get_bd_pins sys_concat_intc/In9] [get_bd_pins sys_concat_intc/In10] [get_bd_pins sys_concat_intc/In14]
  connect_bd_net -net M02_AXI_arready_1 [get_bd_pins axi_ad9361_adc_dma/s_axi_arready] [get_bd_pins axi_cpu_interconnect/M02_AXI_arready]
  connect_bd_net -net M02_AXI_awready_1 [get_bd_pins adc_dmac_xlength_sni_0/fwd_s_axi_awready] [get_bd_pins axi_ad9361_adc_dma/s_axi_awready] [get_bd_pins axi_cpu_interconnect/M02_AXI_awready]
  connect_bd_net -net M02_AXI_bresp_1 [get_bd_pins axi_ad9361_adc_dma/s_axi_bresp] [get_bd_pins axi_cpu_interconnect/M02_AXI_bresp]
  connect_bd_net -net M02_AXI_bvalid_1 [get_bd_pins adc_dmac_xlength_sni_0/fwd_s_axi_bvalid] [get_bd_pins axi_ad9361_adc_dma/s_axi_bvalid] [get_bd_pins axi_cpu_interconnect/M02_AXI_bvalid]
  connect_bd_net -net M02_AXI_rdata_1 [get_bd_pins axi_ad9361_adc_dma/s_axi_rdata] [get_bd_pins axi_cpu_interconnect/M02_AXI_rdata]
  connect_bd_net -net M02_AXI_rresp_1 [get_bd_pins axi_ad9361_adc_dma/s_axi_rresp] [get_bd_pins axi_cpu_interconnect/M02_AXI_rresp]
  connect_bd_net -net M02_AXI_rvalid_1 [get_bd_pins axi_ad9361_adc_dma/s_axi_rvalid] [get_bd_pins axi_cpu_interconnect/M02_AXI_rvalid]
  connect_bd_net -net M02_AXI_wready_1 [get_bd_pins adc_dmac_xlength_sni_0/fwd_s_axi_wready] [get_bd_pins axi_ad9361_adc_dma/s_axi_wready] [get_bd_pins axi_cpu_interconnect/M02_AXI_wready]
  connect_bd_net -net M03_AXI_arready_1 [get_bd_pins axi_ad9361_dac_dma/s_axi_arready] [get_bd_pins axi_cpu_interconnect/M03_AXI_arready]
  connect_bd_net -net M03_AXI_awready_1 [get_bd_pins axi_ad9361_dac_dma/s_axi_awready] [get_bd_pins axi_cpu_interconnect/M03_AXI_awready] [get_bd_pins dac_dmac_xlength_sni_0/fwd_s_axi_awready] [get_bd_pins dac_dmac_xlength_sni_1/fwd_s_axi_awready]
  connect_bd_net -net M03_AXI_bresp_1 [get_bd_pins axi_ad9361_dac_dma/s_axi_bresp] [get_bd_pins axi_cpu_interconnect/M03_AXI_bresp]
  connect_bd_net -net M03_AXI_bvalid_1 [get_bd_pins axi_ad9361_dac_dma/s_axi_bvalid] [get_bd_pins axi_cpu_interconnect/M03_AXI_bvalid] [get_bd_pins dac_dmac_xlength_sni_0/fwd_s_axi_bvalid] [get_bd_pins dac_dmac_xlength_sni_1/fwd_s_axi_bvalid]
  connect_bd_net -net M03_AXI_rdata_1 [get_bd_pins axi_ad9361_dac_dma/s_axi_rdata] [get_bd_pins axi_cpu_interconnect/M03_AXI_rdata]
  connect_bd_net -net M03_AXI_rresp_1 [get_bd_pins axi_ad9361_dac_dma/s_axi_rresp] [get_bd_pins axi_cpu_interconnect/M03_AXI_rresp]
  connect_bd_net -net M03_AXI_rvalid_1 [get_bd_pins axi_ad9361_dac_dma/s_axi_rvalid] [get_bd_pins axi_cpu_interconnect/M03_AXI_rvalid]
  connect_bd_net -net M03_AXI_wready_1 [get_bd_pins axi_ad9361_dac_dma/s_axi_wready] [get_bd_pins axi_cpu_interconnect/M03_AXI_wready] [get_bd_pins dac_dmac_xlength_sni_0/fwd_s_axi_wready] [get_bd_pins dac_dmac_xlength_sni_1/fwd_s_axi_wready]
  connect_bd_net -net RF_configuration_control_dout [get_bd_pins RF_configuration_control/dout] [get_bd_pins adc_fifo_timestamp_e_1/ADC_clk_division] [get_bd_pins dac_control_s_axi_ac_0/DAC_clk_division] [get_bd_pins dac_fifo_timestamp_e_0/DAC_clk_division] [get_bd_pins timestamp_unit_lclk_0/ADC_clk_division]
  connect_bd_net -net adc_dma_packet_contr_0_fwd_adc_data_0 [get_bd_pins adc_dma_packet_contr_0/fwd_adc_data_0] [get_bd_pins cpack/fifo_wr_data_0]
  connect_bd_net -net adc_dma_packet_contr_0_fwd_adc_data_1 [get_bd_pins adc_dma_packet_contr_0/fwd_adc_data_1] [get_bd_pins cpack/fifo_wr_data_1]
  connect_bd_net -net adc_dma_packet_contr_0_fwd_adc_data_2 [get_bd_pins adc_dma_packet_contr_0/fwd_adc_data_2] [get_bd_pins cpack/fifo_wr_data_2]
  connect_bd_net -net adc_dma_packet_contr_0_fwd_adc_data_3 [get_bd_pins adc_dma_packet_contr_0/fwd_adc_data_3] [get_bd_pins cpack/fifo_wr_data_3]
  connect_bd_net -net adc_dma_packet_contr_0_fwd_adc_enable_0 [get_bd_pins adc_dma_packet_contr_0/fwd_adc_enable_0] [get_bd_pins cpack/enable_0]
  connect_bd_net -net adc_dma_packet_contr_0_fwd_adc_enable_1 [get_bd_pins adc_dma_packet_contr_0/fwd_adc_enable_1] [get_bd_pins cpack/enable_1]
  connect_bd_net -net adc_dma_packet_contr_0_fwd_adc_enable_2 [get_bd_pins adc_dma_packet_contr_0/fwd_adc_enable_2] [get_bd_pins cpack/enable_2]
  connect_bd_net -net adc_dma_packet_contr_0_fwd_adc_enable_3 [get_bd_pins adc_dma_packet_contr_0/fwd_adc_enable_3] [get_bd_pins cpack/enable_3]
  connect_bd_net -net adc_dma_packet_contr_0_fwd_adc_valid_0 [get_bd_pins adc_dma_packet_contr_0/fwd_adc_valid_0] [get_bd_pins cpack/fifo_wr_en]
  connect_bd_net -net adc_dmac_xlength_sni_0_DMA_x_length [get_bd_pins adc_dmac_xlength_sni_0/DMA_x_length] [get_bd_pins adc_fifo_timestamp_e_1/DMA_x_length]
  connect_bd_net -net adc_dmac_xlength_sni_0_DMA_x_length_valid [get_bd_pins adc_dmac_xlength_sni_0/DMA_x_length_valid] [get_bd_pins adc_fifo_timestamp_e_1/DMA_x_length_valid]
  connect_bd_net -net adc_fifo_timestamp_e_1_fwd_DMA_x_length [get_bd_pins adc_dma_packet_contr_0/DMA_x_length] [get_bd_pins adc_fifo_timestamp_e_1/fwd_DMA_x_length]
  connect_bd_net -net adc_fifo_timestamp_e_1_fwd_DMA_x_length_valid [get_bd_pins adc_dma_packet_contr_0/DMA_x_length_valid] [get_bd_pins adc_fifo_timestamp_e_1/fwd_DMA_x_length_valid]
  connect_bd_net -net adc_fifo_timestamp_e_1_fwd_adc_data_0 [get_bd_pins adc_dma_packet_contr_0/adc_data_0] [get_bd_pins adc_fifo_timestamp_e_1/fwd_adc_data_0]
  connect_bd_net -net adc_fifo_timestamp_e_1_fwd_adc_data_1 [get_bd_pins adc_dma_packet_contr_0/adc_data_1] [get_bd_pins adc_fifo_timestamp_e_1/fwd_adc_data_1]
  connect_bd_net -net adc_fifo_timestamp_e_1_fwd_adc_data_2 [get_bd_pins adc_dma_packet_contr_0/adc_data_2] [get_bd_pins adc_fifo_timestamp_e_1/fwd_adc_data_2]
  connect_bd_net -net adc_fifo_timestamp_e_1_fwd_adc_data_3 [get_bd_pins adc_dma_packet_contr_0/adc_data_3] [get_bd_pins adc_fifo_timestamp_e_1/fwd_adc_data_3]
  connect_bd_net -net adc_fifo_timestamp_e_1_fwd_adc_enable_0 [get_bd_pins adc_dma_packet_contr_0/adc_enable_0] [get_bd_pins adc_fifo_timestamp_e_1/fwd_adc_enable_0]
  connect_bd_net -net adc_fifo_timestamp_e_1_fwd_adc_enable_1 [get_bd_pins adc_dma_packet_contr_0/adc_enable_1] [get_bd_pins adc_fifo_timestamp_e_1/fwd_adc_enable_1]
  connect_bd_net -net adc_fifo_timestamp_e_1_fwd_adc_enable_2 [get_bd_pins adc_dma_packet_contr_0/adc_enable_2] [get_bd_pins adc_fifo_timestamp_e_1/fwd_adc_enable_2]
  connect_bd_net -net adc_fifo_timestamp_e_1_fwd_adc_enable_3 [get_bd_pins adc_dma_packet_contr_0/adc_enable_3] [get_bd_pins adc_fifo_timestamp_e_1/fwd_adc_enable_3]
  connect_bd_net -net adc_fifo_timestamp_e_1_fwd_adc_overflow_BBclk [get_bd_pins adc_fifo_timestamp_e_1/fwd_adc_overflow_BBclk] [get_bd_pins axi_ad9361/adc_dovf]
  connect_bd_net -net adc_fifo_timestamp_e_1_fwd_adc_valid_0 [get_bd_pins adc_dma_packet_contr_0/adc_valid_0] [get_bd_pins adc_fifo_timestamp_e_1/fwd_adc_valid_0]
  connect_bd_net -net adc_fifo_timestamp_e_1_fwd_adc_valid_1 [get_bd_pins adc_dma_packet_contr_0/adc_valid_1] [get_bd_pins adc_fifo_timestamp_e_1/fwd_adc_valid_1]
  connect_bd_net -net adc_fifo_timestamp_e_1_fwd_adc_valid_2 [get_bd_pins adc_dma_packet_contr_0/adc_valid_2] [get_bd_pins adc_fifo_timestamp_e_1/fwd_adc_valid_2]
  connect_bd_net -net adc_fifo_timestamp_e_1_fwd_adc_valid_3 [get_bd_pins adc_dma_packet_contr_0/adc_valid_3] [get_bd_pins adc_fifo_timestamp_e_1/fwd_adc_valid_3]
  connect_bd_net -net axi_ad9361_adc_data_i0 [get_bd_pins adc_fifo_timestamp_e_1/adc_data_0] [get_bd_pins axi_ad9361/adc_data_i0]
  connect_bd_net -net axi_ad9361_adc_data_i1 [get_bd_pins adc_fifo_timestamp_e_1/adc_data_2] [get_bd_pins axi_ad9361/adc_data_i1]
  connect_bd_net -net axi_ad9361_adc_data_q0 [get_bd_pins adc_fifo_timestamp_e_1/adc_data_1] [get_bd_pins axi_ad9361/adc_data_q0]
  connect_bd_net -net axi_ad9361_adc_data_q1 [get_bd_pins adc_fifo_timestamp_e_1/adc_data_3] [get_bd_pins axi_ad9361/adc_data_q1]
  connect_bd_net -net axi_ad9361_adc_dma_fifo_wr_overflow [get_bd_pins axi_ad9361_adc_dma/fifo_wr_overflow] [get_bd_pins cpack/packed_fifo_wr_overflow]
  connect_bd_net -net axi_ad9361_adc_dma_fifo_wr_xfer_req [get_bd_pins adc_dma_packet_contr_0/fifo_wr_xfer_req] [get_bd_pins axi_ad9361_adc_dma/fifo_wr_xfer_req]
  connect_bd_net -net axi_ad9361_adc_dma_irq [get_bd_pins axi_ad9361_adc_dma/irq] [get_bd_pins sys_concat_intc/In13]
  connect_bd_net -net axi_ad9361_adc_enable_i0 [get_bd_pins adc_fifo_timestamp_e_1/adc_enable_0] [get_bd_pins axi_ad9361/adc_enable_i0]
  connect_bd_net -net axi_ad9361_adc_enable_i1 [get_bd_pins adc_fifo_timestamp_e_1/adc_enable_2] [get_bd_pins axi_ad9361/adc_enable_i1]
  connect_bd_net -net axi_ad9361_adc_enable_q0 [get_bd_pins adc_fifo_timestamp_e_1/adc_enable_1] [get_bd_pins axi_ad9361/adc_enable_q0]
  connect_bd_net -net axi_ad9361_adc_enable_q1 [get_bd_pins adc_fifo_timestamp_e_1/adc_enable_3] [get_bd_pins axi_ad9361/adc_enable_q1]
  connect_bd_net -net axi_ad9361_adc_valid_i0 [get_bd_pins adc_fifo_timestamp_e_1/adc_valid_0] [get_bd_pins axi_ad9361/adc_valid_i0]
  connect_bd_net -net axi_ad9361_adc_valid_i1 [get_bd_pins adc_fifo_timestamp_e_1/adc_valid_2] [get_bd_pins axi_ad9361/adc_valid_i1]
  connect_bd_net -net axi_ad9361_adc_valid_q0 [get_bd_pins adc_fifo_timestamp_e_1/adc_valid_1] [get_bd_pins axi_ad9361/adc_valid_q0]
  connect_bd_net -net axi_ad9361_adc_valid_q1 [get_bd_pins adc_fifo_timestamp_e_1/adc_valid_3] [get_bd_pins axi_ad9361/adc_valid_q1]
  connect_bd_net -net axi_ad9361_dac_dma_irq [get_bd_pins axi_ad9361_dac_dma/irq] [get_bd_pins sys_concat_intc/In12]
  connect_bd_net -net axi_ad9361_dac_enable_i0 [get_bd_pins axi_ad9361/dac_enable_i0] [get_bd_pins dac_control_s_axi_ac_0/dac_enable_0]
  connect_bd_net -net axi_ad9361_dac_enable_i1 [get_bd_pins axi_ad9361/dac_enable_i1] [get_bd_pins dac_control_s_axi_ac_0/dac_enable_2]
  connect_bd_net -net axi_ad9361_dac_enable_q0 [get_bd_pins axi_ad9361/dac_enable_q0] [get_bd_pins dac_control_s_axi_ac_0/dac_enable_1]
  connect_bd_net -net axi_ad9361_dac_enable_q1 [get_bd_pins axi_ad9361/dac_enable_q1] [get_bd_pins dac_control_s_axi_ac_0/dac_enable_3]
  connect_bd_net -net axi_ad9361_enable [get_bd_ports enable] [get_bd_pins axi_ad9361/enable]
  connect_bd_net -net axi_ad9361_l_clk [get_bd_pins adc_dmac_xlength_sni_0/ADCxN_clk] [get_bd_pins adc_fifo_timestamp_e_1/ADCxN_clk] [get_bd_pins axi_ad9361/clk] [get_bd_pins axi_ad9361/l_clk] [get_bd_pins dac_control_s_axi_ac_0/DACxN_clk] [get_bd_pins dac_fifo_timestamp_e_0/DACxN_clk] [get_bd_pins timestamp_unit_lclk_0/ADCxN_clk]
  connect_bd_net -net axi_ad9361_rst [get_bd_pins adc_fifo_timestamp_e_1/ADCxN_reset] [get_bd_pins axi_ad9361/rst] [get_bd_pins dac_fifo_timestamp_e_0/DACxN_reset] [get_bd_pins timestamp_unit_lclk_0/ADCxN_reset]
  connect_bd_net -net axi_ad9361_tx_clk_out [get_bd_ports tx_clk_out] [get_bd_pins axi_ad9361/tx_clk_out]
  connect_bd_net -net axi_ad9361_tx_data_out [get_bd_ports tx_data_out] [get_bd_pins axi_ad9361/tx_data_out]
  connect_bd_net -net axi_ad9361_tx_frame_out [get_bd_ports tx_frame_out] [get_bd_pins axi_ad9361/tx_frame_out]
  connect_bd_net -net axi_ad9361_txnrx [get_bd_ports txnrx] [get_bd_pins axi_ad9361/txnrx]
  connect_bd_net -net axi_cpu_interconnect_M02_AXI_araddr [get_bd_pins axi_ad9361_adc_dma/s_axi_araddr] [get_bd_pins axi_cpu_interconnect/M02_AXI_araddr]
  connect_bd_net -net axi_cpu_interconnect_M02_AXI_arprot [get_bd_pins axi_ad9361_adc_dma/s_axi_arprot] [get_bd_pins axi_cpu_interconnect/M02_AXI_arprot]
  connect_bd_net -net axi_cpu_interconnect_M02_AXI_arvalid [get_bd_pins axi_ad9361_adc_dma/s_axi_arvalid] [get_bd_pins axi_cpu_interconnect/M02_AXI_arvalid]
  connect_bd_net -net axi_cpu_interconnect_M02_AXI_awaddr [get_bd_pins adc_dmac_xlength_sni_0/s_axi_awaddr] [get_bd_pins axi_ad9361_adc_dma/s_axi_awaddr] [get_bd_pins axi_cpu_interconnect/M02_AXI_awaddr]
  connect_bd_net -net axi_cpu_interconnect_M02_AXI_awprot [get_bd_pins axi_ad9361_adc_dma/s_axi_awprot] [get_bd_pins axi_cpu_interconnect/M02_AXI_awprot]
  connect_bd_net -net axi_cpu_interconnect_M02_AXI_awvalid [get_bd_pins adc_dmac_xlength_sni_0/s_axi_awvalid] [get_bd_pins axi_ad9361_adc_dma/s_axi_awvalid] [get_bd_pins axi_cpu_interconnect/M02_AXI_awvalid]
  connect_bd_net -net axi_cpu_interconnect_M02_AXI_bready [get_bd_pins adc_dmac_xlength_sni_0/s_axi_bready] [get_bd_pins axi_ad9361_adc_dma/s_axi_bready] [get_bd_pins axi_cpu_interconnect/M02_AXI_bready]
  connect_bd_net -net axi_cpu_interconnect_M02_AXI_rready [get_bd_pins axi_ad9361_adc_dma/s_axi_rready] [get_bd_pins axi_cpu_interconnect/M02_AXI_rready]
  connect_bd_net -net axi_cpu_interconnect_M02_AXI_wdata [get_bd_pins adc_dmac_xlength_sni_0/s_axi_wdata] [get_bd_pins axi_ad9361_adc_dma/s_axi_wdata] [get_bd_pins axi_cpu_interconnect/M02_AXI_wdata]
  connect_bd_net -net axi_cpu_interconnect_M02_AXI_wstrb [get_bd_pins axi_ad9361_adc_dma/s_axi_wstrb] [get_bd_pins axi_cpu_interconnect/M02_AXI_wstrb]
  connect_bd_net -net axi_cpu_interconnect_M02_AXI_wvalid [get_bd_pins adc_dmac_xlength_sni_0/s_axi_wvalid] [get_bd_pins axi_ad9361_adc_dma/s_axi_wvalid] [get_bd_pins axi_cpu_interconnect/M02_AXI_wvalid]
  connect_bd_net -net axi_cpu_interconnect_M03_AXI_araddr [get_bd_pins axi_ad9361_dac_dma/s_axi_araddr] [get_bd_pins axi_cpu_interconnect/M03_AXI_araddr]
  connect_bd_net -net axi_cpu_interconnect_M03_AXI_arprot [get_bd_pins axi_ad9361_dac_dma/s_axi_arprot] [get_bd_pins axi_cpu_interconnect/M03_AXI_arprot]
  connect_bd_net -net axi_cpu_interconnect_M03_AXI_arvalid [get_bd_pins axi_ad9361_dac_dma/s_axi_arvalid] [get_bd_pins axi_cpu_interconnect/M03_AXI_arvalid]
  connect_bd_net -net axi_cpu_interconnect_M03_AXI_awaddr [get_bd_pins axi_ad9361_dac_dma/s_axi_awaddr] [get_bd_pins axi_cpu_interconnect/M03_AXI_awaddr] [get_bd_pins dac_dmac_xlength_sni_0/s_axi_awaddr] [get_bd_pins dac_dmac_xlength_sni_1/s_axi_awaddr]
  connect_bd_net -net axi_cpu_interconnect_M03_AXI_awprot [get_bd_pins axi_ad9361_dac_dma/s_axi_awprot] [get_bd_pins axi_cpu_interconnect/M03_AXI_awprot]
  connect_bd_net -net axi_cpu_interconnect_M03_AXI_awvalid [get_bd_pins axi_ad9361_dac_dma/s_axi_awvalid] [get_bd_pins axi_cpu_interconnect/M03_AXI_awvalid] [get_bd_pins dac_dmac_xlength_sni_0/s_axi_awvalid] [get_bd_pins dac_dmac_xlength_sni_1/s_axi_awvalid]
  connect_bd_net -net axi_cpu_interconnect_M03_AXI_bready [get_bd_pins axi_ad9361_dac_dma/s_axi_bready] [get_bd_pins axi_cpu_interconnect/M03_AXI_bready] [get_bd_pins dac_dmac_xlength_sni_0/s_axi_bready] [get_bd_pins dac_dmac_xlength_sni_1/s_axi_bready]
  connect_bd_net -net axi_cpu_interconnect_M03_AXI_rready [get_bd_pins axi_ad9361_dac_dma/s_axi_rready] [get_bd_pins axi_cpu_interconnect/M03_AXI_rready]
  connect_bd_net -net axi_cpu_interconnect_M03_AXI_wdata [get_bd_pins axi_ad9361_dac_dma/s_axi_wdata] [get_bd_pins axi_cpu_interconnect/M03_AXI_wdata] [get_bd_pins dac_dmac_xlength_sni_0/s_axi_wdata] [get_bd_pins dac_dmac_xlength_sni_1/s_axi_wdata]
  connect_bd_net -net axi_cpu_interconnect_M03_AXI_wstrb [get_bd_pins axi_ad9361_dac_dma/s_axi_wstrb] [get_bd_pins axi_cpu_interconnect/M03_AXI_wstrb]
  connect_bd_net -net axi_cpu_interconnect_M03_AXI_wvalid [get_bd_pins axi_ad9361_dac_dma/s_axi_wvalid] [get_bd_pins axi_cpu_interconnect/M03_AXI_wvalid] [get_bd_pins dac_dmac_xlength_sni_0/s_axi_wvalid] [get_bd_pins dac_dmac_xlength_sni_1/s_axi_wvalid]
  connect_bd_net -net axi_iic_main_iic2intc_irpt [get_bd_pins axi_iic_main/iic2intc_irpt] [get_bd_pins sys_concat_intc/In15]
  connect_bd_net -net axi_spi_io0_o [get_bd_ports spi_sdo_o] [get_bd_pins axi_spi/io0_o]
  connect_bd_net -net axi_spi_ip2intc_irpt [get_bd_pins axi_spi/ip2intc_irpt] [get_bd_pins sys_concat_intc/In11]
  connect_bd_net -net axi_spi_sck_o [get_bd_ports spi_clk_o] [get_bd_pins axi_spi/sck_o]
  connect_bd_net -net axi_spi_ss_o [get_bd_ports spi_csn_o] [get_bd_pins axi_spi/ss_o]
  connect_bd_net -net cpack_packed_fifo_wr_data [get_bd_pins axi_ad9361_adc_dma/fifo_wr_din] [get_bd_pins cpack/packed_fifo_wr_data]
  connect_bd_net -net cpack_packed_fifo_wr_en [get_bd_pins axi_ad9361_adc_dma/fifo_wr_en] [get_bd_pins cpack/packed_fifo_wr_en]
  connect_bd_net -net cpack_packed_fifo_wr_sync [get_bd_pins axi_ad9361_adc_dma/fifo_wr_sync] [get_bd_pins cpack/packed_fifo_wr_sync]
  connect_bd_net -net dac_control_s_axi_ac_0_s_axi_dac_enable_0 [get_bd_pins dac_control_s_axi_ac_0/s_axi_dac_enable_0] [get_bd_pins dac_fifo_timestamp_e_0/dac_enable_0] [get_bd_pins tx_upack/enable_0]
  connect_bd_net -net dac_control_s_axi_ac_0_s_axi_dac_enable_1 [get_bd_pins dac_control_s_axi_ac_0/s_axi_dac_enable_1] [get_bd_pins dac_fifo_timestamp_e_0/dac_enable_1] [get_bd_pins tx_upack/enable_1]
  connect_bd_net -net dac_control_s_axi_ac_0_s_axi_dac_enable_2 [get_bd_pins dac_control_s_axi_ac_0/s_axi_dac_enable_2] [get_bd_pins dac_fifo_timestamp_e_0/dac_enable_2] [get_bd_pins tx_upack/enable_2]
  connect_bd_net -net dac_control_s_axi_ac_0_s_axi_dac_enable_3 [get_bd_pins dac_control_s_axi_ac_0/s_axi_dac_enable_3] [get_bd_pins dac_fifo_timestamp_e_0/dac_enable_3] [get_bd_pins tx_upack/enable_3]
  connect_bd_net -net dac_control_s_axi_ac_0_s_axi_dac_valid_0 [get_bd_pins dac_control_s_axi_ac_0/s_axi_dac_valid_0] [get_bd_pins logic_or/Op1]
  connect_bd_net -net dac_control_s_axi_ac_0_s_axi_dac_valid_1 [get_bd_pins dac_control_s_axi_ac_0/s_axi_dac_valid_1] [get_bd_pins logic_or/Op2]
  connect_bd_net -net dac_dmac_xlength_sni_1_DMA_x_length [get_bd_pins dac_dmac_xlength_sni_1/DMA_x_length] [get_bd_pins dac_fifo_timestamp_e_0/DMA_x_length]
  connect_bd_net -net dac_dmac_xlength_sni_1_DMA_x_length_valid [get_bd_pins dac_dmac_xlength_sni_1/DMA_x_length_valid] [get_bd_pins dac_fifo_timestamp_e_0/DMA_x_length_valid]
  connect_bd_net -net dac_fifo_timestamp_e_0_fwd_dac_data_0 [get_bd_pins axi_ad9361/dac_data_i0] [get_bd_pins dac_fifo_timestamp_e_0/fwd_dac_data_0]
  connect_bd_net -net dac_fifo_timestamp_e_0_fwd_dac_data_1 [get_bd_pins axi_ad9361/dac_data_q0] [get_bd_pins dac_fifo_timestamp_e_0/fwd_dac_data_1]
  connect_bd_net -net dac_fifo_timestamp_e_0_fwd_dac_data_2 [get_bd_pins axi_ad9361/dac_data_i1] [get_bd_pins dac_fifo_timestamp_e_0/fwd_dac_data_2]
  connect_bd_net -net dac_fifo_timestamp_e_0_fwd_dac_data_3 [get_bd_pins axi_ad9361/dac_data_q1] [get_bd_pins dac_fifo_timestamp_e_0/fwd_dac_data_3]
  connect_bd_net -net dac_fifo_timestamp_e_0_fwd_dac_fifo_unf [get_bd_pins axi_ad9361/dac_dunf] [get_bd_pins dac_fifo_timestamp_e_0/fwd_dac_fifo_unf]
  connect_bd_net -net gpio_i_1 [get_bd_ports gpio_i] [get_bd_pins sys_ps7/GPIO_I]
  connect_bd_net -net logic_or_Res [get_bd_pins logic_or/Res] [get_bd_pins tx_upack/fifo_rd_en]
  connect_bd_net -net proc_sys_reset_0_peripheral_aresetn [get_bd_pins adc_dmac_xlength_sni_0/s_axi_aresetn] [get_bd_pins adc_fifo_timestamp_e_1/s_axi_aresetn] [get_bd_pins dac_control_s_axi_ac_0/s_axi_aresetn] [get_bd_pins dac_dmac_xlength_sni_0/s_axi_aresetn] [get_bd_pins dac_dmac_xlength_sni_1/s_axi_aresetn] [get_bd_pins dac_fifo_timestamp_e_0/s00_axi_aresetn] [get_bd_pins dac_fifo_timestamp_e_0/s_axi_aresetn] [get_bd_pins proc_sys_reset_0/peripheral_aresetn] [get_bd_pins util_vector_logic_0/Op1]
  connect_bd_net -net proc_sys_reset_0_peripheral_reset [get_bd_pins proc_sys_reset_0/peripheral_reset] [get_bd_pins tx_upack/reset]
  connect_bd_net -net rx_clk_in_1 [get_bd_ports rx_clk_in] [get_bd_pins axi_ad9361/rx_clk_in]
  connect_bd_net -net rx_data_in_1 [get_bd_ports rx_data_in] [get_bd_pins axi_ad9361/rx_data_in]
  connect_bd_net -net rx_frame_in_1 [get_bd_ports rx_frame_in] [get_bd_pins axi_ad9361/rx_frame_in]
  connect_bd_net -net spi0_clk_i_1 [get_bd_ports spi0_clk_i] [get_bd_pins sys_ps7/SPI0_SCLK_I]
  connect_bd_net -net spi0_csn_i_1 [get_bd_ports spi0_csn_i] [get_bd_pins sys_ps7/SPI0_SS_I]
  connect_bd_net -net spi0_sdi_i_1 [get_bd_ports spi0_sdi_i] [get_bd_pins sys_ps7/SPI0_MISO_I]
  connect_bd_net -net spi0_sdo_i_1 [get_bd_ports spi0_sdo_i] [get_bd_pins sys_ps7/SPI0_MOSI_I]
  connect_bd_net -net spi_clk_i_1 [get_bd_ports spi_clk_i] [get_bd_pins axi_spi/sck_i]
  connect_bd_net -net spi_csn_i_1 [get_bd_ports spi_csn_i] [get_bd_pins axi_spi/ss_i]
  connect_bd_net -net spi_sdi_i_1 [get_bd_ports spi_sdi_i] [get_bd_pins axi_spi/io1_i]
  connect_bd_net -net spi_sdo_i_1 [get_bd_ports spi_sdo_i] [get_bd_pins axi_spi/io0_i]
  connect_bd_net -net sys_200m_clk [get_bd_pins axi_ad9361/delay_clk] [get_bd_pins sys_ps7/FCLK_CLK1]
  connect_bd_net -net sys_concat_intc_dout [get_bd_pins sys_concat_intc/dout] [get_bd_pins sys_ps7/IRQ_F2P]
  connect_bd_net -net sys_cpu_clk [get_bd_pins adc_dma_packet_contr_0/ADCxN_clk] [get_bd_pins adc_dmac_xlength_sni_0/s_axi_aclk] [get_bd_pins adc_fifo_timestamp_e_1/s_axi_aclk] [get_bd_pins axi_ad9361/s_axi_aclk] [get_bd_pins axi_ad9361_adc_dma/fifo_wr_clk] [get_bd_pins axi_ad9361_adc_dma/m_dest_axi_aclk] [get_bd_pins axi_ad9361_adc_dma/s_axi_aclk] [get_bd_pins axi_ad9361_dac_dma/m_axis_aclk] [get_bd_pins axi_ad9361_dac_dma/m_src_axi_aclk] [get_bd_pins axi_ad9361_dac_dma/s_axi_aclk] [get_bd_pins axi_cpu_interconnect/ACLK] [get_bd_pins axi_cpu_interconnect/M00_ACLK] [get_bd_pins axi_cpu_interconnect/M01_ACLK] [get_bd_pins axi_cpu_interconnect/M02_ACLK] [get_bd_pins axi_cpu_interconnect/M03_ACLK] [get_bd_pins axi_cpu_interconnect/M04_ACLK] [get_bd_pins axi_cpu_interconnect/S00_ACLK] [get_bd_pins axi_iic_main/s_axi_aclk] [get_bd_pins axi_spi/ext_spi_clk] [get_bd_pins axi_spi/s_axi_aclk] [get_bd_pins cpack/clk] [get_bd_pins dac_control_s_axi_ac_0/s_axi_aclk] [get_bd_pins dac_dmac_xlength_sni_0/s_axi_aclk] [get_bd_pins dac_dmac_xlength_sni_1/s_axi_aclk] [get_bd_pins dac_fifo_timestamp_e_0/s00_axi_aclk] [get_bd_pins dac_fifo_timestamp_e_0/s_axi_aclk] [get_bd_pins proc_sys_reset_0/slowest_sync_clk] [get_bd_pins sys_ps7/FCLK_CLK0] [get_bd_pins sys_ps7/M_AXI_GP0_ACLK] [get_bd_pins sys_ps7/S_AXI_HP1_ACLK] [get_bd_pins sys_ps7/S_AXI_HP2_ACLK] [get_bd_pins sys_rstgen/slowest_sync_clk] [get_bd_pins tx_upack/clk]
  connect_bd_net -net sys_cpu_reset [get_bd_pins sys_rstgen/peripheral_reset]
  connect_bd_net -net sys_cpu_resetn [get_bd_pins axi_ad9361/s_axi_aresetn] [get_bd_pins axi_ad9361_adc_dma/m_dest_axi_aresetn] [get_bd_pins axi_ad9361_adc_dma/s_axi_aresetn] [get_bd_pins axi_ad9361_dac_dma/m_src_axi_aresetn] [get_bd_pins axi_ad9361_dac_dma/s_axi_aresetn] [get_bd_pins axi_cpu_interconnect/ARESETN] [get_bd_pins axi_cpu_interconnect/M00_ARESETN] [get_bd_pins axi_cpu_interconnect/M01_ARESETN] [get_bd_pins axi_cpu_interconnect/M02_ARESETN] [get_bd_pins axi_cpu_interconnect/M03_ARESETN] [get_bd_pins axi_cpu_interconnect/M04_ARESETN] [get_bd_pins axi_cpu_interconnect/S00_ARESETN] [get_bd_pins axi_iic_main/s_axi_aresetn] [get_bd_pins axi_spi/s_axi_aresetn] [get_bd_pins sys_rstgen/peripheral_aresetn]
  connect_bd_net -net sys_ps7_FCLK_RESET0_N [get_bd_pins proc_sys_reset_0/ext_reset_in] [get_bd_pins sys_ps7/FCLK_RESET0_N] [get_bd_pins sys_rstgen/ext_reset_in]
  connect_bd_net -net sys_ps7_GPIO_O [get_bd_ports gpio_o] [get_bd_pins sys_ps7/GPIO_O]
  connect_bd_net -net sys_ps7_GPIO_T [get_bd_ports gpio_t] [get_bd_pins sys_ps7/GPIO_T]
  connect_bd_net -net sys_ps7_SPI0_MOSI_O [get_bd_ports spi0_sdo_o] [get_bd_pins sys_ps7/SPI0_MOSI_O]
  connect_bd_net -net sys_ps7_SPI0_SCLK_O [get_bd_ports spi0_clk_o] [get_bd_pins sys_ps7/SPI0_SCLK_O]
  connect_bd_net -net sys_ps7_SPI0_SS1_O [get_bd_ports spi0_csn_1_o] [get_bd_pins sys_ps7/SPI0_SS1_O]
  connect_bd_net -net sys_ps7_SPI0_SS2_O [get_bd_ports spi0_csn_2_o] [get_bd_pins sys_ps7/SPI0_SS2_O]
  connect_bd_net -net sys_ps7_SPI0_SS_O [get_bd_ports spi0_csn_0_o] [get_bd_pins sys_ps7/SPI0_SS_O]
  connect_bd_net -net timestamp_unit_lclk_0_current_lclk_count [get_bd_pins adc_fifo_timestamp_e_1/current_lclk_count] [get_bd_pins dac_fifo_timestamp_e_0/current_lclk_count] [get_bd_pins timestamp_unit_lclk_0/current_lclk_count]
  connect_bd_net -net tx_upack_fifo_rd_data_0 [get_bd_pins dac_fifo_timestamp_e_0/dac_data_0] [get_bd_pins tx_upack/fifo_rd_data_0]
  connect_bd_net -net tx_upack_fifo_rd_data_1 [get_bd_pins dac_fifo_timestamp_e_0/dac_data_1] [get_bd_pins tx_upack/fifo_rd_data_1]
  connect_bd_net -net tx_upack_fifo_rd_data_2 [get_bd_pins dac_fifo_timestamp_e_0/dac_data_2] [get_bd_pins tx_upack/fifo_rd_data_2]
  connect_bd_net -net tx_upack_fifo_rd_data_3 [get_bd_pins dac_fifo_timestamp_e_0/dac_data_3] [get_bd_pins tx_upack/fifo_rd_data_3]
  connect_bd_net -net tx_upack_fifo_rd_underflow [get_bd_pins dac_fifo_timestamp_e_0/dac_fifo_unf] [get_bd_pins tx_upack/fifo_rd_underflow]
  connect_bd_net -net tx_upack_fifo_rd_valid [get_bd_pins dac_fifo_timestamp_e_0/dac_valid_0] [get_bd_pins dac_fifo_timestamp_e_0/dac_valid_1] [get_bd_pins dac_fifo_timestamp_e_0/dac_valid_2] [get_bd_pins dac_fifo_timestamp_e_0/dac_valid_3] [get_bd_pins tx_upack/fifo_rd_valid]
  connect_bd_net -net up_enable_1 [get_bd_ports up_enable] [get_bd_pins axi_ad9361/up_enable]
  connect_bd_net -net up_txnrx_1 [get_bd_ports up_txnrx] [get_bd_pins axi_ad9361/up_txnrx]
  connect_bd_net -net util_vector_logic_0_Res [get_bd_pins adc_dma_packet_contr_0/ADCxN_reset] [get_bd_pins cpack/reset] [get_bd_pins util_vector_logic_0/Res]

  # Create address segments
  assign_bd_address -offset 0x00000000 -range 0x20000000 -target_address_space [get_bd_addr_spaces axi_ad9361_adc_dma/m_dest_axi] [get_bd_addr_segs sys_ps7/S_AXI_HP1/HP1_DDR_LOWOCM] -force
  assign_bd_address -offset 0x00000000 -range 0x20000000 -target_address_space [get_bd_addr_spaces axi_ad9361_dac_dma/m_src_axi] [get_bd_addr_segs sys_ps7/S_AXI_HP2/HP2_DDR_LOWOCM] -force
  assign_bd_address -offset 0x7C400000 -range 0x00001000 -target_address_space [get_bd_addr_spaces sys_ps7/Data] [get_bd_addr_segs axi_ad9361_adc_dma/s_axi/axi_lite] -force
  assign_bd_address -offset 0x79020000 -range 0x00010000 -target_address_space [get_bd_addr_spaces sys_ps7/Data] [get_bd_addr_segs axi_ad9361/s_axi/axi_lite] -force
  assign_bd_address -offset 0x7C420000 -range 0x00001000 -target_address_space [get_bd_addr_spaces sys_ps7/Data] [get_bd_addr_segs axi_ad9361_dac_dma/s_axi/axi_lite] -force
  assign_bd_address -offset 0x41600000 -range 0x00001000 -target_address_space [get_bd_addr_spaces sys_ps7/Data] [get_bd_addr_segs axi_iic_main/S_AXI/Reg] -force
  assign_bd_address -offset 0x7C430000 -range 0x00001000 -target_address_space [get_bd_addr_spaces sys_ps7/Data] [get_bd_addr_segs axi_spi/AXI_LITE/Reg] -force


  # Restore current instance
  current_bd_instance $oldCurInst

  save_bd_design
}
# End of create_root_design()


##################################################################
# MAIN FLOW
##################################################################

create_root_design ""


common::send_msg_id "BD_TCL-1000" "WARNING" "This Tcl script was generated from a block design that has not been validated. It is possible that design <$design_name> may result in errors during validation."

