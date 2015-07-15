#include <Python.h>
#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include "interface.h"
#include "util.h"
#include "table.h"

PyObject* netsnmptable_parse_mib(PyObject *self, PyObject *args)
{
    PyObject *varbind;
    PyObject *py_indexes_list;
    PyObject *py_columns_list;
    table_info_t* tbl = NULL;
    char *tag;
    int ret_exceptional = 0;
    int col;

    if (args) {
      if (!PyArg_ParseTuple(args, "OOO", &varbind, &py_indexes_list, &py_columns_list)) {
          PyErr_SetString(PyExc_LookupError, "Invalid arguments");
          ret_exceptional = 1;
        goto done;
      }

      /* get the table root oid out of varbind, which is contained in args */
      if (varbind) {
          if (py_netsnmp_attr_string(varbind, "tag", &tag, NULL) < 0) {
              PyErr_SetString(PyExc_TypeError, "Varbind object has no tag attribute");
              ret_exceptional = 1;
              goto done;
          } else {
              tbl = table_allocate(tag);
              if (!tbl) {
                  ret_exceptional = 1;
                  goto done;
              }

              if (table_get_field_names(tbl) < 0)
                  goto done;

              for (col=0; col<tbl->column_header.fields; col++) {
                  PyList_Append(py_columns_list, tbl->column_header.column[col].py_label_str);
                  PyList_Reverse(py_columns_list);
              }
          }
      }
    }

    done:
      if (ret_exceptional) {
          table_deallocate(tbl);
          return NULL;
      }

      return PyLong_FromVoidPtr((void *)tbl);
}

PyObject* netsnmptable_cleanup(PyObject *self, PyObject *args)
{
    table_info_t* tbl = NULL;
    if (args) {
      if (!PyArg_ParseTuple(args, "O", &tbl)) {
        goto done;
      }
    }

    table_deallocate(tbl);

    done:
        return Py_BuildValue("");
}

PyObject* netsnmptable_fetch(PyObject *self, PyObject *args)
{
  PyObject* table = NULL;
  PyObject* session = NULL;
  PyObject* val_tuple = NULL;
  table_info_t* tbl = NULL;
  long ss_opaque = 0;
  netsnmp_session *ss = NULL;
  long max_repeaters;
  int ret_exceptional = 0;

  if (args) {
    if (!PyArg_ParseTuple(args, "O", &table)) {
      goto done;
    }

    session = py_netsnmp_attr_obj(table, "netsnmp_session");
    if (!session) {
        PyErr_SetString(PyExc_TypeError, "Table object has no netsnmp_session attribute");
        ret_exceptional = 1;
        goto done;
    }

    /* get netsnmp session pointer from python Session instance */
    ss_opaque = py_netsnmp_attr_long(session, "sess_ptr");
    if (ss_opaque < 0) {
        PyErr_SetString(PyExc_TypeError, "Session object has no sess_ptr attribute");
        ret_exceptional = 1;
        goto done;
    }

    /* get netsnmp session pointer from python Session instance */
    tbl = (table_info_t*) py_netsnmp_attr_long(table, "_tbl_ptr");
    if (tbl < 0) {
        PyErr_SetString(PyExc_TypeError, "Table object has no _tbl_ptr attribute");
        ret_exceptional = 1;
        goto done;
    }

    max_repeaters = py_netsnmp_attr_long(table, "max_repeaters");
    if (max_repeaters < 0) {
        PyErr_SetString(PyExc_RuntimeError, "Table object has no max_repeaters attribute");
        ret_exceptional = 1;
        goto done;
    }

    /* NetSNMP in 5.4.x used to open session with ss = snmp_open(&session) for SNMPv1/v2,
     * but they changed to ss = snmp_sess_open(&session) with 5.5.
     * TODO: We probably have no chance to detect which API call was used to get the session pointer,
     * and have to introduce our own session.
     */
#ifdef NETSNMP_SINGLE_API
    ss = snmp_sess_session((void*)ss_opaque);
#else
    ss = (netsnmp_session*) ss_opaque;
#endif

     py_netsnmp_attr_oid(table, "start_index_oid", tbl->column_header.start_idx,
             sizeof(tbl->column_header.start_idx), &tbl->column_header.start_idx_length);
     val_tuple = table_getbulk_sub_entries(tbl, ss, max_repeaters, session);
  }

  done:
    if (ret_exceptional)
        return NULL;

    return (val_tuple ? val_tuple : Py_BuildValue(""));
}

static PyMethodDef InterfaceMethods[] = {
    {"table_parse_mib",  netsnmptable_parse_mib, METH_VARARGS,
      "Get table structure from MIB."},
    {"table_fetch",  netsnmptable_fetch, METH_VARARGS,
     "Perform an SNMP table fetch."},
    {"table_cleanup",  netsnmptable_cleanup, METH_VARARGS,
     "Perform an SNMP table fetch."},
    {NULL, NULL, 0, NULL}        /* Sentinel */
};

PyMODINIT_FUNC
initinterface(void)
{
    (void) Py_InitModule("interface", InterfaceMethods);
}
