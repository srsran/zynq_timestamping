HDL_LIBRARY_PATH := hdl/library/

VIVADO := vivado -mode batch -source

CLEAN_TARGET := *.cache
CLEAN_TARGET += *.data
CLEAN_TARGET += *.xpr
CLEAN_TARGET += *.log
CLEAN_TARGET += *.jou
CLEAN_TARGET +=  xgui
CLEAN_TARGET += *.runs
CLEAN_TARGET += *.srcs
CLEAN_TARGET += *.sdk
CLEAN_TARGET += *.hw
CLEAN_TARGET += *.sim
CLEAN_TARGET += .Xil
CLEAN_TARGET += *.ip_user_files
CLEAN_TARGET += *.str
CLEAN_TARGET += mem_init_sys.txt
CLEAN_TARGET += vivado_prj
CLEAN_TARGET += ip_repo_generated

LIB_DEPS += axi_ad9361
LIB_DEPS += axi_dmac
LIB_DEPS += util_pack/util_cpack2
LIB_DEPS += util_pack/util_upack2
LIB_DEPS += util_wfifo
LIB_DEPS += util_rfifo
LIB_DEPS += util_tdd_sync

GEN_REPO := ip_repo_generated
FPGA_PART := xc7z010clg225-1

include $(HDL_LIBRARY_PATH)../quiet.mk

export ADI_IGNORE_VERSION_CHECK=1

clean:
	-rm -f reference.dcp
	$(call clean, \
		$(CLEAN_TARGET), \
		$(HL)$(PROJECT_NAME)$(NC) project)

clean-all: clean
	@for lib in $(LIB_DEPS); do \
		$(MAKE) -C $(HDL_LIBRARY_PATH)$${lib} clean; \
	done
	-rm -rf $(GEN_REPO)

prepare:
	@rm -rf $(GEN_REPO)
	@mkdir $(GEN_REPO)

gen-adi-ips:
	@for lib in $(LIB_DEPS); do \
		$(MAKE) -C $(HDL_LIBRARY_PATH)$${lib} xilinx || exit $$?; \
	done
	-cp -r hdl/library $(GEN_REPO)

gen-boot:
	@cp vivado_prj/pluto.runs/impl_1/system_top.bit ./bootfiles
	@touch 	bootfiles/boot.bif
	@echo "all: {system_top.bit}" >  bootfiles/boot.bif
	@bash -c "cd bootfiles; bootgen -image boot.bif -o i system_top.bit.bin -w"

load-boot:
	@sshpass -p $(BOARD_PASS) scp ./bootfiles/system_top.bit.bin $(BOARD_USER)@$(BOARD_IP):/lib/firmware
	@sshpass -p $(BOARD_PASS) ssh $(BOARD_USER)@$(BOARD_IP) "cd /lib/firmware; echo system_top.bit.bin > /sys/class/fpga_manager/fpga0/firmware"

reset-board:
	@sshpass -p $(BOARD_PASS) ssh $(BOARD_USER)@$(BOARD_IP) "echo 79024000.cf-ad9361-dds-core-lpc > /sys/bus/platform/drivers/cf_axi_dds/unbind"
	@sshpass -p $(BOARD_PASS) ssh $(BOARD_USER)@$(BOARD_IP) "echo 79020000.cf-ad9361-lpc > /sys/bus/platform/drivers/cf_axi_adc/unbind"
	@sshpass -p $(BOARD_PASS) ssh $(BOARD_USER)@$(BOARD_IP) "echo 7c400000.dma > /sys/bus/platform/drivers/dma-axi-dmac/unbind"
	@sshpass -p $(BOARD_PASS) ssh $(BOARD_USER)@$(BOARD_IP) "echo 7c420000.dma > /sys/bus/platform/drivers/dma-axi-dmac/unbind"
	@sshpass -p $(BOARD_PASS) ssh $(BOARD_USER)@$(BOARD_IP) "echo 7c420000.dma > /sys/bus/platform/drivers/dma-axi-dmac/bind"
	@sshpass -p $(BOARD_PASS) ssh $(BOARD_USER)@$(BOARD_IP) "echo 7c400000.dma > /sys/bus/platform/drivers/dma-axi-dmac/bind"
	@sshpass -p $(BOARD_PASS) ssh $(BOARD_USER)@$(BOARD_IP) "echo 79024000.cf-ad9361-dds-core-lpc > /sys/bus/platform/drivers/cf_axi_dds/bind"
	@sshpass -p $(BOARD_PASS) ssh $(BOARD_USER)@$(BOARD_IP) "echo 79020000.cf-ad9361-lpc > /sys/bus/platform/drivers/cf_axi_adc/bind"

load-reset:
	$(MAKE) load-boot
	$(MAKE) reset-board

gen-boot-load:
	$(MAKE) gen-boot
	$(MAKE) load-reset




