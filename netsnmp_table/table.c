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
#include "table.h"

#ifdef DEBUG
#define DEBUG_OUT 1
#else
#define DEBUG_OUT 0
#endif

#define DBGPRINT(...) \
    do { if (DEBUG_OUT) fprintf(stderr, __VA_ARGS__); } while (0)
#define DBGPRINTOID(oidname, oid, len) \
    do { int c; if (DEBUG_OUT) { fprintf(stderr, "%s (length %lu) ", oidname, len); \
    for (c=0; c<len; c++) fprintf(stderr, ".%lu", oid[c]); fprintf(stderr, "\n"); } } while (0)

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
    DBGPRINTOID("Table object: ", table_info->root, table_info->rootlen);
    if (tbl) {
        tbl = tbl->child_list;
        if (tbl) {
            /* If a table has child list, the childs are the entry objects (aka conceptual rows).
             * Usually there is only one conceptual row. */
            DBGPRINT("table has child list: table_info->root[%lu] = %lu\n", table_info->rootlen, tbl->subid);
            DBGPRINTOID("Entry object: ", table_info->root, table_info->rootlen);
            table_info->root[table_info->rootlen++] = tbl->subid;
            tbl = tbl->child_list;
        } else {
            /* table has no children, setting root[<lastsuboid>] to 1 */
            DBGPRINTOID("Table without entry object, assuming .1: ", table_info->root, table_info->rootlen);
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
    DBGPRINT("Tablename = %s\n", table_info->table_name);

    /* now iterate trough column fields */
    column_info->fields = 0;
    while (going) {
        column_info->fields++;
#ifndef NETSNMP_DISABLE_MIB_LOADING
        if (tbl) {
            if (tbl->access == MIB_ACCESS_NOACCESS) {
                DBGPRINT("Column with MAX-ACCESS = not-accessible found, skip it (maybe index field)\n", table_info->table_name);
                column_info->fields--;
                tbl = tbl->next_peer;
                if (!tbl) {
                    going = 0;
                }
                continue;
            }
            DBGPRINT("Column Found: table_info->root[%lu] = %lu\n", table_info->rootlen, tbl->subid);
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
        DBGPRINTOID("Column OID is: ", table_info->root, table_info->rootlen + 1);
        out_len = 0;
        if (sprint_realloc_objid((u_char **) &buf, &buf_len, &out_len, 1, table_info->root,
                table_info->rootlen + 1)) {
            DBGPRINT("Full name of column OID: %s\n", buf);
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
        DBGPRINT("Extracted column name is: %s\n", name_p);

        /* allocating and setting up the column header structs for each column*/
        if (column_info->fields == 1) {
            /* initial malloc on first columns */
            column_info->column = (column_t*) malloc(sizeof(column_t));
        } else {
            /* realloc on following columns */
            column_info->column = (column_t*) realloc(column_info->column,
                    column_info->fields * sizeof(column_t));
        }
        column_info->column[column_info->fields - 1].label = strdup(name_p);
        DBGPRINT("column[fields - 1].label = %s\n", column_info->column[column_info->fields - 1].label);
        column_info->column[column_info->fields - 1].subid = table_info->root[table_info->rootlen];
    }
    /* end while (going) */

    if (column_info->fields == 0) {
        DBGPRINT("No column OIDs found MIB for %s\n", table_info->table_name);
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
        DBGPRINTOID("Column root OID for getbulk: ", column_info->name, column_info->name_length);
    }

    if (buf != NULL) {
        free(buf);
    }

    return SUCCESS;
}

char is_valid_var(table_info_t* table_info, netsnmp_variable_list *vars, int nr_in_response)
{
    if (vars->type == SNMP_ENDOFMIBVIEW) {
        DBGPRINT("Returned varbinding is end of MIB.\n");
        return 1;
    } else if (memcmp(vars->name, table_info->column_header.name, table_info->rootlen * sizeof(oid)) != 0) {
        DBGPRINT("Returned varbinding does not belong to our table.\n");
        return 1;
    } else if (vars->name[table_info->column_header.name_length] != table_info->column_header.column[nr_in_response].subid){
        DBGPRINT("Returned varbinding is not a instance of expected column (%i vs %i).\n",
                vars->name[table_info->column_header.name_length],
                table_info->column_header.column[nr_in_response].subid);
        return 1;
    } else if(memcmp(&vars->name[table_info->column_header.name_length+1], table_info->column_header.start_idx, table_info->column_header.start_idx_length * sizeof(oid)) != 0) {
        DBGPRINT("Returned varbinding does not have requested index.\n");
        oid* p = &vars->name[table_info->column_header.name_length+1];
        DBGPRINTOID(" varbind:    ", p, table_info->column_header.start_idx_length);
        DBGPRINTOID(" startindex: ", table_info->column_header.start_idx, table_info->column_header.start_idx_length);
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
            DBGPRINT("Invalid output format: %d\n",
                    netsnmp_ds_get_int(NETSNMP_DS_LIBRARY_ID,
                            NETSNMP_DS_LIB_OID_OUTPUT_FORMAT));
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

int store_value(char* row_index_str, column_t* column, netsnmp_variable_list *result_varbind, table_info_t* table) {
    PyObject* col_dict = NULL;
    char *buf = NULL;
    char *instance_id = NULL;
    const char delimiter [] = ".";
    size_t buf_len = 0;
    size_t out_len = 0;

    DBGPRINT("add row %s to result dictionary\n", row_index_str);

    sprint_realloc_value((u_char **) &buf, &buf_len, &out_len, 1, result_varbind->name,
            result_varbind->name_length, result_varbind);

    /* Get number of sub-instances. */
    int i, count;
    for (i=0, count = 1; i<strlen(row_index_str); i++)
        if (row_index_str[i] == '.') count++;

    /* Decompose instance string into parts. */
    //DBGPRINT("Create tuple with %i elements\n", count);

    PyObject* instance_tuple = PyTuple_New(count); // create function, we've got ownership of instance_tuple
    PyObject* subinstance = NULL;
    instance_id = strtok(row_index_str, delimiter);
    i = 0;
    while(instance_id) {
        if (instance_id[0] == '"') {
            //DBGPRINT("Found string-type sub instance %s\n", instance_id);
            instance_id++;
            subinstance = PyString_FromStringAndSize(instance_id, strlen(instance_id)-1); // create function, we've got ownership of ref-to-subinstance
        } else {
            //DBGPRINT("Assume int-type sub instance %s\n", instance_id);
            subinstance = PyInt_FromString(instance_id, NULL, 10); // create function, we've got ownership of ref-to-subinstance
        }
        PyTuple_SetItem(instance_tuple, i++, subinstance); // PyTuple_SetItem steals ownership, now ref-to-subinstance is only borrowed
        instance_id = strtok(NULL, delimiter);
    }

    // do we start a new row?
    col_dict = PyDict_GetItem(table->py_rows_dict, instance_tuple); // PyDict_GetItem() only borrows us ref-to-col_dict
    if (!col_dict)
    {
        DBGPRINT("Creating new column dict\n");
        col_dict = PyDict_New(); // create function, we've got ownership of ref-to-col_dict
        PyDict_SetItem(table->py_rows_dict, instance_tuple, col_dict); // PyDict_SetItem() handles INCREF on its own
        Py_XDECREF(col_dict); // now ref-to-coldict is borrowed from rows_dict
    }

    DBGPRINT(" [%s] = %s\n", column->label, buf);

    PyObject* val = PyString_FromString(buf); // create function, we've got ownership of ref-to-val
    PyDict_SetItemString(col_dict, column->label, val); // PyDict_SetItem() handles INCREF on its own, we can DECREF now
    Py_XDECREF(val);

    return out_len;
}

int response_err(netsnmp_pdu *response)
{
    int count;
    int exitval = 0;
    netsnmp_variable_list *vars;

    if (response->errstat == SNMP_ERR_NOSUCHNAME) {
        DBGPRINT("End of MIB\n");
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

PyObject* getbulk_table_sub_entries(table_info_t* table_info, const netsnmp_session* ss, int max_repeaters) {
    column_info_t* column_info = &table_info->column_header;
    int running = 1;
    netsnmp_pdu *pdu, *response;
    netsnmp_variable_list *vars, *last_var;
    int status;
    int col;
    char *buf = NULL;
    size_t buf_len = 0, out_len = 0;
    char *name_p = NULL;
    int exitval = SUCCESS;
    column_t* column;
    int tmp_dont_breakdown_oids;

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
        DBGPRINTOID("appending start index: ", table_info->column_header.start_idx, table_info->column_header.start_idx_length);
        memcpy(&column->last_oid[column->last_oid_len],
                table_info->column_header.start_idx,
                table_info->column_header.start_idx_length * sizeof(oid));
        column->last_oid_len += table_info->column_header.start_idx_length;

        /* a final zero... */
        column->last_oid[column->last_oid_len++] = 0;

        /* initially set to 1, to get it added to the first pdu */
        DBGPRINTOID("column last varbind OID", column->last_oid, column->last_oid_len);
    }

    DBGPRINT("max_repeaters = %i\n", max_repeaters);
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
                DBGPRINTOID("add oid to getbulk request pdu", column->last_oid, column->last_oid_len);
                snmp_add_null_var(pdu, column->last_oid, column->last_oid_len);
            }
            column->last_var = NULL;
            column->end = 0;
        }

        /*
         * do the request
         */
        DBGPRINT("do getbulk request\n");
        DBGPRINT("session - version %lu, community %s\n", ss->version, ss->community);
        status = snmp_synch_response(ss, pdu, &response);

        /* Allocates response->vars list. snmp_free_pdu destroys it at the end of the loop. */
        if (status == STAT_SUCCESS) {
            DBGPRINT("got success response\n");
            if (response->errstat == SNMP_ERR_NOERROR) {
                /*
                 * check resulting variables
                 */
                vars = response->variables;
                last_var = NULL;
                int nr_in_response = 0;
                int nr_columns_ended = 0;
                DBGPRINT("parse response\n");
                while (vars) {
                    /* Get the string representation from returned identifier.
                     * Depending on session settings, this may be dotted notation,
                     * or as MIB name.
                     */
                    out_len = 0;
                    sprint_realloc_objid((u_char **) &buf, &buf_len, &out_len,
                            1, vars->name, vars->name_length);
                    DBGPRINT("varbind = %s\n", buf);

                    int vb_col = nr_in_response % column_info->fields;
                    if (is_valid_var(table_info, vars, vb_col) == 1) {
                        /* skip this variable */
                        if (!column_info->column[vb_col].end) {
                            DBGPRINT("Detected end of column %i\n", vb_col);
                            column_info->column[vb_col].end = 1;
                            nr_columns_ended++;
                            if (nr_columns_ended == column_info->fields) {
                                DBGPRINT("Detected end of all columns\n");
                                running = 0;
                                break;
                            }
                        }
                        nr_in_response++;
                        continue;
                    }

                    column = get_column_by_subid(column_info, vars->name[table_info->rootlen]);
                    if (column) {
                        DBGPRINT("Update latest varbind pointer\n");
                        column->last_var = vars;
                    } else {
                        DBGPRINT("Unexpected column in response\n");
                        running = 0;
                        exitval = FAILURE;
                        break;
                    }

                    DBGPRINT("find row to which this varbind belongs\n");
                    if ((name_p = find_instance_id(buf, table_info->table_name)) == NULL) {
                        /* don't seem to include instance subidentifiers! */
                        DBGPRINT("don't seem to include instance subidentifiers\n");
                        running = 0;
                        exitval = FAILURE;
                        break;
                    }

                    DBGPRINT("name_p now points to %s in %s\n", name_p, buf);

                    /* name_p now points to instance id part of OID */
                    store_value(name_p, column, vars, table_info);
                    buf = NULL;
                    buf_len = 0;
                    last_var = vars;
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
            DBGPRINT("Timeout: No Response from %s\n", ss->peername);
            PyErr_SetString(PyExc_RuntimeError, "snmp_synch_response timed out, no response from agent");
            running = 0;
            exitval = FAILURE;
        } else { /* status == STAT_ERROR */
            DBGPRINT("got error response\n");
            PyErr_SetString(PyExc_RuntimeError, "snmp_synch_response returned error");
            //snmp_sess_perror("snmptable", ss);
            running = 0;
            exitval = FAILURE;
        }
        if (response)
            snmp_free_pdu(response);
    }
    /* end of send request loop */

    netsnmp_ds_set_boolean(NETSNMP_DS_LIBRARY_ID, NETSNMP_DS_LIB_DONT_BREAKDOWN_OIDS, tmp_dont_breakdown_oids);

    if (exitval == SUCCESS) {
        DBGPRINT("Returning successful\n");
        return table_info->py_rows_dict;
    }

    DBGPRINT("Returning exceptional\n");
    Py_DECREF(table_info->py_rows_dict);
    return NULL;
}
