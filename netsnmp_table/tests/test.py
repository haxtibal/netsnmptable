""" Runs all unit tests for the netsnmp_table package.   """

import sys
import unittest
import netsnmp
import netsnmp_table
import pprint
import time

class BasicTests(unittest.TestCase):
    def __init__(self, *args, **kwargs):
        super(BasicTests, self).__init__(*args, **kwargs)
        self.netsnmp_session = netsnmp.Session(Version=2,
            DestHost='localhost',
            Community='public')

    def test_get(self):
        vb = netsnmp.Varbind('MYTABLETEST::testTable', 0)
        table = netsnmp_table.Table(self.netsnmp_session)
        tbldict = table.get(vb)
        pprint.pprint(tbldict)

    def test_start_idx(self):
        vb = netsnmp.Varbind('MYTABLETEST::testTable', 0)
        table = netsnmp_table.Table(self.netsnmp_session)
        table.set_start_index(("First",))
        tbldict = table.get(vb)
        pprint.pprint(tbldict)

    def test_multiple_getbulk(self):
        vb = netsnmp.Varbind('MYTABLETEST::testTable', 0)
        table = netsnmp_table.Table(self.netsnmp_session)
        table.max_repeaters = 1
        tbldict = table.get(vb)
        pprint.pprint(tbldict)

if __name__=='__main__':
    unittest.main()
