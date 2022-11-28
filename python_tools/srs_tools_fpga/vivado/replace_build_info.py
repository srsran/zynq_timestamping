import argparse
import re
import os
import git
import datetime
from srs_tools_fpga.vivado import srs_logging


def replace_txt(txt, regex, new_txt):
    data = re.sub(regex, new_txt, txt)
    return data


def main():
    parser = argparse.ArgumentParser(
        description="Replace build info: G_BUILD_COMMIT_HASH, G_BUILD_DATE, G_BUILD_TIME."
    )
    parser.add_argument("--input", default="./system.tcl", help="TCL input file.")

    args = parser.parse_args()
    input_file = args.input

    output_file = input_file

    with open(input_file, "r", encoding="UTF-8") as file:
        data = file.read()

    # Commit
    dirname = os.path.dirname(os.path.abspath(input_file))
    repo = git.Repo(dirname, search_parent_directories=True)
    commit_hash = "G_BUILD_COMMIT_HASH {" + f'0x{repo.git.rev_parse("HEAD")[:8]}' + "}"
    regex_0 = r"G_BUILD_COMMIT_HASH \{(.*?)\}"

    data = replace_txt(data, regex_0, commit_hash)
    msg = f"• Commit hash: {commit_hash}"
    srs_logging.print_info_grey(msg)

    todays_date = datetime.datetime.now()
    # Build date
    year = str(todays_date.year).rjust(4, "0")
    month = str(todays_date.month).rjust(2, "0")
    day = str(todays_date.day).rjust(2, "0")
    date_str = "G_BUILD_DATE {" + f"0x{year}{month}{day}" + "}"
    regex_1 = r"G_BUILD_DATE \{(.*?)\}"

    data = replace_txt(data, regex_1, date_str)

    # Build time
    hour = str(todays_date.hour).rjust(2, "0")
    minute = str(todays_date.minute).rjust(2, "0")
    second = str(todays_date.second).rjust(2, "0")
    date_str = "G_BUILD_TIME {" + f"0x{hour}{minute}{second}00" + "}"
    regex_2 = r"G_BUILD_TIME \{(.*?)\}"

    data = replace_txt(data, regex_2, date_str)

    msg = f"• Build time: {day}/{month}/{year} {hour}:{minute}:{second}"
    srs_logging.print_info_grey(msg)

    with open(output_file, "w", encoding="UTF-8") as file:
        file.write(data)


if __name__ == "__main__":
    main()
    exit(0)
