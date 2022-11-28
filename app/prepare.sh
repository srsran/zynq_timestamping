#! /bin/bash

################################################################################
# Common
################################################################################
pwd="$( pwd )"
TEST_OUTPUT=$pwd/bin_app
mkdir "$TEST_OUTPUT"

board_name=$1
if [[ "$board_name" == "zcu111" ]]; then
  cmake_rf_cfg="-DENABLE_IIO=Off -DENABLE_RFDC=On"
else
  cmake_rf_cfg="-DENABLE_IIO=On -DENABLE_RFDC=Off"
fi

BOARD_IP=${BOARD_IP:-'10.12.1.201'}
FREQ_OFFSET=${FREQ_OFFSET:-'-4100'}
RX_GAIN=${RX_GAIN:-'50'}
TIME_ADV_NSAMPLES=${TIME_ADV_NSAMPLES:-'82'}

echo "--> Current UE configuration"
echo "• Board IP: $BOARD_IP"
echo "• freq_offset: $FREQ_OFFSET"
echo "• rx_gain: $RX_GAIN"
echo "• time_adv_nsamples: $TIME_ADV_NSAMPLES"
echo ""

################################################################################
# Build test
################################################################################
cp txrx_test.c srsRAN/lib/examples/usrp_txrx.c
cd srsRAN || exit
if [ -z "$CROSS_COMPILE" ]
then
  toolchain_cmd=""
  build_dir=build
else
  toolchain_cmd="-DCMAKE_TOOLCHAIN_FILE=$pwd/toolchain.cmake"
  build_dir=build_arm
fi

mkdir $build_dir
cd $build_dir || exit
if [ -z "$CROSS_COMPILE" ]
then
  cmake -DCMAKE_BUILD_TYPE=Release -DUSE_LTE_RATES=On -DRF_FOUND=TRUE -DENABLE_SRSENB=ON -DENABLE_SRSEPC=ON -DENABLE_SRSUE=ON ..
  make -j4 usrp_txrx pdsch_ue pdsch_enodeb srsenb srsepc srsue
else
  # if we are cross-compiling for zcu102 or zcu111, we only need to test txrx_test (but user may want to play with pdsch_ue too)
  echo "Preparing for cross-compilation"
  cmake -DCMAKE_BUILD_TYPE=Release -DUSE_LTE_RATES=On -DRF_FOUND=TRUE -DENABLE_SRSEPC=OFF -DENABLE_SRSENB=OFF -DENABLE_SRSUE=ON $toolchain_cmd ..
  make -j4 usrp_txrx pdsch_ue srsue
fi

echo 'Copying configuration files...'

cp lib/examples/usrp_txrx "$TEST_OUTPUT"/txrx_test
cp lib/examples/pdsch_ue "$TEST_OUTPUT"

# if not cross compiling
if [ -z "$CROSS_COMPILE" ]
then
  cp lib/examples/pdsch_enodeb "$TEST_OUTPUT"

  mkdir -p "$TEST_OUTPUT"/srsue
  cp srsue/src/srsue "$TEST_OUTPUT"/srsue/
  cp ../srsue/ue.conf.example "$TEST_OUTPUT"/srsue/ue.conf

  ################################################################################
  # Adjust UE config
  ################################################################################
  sed -i "s/freq_offset = 0/freq_offset = $FREQ_OFFSET/g" "$TEST_OUTPUT"/srsue/ue.conf
  sed -i "s/#rx_gain = 40/rx_gain = $RX_GAIN/g" "$TEST_OUTPUT"/srsue/ue.conf
  sed -i "s/#time_adv_nsamples = auto/time_adv_nsamples = $TIME_ADV_NSAMPLES/g" "$TEST_OUTPUT"/srsue/ue.conf
  sed -i 's/#continuous_tx     = auto/continuous_tx     = no/g' "$TEST_OUTPUT"/srsue/ue.conf
  sed -i "s/#device_args = auto/device_name = iio\ndevice_args = n_prb=6,context=ip:$BOARD_IP/g" "$TEST_OUTPUT"/srsue/ue.conf
  sed -i 's/#nof_phy_threads     = 3/nof_phy_threads     = 3/g' "$TEST_OUTPUT"/srsue/ue.conf
  echo '[expert]' >> "$TEST_OUTPUT"/srsue/ue.conf
  echo 'lte_sample_rates = true' >> "$TEST_OUTPUT"/srsue/ue.conf
  
  mkdir -p "$TEST_OUTPUT"/srsenb
  cp srsenb/src/srsenb "$TEST_OUTPUT"/srsenb/
  cp ../srsenb/enb.conf.example "$TEST_OUTPUT"/srsenb/enb.conf
  cp ../srsenb/rb.conf.example "$TEST_OUTPUT"/srsenb/rb.conf
  cp ../srsenb/rr.conf.example "$TEST_OUTPUT"/srsenb/rr.conf
  cp ../srsenb/sib.conf.example "$TEST_OUTPUT"/srsenb/sib.conf
  
  ################################################################################
  # Adjust eNodeB config
  ################################################################################
  sed -i 's/n_prb = 50/n_prb = 6/g' "$TEST_OUTPUT"/srsenb/enb.conf
  sed -i 's/rx_gain = 40/rx_gain = 80/g' "$TEST_OUTPUT"/srsenb/enb.conf
  sed -i 's/#max_prach_offset_us  = 30/max_prach_offset_us  = 1000/g' "$TEST_OUTPUT"/srsenb/enb.conf
  echo 'lte_sample_rates = true' >> "$TEST_OUTPUT"/srsenb/enb.conf
  sed -i 's/prach_freq_offset = 4/prach_freq_offset = 0/g' "$TEST_OUTPUT"/srsenb/sib.conf
  sed -i 's/zero_correlation_zone_config = 5/zero_correlation_zone_config = 0/g' "$TEST_OUTPUT"/srsenb/sib.conf
  
  mkdir -p "$TEST_OUTPUT"/srsepc
  cp srsepc/src/srsepc "$TEST_OUTPUT"/srsepc/
  cp ../srsepc/epc.conf.example "$TEST_OUTPUT"/srsepc/epc.conf
  cp ../srsepc/user_db.csv.example  "$TEST_OUTPUT"/srsepc/user_db.csv
fi

################################################################################
# Compile rf lib
################################################################################
cd "$pwd" || exit
cd ../sw || exit
mkdir -p $build_dir
cd $build_dir || exit
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=./install $cmake_rf_cfg  $toolchain_cmd ..
make && make install "$TEST_OUTPUT"
cp -R ./install/lib/* "$TEST_OUTPUT"
echo 'Done'
