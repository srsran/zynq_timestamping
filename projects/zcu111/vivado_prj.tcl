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
create_project vivado_prj ${script_dir}/vivado_prj -force
# Properties 
set_property target_language Verilog [current_project]
set_property default_lib xil_defaultlib [current_project]
set_property part xczu28dr-ffvg1517-2-e [current_project]
set_property board_part xilinx.com:zcu111:part0:1.2 [current_project]
# Sources
update_compile_order -fileset [get_filesets sources_1]
remove_files [get_files -filter {IS_AUTO_DISABLED}]
# HDL Sources
# IP repo
set repo_path [lindex $argv 1]
set_property ip_repo_paths $repo_path [current_project]
update_ip_catalog
# XDC files
add_files -fileset [get_filesets constrs_1] ${script_dir}/src/constraints/qorvo_spi_slave.xdc
add_files -fileset [get_filesets constrs_1] ${script_dir}/src/constraints/zcu111_base_constraints.xdc
################################################################################
# Synthesis config
################################################################################
set_property strategy Flow_PerfOptimized_high [get_runs synth_1]
set_property STEPS.SYNTH_DESIGN.ARGS.RETIMING true [get_runs synth_1]
################################################################################
# Implementation config
################################################################################
set_property strategy Performance_ExtraTimingOpt [get_runs impl_1]

set_property STEPS.OPT_DESIGN.ARGS.DIRECTIVE ExploreWithRemap [get_runs impl_1]

set_property STEPS.POST_ROUTE_PHYS_OPT_DESIGN.IS_ENABLED true [get_runs impl_1]
set_property STEPS.POST_ROUTE_PHYS_OPT_DESIGN.ARGS.DIRECTIVE AddRetime [get_runs impl_1]

set_property -name {STEPS.ROUTE_DESIGN.ARGS.MORE OPTIONS} -value -tns_cleanup -objects [get_runs impl_1]

# IP tcl
source ${script_dir}/src/ip/ips.tcl
# BD tcl
source ${script_dir}/src/bd/design_1.tcl
# Project top
add_files -fileset [get_filesets sources_1] ${script_dir}/src/hdl/design_1_wrapper.v
set_property top design_1_wrapper [current_fileset]
set project_top [get_property top [current_fileset]]

########################################################################################################################
update_compile_order -fileset sources_1

# Generate bitstream
set run_step [lindex $argv 0]
if { $run_step == "bitstream" } {
  launch_runs impl_1 -to_step write_bitstream -jobs 5
  wait_on_run impl_1
  if {[get_property PROGRESS [get_runs impl_1]] != "100%"} {   
    error "ERROR: impl_1 failed"   
  } 
      
  set wns_number [get_property STATS.WNS [get_runs impl_1]]
  set tns_number [get_property STATS.TNS [get_runs impl_1]]
  set whs_number [get_property STATS.WHS [get_runs impl_1]]

  puts "*********************************************************************"
  puts "wns_number = $wns_number"
  puts "tns_number = $tns_number"
  puts "whs_number = $whs_number"
  puts "*********************************************************************"

  if [expr {$wns_number < 0.0 || $whs_number < 0.0}] { 

    # Set auto incremental implementation
    set_property AUTO_INCREMENTAL_CHECKPOINT 1 [get_runs impl_1]
    set_property -name INCREMENTAL_CHECKPOINT.MORE_OPTIONS -value {-directive TimingClosure} -objects [get_runs impl_1]
    puts "Using auto incremental implementation"

    reset_run impl_1
    launch_runs impl_1 -to_step write_bitstream -jobs 4
    wait_on_run impl_1
    if {[get_property PROGRESS [get_runs impl_1]] != "100%"} {   
      error "ERROR: impl_1 failed"   
    } 

    set wns_number [get_property STATS.WNS [get_runs impl_1]]
    set tns_number [get_property STATS.TNS [get_runs impl_1]]
    set whs_number [get_property STATS.WHS [get_runs impl_1]]

    puts "*********************************************************************"
    puts "wns_number = $wns_number"
    puts "tns_number = $tns_number"
    puts "whs_number = $whs_number"
    puts "*********************************************************************"

    if [expr {$wns_number < 0.0 || $whs_number < 0.0}] { 
      error "ERROR: Failed to meet timings" 
    }

  }
}
