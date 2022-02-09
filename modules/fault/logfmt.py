"""
This program interprets and formats the output of the lwl and fault
modules. Note that the fault module outputs the lwl buffer as well as other
fault information.

For the fault module, that output is (hopefully) generated when the system
crashes, just before it restarts.

If there is a console connected at the time of the crash, you can possibly cut
the fault data (text) from there. Otherwise, if the fault data was successfully
written to flash, you can use the "fault data" command to print it out. In
either case, put the data in a file, and pass it to this program.

For the lwl module, this program can interpret and format the output of the "lwl
dump" command.

This program does the following:
- Searches through your source code to get metadata about the LWL statements
  and other fault data.
- Reads in the lwl or fault data (hexadecimal text).
- Outputs the data in an easy-to-read format.

This program is picky about the module data data format. The idea is if there is
corruption, you should fix it manually, as best you can do, and then run this
program again.
"""

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

g_swab = False

################################################################################

class Data:
    """
    This class represents the fault data being processed.
    """

    # Fault data section tokens, taken from module.h. These are mapped to
    # to section IDs.

    MOD_MAGIC_FAULT = 0xdead0001
    MOD_MAGIC_LWL = 0xf00d0001
    MOD_MAGIC_TRAILER = 0xc0da0001                  

    SECTION_TYPE_FAULT = 0
    SECTION_TYPE_LWL = 1
    SECTION_TYPE_TRAILER = 2

    magic_to_fault_type = {
        MOD_MAGIC_FAULT : SECTION_TYPE_FAULT,
        MOD_MAGIC_LWL : SECTION_TYPE_LWL,
        MOD_MAGIC_TRAILER : SECTION_TYPE_TRAILER,
        }

    def __init__(self):
        self.data_array = bytearray()
        self.data_len = 0

    def read_data_file(self, file_path):
        """
        Read file containing fault data.

        Parameters:
            file_path : The file to read.

        Return:
            True if successful, otherwise False.

        The file lines have this format, where all data is hexadecimal text:
            <offset>: <raw-data>
        """

        _log.debug('read_data_file(file_path=%s)', file_path)
        line_pat = re.compile('([0-9a-fA-F]+):\s*([0-9a-fA-F]+)')
        expected_offset = 0
        with open(file_path) as f:
            line_num = 0
            for line in f:
                line_num += 1
                line = line.strip()
                #_log.debug('Got line: %s', line)

                m = re.search(line_pat, line)
                if m:
                    offset_hex = m.group(1)
                    data_hex = m.group(2)

                    if (len(offset_hex) % 2) != 0:
                        print('ERROR: %s:%d: Odd number of hex chars in offset '
                              'field (%d)' %
                              (file_path, line_num, len(offset_hex)))
                        return False

                    if (len(data_hex) % 2) != 0:
                        print('ERROR: %s:%d: Odd number of hex chars in data '
                              'field (%d)' %
                              (file_path, line_num, len(data_hex)))
                        return False

                    offset = int(offset_hex, 16)
                    if offset != expected_offset:
                        print('ERROR: %s:%d: Expected offset of 0x%08x but '
                              'got 0x%08x' %
                              (file_path, line_num, expected_offset, offset))
                        return False

                    _log.debug('Got fault data at offset 0x%08x',  offset)
                    self.data_array.extend(bytearray.fromhex(data_hex))
                    expected_offset += len(data_hex)/2

        self.data_len = len(self.data_array)
        return True

    def process_data(self):
        """
        Process fault data.

        The fault data consists of one of more sections, which have this format:
        - magic number (4 bytes)
        - number of section data bytes, starting with the magic number (4 bytes)
        - section data bytes

        This function finds each section, determines the section type based on
        the magic number, and calls other functions to "pretty print" the data.

        If corruption is found, we print a warning and continue on if possible.
        In this way, we still might be able to print some useful data. However,
        you should look at the warning, as you might be able to fix it and get
        more data.
        """

        _log.debug('process_data()')
        idx = 0
        section_type = 0
        section_len = 0

        while idx < self.data_len:
            if idx + 8 > self.data_len:
                print('ERROR: Insufficient bytes for a section header '
                      'idx=%d data_len=%d' % (idx, self.data_len))
                return False

            # Get magic of next segment and determine data type. We determine
            # endianess by trial and error.

            magic = self.get_data(idx, 4)
            if magic not in self.magic_to_fault_type:
                g_swab = not g_swab
                magic = self.get_data(idx, 4)
                if magic not in self.magic_to_fault_type:
                    print('ERROR: Can not determine section type at idx %d' %
                          (idx))
                    return False
            section_type = self.magic_to_fault_type[magic]
            section_len = self.get_data(idx + 4, 4)
            _log.debug('Got section magic=0x%08x type=%d len=%d',
                       magic, section_type, section_len)
            if idx + section_len > self.data_len:
                print('ERROR: Insufficient bytes for a section header '
                      'idx=%d section_len=%d data_len=%d' %
                      (idx, section_len, self.data_len))
                return False
            if section_type == self.SECTION_TYPE_FAULT:
                g_fault_data.pretty_print(idx, section_len)
            elif section_type == self.SECTION_TYPE_LWL:
                lwl_printer.pretty_print(idx, section_len)
            elif section_type == self.SECTION_TYPE_TRAILER:
                print('=' * 80)
                print("End of fault data")
                print('=' * 80)
            idx += section_len

        return True

    def get_data(self, idx, num_bytes):
        """
        Return value from data array as an int.

        Parameters:
            idx (int)              : The starting point for the data
            num_bytes (int)        : Number of bytes in value

        Raises:
            EOFError if out of data.

        Returns the value.

        Note that little endian encoding is assumed, but if g_swab is True, we
        use big endian.
        """

        _log.debug('get_data(idx=%d, num_bytes=%d)', idx, num_bytes)

        save_idx = idx
        value = 0
        shift = 0

        for loop_counter in range(num_bytes):
            if idx >= self.data_len:
                _log.debug('get_data() out of bytes')
                raise EOFError
            if g_swab:
                value = (value << 8) + self.data_array[idx]
            else:
                value = value + (self.data_array[idx] << shift)
                shift += 8
            idx = idx + 1

        _log.debug('get_data(idx=%d, num_bytes=%d) returns %d)',
                   save_idx, num_bytes, value)
        return value

    def get_data_circ(self, idx, num_bytes, buf_start_idx, buf_len,
                      put_idx, first_get):
        """
        Return value from a circular buffer (within the data array) as an int.

        Parameters:
            idx (int)           : Current (relative) idx into buffer.
            num_bytes (int)     : Number of bytes in value.
            buf_start_idx (int) : Starting index of buffer in data array.
            buf_len (int)       : Number of bytes in the buffer.
            put_idx (int)       : Buffer put_idx value.
            first_get (boolean) : True if getting first data from the buffer.

        Raises:
            EOFError if out of data

        Note that big endian encoding is assumed.
        """

        save_idx = idx
        _log.debug('get_data_circ(idx=%d num_bytes=%d buf_start_idx=%d '
                   'buf_len=%d first_get=%s)',
                   idx, num_bytes, buf_start_idx, buf_len, first_get)
        if idx >= buf_len:
            raise IndexError('ERROR: Invalid idx %d in get_data_circ' % idx)
        if put_idx >= buf_len:
            raise IndexError('ERROR: Invalid put_idx %d in get_data_circ' % idx)

        value = 0
        for loop_counter in range(num_bytes):
            if idx == put_idx and not first_get:
                raise EOFError
            value = (value << 8) + self.data_array[buf_start_idx + idx]
            idx = idx + 1
            if idx >= buf_len:
                idx = 0
        _log.debug('get_data_circ(idx=%d, num_bytes=%d) returns (d=%d, idx=%d)',
                   save_idx, num_bytes, value, idx)
        return value, idx

    def get_bytes_left_circ(self, idx,num_to_check_for, buf_start_idx, buf_len,
                            put_idx, first_get):
        """
        Check if there are at least N bytes left in buffer.

        Parameters:
            idx (int)              : Current (relative) idx into buffer.
            num_to_check_for (int) : Number of bytes to check for.
            buf_start_idx (int)    : Starting index of buffer in data array.
            buf_len (int)          : Number of bytes in the buffer.
            put_idx (int)          : Buffer put_idx value.
            first_get (boolean)    : True if getting first data from the buffer.
        """

        save_idx = idx
        _log.debug('get_bytes_left_circ(idx=%d num_to_check_for=%d '
                   'buf_start_idx=%d buf_len=%d first_get=%s)',
                   idx, num_to_check_for, buf_start_idx, buf_len, first_get)
        if idx >= buf_len:
            raise IndexError('ERROR: Invalid idx %d in get_data_circ' % idx)
        if put_idx >= buf_len:
            raise IndexError('ERROR: Invalid put_idx %d in get_data_circ' % idx)
        bytes_left = 0

        for loop_counter in range(num_to_check_for):
            if idx == put_idx and not first_get:
                break
            idx += 1
            if idx >= buf_len:
                idx = 0
            bytes_left += 1
        _log.debug('get_bytes_left_circ(idx=%d, num_to_check_for=%d) returns '
                   '(bytes_left=%d, idx=%d)',
                   save_idx, num_to_check_for, bytes_left, idx)
        return bytes_left, idx

