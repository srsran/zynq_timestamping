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
FPGA_PART := xc7z020clg400-2

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
	@cp vivado_prj/ant.runs/impl_1/system_top.bit ./bootfiles
	@touch 	bootfiles/boot.bif
	@echo "image : {[bootloader] ./fsbl.elf  ./system_top.bit  ./u-boot.elf}" >  bootfiles/boot.bif
	@bash -c "cd bootfiles; bootgen -image boot.bif -o i BOOT.bin -w"

load-boot:
	@sshpass -p $(BOARD_PASS) ssh -o StrictHostKeyChecking=no $(BOARD_USER)@$(BOARD_IP) "mkdir /mnt/data"
	@sshpass -p $(BOARD_PASS) ssh -o StrictHostKeyChecking=no $(BOARD_USER)@$(BOARD_IP) "mount /dev/mmcblk0p1 /mnt/data"
	@sshpass -p $(BOARD_PASS) scp ./bootfiles/BOOT.bin $(BOARD_USER)@$(BOARD_IP):/mnt/data/BOOT.bin
	@sshpass -p $(BOARD_PASS) ssh -o StrictHostKeyChecking=no $(BOARD_USER)@$(BOARD_IP) "sync"
	@sshpass -p $(BOARD_PASS) ssh -o StrictHostKeyChecking=no $(BOARD_USER)@$(BOARD_IP) "umount /mnt/data"

gen-boot-load:
	$(MAKE) gen-boot
	$(MAKE) load-boot

