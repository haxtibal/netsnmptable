""" Runs all unit tests for the netsnmptable package.   """

import sys
import unittest
import netsnmp
import netsnmptable
import pprint
import time

class BasicTests(unittest.TestCase):
    def __init__(self, *args, **kwargs):
        super(BasicTests, self).__init__(*args, **kwargs)
        self.netsnmp_session = netsnmp.Session(Version=2,
            DestHost='localhost',
            Community='public')

    def test_parse_mib(self):
        table = netsnmptable.Table(self.netsnmp_session)
        table.parse_mib(netsnmp.Varbind('MYTABLETEST::testTable', 0))
        pprint.pprint(table.columns)

    def test_fetch(self):
        table = netsnmptable.Table(self.netsnmp_session)
        table.parse_mib(netsnmp.Varbind('MYTABLETEST::testTable', 0))
        tbldict = table.fetch()
        self.assertEqual(self.netsnmp_session.ErrorStr, "", msg="Error during SNMP request: %s" % self.netsnmp_session.ErrorStr)
        self.assertEqual(self.netsnmp_session.ErrorNum, 0)
        self.assertEqual(self.netsnmp_session.ErrorInd, 0)
        self.assertIsNotNone(tbldict)
        pprint.pprint(tbldict)

    def test_fetch_with_row_idx(self):
        table = netsnmptable.Table(self.netsnmp_session)
        table.parse_mib(netsnmp.Varbind('MYTABLETEST::testTable', 0))
        tbldict = table.fetch(iid = netsnmptable.str_to_varlen_iid("First"))
        self.assertEqual(self.netsnmp_session.ErrorStr, "", msg="Error during SNMP request: %s" % self.netsnmp_session.ErrorStr)
        self.assertEqual(self.netsnmp_session.ErrorNum, 0)
        self.assertEqual(self.netsnmp_session.ErrorInd, 0)
        self.assertIsNotNone(tbldict)
        pprint.pprint(tbldict)

    def test_multiple_getbulk(self):
        """This test forces multiple getbulks by setting max_repeaters to 1"""
        table = netsnmptable.Table(self.netsnmp_session)
        table.parse_mib(netsnmp.Varbind('MYTABLETEST::testTable', 0))
        tbldict = table.fetch(max_repeaters = 1)
        self.assertEqual(self.netsnmp_session.ErrorStr, "", msg="Error during SNMP request: %s" % self.netsnmp_session.ErrorStr)
        self.assertEqual(self.netsnmp_session.ErrorNum, 0)
        self.assertEqual(self.netsnmp_session.ErrorInd, 0)
        self.assertIsNotNone(tbldict)
        pprint.pprint(tbldict)

    def test_fetch_print_varbinds(self):
        table = netsnmptable.Table(self.netsnmp_session)
        table.parse_mib(netsnmp.Varbind('MYTABLETEST::testTable', 0))
        tbldict = table.fetch()
        self.assertEqual(self.netsnmp_session.ErrorStr, "", msg="Error during SNMP request: %s" % self.netsnmp_session.ErrorStr)
        self.assertEqual(self.netsnmp_session.ErrorNum, 0)
        self.assertEqual(self.netsnmp_session.ErrorInd, 0)
        self.assertIsNotNone(tbldict)
        pprint.pprint(tbldict)
        print("Result at ['First', 'SubFirst']['aValue'] has value %s of type %s" %
              (tbldict[('First', 'SubFirst')]['aValue'].type,
               tbldict[('First', 'SubFirst')]['aValue'].val))

    def test_host_resources(self):
        table = netsnmptable.Table(self.netsnmp_session)
        table.parse_mib(netsnmp.Varbind('HOST-RESOURCES-MIB:hrStorageTable', 0))
        tbldict = table.fetch()
        self.assertEqual(self.netsnmp_session.ErrorStr, "", msg="Error during SNMP request: %s" % self.netsnmp_session.ErrorStr)
        self.assertEqual(self.netsnmp_session.ErrorNum, 0)
        self.assertEqual(self.netsnmp_session.ErrorInd, 0)
        self.assertIsNotNone(tbldict)
        pprint.pprint(tbldict)

if __name__=='__main__':
    unittest.main()
