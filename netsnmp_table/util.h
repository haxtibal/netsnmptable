#ifndef UTIL_H_
#define UTIL_H_

extern PyObject* py_netsnmp_attr_obj(PyObject *obj, char * attr_name);
extern long long py_netsnmp_attr_long(PyObject *obj, char * attr_name);
extern int py_netsnmp_attr_set_string(PyObject *obj, char *attr_name, char *val, size_t len);
extern int py_netsnmp_attr_string(PyObject *obj, char * attr_name, char **val, Py_ssize_t *len);
extern __snprint_value (buf, buf_len, var, tp, type, flag);

#endif /* UTIL_H_ */
