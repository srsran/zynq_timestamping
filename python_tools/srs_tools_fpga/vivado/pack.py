import os
import argparse
import subprocess
import shutil
import sys
import time
import pathlib
import yaml
from srs_tools_fpga.vivado import srs_logging
from rich.console import Console
from rich.table import Table
from jinja2 import Template

TCL_NAME = "custom_ip_script.tcl"


def create_tcl(part, ip_declaration_path, debug_mode):
    output_path = os.path.join(os.path.dirname(ip_declaration_path), TCL_NAME)

    with open(ip_declaration_path, "r") as file:
        data = file.read()
        config = yaml.safe_load(data)

    if debug_mode is True and "hdl_sources_debug" in config:
        config["hdl_sources"] = config["hdl_sources_debug"]
    else:
        config["hdl_sources"] = config["hdl_sources"]

    if "xilinx_ips" not in config:
        config["xilinx_ips"] = []
    else:
        config["xilinx_ips"] = [config["xilinx_ips"]]

    config["part"] = part

    if "constraint_sources" not in config:
        config["constraint_sources"] = []

    if "extra_ip_config" not in config:
        config["extra_ip_config"] = []
    else:
        config["extra_ip_config"] = [config["extra_ip_config"]]

    if "readme" not in config:
        config["readme"] = ""

    if "resets" not in config:
        config["resets"] = []

    config["logo"] = os.path.join(str(pathlib.Path(__file__).parent), "logo_srs.png")

    # Configure sources
    hdl_sources = config["hdl_sources"]
    for i in range(0, len(hdl_sources)):
        hdl_file = hdl_sources[i]
        if (hdl_file.startswith("$SRS_")):
            try:
                hdl_sources[i] = os.getenv(hdl_file[1:])
            except Exception as e:
                msg = f"Set the enviroment variable: {hdl_file[1:]}"
                srs_logging.print_error(msg)
        else:
            hdl_file = f"$script_path/{hdl_file}"
    config["hdl_sources"] = hdl_sources

    # Get template
    template_path = os.path.join(str(pathlib.Path(__file__).parent), "ip_template.j2")
    with open(template_path, encoding="UTF-8") as file_i:
        template_vivado = Template(file_i.read())

    tcl_script = str(template_vivado.render(config=config))

    with open(ip_declaration_path, "r") as file:
        data = file.read()
        config = yaml.safe_load(data)

    if os.path.exists(output_path):
        os.remove(output_path)

    with open(output_path, "w") as text_file:
        text_file.write(tcl_script)
        text_file.close()


def get_childs_directory(directory):
    return [
        name
        for name in os.listdir(directory)
        if os.path.isdir(os.path.join(directory, name))
    ]


def create_tcl_and_pack(part, input_path, output_dir, debug_mode):
    console = Console(log_path=False)

    ip_name = get_name_of_ip(input_path)

    with console.status(f"[bold green]Working on {ip_name}...") as _:
        tcl_path = os.path.abspath(os.path.join(os.path.dirname(input_path), TCL_NAME))

        create_tcl(part, input_path, debug_mode)

        input_path_directory = os.path.abspath(os.path.dirname(input_path))

        vivado_output_directory = os.path.join(
            input_path_directory, "ip_repo_generated"
        )
        # Remove last IP pack
        if os.path.exists(vivado_output_directory):
            shutil.rmtree(vivado_output_directory)
        # Call to vivado
        pack_command = f"vivado -mode batch -source {tcl_path}"

        start = time.time()
        try:
            subprocess.check_output(
                pack_command.split(" "),
                cwd=input_path_directory,
                stderr=subprocess.STDOUT,
            )
        except subprocess.CalledProcessError as e:
            srs_logging.print_error(e.output.decode(sys.getfilesystemencoding()))
            console.log(
                f"{ip_name} [bold red]error[/bold red] :thumbs_down:", emoji=True
            )
            exit(-1)
        end = time.time()
        duration = round(end - start, 2)

        # Copy directory
        ip_directory = get_childs_directory(vivado_output_directory)[0]
        complete_ip_directory_output = os.path.join(output_dir, ip_directory)
        complete_ip_directory_source = os.path.join(
            vivado_output_directory, ip_directory
        )
        # Remove last IP pack
        if os.path.exists(complete_ip_directory_output):
            shutil.rmtree(complete_ip_directory_output)
        shutil.copytree(complete_ip_directory_source, complete_ip_directory_output)

        console.log(
            f"{ip_name} [yellow](debug_mode = {debug_mode})[/yellow] [bold green]complete[/bold green] ({duration} seconds) :thumbs_up:",
            emoji=True,
        )


