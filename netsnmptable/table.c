/**********************************************************************
 Copyright 2015 Tobias Deiminger
 Copyright 1997 Niels Baggesen

 All Rights Reserved

 Permission to use, copy, modify, and distribute this software and its
 documentation for any purpose and without fee is hereby granted,
 provided that the above copyright notice appear in all copies.
 I DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
 ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
 I BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
 ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
 WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
 ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 SOFTWARE.
 ******************************************************************/

#include <Python.h>
#include <arpa/inet.h>
#include "util.h"
#include "table.h"

#define SUCCESS (0)
#define FAILURE (-1)
#define FAILURE_EXCEPTIONAL (-2)

#define NO_FLAGS 0x00
#define NON_LEAF_NAME 0x04

struct table_tree_pointer {
    struct tree *conceptual_table;
    struct tree *conceptual_row;
    struct tree *conceptual_column;
};

table_info_t* table_allocate(char* tablename) {
    table_info_t* table_info;

    table_info = calloc(1, sizeof(table_info_t));
    if (!table_info)
        return NULL;

    table_info->rootlen = MAX_OID_LEN;

    if (!snmp_parse_oid(tablename, table_info->root, &table_info->rootlen)) {
        PyErr_SetString(PyExc_RuntimeError,
                "snmp_parse_oid failed to lookup table in MIB.");
        //original was: snmp_perror(argv[optind]);
        return NULL;
    }

    table_info->table_name = NULL;
    table_info->column_scheme.column = NULL;
    table_info->column_scheme.fields = 0;
    table_info->column_scheme.name_length = 0;
    table_info->column_scheme.start_idx_length = 0;
    table_info->column_scheme.position_map = NULL;
    table_info->index_vars = NULL;
    table_info->index_vars_nrof = 0;

    return table_info;
}

void table_deallocate(table_info_t* table) {
    int col;

    if (table) {
    	if (table->table_name) {
    		free(table->table_name);
    	}
        if (table->column_scheme.column) {
            for (col = 0; col < table->column_scheme.fields; col++) {
                Py_DECREF(table->column_scheme.column[col].py_label_str);
            }
            free(table->column_scheme.column);
            table->column_scheme.column = NULL;
        }
        if (table->column_scheme.position_map) {
            free(table->column_scheme.position_map);
            table->column_scheme.position_map = NULL;
        }
        if (table->index_vars) {
            free(table->index_vars);
            table->index_vars = NULL;
        }
        free(table);
    }
}

void reverse_fields(column_scheme_t* column_scheme) {
    column_t tmp;
    int i;

    for (i = 0; i < column_scheme->fields / 2; i++) {
        memcpy(&tmp, &(column_scheme->column[i]), sizeof(column_t));
        memcpy(&(column_scheme->column[i]),
                &(column_scheme->column[column_scheme->fields - 1 - i]),
                sizeof(column_t));
        memcpy(&(column_scheme->column[column_scheme->fields - 1 - i]), &tmp,
                sizeof(column_t));
    }
}

/* Read the MIB to get conceptual table-, row- and column objects. */
static void get_table_nodes(struct table_tree_pointer* tbl_tree, oid* table_oid, size_t oid_len) {
    /* get the conceptual table object */
    tbl_tree->conceptual_table = NULL;
    tbl_tree->conceptual_row = NULL;
    tbl_tree->conceptual_column = NULL;
#ifndef NETSNMP_DISABLE_MIB_LOADING
    tbl_tree->conceptual_table = get_tree(table_oid, oid_len, get_tree_head());
    DBPRTOID(D_DBG, "Table object: ", table_oid, oid_len);
    if (tbl_tree->conceptual_table) {
        /* Children of a table object is a conceptual row object, also known as entry.
         * There is only one conceptual row object, as RFC4181 states:
         * 'The OID assigned to the conceptual row MUST be derived by appending a sub-identifier
         *  of "1" to the OID assigned to the conceptual table.'
         */
        DBPRT(D_DBG, ("Conceptual table object found, sub id = %lu\n", tbl_tree->conceptual_table->subid));
        tbl_tree->conceptual_row = tbl_tree->conceptual_table->child_list;
        if (tbl_tree->conceptual_row) {
            /* First children of conceptual row object is the first conceptual column object. */
            DBPRT(D_DBG, ("Conceptual row object found, sub id  = %lu\n", tbl_tree->conceptual_row->subid));
            tbl_tree->conceptual_column = tbl_tree->conceptual_row->child_list;
        } else {
            /* table has no conceptual rows */
            DBPRT(D_DBG, ("Table without entry object (will assume sub id .1)\n"));
        }
    }
#endif /* NETSNMP_DISABLE_MIB_LOADING */
}

