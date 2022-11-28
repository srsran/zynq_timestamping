#------------------------------------------
# TIMING CONSTRAINTS
#------------------------------------------
# Set AXI-Lite Clock to 100.0MHz
create_clock -period 10.000 -name usp_rf_data_converter_0_axi_aclk [get_ports s_axi_aclk]
# ADC Reference Clock for Tile 1 running at 245.76 MHz
create_clock -period 4.069 -name usp_rf_data_converter_0_adc1_clk [get_ports adc0_clk_clk_p]
# DAC Reference Clock for Tile 1 running at 245.76 MHz
create_clock -period 4.069 -name usp_rf_data_converter_0_dac1_clk [get_ports dac1_clk_clk_p]
#AXI Streaming Clock for ADC1 at 122.88 MHz
create_clock -period 8.138020833 -name usp_rf_data_converter_0_m1_axis_aclk [get_ports m1_axis_aclk]
#AXI Streaming Clock for DAC1 at 245.76 MHz
create_clock -period 4.069010417 -name usp_rf_data_converter_0_s1_axis_aclk [get_ports s1_axis_aclk]


#ADCxN Clock constrained at 122.88 MHz (2x current maximum required clock)
create_clock -period 8.138 -name ADCxN_clk [get_pins -filter REF_PIN_NAME=~ADCxN_clk* -of [get_cells -hier -filter {NAME =~ *rfdc_adc_data_decim_0}]]
#create_clock -period 4.069010417 -name ADCxN_clk [get_pins -filter REF_PIN_NAME=~ADCxN_clk* -of [get_cells -hier -filter {NAME =~ *rfdc_adc_data_decim_0}]]
#DACxN Clock constrained at 122.88 MHz (2x current maximum required clock)
create_clock -period 8.138 -name DACxN_clk [get_pins -filter REF_PIN_NAME=~DACxN_clk* -of [get_cells -hier -filter {NAME =~ *rfdc_dac_data_interp_0}]]
#create_clock -period 4.069010417 -name DACxN_clk [get_pins -filter REF_PIN_NAME=~DACxN_clk* -of [get_cells -hier -filter {NAME =~ *rfdc_dac_data_interp_0}]]
#------------------------------------------
# UART
#------------------------------------------
set_property IOSTANDARD LVCMOS18 [get_ports emio_uart1_rxd_0]
set_property IOSTANDARD LVCMOS18 [get_ports emio_uart1_txd_0]
set_property PACKAGE_PIN AU15 [get_ports emio_uart1_rxd_0]
set_property PACKAGE_PIN AT15 [get_ports emio_uart1_txd_0]
#------------------------------------------
# PINS
#------------------------------------------
set_property IOSTANDARD LVCMOS18 [get_ports vin0_23_v_n]
set_property IOSTANDARD LVCMOS18 [get_ports vin0_23_v_p]
set_property IOSTANDARD LVCMOS18 [get_ports vout13_v_n]
set_property IOSTANDARD LVCMOS18 [get_ports vout13_v_p]
#------------------------------------------
# custom IPs
#------------------------------------------
#this constraint aims at helping Vivado understand that these signals are used in the 'DACxN_clk' domain (constrained at 122.88 MHz; 2x current maximum required clock)
set_max_delay -through [get_nets -hier -filter {NAME =~ *dac_fifo_timestamp_e_0/U0/*fwd_dac_data_0*}] 8.138
set_max_delay -through [get_nets -hier -filter {NAME =~ *dac_fifo_timestamp_e_0/U0/*fwd_dac_valid_0*}] 8.138
set_max_delay -through [get_nets -hier -filter {NAME =~ *dac_fifo_timestamp_e_0/U0/*fwd_dac_enable_0*}] 8.138020833
set_max_delay -through [get_nets -hier -filter {NAME =~ *dac_fifo_timestamp_e_0/U0/*fwd_dac_data_1*}] 8.138
set_max_delay -through [get_nets -hier -filter {NAME =~ *dac_fifo_timestamp_e_0/U0/*fwd_dac_valid_1*}] 8.138
set_max_delay -through [get_nets -hier -filter {NAME =~ *dac_fifo_timestamp_e_0/U0/*fwd_dac_enable_1*}] 8.138020833
#set_max_delay -through [get_nets -hier -filter {NAME =~ *dac_fifo_timestamp_e_0/U0/*fwd_dac_data_0*}] 4.069010417
#set_max_delay -through [get_nets -hier -filter {NAME =~ *dac_fifo_timestamp_e_0/U0/*fwd_dac_valid_0*}] 4.069010417
#set_max_delay -through [get_nets -hier -filter {NAME =~ *dac_fifo_timestamp_e_0/U0/*fwd_dac_enable_0*}] 4.069010417
#set_max_delay -through [get_nets -hier -filter {NAME =~ *dac_fifo_timestamp_e_0/U0/*fwd_dac_data_1*}] 4.069010417
#set_max_delay -through [get_nets -hier -filter {NAME =~ *dac_fifo_timestamp_e_0/U0/*fwd_dac_valid_1*}] 4.069010417
#set_max_delay -through [get_nets -hier -filter {NAME =~ *dac_fifo_timestamp_e_0/U0/*fwd_dac_enable_1*}] 4.069010417
set_false_path -from [get_pins {design_1_i/rfdc_dac_data_interp_0/U0/synchronizer_initial_clk_config_provided_ins/dst_data_s_reg[0]/C}] -to [get_pins design_1_i/rfdc_dac_data_interp_0/U0/BUFGCE_DIV_inst/CE]
#set_property BITSTREAM.GENERAL.COMPRESS TRUE [current_design]
