import netsnmp
import interface

def create_from_mib(self, conceptual_table_name):
    """Create a table query object from MIB definition."""
    table = Table(self)
    table._parse_mib(netsnmp.Varbind(conceptual_table_name, 0))
    return table

class Table(object):
    def __init__(self, session):
        self.max_repeaters = 10
        self.start_index_oid = []
        self.indexes = []
        self.columns = []
        self.netsnmp_session = session
        self._tbl_ptr = None

    def get_entries(self, iid=None, max_repeaters=10):
        """Get entries from a SNMP table, or parts of a table.

        All information required to query a table is taken from MIB.
        Required MIB files need to be present on your system.

        Args:
            varbind: The table parent object. Tag can be given as name, full name, or dotted numericals.
            iid:     Instance ID as list of integers, to limit query with to instances starting with iid.
                     Can be used to query a smaller subset of large tables efficiently. 
                     If not None, the underlying getbulk/getnext requests will be started like
                     getbulk(<parent>.<entry>.<column_1>[.<iid_1>...<iid_m>],
                             <parent>.<entry>.<column_n>[.<iid_1>...<iid_m>],
            max_repeaters: Number of conceptual column instances which are transfered at once in a getbulk response.
                           Adjust this to the number of expected rows to make the query more efficient.

        Returns:
            On success, a dictionary of dictionaries is returned.
            Outer dictionary takes a tuple of row indexes as key.
            Inner dictionary takes the conceptual column name as key.
            On error, None is returned, and related netsnmp.Session attributes
            ErrorStr, ErrorNum and ErrorInd are updated.

        """
        #if (iid):
        #    self._set_start_index(row_index)
        self.max_repeaters = max_repeaters
        res = interface.table_fetch(self, iid)
        return res

    def _parse_mib(self, varbind):
        """Determine the table structure by parsing the MIB.
        After a successful run, table headers are available in indexes and columns dictionary.
        """
        tbl_ptr = interface.table_parse_mib(self, varbind) #, self.indexes, self.columns)
        if (tbl_ptr):
            if self._tbl_ptr:
                interface.table_cleanup()
            self._tbl_ptr = tbl_ptr

    def __del__(self):
        interface.table_cleanup(self._tbl_ptr)

def str_to_varlen_iid(index_str):
    """Encodes a string to an variable-length string index iid.
    Example: str_to_vlen_iid("dave") gives [4, ord('d'), ord('a'), ord('v'), ord('e')]
    Returns: List of integers
    """
    return [len(index_str)] + [ord(element) for element in list(index_str)]

def str_to_fixlen_iid(index_str):
    """Encodes a string to an fixed-length string index iid.
    Example: str_to_vlen_iid("dave") gives [ord('d'), ord('a'), ord('v'), ord('e')]
    Returns: List of integers
    """
    return + [ord(element) for element in list(index_str)]

