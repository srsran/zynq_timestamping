import os
import argparse
import subprocess
import logging
from rich.console import Console
from rich.live import Live
from rich.panel import Panel
from rich.progress import (
    Progress,
    SpinnerColumn,
    BarColumn,
    TextColumn,
    Group,
    TimeElapsedColumn,
)
from rich.logging import RichHandler
from srs_tools_fpga.vivado import vivado_logging_parser


class Vivado_executor:
    def __init__(self):
        self.command_term = Progress(
            TextColumn("{task.description}", markup=False), expand=True
        )
        self.command_term_id = self.command_term.add_task("")
        self.console = Console()

    def execute_project(self, input_path, severity, arguments):
        output_str = ""

        with open(input_path, encoding="UTF-8") as f:
            number_of_lines = len(f.readlines())

        input_path_directory = os.path.abspath(os.path.dirname(input_path))
        vivado_cmd = f"vivado -verbose -mode batch -source {input_path}"
        if arguments != "":
            vivado_cmd += f" -tclargs {arguments}"

        # progress bar for command
        self.all_prog = Progress(
            TimeElapsedColumn(),
            BarColumn(),
            TextColumn("{task.percentage:>3.0f}% ({task.completed}/{task.total})"),
            TextColumn("{task.description}"),
        )
        self.all_id = self.all_prog.add_task("", total=number_of_lines)

        # progress bar for steps in bar_items
        self.step_prog = Progress(
            SpinnerColumn("dots"),
            TextColumn("[purple]{task.description}"),
        )
        self.step_id = self.step_prog.add_task("Executing Vivado", step=0)

        command_panel = Panel(
            self.command_term,
            title="Running Vivado",
            subtitle=vivado_cmd,
            border_style="red",
        )

        with Live(Group(command_panel, self.all_prog, self.step_prog)) as live:
            process = subprocess.Popen(
                vivado_cmd.split(" "),
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                cwd=input_path_directory,
            )

            for line in iter(process.stdout.readline, b""):
                line_vivado = line.decode("utf-8")
                output_str += line_vivado
                line_vivado = line_vivado.strip()

                if line_vivado != "":
                    self.update_term(line_vivado, live, severity)

            filename_out = input_path_directory + "/vivado_output_log.txt"
            with open(filename_out, "w") as out:
                out.write(output_str)

            process.communicate()[0]
            returncode = process.returncode
            if returncode == 0:
                self.all_prog.advance(self.all_id, number_of_lines)
                self.step_prog.update(
                    self.step_id,
                    description=f"[green]Complete, check the log in:[/green] {filename_out}",
                )
            else:
                self.step_prog.update(
                    self.step_id,
                    description=f"[red]Error in Vivado, check the log in:[/red] {filename_out}",
                )
                exit(-1)

    def update_term(self, line, live, severity):
        message = vivado_logging_parser.get_message(line)
        if message["type"] == "":
            self.all_prog.advance(self.all_id, 1)
            self.step_prog.advance(self.step_id, 1)

            return

        if message["type"] == "command":
            self.all_prog.advance(self.all_id, 1)
            self.step_prog.advance(self.step_id, 1)

            self.command_term.update(
                self.command_term_id, description=message["body"], advance=1
            )
            self.command_term.reset(self.command_term_id)
            live.refresh()
        else:
            FORMAT = "%(message)s"
            logging.basicConfig(
                level=severity.upper(),
                format=FORMAT,
                datefmt="[%X]",
                handlers=[RichHandler(show_path=False, show_level=False)],
            )

            log = logging.getLogger("rich")

            if message["type"] == "warning":
                log.warning(message["body"])
            elif message["type"] == "info":
                log.info(message["body"])
            elif message["type"] == "error":
                log.error(message["body"])


########################################################################################################################
# Main
########################################################################################################################
def main():
    parser = argparse.ArgumentParser(description="Execute a Vivado TCL project.")

    parser.add_argument("--input", default="./project.tcl", help="TCL project file")

    parser.add_argument("--args", default="", help="Arguments passed to TCL file.")

    parser.add_argument(
        "--severity",
        choices=["error", "warning", "info"],
        default="info",
        help="Severity logging. error > warning > info. E.g: if you set warning it will show warning and info messages",
    )

    args = parser.parse_args()

    current_dir = os.getcwd()
    input_path = os.path.join(current_dir, args.input)
    arguments = args.args

    severity = args.severity

    vivado_executor = Vivado_executor()
    vivado_executor.execute_project(input_path, severity, arguments)


if __name__ == "__main__":
    main()
    exit(0)