################################################################################

g_data = Data()

################################################################################

class FaultField:

    def __init__(self, name, offset, num_bytes):
        self.name = name
        self.offset = offset
        self.num_bytes = num_bytes
        _log.debug('Create FaultField name=%s offset=%d num_bytes=%d',
                   self.name, self.offset, self.num_bytes)

################################################################################

class FaultData:

    def __init__(self):
        self.fields = []
        self.max_name_len = 0

    def add_fault_field(self, name, offset, num_bytes):
        """
        Add a fault information field.

        Parameters:
            name (str)      : Name of field.
            offset (int)    : Offset of field in section data.
            num_bytes (int) : Number of bytes for field.
        """

        self.fields.append(FaultField(name, offset, num_bytes))
        if len(name) > self.max_name_len:
            self.max_name_len = len(name)

    def pretty_print(self, section_offset, section_len):
        """
        Pretty print the fault data.

        Parameters:
            section_offset (int) : Data array index of start of the section.
            section_len (int)    : Length of segment in bytes.
        """

        print('=' * 80)
        print('Fault data')
        print('=' * 80)
        for field in self.fields:
            if (field.name.startswith('pad') or
                field.name in ('magic', 'num_section_bytes')):
                continue
            
            value = g_data.get_data(section_offset + field.offset,
                                    field.num_bytes)
            print('%*s: 0x%08x (%d)' %
                  (self.max_name_len, field.name, value, value))

