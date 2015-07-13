#include <Python.h>
#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include "interface.h"
#include "util.h"
#include "table.h"

PyObject * netsnmp_table(PyObject *self, PyObject *args)
{
  PyObject *session;
  PyObject *varbind;
  PyObject *val_tuple = NULL;
  void* ss_opaque = NULL;
  netsnmp_session *ss = NULL;
  long max_repeaters;
  char *tag;
  char *iid;

  table_info_t tbl;

  if (args) {

    PyObject* self;
    if (!PyArg_ParseTuple(args, "OO", &self, &varbind)) {
      goto done;
    }

    session = py_netsnmp_attr_obj(self, "netsnmp_session");
    if (!session) {
        goto done;
    }

    /* get netsnmp session pointer from python Session instance */
    ss_opaque = (void*) py_netsnmp_attr_long(session, "sess_ptr");
    if (!ss_opaque) {
        PyErr_SetString(PyExc_RuntimeError, "invalid netsnmp session pointer");
        goto done;
    }

    /* NetSNMP in 5.4.x used to open session with ss = snmp_open(&session) for SNMPv1/v2,
     * but they changed to ss = snmp_sess_open(&session) with 5.5.
     * TODO: We probably have no chance to detect which API call was used to get the session pointer,
     * and have to introduce our own session.
     */
#ifdef NETSNMP_SINGLE_API
    ss = snmp_sess_session(ss_opaque);
#else
    ss = (netsnmp_session*) ss_opaque;
#endif

    max_repeaters = py_netsnmp_attr_long(self, "max_repeaters");
    if (max_repeaters < 0) {
        PyErr_SetString(PyExc_RuntimeError, "bad max_repeaters attribute");
        goto done;
    }

    /* get the table root oid out of varbind, which is contained in args */
    if (varbind) {
        if (py_netsnmp_attr_string(varbind, "tag", &tag, NULL) < 0 ||
                py_netsnmp_attr_string(varbind, "iid", &iid, NULL) < 0)
        {
            goto done;
        } else {
            if (init_table(&tbl, tag) < 0) {
                goto done;
            }

            py_netsnmp_attr_oid(self, "start_index_oid", tbl.column_header.start_idx, sizeof(tbl.column_header.start_idx), &tbl.column_header.start_idx_length);

            if (get_field_names(&tbl) < 0)
                goto done;
            val_tuple = getbulk_table_sub_entries(&tbl, ss, max_repeaters);
        }
    }
  }

  done:
   return (val_tuple ? val_tuple : NULL);
}

static PyMethodDef ClientMethods[] = {
  {"table",  netsnmp_table, METH_VARARGS,
    "perform an SNMP table operation."},
  {NULL, NULL, 0, NULL}        /* Sentinel */
};

PyMODINIT_FUNC
initinterface(void)
{
    (void) Py_InitModule("interface", ClientMethods);
}
