""" Runs all unit tests for the netsnmptable package.   """

import inspect
import netsnmp
import netsnmptable
import os
import pprint
import sys
import time
import thread
import unittest
from types import MethodType

try:
    import testagent
except ImportError:
    print("No suitable testagent available, can't execute unit tests.")
    raise

# I don't know why configure only works here, if it is executed outside of unittest.TestCase
os.environ['MIBDIRS'] = os.path.dirname(os.path.abspath(__file__))
testagent.configure(agent_address = "localhost:1234",
    rocommunity='public', rwcommunity='private')

def setup_oids():
    singleIdxTable = testagent.Table(
        oidstr = "TEST-MIB::singleIdxTable",
        indexes = [
            testagent.DisplayString()
        ],
        columns = [
            (2, testagent.DisplayString("")),
            (3, testagent.Integer32(0))
        ],
    )
    singleIdxTableRow1 = singleIdxTable.addRow([testagent.DisplayString("ThisIsRow1")])
    singleIdxTableRow1.setRowCell(2, testagent.DisplayString("ContentOfRow1_Column1"))
    singleIdxTableRow1.setRowCell(3, testagent.Integer32(1))
    singleIdxTableRow2 = singleIdxTable.addRow([testagent.DisplayString("ThisIsRow2")])
    singleIdxTableRow2.setRowCell(2, testagent.DisplayString("ContentOfRow2_Column1"))
    singleIdxTableRow2.setRowCell(3, testagent.Integer32(2))
    singleIdxTableRow3 = singleIdxTable.addRow([testagent.DisplayString("ThisIsRow3")])
    singleIdxTableRow3.setRowCell(2, testagent.DisplayString("ContentOfRow3_Column1"))
    singleIdxTableRow3.setRowCell(3, testagent.Integer32(3))

    multiIdxTable = testagent.Table(
        oidstr = "TEST-MIB::multiIdxTable",
        indexes = [
            testagent.DisplayString(),
            testagent.Integer32(),
        ],
        columns = [
            (3, testagent.DisplayString("")),
            (4, testagent.Integer32(0))
        ],
    )
    multiIdxTableRow1 = multiIdxTable.addRow([testagent.DisplayString("ThisIsRow1"), testagent.Integer32(1)])
    multiIdxTableRow1.setRowCell(3, testagent.DisplayString("ContentOfRow1.1_Column1"))
    multiIdxTableRow1.setRowCell(4, testagent.Integer32(1))
    multiIdxTableRow2 = multiIdxTable.addRow([testagent.DisplayString("ThisIsRow1"), testagent.Integer32(2)])
    multiIdxTableRow2.setRowCell(3, testagent.DisplayString("ContentOfRow1.2_Column1"))
    multiIdxTableRow2.setRowCell(4, testagent.Integer32(2))
    multiIdxTableRow3 = multiIdxTable.addRow([testagent.DisplayString("ThisIsRow2"), testagent.Integer32(1)])
    multiIdxTableRow3.setRowCell(3, testagent.DisplayString("ContentOfRow2.1_Column1"))
    multiIdxTableRow3.setRowCell(4, testagent.Integer32(3))
    multiIdxTableRow3 = multiIdxTable.addRow([testagent.DisplayString("ThisIsRow2"), testagent.Integer32(2)])
    multiIdxTableRow3.setRowCell(3, testagent.DisplayString("ContentOfRow2.2_Column1"))
    multiIdxTableRow3.setRowCell(4, testagent.Integer32(4))

def varbind_to_repr(self):
    """Can be dynamically added to Varbind objects, for nice pprint output"""
    return self.type + ":" + self.val

class UtilTests(unittest.TestCase):
    def test_str_to_varlen_iid(self):
        iid = netsnmptable.str_to_varlen_iid("someVariableLengthString")
        self.assertEqual(iid, [24, 115, 111, 109, 101, 86, 97, 114, 105, 97, 98, 108, 101, 76, 101, 110, 103, 116, 104, 83, 116, 114, 105, 110, 103])

    def test_str_to_fixlen_iid(self):
        iid = netsnmptable.str_to_fixlen_iid("someFixedLengthString")
        self.assertEqual(iid, [115, 111, 109, 101, 70, 105, 120, 101, 100, 76, 101, 110, 103, 116, 104, 83, 116, 114, 105, 110, 103])

class TableTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        setup_oids()
        testagent.start_server()

    @classmethod
    def tearDownClass(cls):
        testagent.stop_server()

    def __init__(self, *args, **kwargs):
        super(TableTests, self).__init__(*args, **kwargs)
        netsnmp.Varbind.__repr__ = MethodType(varbind_to_repr, None, netsnmp.Varbind)
        self.netsnmp_session = netsnmp.Session(Version=2, DestHost='localhost:1234', Community='public')

    def test_singleIdxTable(self):
        table = self.netsnmp_session.table_from_mib('TEST-MIB::singleIdxTable')
        tbldict = table.get_entries()
        self.assertEqual(self.netsnmp_session.ErrorStr, "")
        self.assertEqual(self.netsnmp_session.ErrorNum, 0)
        self.assertEqual(self.netsnmp_session.ErrorInd, 0)
        self.assertIsNotNone(tbldict)
        self.assertIsNotNone(tbldict.get(('ThisIsRow1',)))
        self.assertIsNotNone(tbldict.get(('ThisIsRow2',)))
        self.assertIsNotNone(tbldict.get(('ThisIsRow3',)))
        self.assertEqual(tbldict[('ThisIsRow1',)].get('singleIdxTableEntryDesc').val, "ContentOfRow1_Column1")
        self.assertEqual(tbldict[('ThisIsRow1',)].get('singleIdxTableEntryValue').val, "1")
        self.assertEqual(tbldict[('ThisIsRow2',)].get('singleIdxTableEntryDesc').val, "ContentOfRow2_Column1")
        self.assertEqual(tbldict[('ThisIsRow2',)].get('singleIdxTableEntryValue').val, "2")
        self.assertEqual(tbldict[('ThisIsRow3',)].get('singleIdxTableEntryDesc').val, "ContentOfRow3_Column1")
        self.assertEqual(tbldict[('ThisIsRow3',)].get('singleIdxTableEntryValue').val, "3")
        pprint.pprint(tbldict)

    def test_multiIdxTable(self):
        table = self.netsnmp_session.table_from_mib('TEST-MIB::multiIdxTable')
        tbldict = table.get_entries()
        self.assertEqual(self.netsnmp_session.ErrorStr, "")
        self.assertEqual(self.netsnmp_session.ErrorNum, 0)
        self.assertEqual(self.netsnmp_session.ErrorInd, 0)
        self.assertIsNotNone(tbldict)
        self.assertIsNotNone(tbldict.get(('ThisIsRow1', 1)))
        self.assertIsNotNone(tbldict.get(('ThisIsRow1', 2)))
        self.assertIsNotNone(tbldict.get(('ThisIsRow2', 1)))
        self.assertIsNotNone(tbldict.get(('ThisIsRow2', 2)))
        self.assertEqual(tbldict[('ThisIsRow1', 1)].get('multiIdxTableEntryDesc').val, "ContentOfRow1.1_Column1")
        self.assertEqual(tbldict[('ThisIsRow1', 1)].get('multiIdxTableEntryValue').val, "1")
        self.assertEqual(tbldict[('ThisIsRow1', 2)].get('multiIdxTableEntryDesc').val, "ContentOfRow1.2_Column1")
        self.assertEqual(tbldict[('ThisIsRow1', 2)].get('multiIdxTableEntryValue').val, "2")
        self.assertEqual(tbldict[('ThisIsRow2', 1)].get('multiIdxTableEntryDesc').val, "ContentOfRow2.1_Column1")
        self.assertEqual(tbldict[('ThisIsRow2', 1)].get('multiIdxTableEntryValue').val, "3")
        self.assertEqual(tbldict[('ThisIsRow2', 2)].get('multiIdxTableEntryDesc').val, "ContentOfRow2.2_Column1")
        self.assertEqual(tbldict[('ThisIsRow2', 2)].get('multiIdxTableEntryValue').val, "4")
        pprint.pprint(tbldict)

    def test_get_entries_iid(self):
        table = self.netsnmp_session.table_from_mib('TEST-MIB::multiIdxTable')
        tbldict = table.get_entries(iid = netsnmptable.str_to_varlen_iid("ThisIsRow1"))
        self.assertEqual(self.netsnmp_session.ErrorStr, "")
        self.assertEqual(self.netsnmp_session.ErrorNum, 0)
        self.assertEqual(self.netsnmp_session.ErrorInd, 0)
        self.assertIsNotNone(tbldict)
        self.assertIsNotNone(tbldict.get(('ThisIsRow1', 1)))
        self.assertIsNotNone(tbldict.get(('ThisIsRow1', 2)))
        self.assertEqual(tbldict[('ThisIsRow1', 1)].get('multiIdxTableEntryDesc').val, "ContentOfRow1.1_Column1")
        self.assertEqual(tbldict[('ThisIsRow1', 1)].get('multiIdxTableEntryValue').val, "1")
        self.assertEqual(tbldict[('ThisIsRow1', 2)].get('multiIdxTableEntryDesc').val, "ContentOfRow1.2_Column1")
        self.assertEqual(tbldict[('ThisIsRow1', 2)].get('multiIdxTableEntryValue').val, "2")

    def test_get_entries_max_repeaters(self):
        table = self.netsnmp_session.table_from_mib('TEST-MIB::multiIdxTable')
        # causes several getbulks
        tbldict = table.get_entries(max_repeaters = 1)
        self.assertEqual(self.netsnmp_session.ErrorStr, "")
        self.assertEqual(self.netsnmp_session.ErrorNum, 0)
        self.assertEqual(self.netsnmp_session.ErrorInd, 0)
        self.assertIsNotNone(tbldict)
        self.assertIsNotNone(tbldict.get(('ThisIsRow1', 1)))
        self.assertIsNotNone(tbldict.get(('ThisIsRow1', 2)))
        self.assertIsNotNone(tbldict.get(('ThisIsRow2', 1)))
        self.assertIsNotNone(tbldict.get(('ThisIsRow2', 2)))
        self.assertEqual(tbldict[('ThisIsRow1', 1)].get('multiIdxTableEntryDesc').val, "ContentOfRow1.1_Column1")
        self.assertEqual(tbldict[('ThisIsRow1', 1)].get('multiIdxTableEntryValue').val, "1")
        self.assertEqual(tbldict[('ThisIsRow1', 2)].get('multiIdxTableEntryDesc').val, "ContentOfRow1.2_Column1")
        self.assertEqual(tbldict[('ThisIsRow1', 2)].get('multiIdxTableEntryValue').val, "2")
        self.assertEqual(tbldict[('ThisIsRow2', 1)].get('multiIdxTableEntryDesc').val, "ContentOfRow2.1_Column1")
        self.assertEqual(tbldict[('ThisIsRow2', 1)].get('multiIdxTableEntryValue').val, "3")
        self.assertEqual(tbldict[('ThisIsRow2', 2)].get('multiIdxTableEntryDesc').val, "ContentOfRow2.2_Column1")
        self.assertEqual(tbldict[('ThisIsRow2', 2)].get('multiIdxTableEntryValue').val, "4")

#deactivated for now, as required mibs are not included in package
class ExternalServiceTests():
    def __init__(self, *args, **kwargs):
        super(BasicTests, self).__init__(*args, **kwargs)
        netsnmp.Varbind.__repr__ = MethodType(varbind_to_repr, None, netsnmp.Varbind)
        self.netsnmp_session = netsnmp.Session(Version=2,
            DestHost='localhost',
            Community='public')

    def test_host_resources(self):
        table = self.netsnmp_session.table_from_mib('HOST-RESOURCES-MIB:hrStorageTable')
        tbldict = table.get_entries()
        self.assertEqual(self.netsnmp_session.ErrorStr, "", msg="Error during SNMP request: %s" % self.netsnmp_session.ErrorStr)
        self.assertEqual(self.netsnmp_session.ErrorNum, 0)
        self.assertEqual(self.netsnmp_session.ErrorInd, 0)
        self.assertIsNotNone(tbldict)
        pprint.pprint(tbldict)

    def test_host_resources_ps(self):
        table = self.netsnmp_session.table_from_mib('HOST-RESOURCES-MIB:hrStorageTable')
        tbldict = table.get_entries()
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
