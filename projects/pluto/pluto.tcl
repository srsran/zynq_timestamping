if { $argc == 0 } {
  puts "ERRORINFO: The script needs 1 or 2 arguments:
    --> Argument 0: project, bitstream
    --> Argument 1: IP repository path"
  exit -1
}
# Init Project 
set script_path [file normalize [info script]]
set script_dir [file dirname ${script_path}]
file mkdir ${script_dir}/vivado_prj
create_project pluto ${script_dir}/vivado_prj -force
# Properties 
set_property target_language Verilog [current_project]
set_property default_lib xil_defaultlib [current_project]
set_property part xc7z010clg225-1 [current_project]
# Sources
update_compile_order -fileset [get_filesets sources_1]
remove_files [get_files -filter {IS_AUTO_DISABLED}]
# HDL Sources
add_files -fileset [get_filesets sources_1] ${script_dir}/hdl/library/util_cdc/sync_bits.v
add_files -fileset [get_filesets sources_1] ${script_dir}/hdl/library/common/util_pulse_gen.v
add_files -fileset [get_filesets sources_1] ${script_dir}/hdl/library/common/ad_bus_mux.v
add_files -fileset [get_filesets sources_1] ${script_dir}/hdl/library/common/ad_iobuf.v
add_files -fileset [get_filesets sources_1] ${script_dir}/hdl/library/common/ad_perfect_shuffle.v
add_files -fileset [get_filesets sources_1] ${script_dir}/hdl/projects/pluto/system_top.v
# IP repo
if { $argc == 2 } {
  set repo_path [lindex $argv 1]
  set_property ip_repo_paths $repo_path [current_project]
  update_ip_catalog
}
update_ip_catalog
# XDC files
add_files -fileset [get_filesets constrs_1] ${script_dir}/hdl/projects/common/xilinx/adi_fir_filter_constr.xdc
add_files -fileset [get_filesets constrs_1] ${script_dir}/hdl/projects/pluto/system_constr.xdc
# Design Checkpoint files
#  Synthesis config
# Config tcl
# IP tcl
source ${script_dir}/src/ip/ips.tcl
# BD tcl
source ${script_dir}/src/bd/system.tcl
# Project top
set_property top system_top [current_fileset]
set project_top [get_property top [current_fileset]]
# Top
make_wrapper -files [get_files ${script_dir}/vivado_prj/pluto.srcs/sources_1/bd/system/system.bd] -top
add_files -norecurse ${script_dir}/vivado_prj/pluto.srcs/sources_1/bd/system/hdl/system_wrapper.v
# ADC
#source ${script_dir}/plutosdr-fw/hdl/library/axi_ad9361/axi_ad9361_delay.tcl
# Generate bitstream
set run_step [lindex $argv 0]
if { $run_step == "bitstream" } {
  launch_runs impl_1 -verbose -to_step write_bitstream -jobs 6
  wait_on_run impl_1
  if {[get_property PROGRESS [get_runs impl_1]] != "100%"} {   
    error "ERROR: impl_1 failed"   
  } 

  set wns_number [get_property STATS.WNS [get_runs impl_1]]
  set whs_number [get_property STATS.WHS [get_runs impl_1]]
  puts "wns_number = $wns_number"
  puts "whs_number = $whs_number"

  if [expr {$wns_number < 0.0 || $whs_number < 0.0}] { error "ERROR: Failed to meet timings" }
}
