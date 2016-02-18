# TODO #

## Refactor API ##

### Avoid passing netsnmp.Session to Table ###
With current API:
```python
import netsnmp
import netsnmptable
table = netsnmptable.Table(netsnmp.Session(Version=2,
            DestHost='localhost',
            Community='public'))
table.parse_mib(netsnmp.Varbind('HOST-RESOURCES-MIB:hrStorageTable', 0))
table.fetch()
```

Better:
```python
import netsnmp
import netsnmptable
session = netsnmp.Session(Version=2, DestHost='localhost', Community='public')
table = session.table_from_mib(netsnmp.Varbind('HOST-RESOURCES-MIB:hrStorageTable', 0))
table.fetch()
```
Done. But needs monkey patching for now.

### Do it like the Perl Module? ###
Evaluate the [table API of Net::SNMP Perl Module](http://cpansearch.perl.org/src/DTOWN/Net-SNMP-v6.0.1/examples/table.pl) to get some ideas.

There're two functions for table, namely get_table and get_entries. From the [source code](http://cpansearch.perl.org/src/DTOWN/Net-SNMP-v6.0.1/lib/Net/SNMP.pm):
```perl
$result = $session->get_table(
                       [-callback        => sub {},]     # non-blocking
                       [-delay           => $seconds,]   # non-blocking 
                       [-contextengineid => $engine_id,] # v3 
                       [-contextname     => $name,]      # v3
                       -baseoid          => $oid,
                       [-maxrepetitions  => $max_reps,]  # v2c/v3
                    );
```
Gets everything below the baseoid. No column definition is required.
May result in big data sets, even if you're only interested in a few rows.

```perl
$result = $session->get_entries(
                       [-callback        => sub {},]     # non-blocking
                       [-delay           => $seconds,]   # non-blocking
                       [-contextengineid => $engine_id,] # v3
                       [-contextname     => $name,]      # v3
                       -columns          => \@columns,
                       [-startindex      => $start,]
                       [-endindex        => $end,]
                       [-maxrepetitions  => $max_reps,]  # v2c/v3
                    );
```
Gets everything between a startindex and endindex. Needs a column definition.
This a bit like the current netsnmptable implementation.

Synchronous API for get_table:
```perl
my ($session, $error) = Net::SNMP->session(
   -hostname  => $ARGV[0] || 'localhost',
   -community => $ARGV[1] || 'public',
   -port      => $ARGV[2] || SNMP_PORT,
   -version   => 'snmpv2c',
);
my $result = $session->get_table(-baseoid => $OID_ifTable))
```

### How to represent SNMP types in Python ###
Example: Query a INTEGER32 variable with value 1. In Python, shall the return value be int(1) or str("1")?

netsnmp bindings Varbind handling:
- [Varbind.__init__()](https://sourceforge.net/p/net-snmp/code/ci/master/tree/python/netsnmp/client.py#l52) converts every initialization argument into a string object.
- [netsnmp_get()](https://sourceforge.net/p/net-snmp/code/ci/master/tree/python/netsnmp/client_intf.c#l1597) saves the 'val' attribute as string
- UseSprintValue = <non-zero> means "certain data types being returned in non-canonical format"
- __translate_appl_type() turns 'INTEGER32' into TYPE_INTEGER32
- __add_var_val_str() converts string to long ```*(vars->val.integer) = strtol(val,NULL,0);```

README says "for set operations the <val> format must be canonical to ensure unambiguous translation.",
and defines canonical forms as
- OBJECTID => dotted-decimal (e.g., .1.3.6.1.2.1.1.1)
- OCTETSTR => perl scalar containing octets
- INTEGER => decimal signed integer (or enum)
- NETADDR => dotted-decimal
- IPADDR => dotted-decimal
- COUNTER => decimal unsigned integer
- COUNTER64 => decimal unsigned integer
- GAUGE => decimal unsigned integer
- UINTEGER => decimal unsigned integer
- TICKS  => decimal unsigned integer
- OPAQUE => perl scalar containing octets
- NULL => perl scalar containing nothing
This is obviously not fully finished, as there are no "perl scalars" in python.

A test shows that INTEGER32 is given as str type.
```python
import os
import netsnmp
netsnmp_session = netsnmp.Session(Version=2, DestHost='localhost:1234', Community='public')
netsnmp_session.UseSprintValue=0
vl = netsnmp.VarList(netsnmp.Varbind('TEST-MIB::singleIdxTableEntryValue', '10.84.104.105.115.73.115.82.111.119.49'))
result = netsnmp_session.get(vl)
print(type(vl[0].val))
 <type 'str'>
```

## Re-use existing getbulk implementation? ##
Maybe [netsnmp_getbulk](http://sourceforge.net/p/net-snmp/code/ci/master/tree/python/netsnmp/client_intf.c#l2207) could be reused somehow.

Issues:
- netsnmp_getbulk returns a list (class VarList) of variable bindings (class Varbind). Actually, the Varbind content is a bit to much abstracted and depends on settings
 - UseLongNames: 0 = tags in short name (e.g. sysDescr), non-zero: tags in longer MIB name convention (e.g. system.sysDescr)
 - UseSprintValue: 0 = store return values in canonical format, non-zero = store return values in non-canonical format
 - UseEnums: 0 = store integer return values unconverted, non-zero = store integer return values converted to enumeration identifiers
 - UseNumeric: 0 = tags are translated, non-zero = tags are untranslated (i.e. dotted-decimal)

Varbind
|\
| tag
|\
| iid
|\
| val
 \
  type

For positioning the response values into a table, and detecting the table end, reliable low-level information is needed.
This code-part shows why:

```c
    if (vars->type == SNMP_ENDOFMIBVIEW) {
        // end of mib detected
        return NULL;
    } else if (memcmp(vars->name, table_info->column_scheme.name,
        table_info->rootlen * sizeof(oid)) != 0) {
        // if all OIDs up to conceptual row sub-id do not match, response-varbinding does not belong to table
        return NULL;
    } else if (vars->name[table_info->column_scheme.name_length]
            != column->subid) {
        // if conceptual column sub-ids don't match, resposne-varbinding does not bleong to expected column 
        return NULL;
    } else if (memcmp(&vars->name[table_info->column_scheme.name_length + 1],
            table_info->column_scheme.start_idx,
            table_info->column_scheme.start_idx_length * sizeof(oid)) != 0) {
        // (multiple) index values do not match the expected ones
        return NULL;
    }
```

## Shift some table-handing to pure Python? ##
With a suitable getbulk C-implementation, a good amount of the table handling could be shifted from C to pure python...

## What's going on with the python bindings? ##
This are the current [netsnmp developers](https://sourceforge.net/p/net-snmp/_members).

Recently from the mailing list
- https://sourceforge.net/p/net-snmp/mailman/message/21556905/
- https://sourceforge.net/p/net-snmp/mailman/message/34742260/
- https://sourceforge.net/p/net-snmp/mailman/message/32049411/

Recent changes:
- https://sourceforge.net/p/net-snmp/code/ci/99f836007fd823014b0efb037a6e707b56807ffb

## Licensing ##

If possible, the package shall be licensed BSD like.

Which work of other people is included?

Inlcuded as code (or snippets)
- table.c includes code from netsnmp, apps/snmptable.c
 - Copyright 1997 Niels Baggesen
- util.c includes code from netsnmp, python/netsnmp/client_intf.c
 - file: no license or copyright
 - directory: copyright (but no license) in python bindings: Copyright (c) 2006, ScienceLogic, LLC
 - package: netsnmp contains a top level COPYING file (all entries BSD-like)
- (removed for now) tests/testagent/netsnmpagent.py and netsnmpapi.py is copied from [python-netsnmpagent](https://github.com/pief/python-netsnmpagent)
 - Copyright (c) 2013 Pieter Hollants <pieter@hollants.com>
 - Licensed under the GNU Public License (GPL) version 3
- tests/ include MIBs that have been stripped from IETF RFCs

Used
- we import package netsnmp
- we derive from class netsnmpagent

Can GPLv3 work be bundled and used for unittests?

GPLv3, comma 5, applies to "a work based on the Program":
"You must license the entire work, as a whole, under this License to anyone who comes into possession of a copy. This License will therefore apply, along with any applicable section 7 additional terms, to the whole of the work, and all its parts, regardless of how they are packaged. This License gives no permission to license the work in any other way, but it does not invalidate such permission if you have separately received it."

## Planned features / requirements ##

### Table queries ###

Query works with SNMPv1. Note: only possible with getnext, as getbulk is not available in v1.
(draft)

Query works with SNMPv2c.
(implemented)

A table can be queried all at once.
(implemented)

A table with multiple indexes can be queried, where the query is limited to outermost sub-indexes.
(implemented)

A row can be queried exactly. (possible at all? getnext/getbulk may be not suitable)
(draft)

A table containing "wholes" can be queried without errors.
(draft)

integer-valued indexes are supported.
(implemented)

string-valued, fixed-length string indexes are supported.
Note: These indexes are marked as IMPLIED in MIB, and can only appear as last index for a table.
Example: idx = "dave"  =  'd'.'a'.'v'.'e'.
(implemented)

string-valued, variable-length strings indexes are supported.

Example: idx = "dave"  =  4.'d'.'a'.'v'.'e'.
(implemented)

object identifier-valued - preceded by the IMPLIED keyword - indexes are supported.
(draft)

object identifier-valued - preceded by the IMPLIED keyword - indexes are supported.
(draft)

IpAddress-valued indexes are supported.
(draft)

### Table options ###

max_repeaters is adjustable for getbulk requests. Default value is 10.
(implemented)

### Table structure ###

Table structure is determined automatically (from MIBs). The table root node must be supplied. 
(implemented)

### Output format ###

Results are provided as dictionary of dictionaries.
(implemented)

Key of the outer dictionary is a tuple containing one or more row indexes.
(implemented)

Key of the inner dictionary is the column OID string, as it is defined in the MIB.
(implemented)

Column instance values are stored as the values of the inner dictionary.
(implemented)

Column instance values are represented as their equivalent python type.
Note: Done in accordance to the netsnmp python bindings get* methods.
SMIv2 base type mapping: INTEGER -> int, OCTET STRING -> string, OBJECT IDENTIFIER -> ?
SMIv2 application-defined: Integer32 -> int, IpAddress -> ?, Counter32 -> ?, Gauge32 ->?, Unsigned32 -> ?, TimeTicks, Opaque, and Counter64.
(draft)

### Error Handling ###

Querying a host without SNMP service running results in reasonable error response.
(implemented)

Querying a table without any entry results in an empty dictionary.
(implemented)

Querying when required MIBs are not available results in reasonable error response. (Excpetion?)
(draft)
