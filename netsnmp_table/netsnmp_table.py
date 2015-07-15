import netsnmp
import interface

class Table(object):
    def __init__(self, session):
        self.netsnmp_session = session
        self.max_repeaters = 10
        self.start_index_oid = []

    def fetch(self, varbind, row_index=None, max_repeaters = 10):
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
        res = interface.table(self, varbind)
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

