#include <Python.h>
#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include "interface.h"
#include "table.h"

PyObject*
py_netsnmp_attr_obj(PyObject *obj, char * attr_name)
{
  if (obj && attr_name  && PyObject_HasAttrString(obj, attr_name)) {
    PyObject *attr = PyObject_GetAttrString(obj, attr_name);
    if (attr) {
      return attr;
    }
  }

  return NULL;
}

long long
py_netsnmp_attr_long(PyObject *obj, char * attr_name)
{
  long long val = -1;

  if (obj && attr_name  && PyObject_HasAttrString(obj, attr_name)) {
    PyObject *attr = PyObject_GetAttrString(obj, attr_name);
    if (attr) {
      val = PyInt_AsLong(attr);
      Py_DECREF(attr);
    }
  }

  return val;
}

int
py_netsnmp_attr_string(PyObject *obj, char * attr_name, char **val,
    Py_ssize_t *len)
{
  *val = NULL;
  if (obj && attr_name && PyObject_HasAttrString(obj, attr_name)) {
    PyObject *attr = PyObject_GetAttrString(obj, attr_name);
    if (attr) {
      int retval;
      retval = PyString_AsStringAndSize(attr, val, len);
      Py_DECREF(attr);
      return retval;
    }
  }

  return -1;
}

int
py_netsnmp_attr_set_string(PyObject *obj, char *attr_name,
               char *val, size_t len)
{
  int ret = -1;
  if (obj && attr_name) {
    PyObject* val_obj =  (val ?
              Py_BuildValue("s#", val, len) :
              Py_BuildValue(""));
    ret = PyObject_SetAttrString(obj, attr_name, val_obj);
    Py_DECREF(val_obj);
  }
  return ret;
}

int py_netsnmp_attr_oid(PyObject* self, char *attr_name, oid* p_oid, size_t maxlen, size_t* len)
{
    PyObject* obj;
    PyObject* seq;
    int i, seqlen;

    /* get the fixed index tuple from python side */
    obj = py_netsnmp_attr_obj(self, attr_name);
    if (!obj) {
        return -1;
    }

    seq = PySequence_Fast(obj, "expected a sequence");
    seqlen = PySequence_Size(obj);
    for (i = 0; i < seqlen && i < maxlen; i++) {
        PyObject* item = PySequence_Fast_GET_ITEM(seq, i); // returns borrowed reference.
        p_oid[i] = PyInt_AS_LONG(item);
    }
    *len = i;
    Py_DECREF(seq); // we don't reference sequence anymore

    return SUCCESS;
}

PyObject * netsnmp_table(PyObject *self, PyObject *args)
{
  PyObject *session;
  PyObject *varbind;
  PyObject *val_tuple = NULL;
  netsnmp_session *ss;
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
    ss = (SnmpSession *)py_netsnmp_attr_long(session, "sess_ptr");
    if (!ss) {
        PyErr_SetString(PyExc_RuntimeError, "invalid netsnmp session pointer");
        goto done;
    }

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