/*
 * Get number and type of indexes from MIB.
 * With this information parse_oid_indexes() can extract index values from the response OIDs later.
 * Returns number of indexes on success, -1 otherwise.
 */
static int get_table_indexes(index_scheme_t **index_var, struct table_tree_pointer* tbl_tree) {
    struct index_list *index;
    size_t list_size = sizeof(index_scheme_t);
    int count=0, i=0;

    *index_var = NULL;

    for (index = tbl_tree->conceptual_row->indexes; index; index = index->next) {
        struct tree *indexnode = NULL;
        unsigned char type;
        int length = 0;

        oid name[MAX_OID_LEN];
        size_t name_length = MAX_OID_LEN;
        if (!snmp_parse_oid(index->ilabel, name, &name_length) ||
                (NULL == (indexnode = get_tree(name, name_length, get_tree_head())))) {
            DBPRT(D_DBG, ("get_table_indexes: Can't get index from MIB.\n"));
            return -1;
        }

        type = mib_to_asn_type(indexnode->type);
        if (type == (u_char) -1) {
            DBPRT(D_DBG, ("get_table_indexes: Bad index type.\n"));
            return -1;
        } else {
            /*
             * if implied, mark it as such. also mark fixed length
             * octet strings as implied (ie no length prefix) as well.
             * */
            if ((indexnode->type == TYPE_OCTETSTR) &&  /* octet str */
                (indexnode->ranges != NULL) &&         /* & has range */
                (indexnode->ranges->next == NULL) &&   /*   but only one */
                (indexnode->ranges->high == indexnode->ranges->low) /*   & high==low */ ) {
                type |= ASN_PRIVATE;
                length = indexnode->ranges->high;
            }
            else if (index->isimplied) {
                type |= ASN_PRIVATE;
            }

            if (*index_var == NULL) {
                *index_var = malloc(list_size);
            }
            else {
                list_size += sizeof(index_scheme_t);
                *index_var = realloc(*index_var, list_size);
            }
            index_scheme_t *index_scheme = *index_var;
            memset(&index_scheme[count].vars, 0x00, sizeof(netsnmp_variable_list));
            index_scheme[count].type = type;
            index_scheme[count].val_len = length;
            count++;
        }
    }

    for (i=1; i<count; i++) {
        index_scheme_t *index_scheme = *index_var;
        index_scheme[i-1].vars.next_variable = &index_scheme[i].vars;
    }

    return count;
}

/*
 * Convert table (array of OIDs) to table name.
 */
static char* get_table_name(oid* table_oid, size_t oid_len) {
    char *table_name = NULL, *buf = NULL;
    size_t out_len = 0, buf_len = 0;

    if (sprint_realloc_objid((u_char **) &buf, &buf_len, &out_len, 1,
            table_oid, oid_len)) {
        table_name = buf;
    } else {
        table_name = strdup("[TRUNCATED]");
    }
    return table_name;
}

/*
 * Convert column (array of OIDs) to column name.
 *
 * Return value: New reference.
 */
