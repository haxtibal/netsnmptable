# TODO #

## ##

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