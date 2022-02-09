import argparse
import logging
import os
import re
import string
import struct
import sys

_log = logging.getLogger('lwl')
_log_handler = logging.StreamHandler(sys.stdout)
_log.addHandler(_log_handler)

class LwlMsg:
    """
    This class represents the meta data for a single LWL messages.

    This data is extracted from source files, and is used to format the raw
    data.
    """

    def __init__(self, id, fmt, arg_bytes, file_path, line_num, lwl_statement):
        self.id = id
        self.fmt = fmt
        self.arg_bytes = arg_bytes
        self.file_path = file_path
        self.line_num = line_num
        self.lwl_statement = lwl_statement
        self.num_arg_bytes = 0
        for d in self.arg_bytes:
            self.num_arg_bytes += int(d)
        _log.debug('Create LwlMsg ID=%d arg_bytes=%s(%d) for %s:%d' %
                   (id, arg_bytes, self.num_arg_bytes, file_path, line_num))

class LwlMsgSet:
    """
    This class represents the set of meta data for all LWL messages.

    This data is extracted from source files, and is used to format the raw
    data. Error checking is done to ensure consistency of the message set.
    """

    def __init__(self):
        self.lwl_msgs = {}
        self.max_msg_len = 0
    def add_lwl_msg(self, id, fmt, arg_bytes, lwl_statement, file_path,
                    line_num):
        if id in self.lwl_msgs:
            print('%s:%d Duplicate ID %d: %s, previously at %s:%d: %s' %
                  (file_path, line_num, id, lwl_statement,
                   self.lwl_msgs[id].file_path,
                   self.lwl_msgs[id].line_num, 
                   self.lwl_msgs[id].lwl_statement))
            return False
        else:
            self.lwl_msgs[id] = LwlMsg(id, fmt, arg_bytes,
                                       file_path, line_num, lwl_statement)
            msg_len = 1
            for l in arg_bytes:
                msg_len += int(l)
            if msg_len > self.max_msg_len:
                self.max_msg_len = msg_len
                _log.debug('New max msg len %d for %s:%d %s' %
                           (msg_len, file_path, line_num, lwl_statement))
        return True

    def get_metadata(self, id):
        try:
            return self.lwl_msgs[id]
        except KeyError:
            return None

lwl_msg_set = LwlMsgSet()

def get_num_fmt_params(fmt):
    """ Returns the number of parameters required for a format string."""

    state = 'idle'
    num_params = 0
    for c in fmt:
        if state == 'idle':
            if c == "%":
                state = 'got_percent'
            continue
        if state == 'got_percent':
            if c == "%":
                state = 'idle'
            else:
                num_params += 1
                state = 'idle'
            continue
    _log.debug('get_num_fmt_params(%s) returns %d', fmt, num_params)
    return num_params

