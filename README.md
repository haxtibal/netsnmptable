# netsnmptable #
*A Python C Extension package to query SNMP tables and table subsets based on the Net-SNMP libraries.*

## Goals ##
The project aims to bring SNMP table query functionality for Python.
- Based on the native Net-SNMP libraries
- High-level Python interface
- Resulting data structures behave like tables, in the sense that a cell is addressable by row and column
- Table structure can be defined from MIB
- Indexes are selectable to some extent, to save bandwidth and memory when you want to only retrieve a small portion of a large table
- Include a unit test solution that does not depend on external services
- Focus on table functions only, don't re-implement other manager functionality
- Eventually find a parent-project that will integrate this package. Implementation concepts and coding conventions from the parent project can be adapted.

## Current state and future ##
The API is currently not yet stable. Implementation works and is tested on following targets
- Linux x86_64
 - Debian Wheezy, python 2.7.3, netsnmp 5.4.3
 - Ubuntu Wily, python 2.7.9, netsnmp 5.7.3

IMO it would be good to eventually integrate the table functions into a more complete, all-in-one Python SNMP Client package.
If you are the maintainer of such a package and like the idea, please let me know.

I currently do monkey patching of the [original Net-SNMP Python Bindings](http://net-snmp.sourceforge.net/wiki/index.php/Python_Bindings) to fake that integration, but are looking forward for better options.

## How to build and install ##

Prerequisites:
- C compiler toolchain
- Python 2
- Net-SNMP libraries and headers

Download and extract the source package, then
```shell
~/netsnmptable$ python setup.py build
~/netsnmptable$ sudo python setup.py install
```

## License ##

This software is released under the LGPLv3 license.

Some re-used code is copyrighted. See the [LICENSE](https://github.com/haxtibal/netsnmptable/blob/master/LICENSE) file for full license text and the original copyright notices.

## Examples ##
Here are some examples how to use netsnmptable.

### Example 1: Print out the Host Resources Storage Table###
```python
import netsnmp
import netsnmptable

# create a table object
session = netsnmp.Session(Version=2, DestHost='localhost', Community='public')
table = session.table_from_mib('HOST-RESOURCES-MIB:hrStorageTable')

# go and get the table...
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

session = netsnmp.Session(Version=2, DestHost='localhost', Community='public')
table = session.table_from_mib('MYTABLETEST::testTable')
tbldict = table.get_entries()
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
session = netsnmp.Session(Version=2, DestHost='localhost', Community='public')
table = session.table_from_mib('MYTABLETEST::testTable')
tbldict = table.get_entries(iid = netsnmptable.str_to_varlen_iid("OuterIdx_2"))
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
- Net-SNMP [source code](http://sourceforge.net/p/net-snmp/code)
- Net-SNMP [library API](http://www.net-snmp.org/dev/agent/group__library.html)
- Python C Extensions: [Python/C API Reference](https://docs.python.org/2/c-api/), [Example on python.org](https://docs.python.org/2/extending/extending.html) 
- SMIv2 Conceptual Table definitions, [RFC 2578 Chapter 7.1.12 and 7.1.12.1](https://tools.ietf.org/html/rfc2578#section-7.1.12)
- SMIv2 Conceptual Table indexing rules, [RFC 2578 Chapter 7.7, 7.8, and 7.8.1](https://tools.ietf.org/html/rfc2578#section-7.7)
- SMIv2 Conceptual Row sub-identifier (also known as Entry), [RFC 4181 Chapter 4.6.5](https://tools.ietf.org/html/rfc4181#section-4.6.5)
- SNMP GetBulkRequest-PDU specification, [RFC 3416 Chapter 4.2.3](https://tools.ietf.org/html/rfc3416#section-4.2.3)

## Related work ##
Other people also tackle the hassle of dealing with SNMP in Python and found interesting approaches.
Below is an incomplete and unordered list.

### Libraries ###
- [easysnmp](https://github.com/fgimian/easysnmp) - "Pythonic SNMP library based on the official Net-SNMP bindings". Very nice approach to improve the original by sophisticated tests and more pythonic interface and design. 
- [PySNMP](http://pysnmp.sourceforge.net) - Pure Python SNMP library, to act in Agent/Manager/Proxy role.
- [snimpy](https://github.com/vincentbernat/snimpy) - Library on top of PySNMP. "Snimpy is aimed at being the more Pythonic possible. You should forget that you are doing SNMP requests."
- [Net::SNMP](http://search.cpan.org/~dtown/Net-SNMP-v6.0.1/) - Object oriented Perl interface to Net-SNMP with [table handling](http://search.cpan.org/~dtown/Net-SNMP-v6.0.1/lib/Net/SNMP.pm#get_table%28%29_-_retrieve_a_table_from_the_remote_agent).
- [Multi-core SNMP](https://code.google.com/p/multicore-snmp) Works around lacking asynchronous support in original Python Bindings by using a process pool.

### Table handling ###
- [HNMP](https://github.com/trehn/hnmp) - PySNMP based package, to "ease the pain of retrieving and processing data from SNMP-capable devices". Let's you define table structures manually, rather than parsing them from MIB, because "Depending on MIB files would make the calling piece of code harder to distribute". 
- [A Munin Plugin](https://raw.githubusercontent.com/munin-monitoring/contrib/master/plugins/snmp/snmp__airport), get's table via netsnmp.snmpwalk and converts it to a dictionary.

### Other SNMP tools ###
- [python-netsnmpagent](https://github.com/pief/python-netsnmpagent) - Let's you implement agents in Python (incorporates netsnmp via ctypes). Very useful for testing.
