#ifndef SNMPTABLE_H_
#define SNMPTABLE_H_

#include <Python.h>
#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>

/* column specific data - one per column */
typedef struct column_s {
    oid subid; // The table column ID. Assumes that the innermost index is an integer. How to deal with tables where this is a string index? Can it work at all?
    PyObject* py_label_str;
    // for response PDU tracking
    oid last_oid[MAX_OID_LEN];
    size_t last_oid_len;
    netsnmp_variable_list *last_var; // most recent varbind for this column in a getbulk response
    char end;
} column_t;

/* column general data - one per table */
typedef struct column_scheme_s {
    oid name[MAX_OID_LEN];
    size_t name_length;
    oid start_idx[MAX_OID_LEN];
    size_t start_idx_length;
    int fields;
    column_t* column;
    column_t** position_map; // maps column by number in response to a column entry
} column_scheme_t;

typedef struct index_scheme_s {
	netsnmp_variable_list vars;
	unsigned char type;
	int val_len;
} index_scheme_t;

typedef struct t_info_s {
    oid root[MAX_OID_LEN];
    size_t rootlen;
    char *table_name;
    int getlabel_flag;
    int sprintval_flag;
    column_scheme_t column_scheme;
    index_scheme_t* index_vars;
    int index_vars_nrof;
} table_info_t;

extern table_info_t* table_allocate(char* tablename);
extern void table_deallocate(table_info_t* table);
extern int table_get_field_names(table_info_t* table_info);
extern PyObject* table_getbulk_sub_entries(table_info_t* table_info,
		void* ss_opaque, int max_repeaters, PyObject *session);

#endif /* SNMPTABLE_H_ */
