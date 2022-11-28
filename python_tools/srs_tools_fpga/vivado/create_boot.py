import argparse
import os
import shutil
from os.path import exists

DEFAULT_PRJ_PATH = '/home/carlos/Public/remote_vivado'
# DEFAULT_PRJ_PATH = 'vivado_prj'
DEFAULT_IMAGE_PATH = '/home/carlos/Public/bootfiles'
# DEFAULT_IMAGE_PATH = 'bootfiles'

def main():
    parser = argparse.ArgumentParser(description='Create the boot file.')

    parser.add_argument('--boot_files_folder', default=DEFAULT_IMAGE_PATH,
                        help='Path to the folder with Input Boot Image File (.bif)')
    parser.add_argument('--root_project_path', default=DEFAULT_PRJ_PATH)
    parser.add_argument('--arch', default='zynqmp',
                        help='Xilinx Architecture. Options: [zynq, zynqmp, fpga, versal]')

    args = parser.parse_args()

    boot_files_folder = args.boot_files_folder
    # Get .bif name
    bif_name = ''
    files_in_boot_files_folder = os.listdir(boot_files_folder)
    for file_name in files_in_boot_files_folder:
        if ".bif" in file_name:
            bif_name = file_name
            break

    prj_path = args.root_project_path
    arch = args.arch

    # Search bitstream
    folders_in_prj_path = os.listdir(prj_path)
    bitstream_path = prj_path
    for folder in folders_in_prj_path:
        if ".runs" in folder:
            bitstream_path = bitstream_path + '/' + folder
            break

    folders_in_impl_path = os.listdir(bitstream_path)
    for folder in folders_in_impl_path:
        if "impl_" in folder:
            bitstream_path = bitstream_path + '/' + folder
            break

    files_in_impl_path = os.listdir(bitstream_path)
    bitstream_name = ''
    for file_name in files_in_impl_path:
        if ".bit" in file_name:
            bitstream_name = file_name
            bitstream_path = bitstream_path + '/' + file_name
            break

    # Debug file
    bitstream_debug_name = os.path.splitext(bitstream_name)[0] + '.ltx'
    bitstream_debug_path = os.path.dirname(bitstream_path) + '/' + bitstream_debug_name

    # Create tmp dir
    PATH_OUTPUT = boot_files_folder
    if not os.path.exists(PATH_OUTPUT):
        os.mkdir(PATH_OUTPUT)

    # Copy .bit
    shutil.copy(bitstream_path, PATH_OUTPUT)
    print(f"+ Copy {bitstream_path} to {PATH_OUTPUT}")


    file_exists = exists(bitstream_debug_path)
    # Copy .ltx
    if file_exists:
        shutil.copy(bitstream_debug_path, PATH_OUTPUT)
        print(f"+ Copy {bitstream_debug_path} to {PATH_OUTPUT}")

    # Generate boot file
    boot_gen_command = f"cd {PATH_OUTPUT}; bootgen -image {bif_name} -arch {arch} -o BOOT.BIN -w"
    os.system(boot_gen_command)

    print(f"+ Boot file generated path: {PATH_OUTPUT}/BOOT.BIN")


if __name__ == '__main__':
    main()
    exit(0)
