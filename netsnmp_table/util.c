#include <Python.h>
#include "util.h"

#if PY_VERSION_HEX < 0x02050000
typedef int Py_ssize_t;
#define PY_SSIZE_T_MAX INT_MAX
#define PY_SSIZE_T_MIN INT_MIN
#endif

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <ctype.h>
#ifdef I_SYS_TIME
#include <sys/time.h>
#endif
#include <netdb.h>
#include <stdlib.h>

#ifdef HAVE_REGEX_H
#include <regex.h>
#endif

#define SUCCESS 1
#define FAILURE 0

#define VARBIND_TAG_F 0
#define VARBIND_IID_F 1
#define VARBIND_VAL_F 2
#define VARBIND_TYPE_F 3

#define TYPE_UNKNOWN 0
#define ENG_ID_BUF_SIZE 32

#define NO_RETRY_NOSUCH 0

/* these should be part of transform_oids.h ? */
#define USM_AUTH_PROTO_MD5_LEN 10
#define USM_AUTH_PROTO_SHA_LEN 10
#define USM_PRIV_PROTO_DES_LEN 10

#define STRLEN(x) (x ? strlen(x) : 0)

int _debug_level = 0;

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

PyObject *
py_netsnmp_construct_varbind(void)
{
  PyObject *module;
  PyObject *dict;
  PyObject *callable;

  module = PyImport_ImportModule("netsnmp");
  dict = PyModule_GetDict(module);

  callable = PyDict_GetItemString(dict, "Varbind");

  return PyObject_CallFunction(callable, "");
}


int __sprint_num_objid (buf, objid, len)
char *buf;
oid *objid;
int len;
{
   int i;
   buf[0] = '\0';
   for (i=0; i < len; i++) {
    sprintf(buf,".%lu",*objid++);
    buf += STRLEN(buf);
   }
   return SUCCESS;
}