def parse_source_file(file_path):
    """
    Search through a source file (normally a .c or .h), find all LWL message
    defintions, and add them to the message set.

    Example statements:
        LWL(3, "Trace fmt %d", "2", abc);
        LWL(10, "Simple trace", NULL)
        LWLX(10, "System tick", "1ms", NULL)

    The message defintions are checked for syntax and consistency, and errors
    reported. The number of errors is returned.

    Multi-line LWL statements complicate this task.  But we make simplifying
    assumptions to help:
    - All non-final lines end with a comma (optionally followed by whitespace)
    - The final line ends with ");" (optionally followed by whitespace)

    """

    lwl_bid = None
    error_count = 0
    _log.debug('parse_source_file(file_path=%s)', file_path)
    lwl_bid_pat = re.compile('^\s*#define\s+LWL_BASE_ID\s+(\d+)')
    detect_lwl_pat = re.compile('^\s*LWLX?\(')
    parse_id_pat = re.compile('^\s*(LWLX?)\((\d+)')
    parse_fmt_pat = re.compile('\s*,\s*"([^"]*)"')
    parse_null_arg_string_pat = re.compile('\s*,\s*NULL')
    parse_arg_string_pat = re.compile('\s*,\s*"([^"]*)"')
    with open(file_path) as f:
        line_num = 0
        lwl_statement = None
        for line in f:
            line_num += 1
            line = line.strip()
            if not line:
                continue
            #_log.debug('line=[%s]', line)

            m = re.match(lwl_bid_pat, line)
            if m:
                lwl_bid = int(m.group(1))
                lwl_line_num = line_num
                _log.debug('%s:%d LWL_BASE_ID=%d' % (file_path, line_num, lwl_bid))
                if lwl_bid == 0:
                    print('ERROR: %s:%d: Invalid LWL base ID %d' %
                          (file_path, line_num, lwl_bid))
            if lwl_statement:
               # Check for acceptable line ending.
               if (line[-1] != ',') and (line[-2:] != ');'):
                    print('ERROR: %s:%d: Invalid LWL continuation line: %s' %
                          (file_path, line_num, line))
                    error_count += 1
                    lwl_statement = None
               else:
                    lwl_statement += line
            else:
                # Check for start of statement.
                if re.match(detect_lwl_pat, line):
                    # Check for acceptable line ending.
                    if (line[-2:] != ');') and (line[-1] != ','):
                        print('ERROR: %s:%d: Invalid LWL line ending: %s' %
                              (file_path, line_num, line))
                        error_count += 1
                    else:
                        lwl_statement = line
            if (not lwl_statement) or (lwl_statement[-2:] != ');'):
                continue

            # Got a complete statement. First get the ID
            _log.debug('Got statement: %s' % lwl_statement)
            m = re.match(parse_id_pat, lwl_statement)
            if not m:
                print('ERROR: %s:%d Cannot parse LWL ID in %s' %
                      (file_path, line_num, lwl_statement))
                error_count += 1
                lwl_statement = None
                continue
            lwl_cmd = m.group(1)
            lwl_id_offset = m.group(2)
            lwl_remain = lwl_statement[m.end():]
            _log.debug('Got ID: %s, remain=%s' % (lwl_id_offset, lwl_remain))

            # Get format string
            m = re.match(parse_fmt_pat, lwl_remain)
            if not m:
                print('ERROR: %s:%d Cannot parse LWL fmt in %s' %
                      (file_path, line_num, lwl_statement))
                error_count += 1
                lwl_statement = None
                continue

            lwl_fmt = m.group(1)
            lwl_remain = lwl_remain[m.end():]
            _log.debug('Got fmt: %s, remain=%s' % (lwl_fmt, lwl_remain))

            # If LWLX, get arg type string (or NULL)
            if lwl_cmd == 'LWLX':
                m = re.match(parse_null_arg_string_pat, lwl_remain)
                if m:
                    lwl_arg_types = ''
                else:
                    m = re.match(parse_arg_string_pat, lwl_remain)
                    if m:
                        lwl_arg_types = m.group(1)
                    else:
                        print('ERROR: %s:%d Cannot parse LWL arg types string '
                              'in %s' % (file_path, line_num, lwl_statement))
                        error_count += 1
                        lwl_statement = None
                        continue
                lwl_remain = lwl_remain[m.end():]
                _log.debug('Got lwl_arg_types: %s, remain=%s' %
                           (lwl_arg_types, lwl_remain))

            # Get arg bytes (lengths) string (or NULL)
            m = re.match(parse_null_arg_string_pat, lwl_remain)
            if m:
                lwl_arg_bytes = ''
            else:
                m = re.match(parse_arg_string_pat, lwl_remain)
                if m:
                    lwl_arg_bytes = m.group(1)
                else:
                    print('ERROR: %s:%d Cannot parse LWL arg bytes string '
                          'in %s' %(file_path, line_num, lwl_statement))
                    error_count += 1
                    lwl_statement = None
                    continue
            _log.debug('Got arg_bytes: %s' % (lwl_arg_bytes))

            if get_num_fmt_params(lwl_fmt) != len(lwl_arg_bytes):
                print('ERROR: %s:%d Inconsistent fmt and arg length strings: "%s" vs "%s"' %
                      (file_path, line_num, lwl_fmt, lwl_arg_bytes))
                error_count += 1
                lwl_statement = None
                continue

            _log.debug('[%s] [%s] [%s]' % (lwl_id_offset, lwl_fmt,
                                           lwl_arg_bytes))
            if lwl_bid is None:
                print('ERROR: %s:%d No #define LWL_BASE_ID present' %
                      (file_path, line_num))
                error_count += 1
                lwl_statement = None
                continue
            
            if not lwl_msg_set.add_lwl_msg(lwl_bid + int(lwl_id_offset),
                                           lwl_fmt, lwl_arg_bytes,
                                           lwl_statement, file_path,
                                           lwl_line_num):
                error_count += 1

            lwl_statement = None
            continue
    return error_count

