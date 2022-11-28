#!/bin/bash

exit_on_error() {
    exit_code=$1
    last_command=${@:2}
    if [ "$exit_code" -ne 0 ]; then
        >&2 echo "\"${last_command}\" command failed with exit code ${exit_code}."
        exit $exit_code
    fi
}

if [ "$#" != "1" ]; then
    echo "You must provide one arg: mode [project, bitstream]. E.g: ./create_project.sh bitstream"
    exit 9
fi

steps=$1
cwd=$(pwd)

echo "###################################################################"
echo "- Mode: ${steps}"
echo "###################################################################"

VIVADO_VERSION="2019.2"
IP_DECLARATION="../../ip/ip_declaration.yml"
FPGA_PART="xczu28dr-ffvg1517-2-e"
IP_REPO=$cwd/ip_repo_generated
IP_LIST="./ip_list.csv"

# Preparation
if [ -z "$SRS_VIVADO_PATH" ]
then
    VIVADO_VERSION="2019.2"
    export SRS_VIVADO_PATH=/opt/Xilinx/Vivado/$VIVADO_VERSION/
fi
source $SRS_VIVADO_PATH/settings64.sh
export MYVIVADO=$SRS_VIVADO_PATH/

# Generate IPs
srs-tools-fpga-vivado-ip-pack --mode pack_ip_list --input $IP_DECLARATION \
     --fpga_part $FPGA_PART --output "$IP_REPO" --ip_list $IP_LIST

exit_on_error $? !!

# Qorvo IP
cp -R ../../ip/RFdc_timestamping/avnet_IP_cores/ "$IP_REPO"

cd "$cwd" || exit
# Set build info
srs-tools-fpga-set-build-info --input ./src/bd/design_1.tcl
# Generate bitstream
rm -rf vivado_prj
srs-tools-fpga-vivado-run --input vivado_prj.tcl --args "$steps $IP_REPO"

exit_on_error $? !!