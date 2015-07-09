""" Runs all unit tests for the netsnmp_table package.   """

import sys
import unittest
import netsnmp
import netsnmp_table
import pprint
import time

class BasicTests(unittest.TestCase):
    def test_get(self):
        return
        netsnmp_session = netsnmp.Session(Version=2,
                               DestHost='localhost',
                               Community='public')
        vb = netsnmp.Varbind('MYTABLETEST::testTable', 0)
        
        table = netsnmp_table.Table(netsnmp_session)
        tbldict = table.get(vb)
        pprint.pprint(tbldict)

    def test_start_idx(self):
        netsnmp_session = netsnmp.Session(Version=2,
                               DestHost='localhost',
                               Community='public')
        vb = netsnmp.Varbind('MYTABLETEST::testTable', 0)

        table = netsnmp_table.Table(netsnmp_session)
        table.set_start_index(("First",))
        tbldict = table.get(vb)
        pprint.pprint(tbldict)

if __name__=='__main__':
    unittest.main()