static PyObject* get_column_name(oid* column_oid, size_t oid_len) {
	PyObject* py_column_name = NULL;
    char *name_p = NULL, *buf = NULL;
    size_t out_len = 0, buf_len = 0;

    if (sprint_realloc_objid((u_char **) &buf, &buf_len, &out_len, 1,
            column_oid, oid_len)) {
        DBPRT(D_DBG, ("Full name of column OID: %s\n", buf));
        name_p = strrchr(buf, '.');
        if (name_p == NULL) {
            name_p = strrchr(buf, ':');
        }
        if (name_p == NULL) {
            name_p = buf;
        } else {
            name_p++;
        }

        if ('0' <= name_p[0] && name_p[0] <= '9') {
        	name_p = NULL;
        }
    }
    if (name_p != NULL) {
        py_column_name = PyString_FromString(name_p);
    }
    free(buf);
    return py_column_name;
}

/*
 * Get table structure information from MIB.
 */
int table_get_field_names(table_info_t* table_info) {
    struct table_tree_pointer tbl_tree;
    column_scheme_t* column_info = &table_info->column_scheme;
    PyObject* py_col_name = NULL;
    int going = 1;

    get_table_nodes(&tbl_tree, table_info->root, table_info->rootlen);
    if (tbl_tree.conceptual_row->child_list) {
        table_info->root[table_info->rootlen++] = tbl_tree.conceptual_row->subid;
    } else {
        table_info->root[table_info->rootlen++] = 1;
        going = 0;
    }

    table_info->table_name = get_table_name(table_info->root, table_info->rootlen - 1);
    table_info->index_vars_nrof = get_table_indexes(&table_info->index_vars, &tbl_tree);

    DBPRT(D_DBG, ("Tablename = %s\n", table_info->table_name));

    /* now iterate trough column fields */
    column_info->fields = 0;
    while (going) {
        column_info->fields++;
#ifndef NETSNMP_DISABLE_MIB_LOADING
        if (tbl_tree.conceptual_column) {
            if (tbl_tree.conceptual_column->access == MIB_ACCESS_NOACCESS) {
                DBPRT(D_DBG, ("Column with MAX-ACCESS = not-accessible found, skip it (maybe index field)\n"));
                column_info->fields--;
                tbl_tree.conceptual_column = tbl_tree.conceptual_column->next_peer;
                if (!tbl_tree.conceptual_column) {
                    going = 0;
                }
                continue;
            } DBPRT(D_DBG, ("Column Found: table_info->root[%lu] = %lu\n", table_info->rootlen, tbl_tree.conceptual_column->subid));
            table_info->root[table_info->rootlen] = tbl_tree.conceptual_column->subid; // store the subid temporarily (gets overwritten in next while iteration)
            tbl_tree.conceptual_column = tbl_tree.conceptual_column->next_peer;
            if (!tbl_tree.conceptual_column)
                going = 0;
        } else {
#endif /* NETSNMP_DISABLE_MIB_LOADING */
            table_info->root[table_info->rootlen] = column_info->fields;
            /* no table found, root[rootlen] = fields */
#ifndef NETSNMP_DISABLE_MIB_LOADING
        }
#endif /* NETSNMP_DISABLE_MIB_LOADING */
        DBPRTOID(D_DBG, "Column OID is: ", table_info->root, table_info->rootlen + 1);
        py_col_name = get_column_name(table_info->root, table_info->rootlen + 1);
        if (!py_col_name) {
            column_info->fields--;
            break;
        }
        DBPRT(D_DBG, ("Column name: %s\n", col_name));

        /* allocating and setting up the column header structs for each column*/
        if (column_info->fields == 1) {
            /* initial malloc on first columns */
            column_info->column = (column_t*) malloc(sizeof(column_t));
        } else {
            /* realloc on following columns */
            column_info->column = (column_t*) realloc(column_info->column,
                    column_info->fields * sizeof(column_t));
        }
        column_info->column[column_info->fields - 1].py_label_str = py_col_name;
        DBPRT(D_DBG, ("column[fields - 1].py_label_str = %s\n", col_name));
        column_info->column[column_info->fields - 1].subid =
                table_info->root[table_info->rootlen];
    }
    /* end while (going) */

    if (column_info->fields == 0) {
        DBPRT(D_DBG, ("No column OIDs found MIB for %s\n", table_info->table_name));
        return FAILURE;
    } else {
        /* copy over from the entry oid */
        memmove(column_info->name, table_info->root,
                table_info->rootlen * sizeof(oid));

        /* add 0 as innermost suboid */
        column_info->name_length = table_info->rootlen; // + 1;
        DBPRTOID(D_DBG, "Column root OID for getbulk: ", column_info->name, column_info->name_length);

        /* additional index to get column bei number in response */
        column_info->position_map = calloc(column_info->fields,
                sizeof(column_t*));
    }

    return SUCCESS;
}

