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
#include "util.h"
#include "table.h"

#define SUCCESS (0)
#define FAILURE (-1)

int init_table(table_info_t* table_info, char* tablename) {
    memset(table_info->root, 0, sizeof(table_info->root));
    memset(table_info->column_header.name, 0, sizeof(table_info->column_header.name));
    memset(table_info->column_header.start_idx, 0, sizeof(table_info->column_header.start_idx_length));

    table_info->rootlen = MAX_OID_LEN;

    if (!snmp_parse_oid(tablename, table_info->root, &table_info->rootlen)) {
        PyErr_SetString(PyExc_RuntimeError, "snmp_parse_oid failed to lookup table in MIB.");
        //original was: snmp_perror(argv[optind]);
        return FAILURE;
    }

    table_info->table_name = NULL;
    table_info->py_rows_dict = NULL;

    table_info->column_header.column = NULL;
    table_info->column_header.fields = 0;
    table_info->column_header.name_length = 0;
    table_info->column_header.start_idx_length = 0;

    table_info->py_rows_dict = PyDict_New(); // create function, we've got ownership of ref-to-rows_dict

    return SUCCESS;
}

void reverse_fields(column_info_t* column_info) {
    column_t tmp;
    int i;

    for (i = 0; i < column_info->fields / 2; i++) {
        memcpy(&tmp, &(column_info->column[i]), sizeof(column_t));
        memcpy(&(column_info->column[i]), &(column_info->column[column_info->fields - 1 - i]), sizeof(column_t));
        memcpy(&(column_info->column[column_info->fields - 1 - i]), &tmp, sizeof(column_t));
    }
}

/*
 * Table Syntax for SMIv2 is specified RFC 2578, 7.1.12. Conceptual Tables (https://tools.ietf.org/html/rfc2578#page-25).
 */
