import netsnmp
from .netsnmptable import (
    create_from_mib, Table
)

# monkey patching netsnmp
netsnmp.Session.table_from_mib = netsnmptable.create_from_mib