static column_t* get_column_validated(table_info_t* table_info,
        netsnmp_variable_list *vars, int nr_in_response) {
    column_t* column = table_info->column_scheme.position_map[nr_in_response];

    if (vars->type == SNMP_ENDOFMIBVIEW) {
        DBPRT(D_DBG, ("Returned varbinding is end of MIB.\n"));
        return NULL;
    } else if (memcmp(vars->name, table_info->column_scheme.name,
            table_info->rootlen * sizeof(oid)) != 0) {
        DBPRT(D_DBG, ("Returned varbinding does not belong to our table.\n")); DBPRTOID(D_DBG, " varbind:     ", vars->name, table_info->rootlen); DBPRTOID(D_DBG, " header name: ", table_info->column_scheme.name, table_info->rootlen);
        return NULL;
    } else if (vars->name[table_info->column_scheme.name_length]
            != column->subid) {
        DBPRT(D_DBG, ("Returned varbinding is not a instance of expected column (%lu vs %lu).\n",
                        (long) vars->name[table_info->column_scheme.name_length],
                        (long) column->subid));
        return NULL;
    } else if (memcmp(&vars->name[table_info->column_scheme.name_length + 1],
            table_info->column_scheme.start_idx,
            table_info->column_scheme.start_idx_length * sizeof(oid)) != 0) {
        DBPRT(D_DBG, ("Returned varbinding does not have requested index.\n"));
        DBPRTOID(D_DBG, " Expected: ", table_info->column_scheme.start_idx, table_info->column_scheme.start_idx_length);
        DBPRTOID(D_DBG, " Actual  : ", (oid*)(vars->name + table_info->column_scheme.name_length + 1), table_info->column_scheme.start_idx_length);
        return NULL;
    }
    return column;
}

int get_nr_of_subidx(oid* start, int max_oid_len, index_scheme_t *index_varlist, int max_nr_of_index) {
    oid * oidIndex = start;
    size_t oidLen = max_oid_len;
    netsnmp_variable_list *var = &index_varlist[0].vars;
    int count = 0;

    while (var && oidLen > 0) {
        index_varlist[count].vars.type = index_varlist[count].type;
        index_varlist[count].vars.val_len = index_varlist[count].val_len;
        if (parse_one_oid_index(&oidIndex, &oidLen, var, 0) != SNMPERR_SUCCESS) {
            break;
        } else {
            var = var->next_variable;
            count++;
        }
    }

    return count;
}

/*
 * Parse index values from a response oid, and create a python tuple from it.
 * This tuple will be used as key in the table dictionary later.
 *
 * Return value: New reference.
 */
