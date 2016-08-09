""" Runs all unit tests for the netsnmptable package.   """

import inspect
import netsnmp
import netsnmptable
import os
import pprint
import sys
import testagent
import time
import thread
import unittest
from types import MethodType
from Crypto import SelfTest

# I don't know why configure only works here, if it is executed outside of unittest.TestCase
os.environ['MIBDIRS'] = os.path.dirname(os.path.abspath(__file__)) + ":/usr/share/mibs/ietf"
testagent.configure(agent_address = "localhost:1234",
    rocommunity='public', rwcommunity='private')

ascii_test_string = ' !"#$%&\'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~'

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
    singleIdxTableRow1.setRowCell(2, testagent.DisplayString("ContentOfRow1_Column1 " + ascii_test_string))
    singleIdxTableRow1.setRowCell(3, testagent.Integer32(1))
    singleIdxTableRow2 = singleIdxTable.addRow([testagent.DisplayString("ThisIsRow2")])
    singleIdxTableRow2.setRowCell(2, testagent.DisplayString("ContentOfRow2_Column1 " + ascii_test_string))
    singleIdxTableRow2.setRowCell(3, testagent.Integer32(2))
    singleIdxTableRow3 = singleIdxTable.addRow([testagent.DisplayString("ThisIsRow3")])
    singleIdxTableRow3.setRowCell(2, testagent.DisplayString("ContentOfRow3_Column1 " + ascii_test_string))
    singleIdxTableRow3.setRowCell(3, testagent.Integer32(3))
    singleIdxTableRow4 = singleIdxTable.addRow([testagent.DisplayString(ascii_test_string)])
    singleIdxTableRow4.setRowCell(2, testagent.DisplayString("ContentOfRow4_Column1 " + ascii_test_string))
    singleIdxTableRow4.setRowCell(3, testagent.Integer32(3))

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
    
    ipaddrIdxTable = testagent.Table(
        oidstr = "TEST-MIB::ipaddrIdxTable",
        indexes = [
            testagent.IpAddress()
        ],
        columns = [
            (2, testagent.DisplayString("Default")),
            (3, testagent.IpAddress("0.0.0.0"))
        ],
    )
    ipaddrIdxTableRow1 = ipaddrIdxTable.addRow([testagent.IpAddress("192.168.0.1")])
    ipaddrIdxTableRow1.setRowCell(2, testagent.DisplayString("ContentOfRow1_Column1"))
    ipaddrIdxTableRow1.setRowCell(3, testagent.IpAddress("192.168.0.1"))
    ipaddrIdxTableRow2 = ipaddrIdxTable.addRow([testagent.IpAddress("192.168.0.2")])
    ipaddrIdxTableRow2.setRowCell(2, testagent.DisplayString("ContentOfRow2_Column1"))
    ipaddrIdxTableRow2.setRowCell(3, testagent.IpAddress("192.168.0.2"))
    ipaddrIdxTableRow3 = ipaddrIdxTable.addRow([testagent.IpAddress("192.168.0.3")])
    ipaddrIdxTableRow3.setRowCell(2, testagent.DisplayString("ContentOfRow3_Column1"))
    ipaddrIdxTableRow3.setRowCell(3, testagent.IpAddress("192.168.0.3"))

def varbind_to_repr(self):
    """Can be dynamically added to Varbind objects, for nice pprint output"""
    return self.type + ":" + self.val

class BasicTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        setup_oids()
        testagent.start_server()

    @classmethod
    def tearDownClass(cls):
        testagent.stop_server()

    def __init__(self, *args, **kwargs):
        super(BasicTests, self).__init__(*args, **kwargs)
        netsnmp.Varbind.__repr__ = MethodType(varbind_to_repr, None, netsnmp.Varbind)
        self.netsnmp_session = netsnmp.Session(Version=2, DestHost='localhost:1234', Community='public')

    def test_singleIdxTable(self):
        table = self.netsnmp_session.table_from_mib('TEST-MIB::singleIdxTable')
        tbldict = table.get_entries()
        self.assertEqual(self.netsnmp_session.ErrorStr,
            "",
            msg="Error during SNMP request: %s" % self.netsnmp_session.ErrorStr
            )
        self.assertEqual(self.netsnmp_session.ErrorNum, 0)
        self.assertEqual(self.netsnmp_session.ErrorInd, 0)
        self.assertIsNotNone(tbldict)
        self.assertIsNotNone(tbldict.get(('ThisIsRow1',)))
        self.assertIsNotNone(tbldict.get(('ThisIsRow2',)))
        self.assertIsNotNone(tbldict.get(('ThisIsRow3',)))
        self.assertEqual(tbldict[('ThisIsRow1',)].get('singleIdxTableEntryDesc').val, "ContentOfRow1_Column1 " + ascii_test_string)
        self.assertEqual(tbldict[('ThisIsRow1',)].get('singleIdxTableEntryValue').val, "1")
        self.assertEqual(tbldict[('ThisIsRow2',)].get('singleIdxTableEntryDesc').val, "ContentOfRow2_Column1 " + ascii_test_string)
        self.assertEqual(tbldict[('ThisIsRow2',)].get('singleIdxTableEntryValue').val, "2")
        self.assertEqual(tbldict[('ThisIsRow3',)].get('singleIdxTableEntryDesc').val, "ContentOfRow3_Column1 " + ascii_test_string)
        self.assertEqual(tbldict[('ThisIsRow3',)].get('singleIdxTableEntryValue').val, "3")
        self.assertEqual(tbldict[(ascii_test_string,)].get('singleIdxTableEntryDesc').val, "ContentOfRow4_Column1 " + ascii_test_string)
        self.assertEqual(tbldict[(ascii_test_string,)].get('singleIdxTableEntryValue').val, "3")
        pprint.pprint(tbldict)

    def test_multiIdxTable(self):
        table = self.netsnmp_session.table_from_mib('TEST-MIB::multiIdxTable')
        tbldict = table.get_entries()
        self.assertEqual(self.netsnmp_session.ErrorStr,
            "",
            msg="Error during SNMP request: %s" % self.netsnmp_session.ErrorStr
            )
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

    def test_ipAddrIdxTable(self):
        table = self.netsnmp_session.table_from_mib('TEST-MIB::ipaddrIdxTable')
        tbldict = table.get_entries()
        self.assertEqual(self.netsnmp_session.ErrorStr,
            "",
            msg="Error during SNMP request: %s" % self.netsnmp_session.ErrorStr
            )
        self.assertEqual(self.netsnmp_session.ErrorNum, 0)
        self.assertEqual(self.netsnmp_session.ErrorInd, 0)
        self.assertIsNotNone(tbldict)
        self.assertIsNotNone(tbldict.get(('192.168.0.1',)))
        self.assertIsNotNone(tbldict.get(('192.168.0.2',)))
        self.assertIsNotNone(tbldict.get(('192.168.0.3',)))
        self.assertEqual(tbldict[('192.168.0.1',)].get('ipaddrIdxTableEntryDesc').val, 'ContentOfRow1_Column1')
        self.assertEqual(tbldict[('192.168.0.1',)].get('ipaddrIdxTableEntryValue').val, '192.168.0.1')
        self.assertEqual(tbldict[('192.168.0.2',)].get('ipaddrIdxTableEntryDesc').val, "ContentOfRow2_Column1")
        self.assertEqual(tbldict[('192.168.0.2',)].get('ipaddrIdxTableEntryValue').val, '192.168.0.2')
        self.assertEqual(tbldict[('192.168.0.3',)].get('ipaddrIdxTableEntryDesc').val, "ContentOfRow3_Column1")
        self.assertEqual(tbldict[('192.168.0.3',)].get('ipaddrIdxTableEntryValue').val, '192.168.0.3')
        pprint.pprint(tbldict)

    def test_create_from_badOid(self):
        with self.assertRaises(RuntimeError):
            self.netsnmp_session.table_from_mib('TEST-MIB::singleIdxTableEntry')

    def _test_objid_index(self):
        # We have no means to test with testagent yet, because Varbind type Object ID is not yet implemented.
        # Use master agent instead, where we can create an entry in SNMP-NOTIFICATION-MIB::snmpNotifyFilterTable for test purpose.
        # snmpset -v2c -c private localhost SNMP-NOTIFICATION-MIB::snmpNotifyFilterMask.\"1stidx\".1.2.3.4.5 s "val1" SNMP-NOTIFICATION-MIB::snmpNotifyFilterRowStatus.\"1stidx\".1.2.3.4.5 i 4
        netsnmp_master_session = netsnmp.Session(Version=2, DestHost='localhost', Community='public')
        table = netsnmp_master_session.table_from_mib('SNMP-NOTIFICATION-MIB::snmpNotifyFilterTable')
        tbldict = table.get_entries()
        self.assertEqual(self.netsnmp_session.ErrorStr,
            "",
            msg="Error during SNMP request: %s" % self.netsnmp_session.ErrorStr
            )
        self.assertIsNotNone(tbldict)
        self.assertIsNotNone(tbldict.get(('1stidx', '1.2.3.4.5')))
        self.assertEqual(tbldict[('1stidx', '1.2.3.4.5')].get('snmpNotifyFilterMask').val, 'val1')
        pprint.pprint(tbldict)

if __name__=='__main__':
    unittest.main()
