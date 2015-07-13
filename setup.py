from distutils.core import setup, Extension
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
    libs = re.findall(r" -l(\S+)", netsnmp_libs)
    libdirs = re.findall(r" -L(\S+)", libdir)
    incdirs = re.findall(r" -I(\S+)", incdir)
else:
    netsnmp_libs = os.popen('net-snmp-config --libs').read()
    libdirs = re.findall(r" -L(\S+)", netsnmp_libs)
    incdirs = []
    libs = re.findall(r" -l(\S+)", netsnmp_libs)

setup(
    name="netsnmp_table", version="0.0.1",
    description = 'A Python package to query SNMP tables and table subsets, on top of the original Net-SNMP Python Bindings.',
    author = 'Tobias Deiminger',
    author_email = 'tobias.deiminger@gmail.com',
    url = 'http://github.com/haxtibal/netsnmp_table',
    license="BSD",
    packages=find_packages(),
    test_suite = "netsnmp_table.tests.test",

    ext_modules = [
       Extension("netsnmp_table.interface", ["netsnmp_table/interface.c", "netsnmp_table/table.c", "netsnmp_table/util.c"],
                 library_dirs=libdirs,
                 include_dirs=incdirs,
                 libraries=libs )
       ]
    )
