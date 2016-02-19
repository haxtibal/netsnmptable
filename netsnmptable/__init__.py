import netsnmp
from .netsnmptable import (
    create_from_mib, str_to_fixlen_iid, str_to_varlen_iid, Table
)

# monkey patching netsnmp
netsnmp.Session.table_from_mib = netsnmptable.create_from_mib
