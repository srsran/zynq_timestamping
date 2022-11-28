import re


################################################################################
# Main
################################################################################


def parse_vivado_output_file(file):

    with open(file, encoding="UTF-8") as f:
        lines = f.readlines()

    messages = {
        "warning": [],
        "error": [],
        "info": [],
        "system": [],
        "none": [],
        "command": [],
        "phase": [],
    }

    for line in lines:
        message = get_message(line)
        message_type = message["type"]
        messages[message_type].append(message)
    return messages


def get_message(line):
    message = {"type": "none", "code": "", "body": ""}

    if line == "":
        return message

    try:
        if check_message_command(line):
            message = parse_message_command(line)

        elif check_message_step(line):
            message = parse_message_step(line)

        elif check_message_system(line):
            message = parse_message_system(line)

        elif check_message_phase(line):
            message = parse_message_phase(line)

        else:
            message = parse_message_info(line)
        return message

    except:
        return message


################################################################################
# Utils
################################################################################
def check_message_start(start_str, line):
    if line[0:len(start_str)] == start_str:
        return True
    return False


################################################################################
# Step
################################################################################
def parse_message_step(line):
    message = {
        "type": "phase",
        "body": line[len("Starting")+1:].rstrip(),
        "body": "",
        "checksum": "",
    }
    return message


def check_message_step(line):
    start_str = "Starting"
    check = check_message_start(start_str, line)
    return check


################################################################################
# Command
################################################################################
def parse_message_command(line):
    message = {"type": "command", "body": line[2:].rstrip()}
    return message


def check_message_command(line):
    if line[0] == "#" and line[1] == " " and line[2].islower():
        return True
    return False


################################################################################
# Phase
################################################################################
def parse_message_phase(line):
    message = {"type": "phase", "number": "", "body": "", "checksum": ""}
    line = line.replace("\t", " ")
    regex = r"Phase ([0-9.]+) (.+)\| Checksum: ([a-zA-Z0-9]+)"
    matches = re.finditer(regex, line, re.MULTILINE)
    for _, match in enumerate(matches, start=1):
        groups = match.groups()
        message["number"] = groups[0].rstrip()
        message["body"] = groups[1].rstrip()
        message["checksum"] = groups[2].rstrip()

    return message


def check_message_phase(line):
    if "Checksum" in line:
        start_str = "Phase "
        check = check_message_start(start_str, line)
        return check
    return False


################################################################################
# System
################################################################################
def parse_message_system(line):
    message = {
        "type": "system",
        "cpu": "",
        "elapsed": "",
        "peak": "",
        "gain": "",
        "free_physical": "",
        "free_virtual": "",
    }

    regex = r"cpu = ([0-9:]+) ; elapsed = ([0-9:]+) . Memory \(MB\): peak = ([0-9:.]+) ; gain = ([0-9:.]+) ; free physical = ([0-9:.]+) ; free virtual = ([0-9:.]+)"
    matches = re.finditer(regex, line, re.MULTILINE)
    for _, match in enumerate(matches, start=1):

        groups = match.groups()
        message["cpu"] = groups[0]
        message["elapsed"] = groups[1]
        message["peak"] = groups[2]
        message["gain"] = groups[3]
        message["free_physical"] = groups[4]
        message["free_virtual"] = groups[5]

    return message


def check_message_system(line):
    start_str = "Time (s):"
    check = check_message_start(start_str, line)
    return check


################################################################################
# Messages
################################################################################
def parse_message_info(line):
    message = {"type": "none", "code": "", "body": ""}

    regex = r"(WARNING|ERROR|INFO|CRITICAL WARNING): \[([A-Za-z\-0-9 ]*)] (.*)"
    matches = re.finditer(regex, line, re.MULTILINE)
    for _, match in enumerate(matches, start=1):

        groups = match.groups()
        if groups[0].lower() == "critical warning":
            message["type"] = "warning"
        else:
            message["type"] = groups[0].lower()
        message["code"] = groups[1]
        message["body"] = groups[2]

    return message