int get_field_names(table_info_t* table_info) {
    column_info_t* column_info = &table_info->column_header;
    char *buf = NULL, *name_p = NULL;
    size_t buf_len = 0, out_len = 0;
#ifndef NETSNMP_DISABLE_MIB_LOADING
    struct tree *tbl = NULL;
#endif /* NETSNMP_DISABLE_MIB_LOADING */
    int going = 1;

#ifndef NETSNMP_DISABLE_MIB_LOADING
    tbl = get_tree(table_info->root, table_info->rootlen, get_tree_head());
    DBPRTOID(D_DBG, "Table object: ", table_info->root, table_info->rootlen);
    if (tbl) {
        tbl = tbl->child_list;
        if (tbl) {
            /* If a table has child list, the childs are the entry objects (aka conceptual rows).
             * Usually there is only one conceptual row. */
            DBPRT(D_DBG, ("table has child list: table_info->root[%lu] = %lu\n", table_info->rootlen, tbl->subid));
            DBPRTOID(D_DBG, "Entry object: ", table_info->root, table_info->rootlen);
            table_info->root[table_info->rootlen++] = tbl->subid;
            tbl = tbl->child_list;
        } else {
            /* table has no children, setting root[<lastsuboid>] to 1 */
            DBPRTOID(D_DBG, "Table without entry object, assuming .1: ", table_info->root, table_info->rootlen);
            table_info->root[table_info->rootlen++] = 1;
            going = 0;
        }
    }
#endif /* NETSNMP_DISABLE_MIB_LOADING */

    if (sprint_realloc_objid((u_char **) &buf, &buf_len, &out_len, 1, table_info->root,
            table_info->rootlen - 1)) {
        table_info->table_name = buf;
        buf = NULL;
        buf_len = out_len = 0;
    } else {
        table_info->table_name = strdup("[TRUNCATED]");
        out_len = 0;
    }
    DBPRT(D_DBG, ("Tablename = %s\n", table_info->table_name));

    /* now iterate trough column fields */
    column_info->fields = 0;
    while (going) {
        column_info->fields++;
#ifndef NETSNMP_DISABLE_MIB_LOADING
        if (tbl) {
            if (tbl->access == MIB_ACCESS_NOACCESS) {
                DBPRT(D_DBG, ("Column with MAX-ACCESS = not-accessible found, skip it (maybe index field)\n"));
                column_info->fields--;
                tbl = tbl->next_peer;
                if (!tbl) {
                    going = 0;
                }
                continue;
            }
            DBPRT(D_DBG, ("Column Found: table_info->root[%lu] = %lu\n", table_info->rootlen, tbl->subid));
            table_info->root[table_info->rootlen] = tbl->subid; // store the subid temporarily (gets overwritten in next while iteration)
            tbl = tbl->next_peer;
            if (!tbl)
                going = 0;
        } else {
#endif /* NETSNMP_DISABLE_MIB_LOADING */
            table_info->root[table_info->rootlen] = column_info->fields;
            /* no table found, root[rootlen] = fields */
#ifndef NETSNMP_DISABLE_MIB_LOADING
        }
#endif /* NETSNMP_DISABLE_MIB_LOADING */
        DBPRTOID(D_DBG, "Column OID is: ", table_info->root, table_info->rootlen + 1);
        out_len = 0;
        if (sprint_realloc_objid((u_char **) &buf, &buf_len, &out_len, 1, table_info->root,
                table_info->rootlen + 1)) {
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
        } else {
            break;
        }

        if ('0' <= name_p[0] && name_p[0] <= '9') {
            column_info->fields--;
            break;
        }
        DBPRT(D_DBG, ("Extracted column name is: %s\n", name_p));

        /* allocating and setting up the column header structs for each column*/
        if (column_info->fields == 1) {
            /* initial malloc on first columns */
            column_info->column = (column_t*) malloc(sizeof(column_t));
        } else {
            /* realloc on following columns */
            column_info->column = (column_t*) realloc(column_info->column,
                    column_info->fields * sizeof(column_t));
        }
        column_info->column[column_info->fields - 1].py_label_str = PyString_FromString(name_p);
        DBPRT(D_DBG, ("column[fields - 1].py_label_str = %s\n", name_p));
        column_info->column[column_info->fields - 1].subid = table_info->root[table_info->rootlen];
    }
    /* end while (going) */

    if (column_info->fields == 0) {
        DBPRT(D_DBG, ("No column OIDs found MIB for %s\n", table_info->table_name));
        PyErr_SetString(PyExc_RuntimeError, "No column OIDs found MIB");
        return FAILURE;
    }

    if (name_p) {
        /* At least one column was found in MIB. */
        *name_p = 0;

        /* copy over from the entry oid */
        memmove(column_info->name, table_info->root, table_info->rootlen * sizeof(oid));

        /* add 0 as innermost suboid */
        column_info->name_length = table_info->rootlen; // + 1;
        DBPRTOID(D_DBG, "Column root OID for getbulk: ", column_info->name, column_info->name_length);
    }

    if (buf != NULL) {
        free(buf);
    }

    return SUCCESS;
}

