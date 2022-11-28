//Copyright 1986-2019 Xilinx, Inc. All Rights Reserved.
//--------------------------------------------------------------------------------
//Tool Version: Vivado v.2019.2.1 (lin64) Build 2729669 Thu Dec  5 04:48:12 MST 2019
//Date        : Tue Jul 12 10:07:30 2022
//Host        : fpga-vivado running 64-bit Ubuntu 20.04.3 LTS
//Command     : generate_target design_1_wrapper.bd
//Design      : design_1_wrapper
//Purpose     : IP block netlist
//--------------------------------------------------------------------------------
`timescale 1 ps / 1 ps

module design_1_wrapper
   (CH1_CLKX_out,
    CH1_LEX_out,
    CH1_RX_DSA_D0X_out,
    CH1_RX_DSA_D1X_out,
    CH1_RX_DSA_D2X_out,
    CH1_RX_DSA_D3X_out,
    CH1_RX_DSA_D4X_out,
    CH1_RX_DSA_D5X_out,
    CH1_RX_LNA0_BYPX_out,
    CH1_RX_LNA0_DISX_out,
    CH1_RX_LNA0_ENX_out,
    CH1_RX_LNA1_BYPX_out,
    CH1_RX_LNA1_DISX_out,
    CH1_RX_OV_in,
    CH1_SIX_out,
    CH1_TX_LNA_DISX_out,
    CH1_TX_PA_ENX_out,
    CH2_CLKX_out,
    CH2_LEX_out,
    CH2_RX_DSA_D0X_out,
    CH2_RX_DSA_D1X_out,
    CH2_RX_DSA_D2X_out,
    CH2_RX_DSA_D3X_out,
    CH2_RX_DSA_D4X_out,
    CH2_RX_DSA_D5X_out,
    CH2_RX_LNA0_BYPX_out,
    CH2_RX_LNA0_DISX_out,
    CH2_RX_LNA0_ENX_out,
    CH2_RX_LNA1_BYPX_out,
    CH2_RX_LNA1_DISX_out,
    CH2_RX_OV_in,
    CH2_SIX_out,
    CH2_TX_LNA_DISX_out,
    CH2_TX_PA_ENX_out,
    COMMS_LEDX_out,
    adc0_clk_clk_n,
    adc0_clk_clk_p,
    dac1_clk_clk_n,
    dac1_clk_clk_p,
    emio_uart1_rxd_0,
    emio_uart1_txd_0,
    sysref_in_diff_n,
    sysref_in_diff_p,
    vin0_23_0_v_n,
    vin0_23_0_v_p,
    vout13_v_n,
    vout13_v_p);

  output CH1_CLKX_out;
  output CH1_LEX_out;
  output CH1_RX_DSA_D0X_out;
  output CH1_RX_DSA_D1X_out;
  output CH1_RX_DSA_D2X_out;
  output CH1_RX_DSA_D3X_out;
  output CH1_RX_DSA_D4X_out;
  output CH1_RX_DSA_D5X_out;
  output CH1_RX_LNA0_BYPX_out;
  output CH1_RX_LNA0_DISX_out;
  output CH1_RX_LNA0_ENX_out;
  output CH1_RX_LNA1_BYPX_out;
  output CH1_RX_LNA1_DISX_out;
  input CH1_RX_OV_in;
  output CH1_SIX_out;
  output CH1_TX_LNA_DISX_out;
  output CH1_TX_PA_ENX_out;
  output CH2_CLKX_out;
  output CH2_LEX_out;
  output CH2_RX_DSA_D0X_out;
  output CH2_RX_DSA_D1X_out;
  output CH2_RX_DSA_D2X_out;
  output CH2_RX_DSA_D3X_out;
  output CH2_RX_DSA_D4X_out;
  output CH2_RX_DSA_D5X_out;
  output CH2_RX_LNA0_BYPX_out;
  output CH2_RX_LNA0_DISX_out;
  output CH2_RX_LNA0_ENX_out;
  output CH2_RX_LNA1_BYPX_out;
  output CH2_RX_LNA1_DISX_out;
  input CH2_RX_OV_in;
  output CH2_SIX_out;
  output CH2_TX_LNA_DISX_out;
  output CH2_TX_PA_ENX_out;
  output COMMS_LEDX_out;
  input adc0_clk_clk_n;
  input adc0_clk_clk_p;
  input dac1_clk_clk_n;
  input dac1_clk_clk_p;
  input emio_uart1_rxd_0;
  output emio_uart1_txd_0;
  input sysref_in_diff_n;
  input sysref_in_diff_p;
  input vin0_23_0_v_n;
  input vin0_23_0_v_p;
  output vout13_v_n;
  output vout13_v_p;

  wire CH1_CLKX_out;
  wire CH1_LEX_out;
  wire CH1_RX_DSA_D0X_out;
  wire CH1_RX_DSA_D1X_out;
  wire CH1_RX_DSA_D2X_out;
  wire CH1_RX_DSA_D3X_out;
  wire CH1_RX_DSA_D4X_out;
  wire CH1_RX_DSA_D5X_out;
  wire CH1_RX_LNA0_BYPX_out;
  wire CH1_RX_LNA0_DISX_out;
  wire CH1_RX_LNA0_ENX_out;
  wire CH1_RX_LNA1_BYPX_out;
  wire CH1_RX_LNA1_DISX_out;
  wire CH1_RX_OV_in;
  wire CH1_SIX_out;
  wire CH1_TX_LNA_DISX_out;
  wire CH1_TX_PA_ENX_out;
  wire CH2_CLKX_out;
  wire CH2_LEX_out;
  wire CH2_RX_DSA_D0X_out;
  wire CH2_RX_DSA_D1X_out;
  wire CH2_RX_DSA_D2X_out;
  wire CH2_RX_DSA_D3X_out;
  wire CH2_RX_DSA_D4X_out;
  wire CH2_RX_DSA_D5X_out;
  wire CH2_RX_LNA0_BYPX_out;
  wire CH2_RX_LNA0_DISX_out;
  wire CH2_RX_LNA0_ENX_out;
  wire CH2_RX_LNA1_BYPX_out;
  wire CH2_RX_LNA1_DISX_out;
  wire CH2_RX_OV_in;
  wire CH2_SIX_out;
  wire CH2_TX_LNA_DISX_out;
  wire CH2_TX_PA_ENX_out;
  wire COMMS_LEDX_out;
  wire adc0_clk_clk_n;
  wire adc0_clk_clk_p;
  wire dac1_clk_clk_n;
  wire dac1_clk_clk_p;
  wire emio_uart1_rxd_0;
  wire emio_uart1_txd_0;
  wire sysref_in_diff_n;
  wire sysref_in_diff_p;
  wire vin0_23_0_v_n;
  wire vin0_23_0_v_p;
  wire vout13_v_n;
  wire vout13_v_p;

  design_1 design_1_i
       (.CH1_CLKX_out(CH1_CLKX_out),
        .CH1_LEX_out(CH1_LEX_out),
        .CH1_RX_DSA_D0X_out(CH1_RX_DSA_D0X_out),
        .CH1_RX_DSA_D1X_out(CH1_RX_DSA_D1X_out),
        .CH1_RX_DSA_D2X_out(CH1_RX_DSA_D2X_out),
        .CH1_RX_DSA_D3X_out(CH1_RX_DSA_D3X_out),
        .CH1_RX_DSA_D4X_out(CH1_RX_DSA_D4X_out),
        .CH1_RX_DSA_D5X_out(CH1_RX_DSA_D5X_out),
        .CH1_RX_LNA0_BYPX_out(CH1_RX_LNA0_BYPX_out),
        .CH1_RX_LNA0_DISX_out(CH1_RX_LNA0_DISX_out),
        .CH1_RX_LNA0_ENX_out(CH1_RX_LNA0_ENX_out),
        .CH1_RX_LNA1_BYPX_out(CH1_RX_LNA1_BYPX_out),
        .CH1_RX_LNA1_DISX_out(CH1_RX_LNA1_DISX_out),
        .CH1_RX_OV_in(CH1_RX_OV_in),
        .CH1_SIX_out(CH1_SIX_out),
        .CH1_TX_LNA_DISX_out(CH1_TX_LNA_DISX_out),
        .CH1_TX_PA_ENX_out(CH1_TX_PA_ENX_out),
        .CH2_CLKX_out(CH2_CLKX_out),
        .CH2_LEX_out(CH2_LEX_out),
        .CH2_RX_DSA_D0X_out(CH2_RX_DSA_D0X_out),
        .CH2_RX_DSA_D1X_out(CH2_RX_DSA_D1X_out),
        .CH2_RX_DSA_D2X_out(CH2_RX_DSA_D2X_out),
        .CH2_RX_DSA_D3X_out(CH2_RX_DSA_D3X_out),
        .CH2_RX_DSA_D4X_out(CH2_RX_DSA_D4X_out),
        .CH2_RX_DSA_D5X_out(CH2_RX_DSA_D5X_out),
        .CH2_RX_LNA0_BYPX_out(CH2_RX_LNA0_BYPX_out),
        .CH2_RX_LNA0_DISX_out(CH2_RX_LNA0_DISX_out),
        .CH2_RX_LNA0_ENX_out(CH2_RX_LNA0_ENX_out),
        .CH2_RX_LNA1_BYPX_out(CH2_RX_LNA1_BYPX_out),
        .CH2_RX_LNA1_DISX_out(CH2_RX_LNA1_DISX_out),
        .CH2_RX_OV_in(CH2_RX_OV_in),
        .CH2_SIX_out(CH2_SIX_out),
        .CH2_TX_LNA_DISX_out(CH2_TX_LNA_DISX_out),
        .CH2_TX_PA_ENX_out(CH2_TX_PA_ENX_out),
        .COMMS_LEDX_out(COMMS_LEDX_out),
        .adc0_clk_clk_n(adc0_clk_clk_n),
        .adc0_clk_clk_p(adc0_clk_clk_p),
        .dac1_clk_clk_n(dac1_clk_clk_n),
        .dac1_clk_clk_p(dac1_clk_clk_p),
        .emio_uart1_rxd_0(emio_uart1_rxd_0),
        .emio_uart1_txd_0(emio_uart1_txd_0),
        .sysref_in_diff_n(sysref_in_diff_n),
        .sysref_in_diff_p(sysref_in_diff_p),
        .vin0_23_0_v_n(vin0_23_0_v_n),
        .vin0_23_0_v_p(vin0_23_0_v_p),
        .vout13_v_n(vout13_v_n),
        .vout13_v_p(vout13_v_p));
endmodule
