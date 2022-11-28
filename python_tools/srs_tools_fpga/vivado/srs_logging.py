import datetime

enable_colors = True

bcolors = {
    'HEADER': '\033[95m',
    'OKBLUE': '\033[94m',
    'OKCYAN': '\033[96m',
    'OKGREEN': '\033[92m',
    'WARNING': '\033[93m',
    'FAIL': '\033[91m',
    'ENDC': '\033[0m',
    'BOLD': '\033[1m',
    'UNDERLINE': '\033[4m',
    'GREY': '\033[94m',
}


def print_start_test(testname):
    print('\n')
    print_separator()
    print_separator()
    print_info_grey(f"Test {testname} is running...")
    print_separator()
    print_separator()


def print_separator():
    print_info_grey("******************************************************")


def print_info(msg, bold=False, date=True):
    sev = 'HEADER'
    print_msg(msg, sev, bold, date)


def print_warning(msg, bold=False, date=True):
    sev = 'WARNING'
    print_msg(msg, sev, bold, date)


def print_error(msg, bold=False, date=True):
    sev = 'FAIL'
    print_msg(msg, sev, bold, date)


def print_ok(msg, bold=False, date=True):
    sev = 'OKGREEN'
    print_msg(msg, sev, bold, date)


def print_info_grey(msg, bold=False, date=True):
    sev = 'GREY'
    print_msg(msg, sev, bold, date)


def print_msg(msg, severity, bold=False, date=True):
    init_color = bcolors[severity]
    bold_str = ''
    if bold:
        bold_str = bcolors['BOLD']
    if date:
        date_and_msg = str(datetime.datetime.now()) + ' | ' + msg
    else:
        date_and_msg = msg

    if enable_colors:
        print(bold_str + init_color + date_and_msg + bcolors['ENDC'])
    else:
        print(date_and_msg)
