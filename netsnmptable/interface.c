#include <Python.h>
#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include "interface.h"
#include "util.h"
#include "table.h"

PyObject* netsnmptable_parse_mib(PyObject *self, PyObject *args) {
    PyObject* py_table = NULL;
    PyObject* py_varbind = NULL;
    PyObject* py_indexes_list = NULL;
    PyObject* py_columns_list = NULL;
    table_info_t* tbl = NULL;
    char *tag = NULL;
    int ret_exceptional = 0;
    int col = 0;

    if (args) {
        if (!PyArg_ParseTuple(args, "OO", &py_table, &py_varbind)) {
            PyErr_SetString(PyExc_LookupError, "Invalid arguments");
            ret_exceptional = 1;
            goto done;
        }

        /* get the table root oid out of varbind, which is contained in args */
        if (py_table && py_varbind) {
            py_indexes_list = py_netsnmp_attr_obj(py_table, "indexes");
            py_columns_list = py_netsnmp_attr_obj(py_table, "columns");
            if (!py_indexes_list || !py_columns_list) {
                PyErr_SetString(PyExc_TypeError,
                        "Table object has no indexes or columns attributes.");
                ret_exceptional = 1;
                goto done;
            }

            if (py_netsnmp_attr_string(py_varbind, "tag", &tag, NULL) < 0) {
                PyErr_SetString(PyExc_TypeError,
                        "Varbind object has no tag attribute");
                ret_exceptional = 1;
                goto done;
            } else {
                tbl = table_allocate(tag);
                if (!tbl) {
                    ret_exceptional = 1;
                    goto done;
                }

                if (table_get_field_names(tbl) < 0) {
                    PyErr_SetString(PyExc_RuntimeError, "Error while parsing table structure from MIB.");
                    ret_exceptional = 1;
                    goto done;
                }

                for (col = 0; col < tbl->column_scheme.fields; col++) {
                    PyList_Append(py_columns_list,
                            tbl->column_scheme.column[col].py_label_str);
                }
                PyList_Reverse(py_columns_list);
            }
        }
    }

    done:
	Py_XDECREF(py_indexes_list);
	Py_XDECREF(py_columns_list);
	if (ret_exceptional) {
        table_deallocate(tbl);
        return NULL;
    }

    return PyLong_FromVoidPtr((void *) tbl);
}

PyObject* netsnmptable_cleanup(PyObject *self, PyObject *args) {
    table_info_t* tbl = NULL;
    if (args) {
        if (!PyArg_ParseTuple(args, "l", &tbl)) {
            goto done;
        }
    }

    table_deallocate(tbl);

    done: return Py_BuildValue("");
}

PyObject* netsnmptable_fetch(PyObject *self, PyObject *args) {
    PyObject* py_table = NULL;
    PyObject* py_session = NULL;
    PyObject* py_val_tuple = NULL;
    PyObject* py_iid = NULL;
    table_info_t* tbl = NULL;
    long ss_opaque = 0;
    long max_repeaters;
    int ret_exceptional = 0;

    if (args) {
        if (!PyArg_ParseTuple(args, "OO", &py_table, &py_iid)) {
            goto done;
        }

        py_session = py_netsnmp_attr_obj(py_table, "netsnmp_session");
        if (!py_session) {
            PyErr_SetString(PyExc_TypeError,
                    "Table object has no netsnmp_session attribute");
            ret_exceptional = 1;
            goto done;
        }

        /* get netsnmp session pointer from python Session instance */
        ss_opaque = py_netsnmp_attr_long(py_session, "sess_ptr");
        if (ss_opaque < 0) {
            PyErr_SetString(PyExc_TypeError,
                    "Session object has no sess_ptr attribute");
            ret_exceptional = 1;
            goto done;
        } else if ((void*)ss_opaque == NULL) {
            PyErr_SetString(PyExc_RuntimeError,
                    "Session pointer not initialized");
            ret_exceptional = 1;
            goto done;
        }

        /* get netsnmp session pointer from python Session instance */
        tbl = (table_info_t*) py_netsnmp_attr_long(py_table, "_tbl_ptr");
        if (tbl < 0) {
            PyErr_SetString(PyExc_TypeError,
                    "Table object has no _tbl_ptr attribute");
            ret_exceptional = 1;
            goto done;
        }

        tbl->getlabel_flag = NO_FLAGS;
        tbl->sprintval_flag = USE_BASIC;
        if (py_netsnmp_attr_long(py_session, "UseLongNames"))
            tbl->getlabel_flag |= USE_LONG_NAMES;
        if (py_netsnmp_attr_long(py_session, "UseNumeric"))
            tbl->getlabel_flag |= USE_NUMERIC_OIDS;
        if (py_netsnmp_attr_long(py_session, "UseLongNames"))
            tbl->getlabel_flag |= USE_LONG_NAMES;
        if (py_netsnmp_attr_long(py_session, "UseEnums"))
            tbl->sprintval_flag = USE_ENUMS;
        if (py_netsnmp_attr_long(py_session, "UseSprintValue"))
            tbl->sprintval_flag = USE_SPRINT_VALUE;

        max_repeaters = py_netsnmp_attr_long(py_table, "max_repeaters");
        if (max_repeaters < 0) {
            PyErr_SetString(PyExc_RuntimeError,
                    "Table object has no max_repeaters attribute");
            ret_exceptional = 1;
            goto done;
        }

        if (py_iid && py_iid != Py_None) {
            py_netsnmp_attr_get_oid(py_iid, tbl->column_scheme.start_idx,
                    sizeof(tbl->column_scheme.start_idx),
                    &tbl->column_scheme.start_idx_length);
        }

        py_val_tuple = table_getbulk_sub_entries(tbl, (void*)ss_opaque, max_repeaters, py_session);
    }

    done:
    Py_XDECREF(py_session);

    if (ret_exceptional)
        return NULL;

    return (py_val_tuple ? py_val_tuple : Py_BuildValue(""));
}

static PyMethodDef InterfaceMethods[] = { { "table_parse_mib",
        netsnmptable_parse_mib, METH_VARARGS, "Get table structure from MIB." },
        { "table_fetch", netsnmptable_fetch, METH_VARARGS,
                "Perform an SNMP table fetch." }, { "table_cleanup",
                netsnmptable_cleanup, METH_VARARGS,
                "Perform an SNMP table fetch." }, { NULL, NULL, 0, NULL } /* Sentinel */
};

PyMODINIT_FUNC initinterface(void) {
    (void) Py_InitModule("interface", InterfaceMethods);
}
