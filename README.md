# netsnmptable #
*A Python C Extension package to query SNMP tables and table subsets, complementing the original Net-SNMP Python Bindings.*

## Goals ##
This project aims to implement SNMP table query functionality for Python using the NetSNMP libraries, with features as:
- High-level, easy to use Python interface
- Resulting data structures behave like tables, in the sense that a cell is addressable by row and column
- Table structure is determined by MIB
- Indexes are selectable to some extent, to save bandwidth and memory when you want to only retrieve a small portion of a large table

## Why not just using existing NetSNMP functionality? ##
In short: Because there's currently no such functionality for Python,
neither in the upstream NetSNMP project, nor in any spin off or extension project (correct me if I'm wrong).

At the command line you've got the snmptable tool, and in the Perl extension there's Net::SNMP gettable.
But in Python there's nothing but plain getnext/getbullk methods.

So, up to now you may either to do something like subprocess.call(["snmptable", ...]),
or write a table handler using netsnmp.Session.getbulk() in pure Python.
But both ways you're technically limited. For example, if using netsnmp.Session.getbulk() you'll miss get_table()
in the bindings, so you can't parse a MIB and detect the column OIDs.
Or, if using subprocess (probably calling snmptranslate + snmpbulkget, or snmptable) you need to do a lot of
string parsing which may get inefficient and error prone.

## Current state and future ##
At the moment, I'm doing this project for my exercise and own usage.
The interface is not stable enough to release anything, and some features
are missing (e.g., support for SNMPv1 via getnext).

From my point of view it would be great if the code could be merged into upstream NetSNMP package some time.
At least, this package was designed with that in mind, so merging would be very easy.
The Python C extension reuses code and concepts from the
[original Net-SNMP Python Bindings](http://net-snmp.sourceforge.net/wiki/index.php/Python_Bindings)
as far as possible. Table handling was initially taken from netsnmp/apps/snmptable.c.

## How does it actually work? ##
Here are some examples how to use netsnmptable.

### Example 1: Print out the Host Resources Storage Table###
```python
import netsnmp
import netsnmptable

# create a table object
table = netsnmptable.Table(netsnmp.Session(Version=2,
            DestHost='localhost',
            Community='public'))
table.parse_mib(netsnmp.Varbind('HOST-RESOURCES-MIB:hrStorageTable', 0))

# go and get the table...
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
```

Results in
```
Index      Description               Units      Size       Used      
1          Physical memory           1024       2054128    233540    
3          Virtual memory            1024       3099628    361064    
6          Memory buffers            1024       2054128    14564     
7          Cached memory             1024       82232      82232     
8          Shared memory             1024       0                    
10         Swap space                1024       1045500    127524    
31         /dev                      4096       2560       0         
32         /                         4096       10063200   2495552   
33         /sys/fs/fuse/connections  4096       0          0         
34         /run/vmblock-fuse         512        0          0         
35         /media/sources            4096       121833215  92355036  
36         /media/sandboxes          4096       121833215  92355036  
37         /media/cdrom0             2048       32876      32876    
```

### Example 2: Query a multi-indexed table ###
Tables with more than one index are no problem. The row key tuple just gets more elements.

Let's setup following table in snmpd.conf:
```
table MYTABLETEST::testTable 
#                               idx1          idx2            aValue  anotherValue
add_row MYTABLETEST::testTable "OuterIdx_1"   "InnerIdx_1"     1       2
add_row MYTABLETEST::testTable "OuterIdx_1"   "InnerIdx_2"     3       4
add_row MYTABLETEST::testTable "OuterIdx_2"   "InnerIdx_1"     5       6
add_row MYTABLETEST::testTable "OuterIdx_2"   "InnerIdx_2"     7       8
add_row MYTABLETEST::testTable "OuterIdx_3"   "InnerIdx_1"     9      10
add_row MYTABLETEST::testTable "OuterIdx_3"   "InnerIdx_2"    11      12
```

Now query it with
```python
import netsnmp
import netsnmptable
import pprint
from types import MethodType

def varbind_to_repr(self):
    return self.type + ":" + self.val
netsnmp.Varbind.__repr__ = MethodType(varbind_to_repr, None, netsnmp.Varbind)

table = netsnmptable.Table(netsnmp.Session(Version=2,
            DestHost='localhost',
            Community='public'))
table.parse_mib(netsnmp.Varbind('MYTABLETEST::testTable', 0))
tbldict = table.fetch()
pprint.pprint(tbldict)
```

This gives
```
{('OuterIdx_1', 'InnerIdx_1'): {'aValue': INTEGER32:1,
                                'anotherValue': INTEGER32:2},
 ('OuterIdx_1', 'InnerIdx_2'): {'aValue': INTEGER32:3,
                                'anotherValue': INTEGER32:4},
 ('OuterIdx_2', 'InnerIdx_1'): {'aValue': INTEGER32:5,
                                'anotherValue': INTEGER32:6},
 ('OuterIdx_2', 'InnerIdx_2'): {'aValue': INTEGER32:7,
                                'anotherValue': INTEGER32:8},
 ('OuterIdx_3', 'InnerIdx_1'): {'aValue': INTEGER32:9,
                                'anotherValue': INTEGER32:10},
 ('OuterIdx_3', 'InnerIdx_2'): {'aValue': INTEGER32:11,
                                'anotherValue': INTEGER32:12}}
```

### Example 3: Query a multi-indexed table with only selected index values ###
Say we want to fetch from the same table, but only entries for "OuterIdx_2".
```python
table = netsnmptable.Table(netsnmp.Session(Version=2,
            DestHost='localhost',
            Community='public'))
table.parse_mib(netsnmp.Varbind('MYTABLETEST::testTable', 0))
tbldict = table.fetch(iid = netsnmptable.str_to_varlen_iid("OuterIdx_2"))
pprint.pprint(tbldict)
```

Results in
```
{('OuterIdx_2', 'InnerIdx_1'): {'aValue': INTEGER32:5,
                                'anotherValue': INTEGER32:6},
 ('OuterIdx_2', 'InnerIdx_2'): {'aValue': INTEGER32:7,
                                'anotherValue': INTEGER32:8}}
```

## Development Resources ##
- NetSNMP [source code](http://sourceforge.net/p/net-snmp/code)
- NetSNMP [library API](http://www.net-snmp.org/dev/agent/group__library.html)
- Python C Extensions: [Python/C API Reference](https://docs.python.org/2/c-api/), [Example on python.org](https://docs.python.org/2/extending/extending.html) 
- SMIv2 Table Syntax specification, [RFC 2578, Chapter 7.1.12 Conceptual Tables](https://tools.ietf.org/html/rfc2578#section-7.1.12)
- SMIv2 INDEX clause specification, [RFC 2578, Chapter 7.7 Mapping of the INDEX clause](https://tools.ietf.org/html/rfc2578#section-7.7)
- SNMP getbulk specification, [RFC 3416, Chapter 4.2.3 The GetBulkRequest-PDU](https://tools.ietf.org/html/rfc3416#section-4.2.3)
