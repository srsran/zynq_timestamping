# Pin map for Avnet's Qorvo 2x2 LTE-band3 Card - Rev B
# Xilinx device: ZCU111 board
# Avnet IP: qorvo_spi_slave.vhd

########################################################################
## CHANNEL 1 - J4 (ZCU111 J94)                                        ##
########################################################################

## PL-based I2C Controller ##

#set_property PACKAGE_PIN A9 [get_ports {I2C_SCLX}];
#set_property IOSTANDARD LVCMOS18 [get_ports {I2C_SCLX}];
#set_property PACKAGE_PIN A10 [get_ports {I2C_SDAX}];
#set_property IOSTANDARD LVCMOS18 [get_ports {I2C_SDAX}];

## Avnet SPI-to-Serial(Qorvo protocol) ##

set_property PACKAGE_PIN B7 [get_ports CH1_LEX_out]
set_property IOSTANDARD LVCMOS18 [get_ports CH1_LEX_out]

set_property PACKAGE_PIN B8 [get_ports CH1_CLKX_out]
set_property IOSTANDARD LVCMOS18 [get_ports CH1_CLKX_out]

set_property PACKAGE_PIN D8 [get_ports CH1_SIX_out]
set_property IOSTANDARD LVCMOS18 [get_ports CH1_SIX_out]

set_property PACKAGE_PIN D9 [get_ports CH1_RX_DSA_D0X_out]
set_property IOSTANDARD LVCMOS18 [get_ports CH1_RX_DSA_D0X_out]

set_property PACKAGE_PIN C7 [get_ports CH1_RX_DSA_D1X_out]
set_property IOSTANDARD LVCMOS18 [get_ports CH1_RX_DSA_D1X_out]

set_property PACKAGE_PIN C8 [get_ports CH1_RX_DSA_D2X_out]
set_property IOSTANDARD LVCMOS18 [get_ports CH1_RX_DSA_D2X_out]

set_property PACKAGE_PIN C10 [get_ports CH1_RX_DSA_D3X_out]
set_property IOSTANDARD LVCMOS18 [get_ports CH1_RX_DSA_D3X_out]

set_property PACKAGE_PIN D10 [get_ports CH1_RX_DSA_D4X_out]
set_property IOSTANDARD LVCMOS18 [get_ports CH1_RX_DSA_D4X_out]

set_property PACKAGE_PIN D6 [get_ports CH1_RX_DSA_D5X_out]
set_property IOSTANDARD LVCMOS18 [get_ports CH1_RX_DSA_D5X_out]

set_property PACKAGE_PIN C6 [get_ports CH1_TX_PA_ENX_out]
set_property IOSTANDARD LVCMOS18 [get_ports CH1_TX_PA_ENX_out]

set_property PACKAGE_PIN B10 [get_ports CH1_TX_LNA_DISX_out]
set_property IOSTANDARD LVCMOS18 [get_ports CH1_TX_LNA_DISX_out]

set_property PACKAGE_PIN A6 [get_ports CH1_RX_LNA0_BYPX_out]
set_property IOSTANDARD LVCMOS18 [get_ports CH1_RX_LNA0_BYPX_out]

set_property PACKAGE_PIN A7 [get_ports CH1_RX_LNA1_BYPX_out]
set_property IOSTANDARD LVCMOS18 [get_ports CH1_RX_LNA1_BYPX_out]

set_property PACKAGE_PIN A5 [get_ports CH1_RX_LNA0_DISX_out]
set_property IOSTANDARD LVCMOS18 [get_ports CH1_RX_LNA0_DISX_out]

set_property PACKAGE_PIN B5 [get_ports CH1_RX_LNA0_ENX_out]
set_property IOSTANDARD LVCMOS18 [get_ports CH1_RX_LNA0_ENX_out]

set_property PACKAGE_PIN B9 [get_ports CH1_RX_LNA1_DISX_out]
set_property IOSTANDARD LVCMOS18 [get_ports CH1_RX_LNA1_DISX_out]

