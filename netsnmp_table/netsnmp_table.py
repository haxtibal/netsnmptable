import netsnmp
import interface

class Table(object):
    def __init__(self, session):
        self.netsnmp_session = session

    def get(self, varbind):
        res = interface.table(self, varbind)
        return res
