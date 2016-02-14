""" Runs all unit tests for the netsnmptable package.   """

import testagent
import netsnmp
import netsnmptable
import os
import pprint
import sys
import time
import unittest
from types import MethodType

def varbind_to_repr(self):
    """Can be dynamically added to Varbind objects, for nice pprint output"""
    return self.type + ":" + self.val

class BasicTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        mib_path = os.path.join(os.path.abspath(os.path.dirname(sys.argv[0])), "netsnmptable/tests/SIMPLE-MIB.txt")
        os.environ['MIBS'] = 'SIMPLE-MIB'
        os.environ['MIBDIRS'] = mib_path
        testagent.configure(agent_address = "localhost:1234", MIBFiles = [mib_path],
            rocommunity='public', rwcommunity='private')

        firstTable = testagent.Table(
        	oidstr = "SIMPLE-MIB::firstTable",
        	indexes = [
        		testagent.DisplayString()
        	],
        	columns = [
        		(2, testagent.DisplayString("Unknown place")),
        		(3, testagent.Integer32(0))
        	],
        )
        firstTableRow1 = firstTable.addRow([testagent.DisplayString("aa")])
        firstTableRow1.setRowCell(2, testagent.DisplayString("Prague"))
        firstTableRow1.setRowCell(3, testagent.Integer32(20))
        firstTableRow2 = firstTable.addRow([testagent.DisplayString("ab")])
        firstTableRow2.setRowCell(2, testagent.DisplayString("Barcelona"))
        firstTableRow2.setRowCell(3, testagent.Integer32(28))
        firstTableRow3 = firstTable.addRow([testagent.DisplayString("bb")])
        firstTableRow3.setRowCell(3, testagent.Integer32(18))

        testagent.start_server()

    @classmethod
    def tearDownClass(cls):
        testagent.stop_server()

    def __init__(self, *args, **kwargs):
        super(BasicTests, self).__init__(*args, **kwargs)
        netsnmp.Varbind.__repr__ = MethodType(varbind_to_repr, None, netsnmp.Varbind)
        self.netsnmp_session = netsnmp.Session(Version=2, DestHost='localhost', RemotePort=1234, Community='public')

    def test_host_resources(self):
        table = self.netsnmp_session.table_from_mib('SIMPLE-MIB::firstTable')
        tbldict = table.fetch()
        self.assertEqual(self.netsnmp_session.ErrorStr, "", msg="Error during SNMP request: %s" % self.netsnmp_session.ErrorStr)
        self.assertEqual(self.netsnmp_session.ErrorNum, 0)
        self.assertEqual(self.netsnmp_session.ErrorInd, 0)
        self.assertIsNotNone(tbldict)
        pprint.pprint(tbldict)

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

if __name__=='__main__':
    unittest.main()
