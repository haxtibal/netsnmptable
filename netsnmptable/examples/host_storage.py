#!/usr/bin/env python

import netsnmp
import netsnmptable

table = netsnmptable.Table(netsnmp.Session(Version=2,
            DestHost='localhost',
            Community='public'))

table.parse_mib(netsnmp.Varbind('HOST-RESOURCES-MIB:hrStorageTable', 0))

table = netsnmptable.Table(self.netsnmp_session)
table.parse_mib(netsnmp.Varbind('HOST-RESOURCES-MIB:hrStorageTable', 0))
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