set_property PACKAGE_PIN C5 [get_ports CH1_RX_OV_in]
set_property IOSTANDARD LVCMOS18 [get_ports CH1_RX_OV_in]


########################################################################
## CHANNEL 2 - J7 (ZCU111 J47)                                        ##
########################################################################

set_property PACKAGE_PIN AU5 [get_ports CH2_LEX_out]
set_property IOSTANDARD LVCMOS18 [get_ports CH2_LEX_out]

set_property PACKAGE_PIN AT5 [get_ports CH2_CLKX_out]
set_property IOSTANDARD LVCMOS18 [get_ports CH2_CLKX_out]

set_property PACKAGE_PIN AU3 [get_ports CH2_SIX_out]
set_property IOSTANDARD LVCMOS18 [get_ports CH2_SIX_out]

set_property PACKAGE_PIN AU4 [get_ports CH2_RX_DSA_D0X_out]
set_property IOSTANDARD LVCMOS18 [get_ports CH2_RX_DSA_D0X_out]

set_property PACKAGE_PIN AV5 [get_ports CH2_RX_DSA_D1X_out]
set_property IOSTANDARD LVCMOS18 [get_ports CH2_RX_DSA_D1X_out]

set_property PACKAGE_PIN AV6 [get_ports CH2_RX_DSA_D2X_out]
set_property IOSTANDARD LVCMOS18 [get_ports CH2_RX_DSA_D2X_out]

set_property PACKAGE_PIN AU1 [get_ports CH2_RX_DSA_D3X_out]
set_property IOSTANDARD LVCMOS18 [get_ports CH2_RX_DSA_D3X_out]

set_property PACKAGE_PIN AU2 [get_ports CH2_RX_DSA_D4X_out]
set_property IOSTANDARD LVCMOS18 [get_ports CH2_RX_DSA_D4X_out]

set_property PACKAGE_PIN AV2 [get_ports CH2_RX_DSA_D5X_out]
set_property IOSTANDARD LVCMOS18 [get_ports CH2_RX_DSA_D5X_out]

set_property PACKAGE_PIN AU8 [get_ports CH2_TX_PA_ENX_out]
set_property IOSTANDARD LVCMOS18 [get_ports CH2_TX_PA_ENX_out]

set_property PACKAGE_PIN AT7 [get_ports CH2_TX_LNA_DISX_out]
set_property IOSTANDARD LVCMOS18 [get_ports CH2_TX_LNA_DISX_out]

set_property PACKAGE_PIN AR6 [get_ports CH2_RX_LNA0_BYPX_out]
set_property IOSTANDARD LVCMOS18 [get_ports CH2_RX_LNA0_BYPX_out]

set_property PACKAGE_PIN AR7 [get_ports CH2_RX_LNA1_BYPX_out]
set_property IOSTANDARD LVCMOS18 [get_ports CH2_RX_LNA1_BYPX_out]

set_property PACKAGE_PIN AV7 [get_ports CH2_RX_LNA0_DISX_out]
set_property IOSTANDARD LVCMOS18 [get_ports CH2_RX_LNA0_DISX_out]

set_property PACKAGE_PIN AU7 [get_ports CH2_RX_LNA0_ENX_out]
set_property IOSTANDARD LVCMOS18 [get_ports CH2_RX_LNA0_ENX_out]

set_property PACKAGE_PIN AT6 [get_ports CH2_RX_LNA1_DISX_out]
set_property IOSTANDARD LVCMOS18 [get_ports CH2_RX_LNA1_DISX_out]

set_property PACKAGE_PIN AP5 [get_ports COMMS_LEDX_out]
set_property IOSTANDARD LVCMOS18 [get_ports COMMS_LEDX_out]

set_property PACKAGE_PIN AV8 [get_ports CH2_RX_OV_in]
set_property IOSTANDARD LVCMOS18 [get_ports CH2_RX_OV_in]