char is_valid_var(table_info_t* table_info, netsnmp_variable_list *vars, int nr_in_response)
{
    if (vars->type == SNMP_ENDOFMIBVIEW) {
        DBPRT(D_DBG, ("Returned varbinding is end of MIB.\n"));
        return 1;
    } else if (memcmp(vars->name, table_info->column_header.name, table_info->rootlen * sizeof(oid)) != 0) {
        DBPRT(D_DBG, ("Returned varbinding does not belong to our table.\n"));
        DBPRTOID(D_DBG, " varbind:     ", vars->name, table_info->rootlen);
        DBPRTOID(D_DBG, " header name: ", table_info->column_header.name, table_info->rootlen);
        return 1;
    } else if (vars->name[table_info->column_header.name_length] != table_info->column_header.column[nr_in_response].subid){
        DBPRT(D_DBG, ("Returned varbinding is not a instance of expected column (%lu vs %lu).\n",
                (long) vars->name[table_info->column_header.name_length],
                (long) table_info->column_header.column[nr_in_response].subid));
        return 1;
    } else if(memcmp(&vars->name[table_info->column_header.name_length+1], table_info->column_header.start_idx, table_info->column_header.start_idx_length * sizeof(oid)) != 0) {
        DBPRT(D_DBG, ("Returned varbinding does not have requested index.\n"));
//        oid* p = &vars->name[table_info->column_header.name_length+1];
//        DBPRTOID(D_DBG, " varbind:    ", p, table_info->column_header.start_idx_length);
//        DBPRTOID(D_DBG, " startindex: ", table_info->column_header.start_idx, table_info->column_header.start_idx_length);
        return 1;
    }
    return 0;
}

/*
 * The representation of the returned instance identifier depends
 * on netsnmp EXTENDED_INDEX and OID_OUTPUT_FORMAT setttings.
 */
char* find_instance_id(char* buf, char* table_name) {
    char* name_p;
    if (netsnmp_ds_get_boolean(NETSNMP_DS_LIBRARY_ID,
            NETSNMP_DS_LIB_EXTENDED_INDEX)) {
        name_p = strchr(buf, '['); // find first occurrence of '[' in buf
    } else {
        switch (netsnmp_ds_get_int(NETSNMP_DS_LIBRARY_ID,
                NETSNMP_DS_LIB_OID_OUTPUT_FORMAT)) {
        case NETSNMP_OID_OUTPUT_MODULE:
        case 0:
            name_p = strchr(buf, ':') + 1;
            break;
        case NETSNMP_OID_OUTPUT_SUFFIX:
            name_p = buf;
            break;
        case NETSNMP_OID_OUTPUT_FULL:
        case NETSNMP_OID_OUTPUT_NUMERIC:
        case NETSNMP_OID_OUTPUT_UCD:
            name_p = buf + strlen(table_name) + 1;
            name_p = strchr(name_p, '.') + 1;
            break;
        default:
            DBPRT(D_DBG, ("Invalid output format: %d\n",
                    netsnmp_ds_get_int(NETSNMP_DS_LIBRARY_ID,
                            NETSNMP_DS_LIB_OID_OUTPUT_FORMAT)));
            PyErr_SetString(PyExc_RuntimeError, "Invalid output format");
            return NULL;
        }

        name_p = strchr(name_p, '.');
        if (name_p != NULL) {
            name_p++; /* Move on to the instance identifier */
        }
    }
    return name_p;
}

/* find entry with suboid in table_info */
column_t* get_column_by_subid(column_info_t* columns, int subid)
{
    int col;
    for (col = 0; col < columns->fields; col++) {
        if (columns->column[col].subid == subid) {
            return &columns->column[col];
        }
    }
    return NULL;
}

