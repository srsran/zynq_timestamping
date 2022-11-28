gen-boot:
	@srs-tools-fpga-generate-bootfile --boot_files_folder ./bootfiles --root_project_path ./vivado_prj --arch zynqmp

load-boot:
	@sshpass -p $(BOARD_PASS) ssh -o StrictHostKeyChecking=no $(BOARD_USER)@$(BOARD_IP) "mkdir /mnt/data"
	@sshpass -p $(BOARD_PASS) ssh -o StrictHostKeyChecking=no $(BOARD_USER)@$(BOARD_IP) "mount /dev/mmcblk0p1 /mnt/data"
	@sshpass -p $(BOARD_PASS) scp ./bootgen/BOOT.bin $(BOARD_USER)@$(BOARD_IP):/mnt/data/BOOT.bin
	@sshpass -p $(BOARD_PASS) ssh -o StrictHostKeyChecking=no $(BOARD_USER)@$(BOARD_IP) "sync"
	@sshpass -p $(BOARD_PASS) ssh -o StrictHostKeyChecking=no $(BOARD_USER)@$(BOARD_IP) "umount /mnt/data"