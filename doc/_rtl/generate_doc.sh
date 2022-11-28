#! /bin/bash
teroshdl-hdl-documenter -o html -s none -c none -p none -f none --symbol_vhdl "!" -i ./ips.csv

sed -i -e 's/max-width: 1080px;//g'  ./doc_internal//adc_dma_packet_controller.html
sed -i -e 's/max-width: 1080px;//g'  ./doc_internal//adc_dmac_xlength_sniffer.html
sed -i -e 's/max-width: 1080px;//g'  ./doc_internal//adc_fifo_timestamp_enabler.html
sed -i -e 's/max-width: 1080px;//g'  ./doc_internal//adc_timestamp_enabler_packetizer.html
sed -i -e 's/max-width: 1080px;//g'  ./doc_internal//dac_dmac_xlength_sniffer.html
sed -i -e 's/max-width: 1080px;//g'  ./doc_internal//dac_fifo_timestamp_enabler.html
sed -i -e 's/max-width: 1080px;//g'  ./doc_internal//dma_depack_channels.html
sed -i -e 's/max-width: 1080px;//g'  ./doc_internal//timestamp_unit_lclk_count.html
sed -i -e 's/max-width: 1080px;//g'  ./doc_internal//rfdc_adc_data_decim_and_depack.html
sed -i -e 's/max-width: 1080px;//g'  ./doc_internal//rfdc_dac_data_interp_and_pack.html

rm -rf index.html