################################################################################

g_fault_data = FaultData()

################################################################################

################################################################################

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
        _log.debug('Create LwlMsg ID=%d arg_bytes=%s(%d) for %s:%d',
                   id, arg_bytes, self.num_arg_bytes, file_path, line_num)

################################################################################

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
        """
        Add a LWL message.

        Parameters:
            id (int)            : LWL statement ID (must be unique).
            fmt (str)           : LWL statement format string.
            arg_bytes (int)     : Number of argument bytes.
            lwl_statement (str) : Full LWL statement text.
            file_path (str)     : File name containing LWL statement.
            line_num (int)      : Line number of LWL statement.
        """

        if id in self.lwl_msgs:
            print('%s:%d Duplicate LWL ID %d: %s, previously at %s:%d: %s' %
                  (file_path, line_num, id, lwl_statement,
                   self.lwl_msgs[id].file_path,
                   self.lwl_msgs[id].line_num, 
                   self.lwl_msgs[id].lwl_statement))
            return False
        else:
            self.lwl_msgs[id] = LwlMsg(id, fmt, arg_bytes, file_path,
                                       line_num, lwl_statement)
            msg_len = 1
            for len in arg_bytes:
                msg_len += int(len)
            if msg_len > self.max_msg_len:
                self.max_msg_len = msg_len
                _log.debug('New max msg len %d for %s:%d %s',
                           msg_len, file_path, line_num, lwl_statement)
        return True

    def get_metadata(self, id):
        try:
            return self.lwl_msgs[id]
        except KeyError:
            return None

    def get_num_lwl_statements(self):
        return len(self.lwl_msgs);

