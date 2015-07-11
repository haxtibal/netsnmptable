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

    def test_fetch(self):
        vb = netsnmp.Varbind('MYTABLETEST::testTable', 0)
        table = netsnmp_table.Table(self.netsnmp_session)
        tbldict = table.fetch(vb)
        pprint.pprint(tbldict)

    def test_fetch_with_row_idx(self):
        vb = netsnmp.Varbind('MYTABLETEST::testTable', 0)
        table = netsnmp_table.Table(self.netsnmp_session)
        tbldict = table.fetch(vb, row_index = ("First",))
        pprint.pprint(tbldict)

    def test_multiple_getbulk(self):
        vb = netsnmp.Varbind('MYTABLETEST::testTable', 0)
        table = netsnmp_table.Table(self.netsnmp_session)
        tbldict = table.fetch(vb, max_repeaters = 1)
        pprint.pprint(tbldict)

if __name__=='__main__':
    unittest.main()