def print_summary(build_summpary, total_duration, ips_not_found):
    console = Console()
    table = Table(show_header=True, header_style="bold magenta")
    table.add_column("IP name")
    table.add_column("Duration (s)", style="dim")

    build_summpary_ordered = sorted(
        build_summpary, key=lambda d: d["duration"], reverse=True
    )

    for build in build_summpary_ordered:
        table.add_row(build["name"], str(round(build["duration"], 2)))
    table.add_row("[bold]Total duration[/bold]", str(round(total_duration, 2)))
    for ip in ips_not_found:
        table.add_row(ip, "[bold red]IP not found[/bold red]")
    console.print(table)


def get_name_of_ip(yml_file):
    with open(yml_file, "r") as file:
        data = file.read()
        config = yaml.safe_load(data)
    name = config["name"]
    return name


########################################################################################################################
# Main
########################################################################################################################
def main():
    parser = argparse.ArgumentParser(description="Create a tcl script to pack an IP.")

    parser.add_argument(
        "--mode",
        choices=["create_tcl", "create_tcl_and_pack", "pack_ip_list"],
        help="create_tcl: creates a tcl script to pack an IP, create_tcl_and_pack: creates a tcl script and packs the \
        IP, pack_ip_list: packs IPs from a list",
    )

    parser.add_argument("--output", default="./", help="Output directory.")

    parser.add_argument(
        "--ip_list",
        default="",
        help="When mode=pack_ip_list it allows to select some IPs. E.g: timestamped_ifft,find_pss. If empty it will pack all IPs.",
    )

    parser.add_argument(
        "--input",
        default="./ip.yml",
        help="File path to the .yml declaration: IP configuration (for modes create_tcl and create_tcl_and_pack) or IP list (for mode pack_ip_list)",
    )

    parser.add_argument(
        "--fpga_part",
        default="xczu28dr-ffvg1517-2-e",
        help="FPGA part number. E.g: xczu28dr-ffvg1517-2-e",
    )

    parser.add_argument(
        "--debug_mode", action="store_true", help="Pack the IPs in debug mode."
    )

    args = parser.parse_args()

    current_dir = os.getcwd()

    mode = args.mode
    part = args.fpga_part
    ip_list = args.ip_list
    debug_mode = args.debug_mode
    input_path = os.path.join(current_dir, args.input)
    output_dir = os.path.join(current_dir, args.output)

    if mode == "create_tcl":
        create_tcl(part, input_path, debug_mode)

    elif mode == "create_tcl_and_pack":
        create_tcl_and_pack(part, input_path, output_dir, debug_mode)

    elif mode == "pack_ip_list":
        all_compile = False
        ips = []

        ip_list_path = os.path.join(current_dir, ip_list)
        if os.path.isfile(ip_list_path):
            with open(ip_list_path, encoding="UTF-8") as f:
                ip_list_str = f.read()

            if ip_list_str == "":
                all_compile = True
            ips = ip_list_str.split("\n")
        else:
            if ip_list == "":
                all_compile = True
            ips = ip_list.split(",")

        with open(input_path, "r", encoding="UTF-8") as file:
            data = file.read()
            ip_list_yml = yaml.safe_load(data)

        build_summary = []
        total_duration = 0
        for ip in ip_list_yml:
            name = ip["name"]
            if all_compile is True or name in ips:
                ips.remove(name)
                input_path_config = os.path.join(
                    os.path.dirname(input_path), ip["config_path"]
                )
                start = time.time()
                create_tcl_and_pack(part, input_path_config, output_dir, debug_mode)
                end = time.time()
                duration = end - start
                total_duration += duration
                build_summary.append({"name": name, "duration": duration})

        print_summary(build_summary, total_duration, ips)


if __name__ == "__main__":
    main()
    exit(0)