################################################################################

g_lwl_msg_set = LwlMsgSet()

################################################################################

class LwlPrinter:

    def pretty_print(self, section_offset, section_len):
        """
        Pretty print the LWL messages.

        Parameters:
            section_offset (int) : Data array index of start of the section.
            section_len (int)    : Length of segment in bytes.

        Note that the "put index" is (was) the next location to be written, and
        thus is the oldest data in the circular buffer. This is where we
        start. But because the messages are variable length, the put index might
        point into the middle of a message. Thus, this function has to get in
        sync with the message boundary, using some ad-hoc process.
        """

        _log.debug('pretty_print(section_offset=%d section_len=%d',
                   section_offset, section_len)

        # Following the section header (magic and length) 
        print('=' * 80)
        print('LWL')
        print('=' * 80)

        # The section layout is as follows:
        #
        # Offset size name
        # ------ ---- ----
        #    0     4  magic
        #    4     4  num_section_bytes
        #    8     4  buf_size
        #   12     4  put_idx
        #   16     N  buf

        # Assume a minimum buffer size of 4, meaning we need at least 20 bytes.
        if section_len < 20:
            print('ERROR: Insufficient LWL buffer size - %d bytes' %
                  section_len);
            return

        buf_len = g_data.get_data(section_offset + 8, 4)
        put_idx = g_data.get_data(section_offset + 12, 4)
        buf_start_idx = section_offset + 16

        _log.debug('pretty_print buf_len=%d put_idx=%d buf_start_idx=%d',
                   buf_len, put_idx, buf_start_idx)

        if buf_len + 16 != section_len:
            print('ERROR: Invalid LWL buf_len: %d bytes' % buf_len)
            return
        if put_idx >= buf_len:
            print('ERROR: Invalid lwl put_idx: %d')
            return

        idx =  self.get_optimal_start_idx(buf_start_idx, buf_len, put_idx)
        first_get = True
        skipped_data = []

        try:        
            while True:
                # First we try to get the message ID. If we are not synced-up,
                # we might have to try several bytes to get a valid message ID.
                # Even if we get a valid message ID, it might be some random
                # data that was a message ID.

                id = None
                id_idx = None
                skipped_data = []
                msg_meta = None

                while True:
                    id_idx = idx;

                    id, idx = g_data.get_data_circ(
                        idx, 1, buf_start_idx, buf_len, put_idx, first_get)
                    first_get = False

                    _log.debug('Got candidate id %d', id)

                    msg_meta = g_lwl_msg_set.get_metadata(id)
                    if msg_meta == None:
                        id_idx = None
                        skipped_data.append(id)
                        continue
                    break

                # We have a potential ID, now get the arguments.
                if skipped_data:
                    print('Skipped data (hex): %s' %
                          ' '.join(['%02x' % tmp for tmp in skipped_data]))

                arg_values = []
                for arg_bytes in msg_meta.arg_bytes:
                    arg_bytes = int(arg_bytes)
                    arg_value, idx = g_data.get_data_circ(
                        idx, arg_bytes, buf_start_idx, buf_len,
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
                    value, idx = g_data.get_data_circ(
                        idx, 1, buf_start_idx, buf_len, put_idx, first_get)
                    unused_data.append(value)
            except EOFError:
                pass
            print('Unused data (hex): %s' %
                  ' '.join(['%02x' % tmp for tmp in unused_data]))

    def get_optimal_start_idx(self, buf_start_idx, buf_len, put_idx):
        """
        Find the optimal starting index to decode log buffer.

        Parameters:
            buf_start_idx (int) : Start of LWL buffer in the fault data.
            buf_len (int)       : Length of LWL buffer.
            put_idx (int)       : LWL buffer put_idx.

        Return:
            The optimal starting index (>= put_idx)

        This is a brute-force solution where we start at the known "put_idx",
        add an offset to it, and using that starting point, determine how
        many invalid log message IDs are encountered.

        The start offset (relative to the put index) is the largest number
        of argument bytes for any LWL statement. This is for the case where
        just the ID for such an LWL statement got overwritten.

        We assume that the optimal starting point is the one that minimizes the
        number of invalid log IDs.  This is not guaranteed to be the truely
        optimal offset, but likely is.
        """

        _log.debug('get_optimal_start_idx() put_idx=%d max_msg_len=%d', put_idx,
                   g_lwl_msg_set.max_msg_len)
        optimal_invalid_ids = buf_len
        optimal_start_idx = None
        optimal_bytes_remaining = buf_len

        for offset in range(g_lwl_msg_set.max_msg_len + 1):
            start_idx = put_idx + offset
            if start_idx >= buf_len:
                start_idx -= buf_len
            _log.debug('Check for offset=%d start_idx=%d', offset, start_idx)
            idx = start_idx
            first_get = offset == 0
            invalid_id_ctr = 0
            valid_id_ctr = 0
            bytes_left = 0

            while (True):
                # Try to get ID.
                try:
                    id, idx = g_data.get_data_circ(
                        idx, 1, buf_start_idx, buf_len, put_idx, first_get)
                except EOFError:
                    break

                first_get = False
                msg_meta = g_lwl_msg_set.get_metadata(id)
                if msg_meta == None:
                    _log.debug('Invalid ID %d at idx %d', id, idx);
                    invalid_id_ctr += 1
                    continue

                _log.debug('Valid ID %d at idx %d', id, idx);

                # The ID is valid. Try to get the argument bytes.
                save_idx = idx
                bytes_left, idx = g_data.get_bytes_left_circ(
                    idx, msg_meta.num_arg_bytes, buf_start_idx,
                    buf_len, put_idx, first_get)
                if bytes_left < msg_meta.num_arg_bytes:
                    _log.debug('Insufficient data for aguments')
                    # Add byte for ID.
                    bytes_left += 1
                    break
                valid_id_ctr += 1

            _log.debug('With offset=%d start_idx=%d invalid_id_ctr=%d '
                       'valid_id_ctr=%d bytes_left=%d',
                       offset, start_idx, invalid_id_ctr, valid_id_ctr,
                       bytes_left)

            if invalid_id_ctr < optimal_invalid_ids:
                optimal_invalid_ids = invalid_id_ctr
                optimal_start_idx = start_idx
                optimal_bytes_left = bytes_left
                _log.debug('New optimal start_idx %d put_idx %d '
                           'invalid_id_ctr=%d valid_id_ctr=%d bytes_left=%d',
                           start_idx, put_idx, invalid_id_ctr, valid_id_ctr,
                           bytes_left)

        return optimal_start_idx

################################################################################

lwl_printer = LwlPrinter()

################################################################################

class SourceParser:

    def parse_source_dir(self, dir_path):
        error_count = 0
        _log.debug('parse_source_dir(dir_path=%s)', dir_path)

        for root_dir_path, dir_names, file_names in os.walk(dir_path):
            for file_name in file_names:
                file_path = os.path.join(root_dir_path, file_name)
                if file_path[-2:] != '.c':
                    _log.debug('Skip parsing for %s', file_path)
                    continue
                error_count += self.parse_source_file(file_path)
        return error_count

    def parse_source_file(self, file_path):
        """
        Search through a source file (normally a .c or .h), finding meta data.
        There are lwl statements, and fault data descriptions.

        lwl statements:

            Each file using lwl needs to have a statement like this before any
            lwl statement:

                #define LWL_BASE_ID 10
                #define LWL_NUM 5

            Example statements:
                LWL("Trace fmt %d", 2, LWL_2(abc));
                LWL("Simple trace", 0)

            The message defintions are checked for syntax and consistency, and
            errors reported.

            Multi-line LWL statements complicate this task.  But we make
            simplifying assumptions to help:
            - All non-final lines end with a comma (optionally followed by
              whitespace)
            - The final line ends with ");" (optionally followed by whitespace)

        fault data descriptions:

            A snippet:

                struct fault_data
                {
                    uint32_t magic;              //@fault_data,magic,4
                    uint32_t num_section_bytes;  //@fault_data,num_section_bytes,4

                    uint32_t fault_type;         //@fault_data,fault_type,4
                    uint32_t fault_param;        //@fault_data,fault_param,4
                    uint32_t return_addr;        //@fault_data,return_addr,4

                    :
                    :

            We assume fault data defintions are always on a single line.

        """

        _log.debug('parse_source_file(file_path=%s)', file_path)

        error_count = 0
        lwl_base_id = None
        fault_field_offset = 0

        parse_fault_data_pat = re.compile('//@fault_data,([^,]+),(\d+)')

        lwl_base_id_pat = re.compile('^\s*#define\s+LWL_BASE_ID\s+(\d+)')
        lwl_num_pat = re.compile('^\s*#define\s+LWL_NUM\s+(\d+)')
        detect_lwl_pat = re.compile('^\s*LWL\(')
        parse_fmt_pat = re.compile('\s*LWL\("([^"]*)"')
        parse_zero_pat = re.compile('\s*,\s*0')
        parse_arg_string_pat = re.compile('\s*,\s*"([^"]*)"')
        parse_num_arg_bytes_pat = re.compile('\s*,\s*([0-9]+)')
        parse_arg_pat = re.compile(',\s*LWL_([\d])\(')
        with open(file_path) as f:
            line_num = 0
            lwl_id_offset = -1
            lwl_statement = None
            for line in f:
                line_num += 1
                line = line.strip()
                if not line:
                    continue
                #_log.debug('line=[%s]', line)

                # Start with fault data descriptions.
                m = re.search(parse_fault_data_pat, line)
                if m:
                    name = m.group(1)
                    num_bytes = int(m.group(2))
                    g_fault_data.add_fault_field(name, fault_field_offset,
                                                 num_bytes)
                    fault_field_offset += num_bytes
                    continue

                # Now for lwl statements.
                m = re.match(lwl_base_id_pat, line)
                if m:
                    lwl_base_id = int(m.group(1))
                    lwl_line_num = line_num
                    _log.debug('%s:%d LWL_BASE_ID=%d',
                               file_path, line_num, lwl_base_id)
                    if lwl_base_id == 0:
                        print('ERROR: %s:%d: Invalid LWL base ID %d' %
                              (file_path, line_num, lwl_base_id))
                if lwl_statement:
                   # Check for acceptable line ending.
                   if (line[-1] != ',') and (line[-2:] != ');'):
                        print('ERROR: %s:%d: Invalid LWL continuation line: %s'
                              % (file_path, line_num, line))
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

                # Got a complete statement. First get the format.
                _log.debug('Got statement: %s', lwl_statement)
                lwl_id_offset += 1
                m = re.match(parse_fmt_pat, lwl_statement)
                if not m:
                    print('ERROR: %s:%d Cannot parse LWL fmt in %s' %
                          (file_path, line_num, lwl_statement))
                    error_count += 1
                    lwl_statement = None
                    continue

                lwl_fmt = m.group(1)
                lwl_remain = lwl_statement[m.end():]
                _log.debug('Got fmt: %s, remain: %s', lwl_fmt, lwl_remain)

                # Get num_arg_bytes
                m = re.match(parse_num_arg_bytes_pat, lwl_remain)
                if m:
                    lwl_num_arg_bytes = int(m.group(1))
                else:
                    print('ERROR: %s:%d Cannot parse LWL num arg bytes '
                          'parameter in %s' %(file_path, line_num,
                                              lwl_statement))
                    error_count += 1
                    lwl_statement = None
                    continue
                lwl_remain = lwl_remain[m.end():]
                _log.debug('Got num_arg_bytes: %d, remain: %s',
                           lwl_num_arg_bytes, lwl_remain)

                # Get the arg lengths
                lwl_arg_lengths = ''
                num_arg_bytes_check = 0
                while True:
                    m = re.search(parse_arg_pat, lwl_remain)
                    if m:
                        lwl_arg_lengths += m.group(1)
                        num_arg_bytes_check += int(m.group(1))
                        lwl_remain = lwl_remain[m.end():]
                        _log.debug('Got arg length: %s, remain: %s',
                                   m.group(1), lwl_remain)
                    else:
                        _log.debug('Out of args')
                        break

                if num_arg_bytes_check != lwl_num_arg_bytes:
                    print('ERROR: %s:%d Inconsistent num arg bytes '
                          '(%d vs %d) in %s' %(file_path, line_num,
                                               num_arg_bytes_check,
                                               lwl_num_arg_bytes,
                                               lwl_statement))

                _log.debug('[%s] [%s] [%d] [%s]', lwl_id_offset, lwl_fmt,
                           lwl_num_arg_bytes, lwl_arg_lengths)
                if lwl_base_id is None:
                    print('ERROR: %s:%d No #define LWL_BASE_ID present' %
                          (file_path, line_num))
                    error_count += 1
                    lwl_statement = None
                    continue
                
                if not g_lwl_msg_set.add_lwl_msg(lwl_base_id +
                                                 int(lwl_id_offset),
                                                 lwl_fmt, lwl_arg_lengths,
                                                 lwl_statement, file_path,
                                                 lwl_line_num):
                    error_count += 1

                lwl_statement = None
                continue
        return error_count

    def get_num_fmt_params(self, fmt):
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

################################################################################

source_parser = SourceParser()

################################################################################


def main():
    _log.setLevel(logging.INFO)

    parser = argparse.ArgumentParser()
    parser.add_argument('-f',
                        help='Fault data file (default=None)',
                        default=None)
    parser.add_argument('-d', action='append', nargs='+',
                        help='Code directory to search for LWL statements and '
                        'fault metadata -- can specify multipe space-separated '
                        'directories, and can be used multiple times')
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
        print('ERROR: Invalid log level. Valid levels are (case does not matter):')
        for level, int_level in log_map.items():
            print('  %s' % (level.lower()))
        exit(1)
    _log.setLevel(log_map[args.log.upper()])

    error_count = 0
    if not args.d:
        print('ERROR: No code directories specified')
        sys.exit(1)

    for dir_list in args.d:
        for dir in dir_list:
            print('INFO: Checking for source code files under directory: %s' % dir)
            error_count += source_parser.parse_source_dir(dir)
            print('INFO: Source code parsing complete, number of errors so far: %d' %
                  error_count)
            print('INFO: Number of LWL statements so far: %d' %
                  g_lwl_msg_set.get_num_lwl_statements())
    if error_count != 0:
        print('WARNING: Errors detected parsing source code')

    if args.f is None:
        print('WARN: No data file provided.')
    else:
        try:
            if g_data.read_data_file(args.f):
                g_data.process_data()
        except FileNotFoundError:
            print('ERROR: Cannot open file: %s' % args.f)
        
if __name__ == '__main__':
    try:
        main()
    except KeyboardInterrupt:
        print('ERROR: Got keyboard interrupt')
