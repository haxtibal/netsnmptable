# netsnmptable #
*A Python package to query SNMP tables and table subsets, complementing the original Net-SNMP Python Bindings.*

## Current state ##
At the moment, main purpose is evaluation and exercise. The project is far from being released.

## What? ##
This project implements SNMP table query functionality for Python using the NetSNMP libraries.
It is a C extension module similar to the [original Net-SNMP Python Bindings](http://net-snmp.sourceforge.net/wiki/index.php/Python_Bindings).

Table handling is close to netsnmp/apps/snmptable.c, and the Python C extension is close to python/netsnmp/client_intf.c.
This shall leave a path for merging it into Net-SNMP.

## Why? ##
In short: Because there's currently no way to do this in Python, while using NetSNMP technology.

If a client wants to fetch SNMP tables efficiently from an agent, this is done by issuing one or several
getbulk or getnext (prior to SNMP v2) request to an agent. This is a rather complicated process.
Users may be happy with the netsnmp commandline tool called snmptable, which handles the process internally
and provides a simple output interface where cells are printed in rows and columns as you'd expect it for a table.

There's no Python equivalent to snmptable if you program using the Net-SNMP Python Bindings.
So you either have to do something like subprocess.call(["snmptable", ...]),
or write a table handler using netsnmp.Session.getbulk() in pure Python.
But both ways you're technically limited.
E.g., if using netsnmp.Session.getbulk() you'll miss get_table() in the bindings, so you can't parse a MIB
and detect the column OIDs.
Or, if using subprocess (probably calling snmptranslate + snmpbulkget, or snmptable) you need to do a lot of
string parsing which may get inefficient and error prone.

## How? ##

The package interface is only draft. It models a table as dictionary of dictionaries.

```python
import netsnmp
import netsnmptable
import pprint

netsnmp_session = netsnmp.Session(Version=2, DestHost='localhost', Community='public')
vb = netsnmp.Varbind('MYTABLETEST::testTable', 0)
table = netsnmptable.Table(netsnmp_session)
tbldict = table.get(vb)
pprint.pprint(tbldict)
```

Results in
```
{('First', 'SubFirst'): {'aValue': '1', 'anotherValue': '1'},
 ('First', 'SubSecond'): {'aValue': '2', 'anotherValue': '2'},
 ('Second', 'SubFirst'): {'aValue': '3', 'anotherValue': '3'},
 ('Second', 'SubSecond'): {'aValue': '4', 'anotherValue': '4'}}
```

Additionally, you can limit the query to certain sub index...
```python
table.set_start_index(("First",))
tbldict = table.get(vb)
pprint.pprint(tbldict)
```

```
{('First', 'SubFirst'): {'aValue': '1', 'anotherValue': '1'},
 ('First', 'SubSecond'): {'aValue': '2', 'anotherValue': '2'}}
```

## Development Resources ##
- NetSNMP [source code](http://sourceforge.net/p/net-snmp/code)
- NetSNMP [library API](http://www.net-snmp.org/dev/agent/group__library.html)
- Python C Extensions: [Python/C API Reference](https://docs.python.org/2/c-api/), [Example on python.org](https://docs.python.org/2/extending/extending.html) 
- SMIv2 Table Syntax specification, [RFC 2578, Chapter 7.1.12 Conceptual Tables](https://tools.ietf.org/html/rfc2578#section-7.1.12)
- SMIv2 INDEX clause specification, [RFC 2578, Chapter 7.7 Mapping of the INDEX clause](https://tools.ietf.org/html/rfc2578#section-7.7)
- SNMP getbulk specification, [RFC 3416, Chapter 4.2.3 The GetBulkRequest-PDU](https://tools.ietf.org/html/rfc3416#section-4.2.3)