def get_data_bytes(data, idx, num_bytes, start_idx, first_get):
    """
    Return next value (one or more bytes) from array and advance the index.

    Parameters:
        data (bytearray)    : Encoded data (little endian)
        num_bytes (int)     : Number of bytes in value
        start_idx (int)     : The starting point for the data (put index)
        first_get (boolean) : True if getting first byte (at start_idx)

    Return:
        d (int)   : The value
        idx (int) : The next index value

    Raises:
        EOFError when all data has been consumed.

    Note that several parameters are provided just to allow this function to
    detect when all data has been consumed.
    """
    _log.debug('get_data_bytes(idx=%d, num_bytes=%d start_idx=%d '
               'first_get=%d)' % (idx, num_bytes, start_idx, first_get))
    if idx >= len(data):
        raise IndexError("len(data)=%d idx=%d" % (len(data), idx))

    save_idx = idx
    d = 0
    factor = 1
    for loop_counter in range(num_bytes):
        if idx == start_idx and not first_get:
            raise EOFError
        d = d + data[idx]*factor
        factor = factor * 256;
        idx = idx + 1
        if idx >= len(data):
            idx = 0
    _log.debug('get_data_bytes(idx=%d, num_bytes=%d) returns (d=%d, idx=%d)',
               save_idx, num_bytes, d, idx)
    return d, idx
    
def get_optimal_start_idx(data, put_idx):
    """
    Find the optimal starting index to decode log buffer.

    Parameters:
        data (bytearray)  : Log data buffer.
        put_idx (idx)     : The index of the next buffer byte to write log data.

    Return:
        The optimal starting index in [put_idx, put_idx + max_msg_len - 1]

    This is a brute-force solution where we start at the known "put idx", add an
    offset to it, and using that starting point, determine how invalid log
    message IDs are encountered

    The maximum offset is the one less than the length of the largest log
    message (ID plus argument bytes).

    We assume that the optimal starting point is the one that minimizes the
    number of invalid log IDs.  This is not guaranteed to be the correct offset,
    but likely is.

    """

    _log.debug('get_optimal_start_idx(put_idx=%d)' % put_idx)
    min_invalid_ids = len(data)
    optimal_start_idx = None
    num_data_bytes = len(data)
    for offset in range(lwl_msg_set.max_msg_len):
        start_idx = put_idx + offset
        if (start_idx >= num_data_bytes):
            start_idx -= num_data_bytes
        idx = start_idx
        first_get = True
        invalid_id_ctr = 0

        while (True):
            try:
                id, idx = get_data_bytes(data, idx, 1, put_idx, first_get)
            except EOFError:
                break

            first_get = False
            msg_meta = lwl_msg_set.get_metadata(id)
            if msg_meta == None:
                _log.debug('Invalid ID %d at idx %d' % (id, idx));
                invalid_id_ctr += 1
                continue

            _log.debug('Valid ID %d at idx %d' % (id, idx));

            # The ID is valid. Try to get the argument bytes.
            save_idx = idx
            try:
                _, idx = get_data_bytes(data, idx, msg_meta.num_arg_bytes,
                                        put_idx, first_get)
            except EOFError:
                _log.debug('Insufficient data for aguments')
                idx = save_idx

        if invalid_id_ctr < min_invalid_ids:
            min_invalid_ids = invalid_id_ctr
            optimal_start_idx = start_idx
            _log.debug('New optimal start_idx %d invalid_id_ctr=%d' %
                       (start_idx, invalid_id_ctr))

    return optimal_start_idx

def print_data(data, start_idx, put_idx):
    """
    Print a set of messages given the raw data (bytearray) and the put index.

    Parameters:
        data (bytearray)    : Encoded data
        put_idx (int)       : Index of next byte to be written (i.e. oldest).

    Returns:
        idx (int)  

    Note that the "put index" is (was) the next location to be written, and thus
    is the oldest data, and where we start. But because the messages are
    variable length, the put index might point into the middle of a
    message. This function has to find the start of the next message, after
    which it should be "in sync".
    """

    idx = start_idx
    in_sync = False
    first_get = True
    skipped_data = []

    try:
        while True:

            # First we try to get the message ID. If we are not synced-up, we
            # might have to try several bytes to get a valid message ID.  Even
            # then, we might later find we are out of sync, and have to go back
            # and search again.

            id = None
            id_idx = None
            skipped_data = []
            msg_meta = None

            while not msg_meta:
                id_idx = idx;
                _log.debug('Get LWL id')
                id, idx = get_data_bytes(data, idx, 1, put_idx,
                                         first_get)
                first_get = False

                msg_meta = lwl_msg_set.get_metadata(id)
                if msg_meta == None:
                    id_idx = None
                    skipped_data.append(id)
                    continue

            # We have a potential ID, now get the arguments.
            arg_values = []
            for arg_bytes in msg_meta.arg_bytes:
                arg_bytes = int(arg_bytes)
                arg_value, idx = get_data_bytes(data, idx, arg_bytes,
                                                put_idx, first_get)
                arg_values.append(arg_value)
            _log.debug('id=%d arg_values=%s', id, arg_values)
            print(msg_meta.fmt % tuple(arg_values))
            _log.debug('----------------------------------------')

    except EOFError:
        pass

    if id is not None:
        # We were in the middle of a message when we ran out of data.
        # We print out the unused data and let the user figure it out.
        unused_data = bytearray()
        try:
            idx = id_idx
            while True:
                d, idx = get_data_bytes(data, idx, 1, put_idx,
                                        first_get)
                unused_data.append(d)
        except EOFError:
            pass
        print('Unused data (hex): %s' %
              ' '.join(['%02x' % tmp for tmp in unused_data]))

