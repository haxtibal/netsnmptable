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
        self.assertEqual(self.netsnmp_session.ErrorStr, "", msg="Error during SNMP request: %s" % self.netsnmp_session.ErrorStr)
        self.assertEqual(self.netsnmp_session.ErrorNum, 0)
        self.assertEqual(self.netsnmp_session.ErrorInd, 0)
        self.assertIsNotNone(tbldict)
        pprint.pprint(tbldict)

    def test_fetch_with_row_idx(self):
        vb = netsnmp.Varbind('MYTABLETEST::testTable', 0)
        table = netsnmp_table.Table(self.netsnmp_session)
        tbldict = table.fetch(vb, row_index = ("First",))
        self.assertEqual(self.netsnmp_session.ErrorStr, "", msg="Error during SNMP request: %s" % self.netsnmp_session.ErrorStr)
        self.assertEqual(self.netsnmp_session.ErrorNum, 0)
        self.assertEqual(self.netsnmp_session.ErrorInd, 0)
        self.assertIsNotNone(tbldict)
        pprint.pprint(tbldict)

    def test_multiple_getbulk(self):
        vb = netsnmp.Varbind('MYTABLETEST::testTable', 0)
        table = netsnmp_table.Table(self.netsnmp_session)
        tbldict = table.fetch(vb, max_repeaters = 1)
        self.assertEqual(self.netsnmp_session.ErrorStr, "", msg="Error during SNMP request: %s" % self.netsnmp_session.ErrorStr)
        self.assertEqual(self.netsnmp_session.ErrorNum, 0)
        self.assertEqual(self.netsnmp_session.ErrorInd, 0)
        self.assertIsNotNone(tbldict)
        pprint.pprint(tbldict)

    def test_fetch_print_varbinds(self):
        vb = netsnmp.Varbind('MYTABLETEST::testTable', 0)
        table = netsnmp_table.Table(self.netsnmp_session)
        tbldict = table.fetch(vb)
        self.assertEqual(self.netsnmp_session.ErrorStr, "", msg="Error during SNMP request: %s" % self.netsnmp_session.ErrorStr)
        self.assertEqual(self.netsnmp_session.ErrorNum, 0)
        self.assertEqual(self.netsnmp_session.ErrorInd, 0)
        self.assertIsNotNone(tbldict)
        pprint.pprint(tbldict)
        print("Result at ['First', 'SubFirst']['aValue'] has value %s of type %s" %
              (tbldict[('First', 'SubFirst')]['aValue'].type,
               tbldict[('First', 'SubFirst')]['aValue'].val))

    def test_host_resources(self):
        vb = netsnmp.Varbind('HOST-RESOURCES-MIB:hrStorageTable', 0)
        table = netsnmp_table.Table(self.netsnmp_session)
        tbldict = table.fetch(vb)
        self.assertEqual(self.netsnmp_session.ErrorStr, "", msg="Error during SNMP request: %s" % self.netsnmp_session.ErrorStr)
        self.assertEqual(self.netsnmp_session.ErrorNum, 0)
        self.assertEqual(self.netsnmp_session.ErrorInd, 0)
        self.assertIsNotNone(tbldict)
        pprint.pprint(tbldict)

if __name__=='__main__':
    unittest.main()
