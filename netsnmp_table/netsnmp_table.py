import netsnmp
import interface

class Table(object):
    def __init__(self, session):
        self.netsnmp_session = session
        self.start_index_oid = []

    def set_start_index(self, index_seq):
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

    def get(self, varbind):
        res = interface.table(self, varbind)
        return res
