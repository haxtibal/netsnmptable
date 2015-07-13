#ifndef SNMPTABLE_H_
#define SNMPTABLE_H_

#include <Python.h>
#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>

/* column specific data - one per column */
typedef struct column_s {
    oid subid; // The table column ID. Assumes that the innermost index is an integer. How to deal with tables where this is a string index? Can it work at all?
    //char *label;
    PyObject* py_label_str;
    // for response PDU tracking
    oid last_oid[MAX_OID_LEN];
    size_t last_oid_len;
    netsnmp_variable_list *last_var; // most recent varbind for this column in a getbulk response
    char end;
} column_t;

/* column general data - one per table request */
typedef struct column_info_s {
    oid name[MAX_OID_LEN];
    size_t name_length;
    oid start_idx[MAX_OID_LEN];
    size_t start_idx_length;
    int fields;
    column_t *column;
} column_info_t;

typedef struct t_info_s {
    oid root[MAX_OID_LEN];
    size_t rootlen;
    column_info_t column_header;
    char *table_name;
    PyObject* py_rows_dict; // Contains 1 key-value pair per row, where value is in turn another dictionary.
} table_info_t;

extern int init_table(table_info_t* table_info, char* tablename);
extern int get_field_names(table_info_t* table_info);
extern PyObject* getbulk_table_sub_entries(table_info_t* table_info, netsnmp_session* ss, int max_repeaters);

#endif /* SNMPTABLE_H_ */