int __snprint_value (buf, buf_len, var, tp, type, flag)
char * buf;
size_t buf_len;
netsnmp_variable_list * var;
struct tree * tp;
int type;
int flag;
{
   int len = 0;
   u_char* ip;
   struct enum_list *ep;


   buf[0] = '\0';
   if (flag == USE_SPRINT_VALUE) {
    snprint_value(buf, buf_len, var->name, var->name_length, var);
    len = STRLEN(buf);
   } else {
     switch (var->type) {
        case ASN_INTEGER:
           if (flag == USE_ENUMS) {
              for(ep = tp->enums; ep; ep = ep->next) {
                 if (ep->value == *var->val.integer) {
                    strlcpy(buf, ep->label, buf_len);
                    len = STRLEN(buf);
                    break;
                 }
              }
           }
           if (!len) {
              snprintf(buf, buf_len, "%ld", *var->val.integer);
              len = STRLEN(buf);
           }
           break;

        case ASN_GAUGE:
        case ASN_COUNTER:
        case ASN_TIMETICKS:
        case ASN_UINTEGER:
           snprintf(buf, buf_len, "%lu", (unsigned long) *var->val.integer);
           len = STRLEN(buf);
           break;

        case ASN_OCTET_STR:
        case ASN_OPAQUE:
           len = var->val_len;
           if (len > buf_len)
               len = buf_len;
           memcpy(buf, (char*)var->val.string, len);
           break;

        case ASN_IPADDRESS:
          ip = (u_char*)var->val.string;
          snprintf(buf, buf_len, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
          len = STRLEN(buf);
          break;

        case ASN_NULL:
           break;

        case ASN_OBJECT_ID:
          __sprint_num_objid(buf, (oid *)(var->val.objid),
                             var->val_len/sizeof(oid));
          len = STRLEN(buf);
          break;

    case SNMP_ENDOFMIBVIEW:
          snprintf(buf, buf_len, "%s", "ENDOFMIBVIEW");
      break;
    case SNMP_NOSUCHOBJECT:
      snprintf(buf, buf_len, "%s", "NOSUCHOBJECT");
      break;
    case SNMP_NOSUCHINSTANCE:
      snprintf(buf, buf_len, "%s", "NOSUCHINSTANCE");
      break;

        case ASN_COUNTER64:
#ifdef OPAQUE_SPECIAL_TYPES
        case ASN_OPAQUE_COUNTER64:
        case ASN_OPAQUE_U64:
#endif
          printU64(buf,(struct counter64 *)var->val.counter64);
          len = STRLEN(buf);
          break;

#ifdef OPAQUE_SPECIAL_TYPES
        case ASN_OPAQUE_I64:
          printI64(buf,(struct counter64 *)var->val.counter64);
          len = STRLEN(buf);
          break;
#endif

        case ASN_BIT_STR:
            snprint_bitstring(buf, buf_len, var, NULL, NULL, NULL);
            len = STRLEN(buf);
            break;
#ifdef OPAQUE_SPECIAL_TYPES
        case ASN_OPAQUE_FLOAT:
      if (var->val.floatVal)
        snprintf(buf, buf_len, "%f", *var->val.floatVal);
         break;

        case ASN_OPAQUE_DOUBLE:
      if (var->val.doubleVal)
        snprintf(buf, buf_len, "%f", *var->val.doubleVal);
         break;
#endif

        case ASN_NSAP:
        default:
      fprintf(stderr,"snprint_value: asn type not handled %d\n",var->type);
     }
   }
   return(len);
}

int __get_type_str (type, str)
int type;
char * str;
{
   switch (type) {
    case TYPE_OBJID:
            strcpy(str, "OBJECTID");
            break;
    case TYPE_OCTETSTR:
            strcpy(str, "OCTETSTR");
            break;
    case TYPE_INTEGER:
            strcpy(str, "INTEGER");
            break;
    case TYPE_INTEGER32:
            strcpy(str, "INTEGER32");
            break;
    case TYPE_UNSIGNED32:
            strcpy(str, "UNSIGNED32");
            break;
    case TYPE_NETADDR:
            strcpy(str, "NETADDR");
            break;
    case TYPE_IPADDR:
            strcpy(str, "IPADDR");
            break;
    case TYPE_COUNTER:
            strcpy(str, "COUNTER");
            break;
    case TYPE_GAUGE:
            strcpy(str, "GAUGE");
            break;
    case TYPE_TIMETICKS:
            strcpy(str, "TICKS");
            break;
    case TYPE_OPAQUE:
            strcpy(str, "OPAQUE");
            break;
    case TYPE_COUNTER64:
            strcpy(str, "COUNTER64");
            break;
    case TYPE_NULL:
                strcpy(str, "NULL");
                break;
    case SNMP_ENDOFMIBVIEW:
                strcpy(str, "ENDOFMIBVIEW");
                break;
    case SNMP_NOSUCHOBJECT:
                strcpy(str, "NOSUCHOBJECT");
                break;
    case SNMP_NOSUCHINSTANCE:
                strcpy(str, "NOSUCHINSTANCE");
                break;
    case TYPE_UINTEGER:
                strcpy(str, "UINTEGER"); /* historic - should not show up */
                                          /* but it does?                  */
                break;
    case TYPE_NOTIFTYPE:
        strcpy(str, "NOTIF");
        break;
    case TYPE_BITSTRING:
        strcpy(str, "BITS");
        break;
    case TYPE_TRAPTYPE:
        strcpy(str, "TRAP");
        break;
    case TYPE_OTHER: /* not sure if this is a valid leaf type?? */
    case TYPE_NSAPADDRESS:
        default: /* unsupported types for now */
           strcpy(str, "");
       if (_debug_level) printf("__get_type_str:FAILURE(%d)\n", type);

           return(FAILURE);
   }
   return SUCCESS;
}

int
__translate_asn_type(type)
int type;
{
   switch (type) {
        case ASN_INTEGER:
            return(TYPE_INTEGER);
    case ASN_OCTET_STR:
            return(TYPE_OCTETSTR);
    case ASN_OPAQUE:
            return(TYPE_OPAQUE);
    case ASN_OBJECT_ID:
            return(TYPE_OBJID);
    case ASN_TIMETICKS:
            return(TYPE_TIMETICKS);
    case ASN_GAUGE:
            return(TYPE_GAUGE);
    case ASN_COUNTER:
            return(TYPE_COUNTER);
    case ASN_IPADDRESS:
            return(TYPE_IPADDR);
    case ASN_BIT_STR:
            return(TYPE_BITSTRING);
    case ASN_NULL:
            return(TYPE_NULL);
    /* no translation for these exception type values */
    case SNMP_ENDOFMIBVIEW:
    case SNMP_NOSUCHOBJECT:
    case SNMP_NOSUCHINSTANCE:
        return(type);
    case ASN_UINTEGER:
            return(TYPE_UINTEGER);
    case ASN_COUNTER64:
            return(TYPE_COUNTER64);
    default:
            fprintf(stderr, "translate_asn_type: unhandled asn type (%d)\n",type);
            return(TYPE_OTHER);
        }
}

int __is_leaf (tp)
struct tree* tp;
{
   char buf[MAX_TYPE_NAME_LEN];
   return (tp && (__get_type_str(tp->type,buf) ||
          (tp->parent && __get_type_str(tp->parent->type,buf) )));
}

/* takes ss and pdu as input and updates the 'response' argument */
/* the input 'pdu' argument will be freed */
int
__send_sync_pdu(netsnmp_session *ss, netsnmp_pdu *pdu, netsnmp_pdu **response,
                int retry_nosuch, char *err_str, int *err_num, int *err_ind)
{
   int status = 0;
   long command = pdu->command;
   char *tmp_err_str;

   *err_num = 0;
   *err_ind = 0;
   *response = NULL;
   tmp_err_str = NULL;
   memset(err_str, '\0', STR_BUF_SIZE);
   if (ss == NULL) {
       *err_num = 0;
       *err_ind = SNMPERR_BAD_SESSION;
       status = SNMPERR_BAD_SESSION;
       strlcpy(err_str, snmp_api_errstring(*err_ind), STR_BUF_SIZE);
       goto done;
   }
retry:

   Py_BEGIN_ALLOW_THREADS
#ifdef NETSNMP_SINGLE_API
   status = snmp_sess_synch_response(ss, pdu, response);
#else
   status = snmp_synch_response(ss, pdu, response);
#endif
   Py_END_ALLOW_THREADS

   if ((*response == NULL) && (status == STAT_SUCCESS)) status = STAT_ERROR;

   switch (status) {
      case STAT_SUCCESS:
     switch ((*response)->errstat) {
        case SNMP_ERR_NOERROR:
           break;

            case SNMP_ERR_NOSUCHNAME:
               if (retry_nosuch && (pdu = snmp_fix_pdu(*response, command))) {
                  if (*response) snmp_free_pdu(*response);
                  goto retry;
               }

            /* Pv1, SNMPsec, Pv2p, v2c, v2u, v2*, and SNMPv3 PDUs */
            case SNMP_ERR_TOOBIG:
            case SNMP_ERR_BADVALUE:
            case SNMP_ERR_READONLY:
            case SNMP_ERR_GENERR:
            /* in SNMPv2p, SNMPv2c, SNMPv2u, SNMPv2*, and SNMPv3 PDUs */
            case SNMP_ERR_NOACCESS:
            case SNMP_ERR_WRONGTYPE:
            case SNMP_ERR_WRONGLENGTH:
            case SNMP_ERR_WRONGENCODING:
            case SNMP_ERR_WRONGVALUE:
            case SNMP_ERR_NOCREATION:
            case SNMP_ERR_INCONSISTENTVALUE:
            case SNMP_ERR_RESOURCEUNAVAILABLE:
            case SNMP_ERR_COMMITFAILED:
            case SNMP_ERR_UNDOFAILED:
            case SNMP_ERR_AUTHORIZATIONERROR:
            case SNMP_ERR_NOTWRITABLE:
            /* in SNMPv2c, SNMPv2u, SNMPv2*, and SNMPv3 PDUs */
            case SNMP_ERR_INCONSISTENTNAME:
            default:
               strlcpy(err_str, (char*)snmp_errstring((*response)->errstat),
               STR_BUF_SIZE);
               *err_num = (int)(*response)->errstat;
           *err_ind = (*response)->errindex;
               status = (*response)->errstat;
               break;
     }
         break;

      case STAT_TIMEOUT:
      case STAT_ERROR:
      snmp_sess_error(ss, err_num, err_ind, &tmp_err_str);
      strlcpy(err_str, tmp_err_str, STR_BUF_SIZE);
         break;

      default:
         strcat(err_str, "send_sync_pdu: unknown status");
         *err_num = ss->s_snmp_errno;
         break;
   }
done:
   if (tmp_err_str) {
    free(tmp_err_str);
   }
   if (_debug_level && *err_num) printf("XXX sync PDU: %s\n", err_str);
   return(status);
}

/**
 * Update python session object error attributes.
 *
 * Copy the error info which may have been returned from __send_sync_pdu(...)
 * into the python object. This will allow the python code to determine if
 * an error occured during an snmp operation.
 *
 * Currently there are 3 attributes we care about
 *
 * ErrorNum - Copy of the value of netsnmp_session.s_errno. This is the system
 * errno that was generated during our last call into the net-snmp library.
 *
 * ErrorInd - Copy of the value of netsmp_session.s_snmp_errno. These error
 * numbers are separate from the system errno's and describe SNMP errors.
 *
 * ErrorStr - A string describing the ErrorInd that was returned during our last
 * operation.
 *
 * @param[in] session The python object that represents our current Session
 * @param[in|out] err_str A string describing err_ind
 * @param[in|out] err_num The system errno currently stored by our session
 * @param[in|out] err_ind The snmp errno currently stored by our session
 */
void
__py_netsnmp_update_session_errors(PyObject *session, char *err_str,
                                    int err_num, int err_ind)
{
    PyObject *tmp_for_conversion;

    py_netsnmp_attr_set_string(session, "ErrorStr", err_str, STRLEN(err_str));

    tmp_for_conversion = PyInt_FromLong(err_num);
    if (!tmp_for_conversion)
        return; /* nothing better to do? */
    PyObject_SetAttrString(session, "ErrorNum", tmp_for_conversion);
    Py_DECREF(tmp_for_conversion);

    tmp_for_conversion = PyInt_FromLong(err_ind);
    if (!tmp_for_conversion)
        return; /* nothing better to do? */
    PyObject_SetAttrString(session, "ErrorInd", tmp_for_conversion);
    Py_DECREF(tmp_for_conversion);
}