int store_varbind(char* row_index_str, column_t* column, netsnmp_variable_list *result_varbind, table_info_t* table, PyObject* py_varbind) {
    PyObject* py_col_dict = NULL;
    char *buf = NULL;
    char *instance_id = NULL;
    const char delimiter [] = ".";
    size_t buf_len = 0;
    size_t out_len = 0;

    DBPRT(D_DBG, ("add row %s to result dictionary\n", row_index_str));

    sprint_realloc_value((u_char **) &buf, &buf_len, &out_len, 1, result_varbind->name,
            result_varbind->name_length, result_varbind);

    /* Get number of sub-instances. */
    int i, count;
    for (i=0, count = 1; i<strlen(row_index_str); i++)
        if (row_index_str[i] == '.') count++;

    /* Decompose instance string into parts. */
    //DBPRT(D_DBG, ("Create tuple with %i elements\n", count));

    PyObject* py_instance_tuple = PyTuple_New(count); // create function, we've got ownership of instance_tuple
    PyObject* py_subinstance = NULL;
    instance_id = strtok(row_index_str, delimiter);
    i = 0;
    while(instance_id) {
        if (instance_id[0] == '"') {
            //DBPRT(D_DBG, ("Found string-type sub instance %s\n", instance_id));
            instance_id++;
            py_subinstance = PyString_FromStringAndSize(instance_id, strlen(instance_id)-1); // create function, we've got ownership of ref-to-subinstance
        } else {
            //DBPRT(D_DBG, ("Assume int-type sub instance %s\n", instance_id));
            py_subinstance = PyInt_FromString(instance_id, NULL, 10); // create function, we've got ownership of ref-to-subinstance
        }
        PyTuple_SetItem(py_instance_tuple, i++, py_subinstance); // PyTuple_SetItem steals ownership, now ref-to-subinstance is only borrowed
        instance_id = strtok(NULL, delimiter);
    }

    // do we start a new row?
    py_col_dict = PyDict_GetItem(table->py_rows_dict, py_instance_tuple); // PyDict_GetItem() only borrows us ref-to-col_dict
    if (!py_col_dict)
    {
        DBPRT(D_DBG, ("Creating new column dict\n"));
        py_col_dict = PyDict_New(); // create function, we've got ownership of ref-to-col_dict
        PyDict_SetItem(table->py_rows_dict, py_instance_tuple, py_col_dict); // PyDict_SetItem() handles INCREF on its own
        Py_XDECREF(py_col_dict); // now ref-to-coldict is borrowed from rows_dict
    }

    DBPRT(D_DBG, (" [%s] = %s\n", PyString_AsString(column->py_label_str), buf));
    PyDict_SetItem(py_col_dict, column->py_label_str, py_varbind); // PyDict_SetItem() handles INCREF on its own, we can DECREF now

    return out_len;
}