PyObject* create_index_tuple(oid* start, int max_oid_len, index_scheme_t *index_varlist, int nr_of_index) {
    int count = 0;
    netsnmp_variable_list *index_var = NULL;
    PyObject* py_instance_tuple = PyTuple_New(nr_of_index);

    /* restore index information, because parse_oid_indexes overwrites some values in buffer */
    for (count=0; count<nr_of_index; count++) {
        index_varlist[count].vars.type = index_varlist[count].type;
        index_varlist[count].vars.val_len = index_varlist[count].val_len;
    }

    if (parse_oid_indexes(start, max_oid_len, &index_varlist[0].vars)) {
        DBPRT(D_DBG, ("parse_oid_indexes failed.\n"));
        return NULL;
    }

    for (count = 0; count < nr_of_index; count++) {
        index_var = &index_varlist[count].vars;
        PyObject* py_subinstance = NULL;
        char buf[MAX_OID_LEN*21];
        char *cur = buf, *const end = buf + sizeof(buf);
        int nrof = 0;
        int i;

        switch (index_var->type) {
        case ASN_INTEGER:
        case ASN_UNSIGNED:
        case ASN_TIMETICKS:
        case ASN_COUNTER:
        case ASN_UINTEGER:
            /* index value will be an integer on python side */
            py_subinstance = PyInt_FromLong(*index_var->val.integer);
            break;

        case ASN_OBJECT_ID:
        case ASN_PRIV_IMPLIED_OBJECT_ID:
        case ASN_PRIV_INCL_RANGE:
        case ASN_PRIV_EXCL_RANGE:
            /* index value will be a string on python side, containing dotted numbers like "1.2.3.4" */
            nrof = index_var->val_len / sizeof(oid);
            for (i=0; i < nrof; i++) {
                cur += snprintf(cur, end-cur, "%lu", index_var->val.objid[i]);
                if (i < (nrof-1)) {
                    *cur = '.';
                    cur++;
                }
            }
            *cur = '\0';
            py_subinstance = PyString_FromStringAndSize((const char *) buf, cur-buf);
            break;

        case ASN_IPADDRESS:
            /* snmp_build_var_op makes a string for ASN_IPADDRESS, e.g. containing '\x01\x00\xa8\xc0' for 192.168.0.1.
               Turn it into an IP address string in dotted notation. */
            py_subinstance = PyString_FromString(inet_ntoa(*(struct in_addr*) index_var->val.string));
            break;

        case ASN_OPAQUE:
        case ASN_OCTET_STR:
        case ASN_PRIV_IMPLIED_OCTET_STR:
            /* index value will be a string on python side */
            py_subinstance = PyString_FromStringAndSize((const char *) index_var->val.string, index_var->val_len);
            free(index_var->val.string);
            break;

        default:
            break;
        }
        PyTuple_SetItem(py_instance_tuple, count, py_subinstance);
    }
    return py_instance_tuple;
}

/*
 * Create a python tuple from index_vars, use it as key in py_table_dict.
 * Insert py_varbind into a python dictionary.
 *
 * py_index_tuple and py_varbind are borrowed refrences.
 */
static int store_varbind(PyObject* py_table_dict, column_t* column, PyObject* py_index_tuple, PyObject* py_varbind) {
    PyObject* py_col_dict = NULL;
    size_t out_len = 0;

    // do we start a new row?
    py_col_dict = PyDict_GetItem(py_table_dict, py_index_tuple);    // PyDict_GetItem() only borrows us ref-to-col_dict
    if (!py_col_dict) {
        DBPRT(D_DBG, ("Creating new column dict\n"));
        py_col_dict = PyDict_New();                                 // PyDict_New INCREF's py_col_dict
        PyDict_SetItem(py_table_dict, py_index_tuple, py_col_dict); // PyDict_SetItem INCREF's py_index_tuple and py_col_dict
        Py_XDECREF(py_col_dict);
    }

    PyDict_SetItem(py_col_dict, column->py_label_str, py_varbind);  // PyDict_SetItem INCREF's column->py_label_str and py_varbind

    return out_len;
}

int response_err(netsnmp_pdu *response) {
    int count;
    int exitval = 0;
    netsnmp_variable_list *vars;

    if (response->errstat == SNMP_ERR_NOSUCHNAME) {
        DBPRT(D_DBG, ("End of MIB\n"));
    } else {
        DBPRT(D_DBG, ("Error in packet.\nReason: %s\n", snmp_errstring(response->errstat)));
        if (response->errstat == SNMP_ERR_NOSUCHNAME) {
            DBPRT(D_DBG, ("The request for this object identifier failed: "));
            for (count = 1, vars = response->variables;
                    vars && count != response->errindex;
                    vars = vars->next_variable, count++)
                /*EMPTY*/;
            if (vars) {
                DBPRTOID(D_DBG, " ", vars->name, vars->name_length);
            }
            DBPRT(D_DBG, ("\n"));
        }
        exitval = 2;
    }
    return exitval;
}

