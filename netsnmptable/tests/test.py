""" Runs all unit tests for the netsnmptable package.   """

import sys
import unittest
import netsnmp
import netsnmptable
import pprint
import time
from types import MethodType

def varbind_to_repr(self):
    """Can be dynamically added to Varbind objects, for nice pprint output"""
    return self.type + ":" + self.val

class BasicTests(unittest.TestCase):
    def __init__(self, *args, **kwargs):
        super(BasicTests, self).__init__(*args, **kwargs)
        netsnmp.Varbind.__repr__ = MethodType(varbind_to_repr, None, netsnmp.Varbind)
        self.netsnmp_session = netsnmp.Session(Version=2,
            DestHost='localhost',
            Community='public')

    def test_host_resources(self):
        table = self.netsnmp_session.table_from_mib('HOST-RESOURCES-MIB:hrStorageTable')
        tbldict = table.fetch()
        self.assertEqual(self.netsnmp_session.ErrorStr, "", msg="Error during SNMP request: %s" % self.netsnmp_session.ErrorStr)
        self.assertEqual(self.netsnmp_session.ErrorNum, 0)
        self.assertEqual(self.netsnmp_session.ErrorInd, 0)
        self.assertIsNotNone(tbldict)
        pprint.pprint(tbldict)

    def test_host_resources_ps(self):
        table = self.netsnmp_session.table_from_mib('HOST-RESOURCES-MIB:hrStorageTable')
        tbldict = table.fetch()
        if (self.netsnmp_session.ErrorNum):
            print("Can't query HOST-RESOURCES-MIB:hrStorageTable.")
            exit(-1)
        
        print("{:10s} {:25s} {:10s} {:10s} {:10s}".format("Index", "Description", "Units", "Size", "Used"))
        row_keys = sorted(list(tbldict.keys()))
        for row_key in row_keys:
            row = tbldict[row_key]
            cell_list = [element.val if element else "" for element in
                         [row.get('hrStorageIndex'), row.get('hrStorageDescr'),
                          row.get('hrStorageAllocationUnits'), row.get('hrStorageSize'),
                          row.get('hrStorageUsed')]]
            print("{:10s} {:25s} {:10s} {:10s} {:10s}".format(*cell_list))

#deactivated for now, as test mib is not available
class PrivateMibTests():
    def __init__(self, *args, **kwargs):
        super(BasicTests, self).__init__(*args, **kwargs)
        netsnmp.Varbind.__repr__ = MethodType(varbind_to_repr, None, netsnmp.Varbind)
        self.netsnmp_session = netsnmp.Session(Version=2,
            DestHost='localhost',
            Community='public')

    def test_parse_mib(self):
        table = self.netsnmp_session.table_from_mib('MYTABLETEST::testTable')
        pprint.pprint(table.columns)

    def test_fetch(self):
        table = self.netsnmp_session.table_from_mib('MYTABLETEST::testTable')
        tbldict = table.fetch()
        self.assertEqual(self.netsnmp_session.ErrorStr, "", msg="Error during SNMP request: %s" % self.netsnmp_session.ErrorStr)
        self.assertEqual(self.netsnmp_session.ErrorNum, 0)
        self.assertEqual(self.netsnmp_session.ErrorInd, 0)
        self.assertIsNotNone(tbldict)
        pprint.pprint(tbldict)

    def test_fetch_with_row_idx(self):
        table = self.netsnmp_session.table_from_mib('MYTABLETEST::testTable')
        tbldict = table.fetch(iid = netsnmptable.str_to_varlen_iid("OuterIdx_2"))
        self.assertEqual(self.netsnmp_session.ErrorStr, "", msg="Error during SNMP request: %s" % self.netsnmp_session.ErrorStr)
        self.assertEqual(self.netsnmp_session.ErrorNum, 0)
        self.assertEqual(self.netsnmp_session.ErrorInd, 0)
        self.assertIsNotNone(tbldict)
        pprint.pprint(tbldict)

    def test_multiple_getbulk(self):
        """This test forces multiple getbulks by setting max_repeaters to 1"""
        table = self.netsnmp_session.table_from_mib('MYTABLETEST::testTable')
        tbldict = table.fetch(max_repeaters = 1)
        self.assertEqual(self.netsnmp_session.ErrorStr, "", msg="Error during SNMP request: %s" % self.netsnmp_session.ErrorStr)
        self.assertEqual(self.netsnmp_session.ErrorNum, 0)
        self.assertEqual(self.netsnmp_session.ErrorInd, 0)
        self.assertIsNotNone(tbldict)
        pprint.pprint(tbldict)

    def test_fetch_print_varbinds(self):
        table = self.netsnmp_session.table_from_mib('MYTABLETEST::testTable')
        tbldict = table.fetch()
        self.assertEqual(self.netsnmp_session.ErrorStr, "", msg="Error during SNMP request: %s" % self.netsnmp_session.ErrorStr)
        self.assertEqual(self.netsnmp_session.ErrorNum, 0)
        self.assertEqual(self.netsnmp_session.ErrorInd, 0)
        self.assertIsNotNone(tbldict)
        pprint.pprint(tbldict)
        print("Result at ['OuterIdx_1', 'InnerIdx_1']['aValue'] has value %s of type %s" %
              (tbldict[('OuterIdx_1', 'InnerIdx_1')]['aValue'].type,
               tbldict[('OuterIdx_1', 'InnerIdx_1')]['aValue'].val))

if __name__=='__main__':
    unittest.main()
