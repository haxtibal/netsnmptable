#include <Python.h>

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>

typedef netsnmp_session SnmpSession;

#define MAX_TYPE_NAME_LEN 32
#define STR_BUF_SIZE (MAX_TYPE_NAME_LEN * MAX_OID_LEN)
#define NO_FLAGS 0x00
#define USE_BASIC 0
#define SAFE_FREE(x) do {if (x != NULL) free(x);} while(/*CONSTCOND*/0)
#define SUCCESS (1)
#define FAILURE (0)

extern PyObject * netsnmp_table(PyObject *self, PyObject *args);
extern PyObject* py_netsnmp_attr_obj(PyObject *obj, char * attr_name);
extern long long py_netsnmp_attr_long(PyObject *obj, char * attr_name);
extern int py_netsnmp_attr_set_string(PyObject *obj, char *attr_name, char *val, size_t len);
extern int py_netsnmp_attr_string(PyObject *obj, char * attr_name, char **val, Py_ssize_t *len);
