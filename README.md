# netsnmp_table #
*A Python package to query SNMP tables and table subsets, on top of the original Net-SNMP Python Bindings.*

## Current state ##
At the moment, main purpose is evaluation and exercise. The project is far from being released.

## What? ##
This project implements SNMP table query functionality for python on top of the
[Net-SNMP Python Bindings](http://net-snmp.sourceforge.net/wiki/index.php/Python_Bindings).
It is a C extension module with full access to the netsnmp C library.

Table handling is close to netsnmp/apps/snmptable.c, and the python C extension is close to the
original netsnmp extension. This shall leave a path for merging it into Net-SNMP.

## Why? ##
If a client wants to fetch SNMP tables efficiently from an agent, this is done by issuing one or several
getbulk or getnext (prior to SNMP v2) request to an agent. This is a rather complicated process.
Users may be happy with the netsnmp commandline tool called snmptable, which handles the process internally
and provides a simple output interface where cells are printed in rows and columns as you'd expect it for a table.

There's no python equivalent to snmptable if you program using the Net-SNMP Python Bindings.
So you either have to do something like subprocess.call(["snmptable", ...]),
or write a table handler using netsnmp.Session.getbulk() in pure python.
But both ways you're technically limited.
E.g., if using netsnmp.Session.getbulk() you'll miss get_table() in the bindings, so you can't parse a MIB
and detect the column OIDs.
Or, if using subprocess (probably calling snmptranslate + snmpbulkget, or snmptable) you need to do a lot of
string parsing which may get inefficient and error prone.

## How? ##

The package interface is only draft. It models a table as dictinory of dictionaries.

```python
import netsnmp
import netsnmp_table
import pprint

netsnmp_session = netsnmp.Session(Version=2, DestHost='localhost', Community='public')
vb = netsnmp.Varbind('MYTABLETEST::testTable', 0)
table = netsnmp_table.Table(netsnmp_session)
tbldict = table.get(vb)
pprint.pprint(tbldict)
```

Gives you
```
{'"First"."SubFirst"': {'aValue': '1', 'anotherValue': '1'},
 '"First"."SubSecond"': {'aValue': '2', 'anotherValue': '2'},
 '"Second"."SubFirst"': {'aValue': '3', 'anotherValue': '3'},
 '"Second"."SubSecond"': {'aValue': '4', 'anotherValue': '4'}}
```