/*
 * vars - variable binding from response
 * tp - pointer to object node in MIB
 * buf - parsed output from a netsnmp print_objid function
 * buf_len - size of buf
 */
PyObject* create_varbind(netsnmp_variable_list *vars, struct tree *tp, int sprintval_flag) {
    int type;
    int getlabel_flag = NO_FLAGS;
    char type_str[MAX_TYPE_NAME_LEN];
    u_char str_buf[STR_BUF_SIZE];
    int len;
    PyObject *varbind = py_netsnmp_construct_varbind();

//    if (py_netsnmp_attr_long(session, "UseEnums"))
//      sprintval_flag = USE_ENUMS;
//    if (py_netsnmp_attr_long(session, "UseSprintValue"))
//      sprintval_flag = USE_SPRINT_VALUE;

    if (__is_leaf(tp)) {
        type = (tp->type ? tp->type : tp->parent->type);
        getlabel_flag &= ~NON_LEAF_NAME;
        DBPRT(D_DBG, ("netsnmp_get:is_leaf:%d\n",type));
    } else {
        getlabel_flag |= NON_LEAF_NAME;
        type = __translate_asn_type(vars->type);
        DBPRT(D_DBG, ("netsnmp_get:!is_leaf:%d\n",tp->type));
    }

    // __get_label_iid((char *) str_buf, &tag, &iid, getlabel_flag);
    __get_type_str(type, type_str);
    DBPRT(D_DBG, ("Detected type id %i, type name = %s\n", type, type_str));

    len = __snprint_value((char *) str_buf, sizeof(str_buf) - 1, vars, tp, type,
            sprintval_flag);
    str_buf[len] = '\0';
    DBPRT(D_DBG, ("Translated value %s\n", str_buf));

    py_netsnmp_attr_set_string(varbind, "type", type_str, strlen(type_str));
    py_netsnmp_attr_set_string(varbind, "val", (char *) str_buf, len);

    return varbind;
}

