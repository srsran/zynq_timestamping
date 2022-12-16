#!/bin/bash

# Preparation
if [ -z "$SRS_VIVADO_PATH" ]
then
    VIVADO_VERSION="2019.2"
    export SRS_VIVADO_PATH=/opt/Xilinx/Vivado/$VIVADO_VERSION/
fi
source $SRS_VIVADO_PATH/settings64.sh

gen_release() {
    board=$1
    pwd=$2
    release_folder=$3

    # Prepare output folder
    output_folder=$release_folder/$board
    mkdir -p $output_folder

    cd projects/$board
    ./create_project.sh bitstream
    make gen-boot
    cd bootfiles
    cp *.BIN $output_folder
    cp *.bin $output_folder
    cp *.dtb $output_folder
    cd "$pwd"
}

pwd="$( pwd )"
release_folder=$pwd/release
rm -rf $release_folder
mkdir $release_folder

gen_release "pluto" "$pwd" "$release_folder"
gen_release "antsdr" "$pwd" "$release_folder"
gen_release "zcu102" "$pwd" "$release_folder"
gen_release "zcu111" "$pwd" "$release_folder"


