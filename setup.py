from distutils.core import setup, Extension
from distutils.version import LooseVersion, StrictVersion
from setuptools import setup, Extension, find_packages
import os
import re
import string
import sys

intree=0

args = sys.argv[:]
for arg in args:
    if string.find(arg,'--basedir=') == 0:
        basedir = string.split(arg,'=')[1]
	sys.argv.remove(arg)
        intree=1

if intree:
    netsnmp_libs = os.popen(basedir+'/net-snmp-config --libs').read()
    libdir = os.popen(basedir+'/net-snmp-config --build-lib-dirs '+basedir).read()
    incdir = os.popen(basedir+'/net-snmp-config --build-includes '+basedir).read()
    netsnmp_version = os.popen(basedir+'/net-snmp-config --version '+basedir).read()
    libs = re.findall(r" -l(\S+)", netsnmp_libs)
    libdirs = re.findall(r" -L(\S+)", libdir)
    incdirs = re.findall(r" -I(\S+)", incdir)
else:
    netsnmp_libs = os.popen('net-snmp-config --libs').read()
    netsnmp_version = os.popen('net-snmp-config --version').read()
    libdirs = re.findall(r" -L(\S+)", netsnmp_libs)
    incdirs = []
    libs = re.findall(r" -l(\S+)", netsnmp_libs)

if LooseVersion(netsnmp_version) >= LooseVersion("5.5.0"):
    cdefs = [("NETSNMP_SINGLE_API", None)]
    #netsnmp from 5.5 uses single API session pointers, like ss = snmp_sess_open(&session);
else:
    cdefs = None
    #netsnmp up to 5.4.4 uses traditional API session pointers, like ss = snmp_open(&session);

setup(
    name="netsnmptable", version="0.0.1",
    description = 'A Python package to query SNMP tables and table subsets, on top of the original Net-SNMP Python Bindings.',
    author = 'Tobias Deiminger',
    author_email = 'tobias.deiminger@gmail.com',
    url = 'http://github.com/haxtibal/netsnmptable',
    license="BSD",
    packages=find_packages(),
    test_suite = "netsnmptable.tests.test",

    ext_modules = [
       Extension("netsnmptable.interface", ["netsnmptable/interface.c", "netsnmptable/table.c", "netsnmptable/util.c"],
                 library_dirs=libdirs,
                 include_dirs=incdirs,
                 libraries=libs,
                 define_macros=cdefs)
       ]
    )
