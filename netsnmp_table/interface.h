#ifndef INTERFACE_H_
#define INTERFACE_H_


#include <Python.h>

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>

extern PyObject * netsnmp_table(PyObject *self, PyObject *args);

#endif
