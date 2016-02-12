import netsnmp
from netsnmptable import *

# monkey patching netsnmp
netsnmp.Session.table_from_mib = netsnmptable.create_from_mib
