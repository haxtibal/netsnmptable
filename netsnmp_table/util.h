#ifndef UTIL_H_
#define UTIL_H_

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>

typedef netsnmp_session SnmpSession;
typedef struct tree SnmpMibNode;

/* Wrapper around fprintf(stderr, ...) for clean and easy debug output. */
extern int _debug_level;
#ifdef  DEBUGGING
#define DBPRT(severity, otherargs)                  \
    do {                                \
        if (_debug_level && severity <= _debug_level) {     \
          (void)printf otherargs;                  \
        }                               \
    } while (/*CONSTCOND*/0)
#define DBPRTOID(severity, oidname, oid, len) \
    do { \
        int c; \
        if (_debug_level && severity <= _debug_level) { \
            fprintf(stderr, "%s (length %lu) ", oidname, len); \
            for (c=0; c<len; c++) fprintf(stderr, ".%lu", oid[c]); \
            fprintf(stderr, "\n"); \
        } \
    } while (0)
#else   /* DEBUGGING */
#define DBPRT(severity, otherargs)  /* Ignore */
#define DBPRTOID(severity, oidname, oid, len)  /* Ignore */
#endif  /* DEBUGGING */

#define D_ERR  1
#define D_WARN 2
#define D_DBG  3

extern PyObject* py_netsnmp_attr_obj(PyObject *obj, char * attr_name);
extern long long py_netsnmp_attr_long(PyObject *obj, char * attr_name);
extern int py_netsnmp_attr_set_string(PyObject *obj, char *attr_name, char *val, size_t len);
extern int py_netsnmp_attr_string(PyObject *obj, char * attr_name, char **val, Py_ssize_t *len);
extern int py_netsnmp_attr_oid(PyObject* self, char *attr_name, oid* p_oid, size_t maxlen, size_t* len);
extern int __sprint_num_objid (char* buf, oid* objid, int len);
extern int __snprint_value (char* buf, size_t buf_len, netsnmp_variable_list* var, struct tree* tp, int type, int flag);
extern int __get_type_str (int type, char* str);
extern int __is_leaf (struct tree* tp);
PyObject* netsnmp_create_session(PyObject *self, PyObject *args);

#endif /* UTIL_H_ */
