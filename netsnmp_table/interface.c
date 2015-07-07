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

PyObject * netsnmp_table(PyObject *self, PyObject *args)
{
  PyObject *session;
// PyObject *varlist;
  PyObject *varbind;
  PyObject *val_tuple = NULL;
//  int varlist_len = 0;
//  int varlist_ind;
  netsnmp_session *ss;
//  netsnmp_pdu *pdu, *response;
//  netsnmp_variable_list *vars;
//  struct tree *tp;
//  int len;
  oid *oid_arr;
//  int oid_arr_len = MAX_OID_LEN;
//  int type;
//  char type_str[MAX_TYPE_NAME_LEN];
//  int status;
//  u_char str_buf[STR_BUF_SIZE], *str_bufp = str_buf;
//  size_t str_buf_len = sizeof(str_buf);
//  size_t out_len = 0;
//  int buf_over = 0;
  char *tag;
  char *iid;
//  int getlabel_flag = NO_FLAGS;
//  int sprintval_flag = USE_BASIC;
//  int verbose = py_netsnmp_verbose();
//  int old_format;
//  int best_guess;
//  int retry_nosuch;
//  int err_ind;
//  int err_num;
//  char err_str[STR_BUF_SIZE];
//  char *tmpstr;
//  Py_ssize_t tmplen;

  table_info_t tbl;

  /* Why? What? Used in snmptable.c:main */
  netsnmp_ds_set_boolean(NETSNMP_DS_LIBRARY_ID, NETSNMP_DS_LIB_QUICK_PRINT, 1);

  /* Get table string index as string, not as dotted numbers. */
  netsnmp_ds_toggle_boolean(NETSNMP_DS_LIBRARY_ID, NETSNMP_DS_LIB_DONT_BREAKDOWN_OIDS);

  oid_arr = calloc(MAX_OID_LEN, sizeof(oid));

  if (oid_arr && args) {

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

    /* get the table root oid out of varbind, which is contained in args */
    if (varbind) {
        if (py_netsnmp_attr_string(varbind, "tag", &tag, NULL) < 0 ||
                py_netsnmp_attr_string(varbind, "iid", &iid, NULL) < 0)
        {
          //oid_arr_len = 0;
            goto done;
        } else {
          //tp = __tag2oid(tag, iid, oid_arr, &oid_arr_len, NULL, best_guess);
            if (init_table(&tbl, tag) < 0) {
                goto done;
            }
            if (get_field_names(&tbl) < 0)
                goto done;
            val_tuple = getbulk_table_sub_entries(&tbl, ss);
        }
    }
  }

  done:
   SAFE_FREE(oid_arr);
   Py_DECREF(session);
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