PyObject* table_getbulk_sub_entries(table_info_t* table_info,
        void* ss_opaque, int max_repeaters, PyObject *session) {
    column_scheme_t* column_scheme = &table_info->column_scheme;
    int running = 1;
    netsnmp_pdu *pdu, *response;
    netsnmp_variable_list *vars;
    struct tree *tp;
    int buf_over = 0;
    int status;
    int col;
    u_char *buf = NULL;
    size_t buf_len = 0;
    size_t out_len;
    int exitval = SUCCESS;
    column_t* column;
    int tmp_dont_breakdown_oids;
    PyObject* py_table_dict = NULL;
    PyObject* py_index_tuple = NULL;
    PyObject* py_varbind = NULL;
    int nr_of_requested_columns = 0;
    int nr_of_subindex = 0;
    int response_vb_count = 0;
    int nr_columns_ended = 0;
    int retry_nosuch = 0;
    char err_str[STR_BUF_SIZE];
    int err_num;
    int err_ind;

    /* Create function, transfers reference ownership. */
    py_table_dict = PyDict_New();

    /* Get table string index as string, not as dotted numbers. 1 = dont print oid indexes specially, 0 = print oid indexes specially. */
    tmp_dont_breakdown_oids = netsnmp_ds_get_boolean(NETSNMP_DS_LIBRARY_ID,
            NETSNMP_DS_LIB_DONT_BREAKDOWN_OIDS);
    netsnmp_ds_set_boolean(NETSNMP_DS_LIBRARY_ID,
            NETSNMP_DS_LIB_DONT_BREAKDOWN_OIDS, 0);

    nr_of_subindex = get_nr_of_subidx(table_info->column_scheme.start_idx, table_info->column_scheme.start_idx_length,
                                       table_info->index_vars, table_info->index_vars_nrof);

    /* initial setup for 1st getbulk request */
    for (col = 0; col < column_scheme->fields; col++) {
        column = &column_scheme->column[col];
        column->last_oid_len = column_scheme->name_length;
        column->end = 0;
        memcpy(column->last_oid, column_scheme->name,
                column_scheme->name_length * sizeof(oid));
        column->last_oid[column->last_oid_len++] = column->subid;

        /* append the start index, if any */
        if (table_info->column_scheme.start_idx_length > 0) {
            DBPRTOID(D_DBG, "appending start index: ", table_info->column_scheme.start_idx, table_info->column_scheme.start_idx_length);
            memcpy(&column->last_oid[column->last_oid_len],
                    table_info->column_scheme.start_idx,
                    table_info->column_scheme.start_idx_length * sizeof(oid));
            column->last_oid_len += table_info->column_scheme.start_idx_length;

            /* Getbulk would retrieve the OID next to start_idx, but we want start_idx itself to be in the result too. */
            if (nr_of_subindex == table_info->index_vars_nrof && column->last_oid[column->last_oid_len-1] >= 0) {
                /* Indexes are fully specified and value. Last sub-identifier is greater than zero. Decrement it by one. */
                column->last_oid[column->last_oid_len-1]--;
            } else if (column->last_oid[column->last_oid_len-1] == 0) {
                /* Last sub-identifier has a value of zero. Sub-identifier is removed from the index. */
                column->last_oid_len--;
            }
        }

        DBPRTOID(D_DBG, "column last varbind OID", column->last_oid, column->last_oid_len);
    }

    DBPRT(D_DBG, ("max_repeaters = %i\n", max_repeaters));
    while (running) {
        nr_of_requested_columns = 0;

        /*
         * create PDU for GETBULK request and add object name to request
         */
        pdu = snmp_pdu_create(SNMP_MSG_GETBULK);
        pdu->non_repeaters = 0;
        pdu->max_repetitions = max_repeaters;

        for (col = 0; col < column_scheme->fields; col++) {
            column = &column_scheme->column[col];

            /* column_varbinds is updated during each resonse parsing */
            if (!column->end) {
                DBPRTOID(D_DBG, "add oid to getbulk request pdu", column->last_oid, column->last_oid_len);
                snmp_add_null_var(pdu, column->last_oid, column->last_oid_len);
                table_info->column_scheme.position_map[nr_of_requested_columns++] =
                        column;
            }
            column->last_var = NULL;
            column->end = 0;
        }

        /*
         * do the request
         */
        DBPRT(D_DBG, ("do getbulk request\n"));

#ifdef NETSNMP_SINGLE_API
        DBPRT(D_DBG, ("session (single API) - version code %lu, community %s\n", snmp_sess_session((void*)ss_opaque)->version, snmp_sess_session((void*)ss_opaque)->community));
#else
        DBPRT(D_DBG, ("session (traditional API) - version code %lu, community %s\n", ((netsnmp_session*)ss_opaque)->version, ((netsnmp_session*)ss_opaque)->community));
#endif

        retry_nosuch = 0; // = py_netsnmp_attr_long(session, "RetryNoSuch");
        status = __send_sync_pdu(ss_opaque, pdu, &response, retry_nosuch, err_str,
                &err_num, &err_ind);
        __py_netsnmp_update_session_errors(session, err_str, err_num, err_ind);

        /* Allocates response->vars list. snmp_free_pdu destroys it at the end of the loop. */
        if (status == STAT_SUCCESS) {
            DBPRT(D_DBG, ("got success response\n"));
            if (response->errstat == SNMP_ERR_NOERROR) {
                /*
                 * check resulting variables
                 */
                vars = response->variables;
                response_vb_count = 0;
                nr_columns_ended = 0;
                DBPRT(D_DBG, ("parse response\n"));
                while (vars && exitval != FAILURE) {
                    /* Get the string representation from returned identifier.
                     * Depending on session settings, this may be dotted notation,
                     * or as MIB name.
                     */
                    out_len = 0;
                    DBPRTOID(D_DBG, "Response OID: ", vars->name, vars->name_length);
                    tp = netsnmp_sprint_realloc_objid_tree(&buf, &buf_len,
                            &out_len, 1, &buf_over, vars->name,
                            vars->name_length);

                    DBPRT(D_DBG, ("print-name of varbind = %s, len = %lu, tp = %p\n", buf, buf_len, tp));

                    int response_slot = response_vb_count
                            % nr_of_requested_columns;
                    column = get_column_validated(table_info, vars,
                            response_slot);

                    if (!column) {
                        /* skip this variable */
                        if (!column_scheme->column[response_slot].end) {
                            DBPRT(D_DBG, ("Detected end of column %i\n", response_slot));
                            column_scheme->column[response_slot].end = 1;
                            nr_columns_ended++;
                            if (nr_columns_ended == nr_of_requested_columns) {
                                DBPRT(D_DBG, ("Detected end of all columns\n"));
                                running = 0;
                                break;
                            }
                        }
                        vars = vars->next_variable;
                        response_vb_count++;
                        continue;
                    }

                    DBPRT(D_DBG, ("Update latest varbind pointer\n"));
                    column->last_var = vars;

                    /* name_p now points to instance id part of OID */
                    py_index_tuple = create_index_tuple(&vars->name[table_info->rootlen+1], vars->name_length-table_info->rootlen-1, table_info->index_vars, table_info->index_vars_nrof);
                    py_varbind = create_varbind(vars, tp, table_info->sprintval_flag);
                    if (py_index_tuple && py_varbind) {
                    	store_varbind(py_table_dict, column, py_index_tuple, py_varbind);
                    } else {
                        running = 0;
                        exitval = FAILURE;
                    }
                    Py_XDECREF(py_index_tuple);
                    Py_XDECREF(py_varbind);

                    vars = vars->next_variable;
                    response_vb_count++;
                }

                /* All varbinds in response have been processed. Check which varbinds have to be added to the next getbulk request. */
                for (col = 0; col < column_scheme->fields; col++) {
                    column = &column_scheme->column[col];
                    /* response vars list will be freed soon, save initial oids for next request */
                    if (!column->end && column->last_var) {
                        column->last_oid_len = column->last_var->name_length;
                        memcpy(column->last_oid, column->last_var->name,
                                column->last_var->name_length * sizeof(oid));
                    }
                }
            } else {
                if (response->errstat == SNMP_ERR_NOSUCHNAME) {
                    /* If we receive SNMP_ERR_NOSUCHNAME error status for a getbulk request,
                       we probably have a buggy agent that tells us "end of mib" the old SNMPv1 way.
                       Because end of MIB is OK, clear errors. */
                    err_str[0] = '\0';
                    err_num = 0;
                    err_ind = 0;
                    __py_netsnmp_update_session_errors(session, err_str, err_num, err_ind);
                } else {
                    /* Error in response, prepare exception. */
                    exitval = response_err(response);
                }
                running = 0;
            }
        } else if (status == STAT_TIMEOUT) {
            DBPRT(D_DBG, ("Timeout: No Response from peer.\n"));
            running = 0;
            exitval = FAILURE;
        } else {
            DBPRT(D_DBG, ("got error response\n"));
            running = 0;
            exitval = FAILURE;
        }
        if (response) {
            snmp_free_pdu(response);
            response = NULL;
        }
    }

    if (buf) {
        free(buf);
    }

    netsnmp_ds_set_boolean(NETSNMP_DS_LIBRARY_ID,
            NETSNMP_DS_LIB_DONT_BREAKDOWN_OIDS, tmp_dont_breakdown_oids);

    if (exitval == SUCCESS || exitval == FAILURE) {
        DBPRT(D_DBG, ("Returning normal.\n"));
        return py_table_dict;
    }

    DBPRT(D_DBG, ("Returning exceptional.\n"));
    Py_DECREF(py_table_dict);
    return NULL;
}
