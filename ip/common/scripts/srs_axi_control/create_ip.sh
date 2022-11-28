srs-vivado-ip-pack --input ./ip.yml --fpga_part $1
vivado -mode batch -source custom_ip_script.tcl