def parse_source_dir(dir_path):
    error_count = 0
    _log.debug('parse_source_dir(dir_path=%s)' % dir_path)

    for root_dir_path, dir_names, file_names in os.walk(dir_path):
        for file_name in file_names:
            file_path = os.path.join(root_dir_path, file_name)
            if file_path[-2:] != '.c':
                _log.debug('Skip parsing for %s' % file_path)
                continue
            error_count += parse_source_file(file_path)
    return error_count

def swap32(i):
    return struct.unpack("<I", struct.pack(">I", i))[0]

def decode_data_file(file_path):
    """
    Decode data in ASCII form from a file.

    The file lines of the file are:

    size:<buffer-size>
    put:<put-index>

    The put index is the next byte to be written, which will be an
    lwl ID.
    """

    _log.debug('decode_data_file(file_path=%s)', file_path)

    error_count = 0
    hex_data = ''
    size_pat = re.compile('^size:(\d+)')
    put_pat = re.compile('^put:(\d+)')
    buf_size = None
    put_idx = None
    with open(file_path) as f:
        line_num = 0
        lwl_statement = ''
        for line in f:
            line_num += 1
            line = line.strip()

            m = re.match(size_pat, line)
            if m:
                buf_size = int(m.group(1))
                continue
            m = re.match(put_pat, line)
            if m:
                put_idx = int(m.group(1))
                continue

            line = ''.join(line.split())
            if not line:
                continue
            #_log.debug('line=[%s]', line)
            is_hex = True
            for c in line:
                if c not in string.hexdigits:
                    is_hex = False
                    break
            if not is_hex:
                continue
            hex_data = hex_data + line

    _log.debug('Got %d hex chars', len(hex_data))

    if (put_idx is None) or (buf_size is None):
        print('Invalid put_idx %d and/or buf_size %d', (put_idx, buf_size))
        return 1;
        
    if put_idx >= buf_size:
        print('Invalid put_idx %d for buf_size %d', (put_idx, buf_size))
        return 1;

    # Verify we have an even number of chars.
    if ((len(hex_data) % 2) != 0):
        print('Odd number of hex chars (%d)' % len(hex_data))
        return 1;
    data = bytearray.fromhex(hex_data)

    start_idx = get_optimal_start_idx(data, put_idx)

    return print_data(data, start_idx, put_idx)

def main():
    _log.setLevel(logging.INFO)

    parser = argparse.ArgumentParser()
    parser.add_argument('-f',
                        help='Code file to find LWL macros definitions (can be used multiple times)')
    parser.add_argument('-d',
                        help='Code directory to find LWL macros definitions (can be used multiple times)')
    parser.add_argument('--log',
                        help='notset|debug|info|warning|error|critical',
                        default='warning')
    args = parser.parse_args()

    log_map = {
        'CRITICAL' : logging.CRITICAL,
        'ERROR' : logging.ERROR,
        'WARNING' : logging.WARNING,
        'INFO' : logging.INFO,
        'DEBUG' : logging.DEBUG,
        'NOTSET' : logging.NOTSET,
        }

    if args.log.upper() not in log_map:
        print('Invalid log level. Valid levels are (case does not matter):')
        for level, int_level in log_map.items():
            print('  %s' % (level.lower()))
        exit(1)
    _log.setLevel(log_map[args.log.upper()])

    error_count = 0
    for dir in (r'C:\Users\gene\Documents\Gene\proj\mcu-classes\modules',
                r'C:\Users\gene\Documents\Gene\proj\mcu-classes\app1'):
        error_count += parse_source_dir(dir)

    if error_count == 0:
        decode_data_file('lwl-dump.txt')

if __name__ == '__main__':
    try:
        main()
    except KeyboardInterrupt:
        print('Got keyboard interrupt')