int response_err(netsnmp_pdu *response)
{
    int count;
    int exitval = 0;
    netsnmp_variable_list *vars;

    if (response->errstat == SNMP_ERR_NOSUCHNAME) {
        DBPRT(D_DBG, ("End of MIB\n"));
    } else {
        fprintf(stderr, "Error in packet.\nReason: %s\n",
                snmp_errstring(response->errstat));
        if (response->errstat == SNMP_ERR_NOSUCHNAME) {
            fprintf(stderr,
                    "The request for this object identifier failed: ");
            for (count = 1, vars = response->variables;
                    vars && count != response->errindex;
                    vars = vars->next_variable, count++)
                /*EMPTY*/;
            if (vars) {
                fprint_objid(stderr, vars->name, vars->name_length);
            }
            fprintf(stderr, "\n");
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
PyObject* create_varbind(netsnmp_variable_list *vars, struct tree *tp, u_char* buf, size_t buf_len)
{
#define USE_BASIC 0
#define NO_FLAGS 0x00
#define NON_LEAF_NAME 0x04
#define MAX_TYPE_NAME_LEN 32
#define STR_BUF_SIZE (MAX_TYPE_NAME_LEN * MAX_OID_LEN)

    int type;
    int getlabel_flag = NO_FLAGS;
    char type_str[MAX_TYPE_NAME_LEN];
    u_char str_buf[STR_BUF_SIZE];
    int len;
    int sprintval_flag = USE_BASIC;
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

    len = __snprint_value((char *) str_buf, sizeof(str_buf) - 1,
                                  vars, tp, type, sprintval_flag);
    str_buf[len] = '\0';
    DBPRT(D_DBG, ("Translated value %s\n", str_buf));

    py_netsnmp_attr_set_string(varbind, "type", type_str, strlen(type_str));
    py_netsnmp_attr_set_string(varbind, "val", (char *) str_buf, len);

    return varbind;
}

PyObject* getbulk_table_sub_entries(table_info_t* table_info, netsnmp_session* ss, int max_repeaters) {
#define STR_BUF_SIZE (MAX_TYPE_NAME_LEN * MAX_OID_LEN)
    column_info_t* column_info = &table_info->column_header;
    int running = 1;
    netsnmp_pdu *pdu, *response;
    netsnmp_variable_list *vars;
    struct tree *tp;
    int buf_over = 0;
    int status;
    int col;
    u_char *buf = NULL;
    size_t buf_len = 0;
    //char *buf = NULL;
    size_t out_len;
    char *name_p = NULL;
    int exitval = SUCCESS;
    column_t* column;
    int tmp_dont_breakdown_oids;
    PyObject* py_varbind;


    /* Get table string index as string, not as dotted numbers. 1 = dont print oid indexes specially, 0 = print oid indexes specially. */
    tmp_dont_breakdown_oids = netsnmp_ds_get_boolean(NETSNMP_DS_LIBRARY_ID, NETSNMP_DS_LIB_DONT_BREAKDOWN_OIDS);
    netsnmp_ds_set_boolean(NETSNMP_DS_LIBRARY_ID, NETSNMP_DS_LIB_DONT_BREAKDOWN_OIDS, 0);

    /* initial setup for 1st getbulk request */
    for (col = 0; col < column_info->fields; col++) {
        column = &column_info->column[col];

        column->last_oid_len = column_info->name_length;
        memcpy(column->last_oid, column_info->name, column_info->name_length * sizeof(oid));
        column->last_oid[column->last_oid_len++] = column->subid;

        /* append the start index, if any */
        DBPRTOID(D_DBG, "appending start index: ", table_info->column_header.start_idx, table_info->column_header.start_idx_length);
        memcpy(&column->last_oid[column->last_oid_len],
                table_info->column_header.start_idx,
                table_info->column_header.start_idx_length * sizeof(oid));
        column->last_oid_len += table_info->column_header.start_idx_length;

        /* a final zero... */
        column->last_oid[column->last_oid_len++] = 0;

        /* initially set to 1, to get it added to the first pdu */
        DBPRTOID(D_DBG, "column last varbind OID", column->last_oid, column->last_oid_len);
    }

    DBPRT(D_DBG, ("max_repeaters = %i\n", max_repeaters));
    while (running) {
        /*
         * create PDU for GETBULK request and add object name to request
         */
        pdu = snmp_pdu_create(SNMP_MSG_GETBULK);
        pdu->non_repeaters = 0;
        pdu->max_repetitions = max_repeaters;

        for (col = 0; col < column_info->fields; col++) {
            column = &column_info->column[col];

            /* column_varbinds is updated during each resonse parsing */
            if (!column->end) {
                DBPRTOID(D_DBG, "add oid to getbulk request pdu", column->last_oid, column->last_oid_len);
                snmp_add_null_var(pdu, column->last_oid, column->last_oid_len);
            }
            column->last_var = NULL;
            column->end = 0;
        }

        /*
         * do the request
         */
        DBPRT(D_DBG, ("do getbulk request\n"));
        DBPRT(D_DBG, ("session - version %lu, community %s\n", ss->version, ss->community));
        status = snmp_synch_response(ss, pdu, &response);

        /* Allocates response->vars list. snmp_free_pdu destroys it at the end of the loop. */
        if (status == STAT_SUCCESS) {
            DBPRT(D_DBG, ("got success response\n"));
            if (response->errstat == SNMP_ERR_NOERROR) {
                /*
                 * check resulting variables
                 */
                vars = response->variables;
                int nr_in_response = 0;
                int nr_columns_ended = 0;
                DBPRT(D_DBG, ("parse response\n"));
                while (vars) {
                    /* Get the string representation from returned identifier.
                     * Depending on session settings, this may be dotted notation,
                     * or as MIB name.
                     */
                    out_len = 0;
                    DBPRTOID(D_DBG, "Response OID: ", vars->name, vars->name_length);
                    tp = netsnmp_sprint_realloc_objid_tree(&buf, &buf_len, &out_len,
                                           1, &buf_over, vars->name, vars->name_length);

                    DBPRT(D_DBG, ("print-name of varbind = %s, len = %lu, tp = %p\n", buf, buf_len, tp));

                    int vb_col = nr_in_response % column_info->fields;
                    if (is_valid_var(table_info, vars, vb_col) == 1) {
                        /* skip this variable */
                        if (!column_info->column[vb_col].end) {
                            DBPRT(D_DBG, ("Detected end of column %i\n", vb_col));
                            column_info->column[vb_col].end = 1;
                            nr_columns_ended++;
                            if (nr_columns_ended == column_info->fields) {
                                DBPRT(D_DBG, ("Detected end of all columns\n"));
                                running = 0;
                                break;
                            }
                        }
                        vars = vars->next_variable;
                        nr_in_response++;
                        continue;
                    }

                    column = get_column_by_subid(column_info, vars->name[table_info->rootlen]);
                    if (column) {
                        DBPRT(D_DBG, ("Update latest varbind pointer\n"));
                        column->last_var = vars;
                    } else {
                        DBPRT(D_DBG, ("Unexpected column in response\n"));
                        running = 0;
                        exitval = FAILURE;
                        break;
                    }

                    DBPRT(D_DBG, ("find row to which this varbind belongs\n"));
                    if ((name_p = find_instance_id((char*)buf, table_info->table_name)) == NULL) {
                        /* don't seem to include instance subidentifiers! */
                        DBPRT(D_DBG, ("don't seem to include instance subidentifiers\n"));
                        running = 0;
                        exitval = FAILURE;
                        break;
                    }

                    DBPRT(D_DBG, ("name_p now points to %s in %s\n", name_p, buf));

                    /* name_p now points to instance id part of OID */
                    py_varbind = create_varbind(vars, tp, buf, buf_len);
                    store_varbind(name_p, column, vars, table_info, py_varbind);
                    Py_DECREF(py_varbind);

                    buf = NULL;
                    buf_len = 0;
                    vars = vars->next_variable;

                    nr_in_response++;
                }
                /* end of request parsing while loop... */

                for (col = 0; col < column_info->fields; col++) {
                    column = &column_info->column[col];

                    /* response vars list will be freed soon, save initial oids for next request */
                    if (!column->end && column->last_var) {
                        column->last_oid_len = column->last_var->name_length;
                        memcpy(column->last_oid, column->last_var->name, column->last_var->name_length * sizeof(oid));
                    }
                }
            } else {
                /*
                 * error in response, print it
                 */
                running = 0;
                exitval = response_err(response);
            }
        } else if (status == STAT_TIMEOUT) {
            DBPRT(D_DBG, ("Timeout: No Response from %s\n", ss->peername));
            PyErr_SetString(PyExc_RuntimeError, "snmp_synch_response timed out, no response from agent");
            running = 0;
            exitval = FAILURE;
        } else { /* status == STAT_ERROR */
            DBPRT(D_DBG, ("got error response\n"));
            PyErr_SetString(PyExc_RuntimeError, "snmp_synch_response returned error");
            //snmp_sess_perror("snmptable", ss);
            running = 0;
            exitval = FAILURE;
        }
        if (response)
            snmp_free_pdu(response);
    }
    /* end of send request loop */

    if (buf)
        free(buf);

    netsnmp_ds_set_boolean(NETSNMP_DS_LIBRARY_ID, NETSNMP_DS_LIB_DONT_BREAKDOWN_OIDS, tmp_dont_breakdown_oids);

    if (exitval == SUCCESS) {
        DBPRT(D_DBG, ("Returning successful\n"));
        return table_info->py_rows_dict;
    }

    DBPRT(D_DBG, ("Returning exceptional\n"));
    Py_DECREF(table_info->py_rows_dict);
    return NULL;
}
