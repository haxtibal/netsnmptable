import netsnmp
import interface

class Table(object):
    def __init__(self, session):
        self.max_repeaters = 10
        self.start_index_oid = []
        self.indexes = []
        self.columns = []
        self.netsnmp_session = session
        self._tbl_ptr = None

    def parse_mib(self, varbind):
        """Determine the table structure by parsing the MIB.
        After a successful run, table headers are available in indexes and columns dictionary.
        """
        tbl_ptr = interface.table_parse_mib(varbind, self.indexes, self.columns)
        if (tbl_ptr):
            if self._tbl_ptr:
                interface.table_cleanup()
            self._tbl_ptr = tbl_ptr

    def fetch(self, row_index=None, max_repeaters = 10):
        """Fetch a SNMP table, or parts of a table.

        All information required to query a table is taken from MIB.
        Required MIB files need to be present on your system.

        Args:
            varbind:   The table parent object. Tag can be given as name, full name, or dotted numericals.
            row_index: Tuple containing outer indexes. Can be used to query a smaller subset of large tables efficiently. 
                       If not None, the underlying getbulk/getnext requests will be started like
                       getbulk(<parent>.<entry>.<column_1>[.<row_index_1>...<row_index_m>],
                               <parent>.<entry>.<column_n>[.<row_index_1>...<row_index_m>],
            max_repeaters: Number of conceptual column instances which are transfered at once in a getbulk response.
                           Adjust this to the number of expected rows to make the query more efficient.

        Returns:
            On success, a dictionary of dictionaries is returned.
            Outer dictionary takes a tuple of row indexes as key.
            Inner dictionary takes the conceptual column name as key.
            On error, None is returned, and related netsnmp.Session attributes
            ErrorStr, ErrorNum and ErrorInd are updated.

        """
        if (row_index):
            self._set_start_index(row_index)
        self.max_repeaters = max_repeaters
        res = interface.table_fetch(self)
        return res

    def _set_start_index(self, index_seq):
        oid = []
        for index in index_seq:
            if isinstance(index, basestring):
                oid.append(len(index))
                for character in index:
                    oid.append(ord(character))
            elif isinstance(index, int):
                oid = []
                oid.append(index)
            else:
                #TODO: error
                return
        self.start_index_oid = oid

    def _del_(self):
        print("destructing...")
        interface.table_cleanup(self)
