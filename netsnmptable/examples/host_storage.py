#!/usr/bin/env python

import netsnmp
import netsnmptable

ns = netsnmp.Session(Version=2, DestHost='localhost', Community='public')
table = ns.table_from_mib('HOST-RESOURCES-MIB::hrStorageTable')
tbldict = table.get_entries()

if (ns.ErrorNum):
    print("Can't query HOST-RESOURCES-MIB::hrStorageTable.")
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
