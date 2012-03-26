/*
    +--------------------------------------------------------------------+
    | PECL :: http                                                       |
    +--------------------------------------------------------------------+
    | Redistribution and use in source and binary forms, with or without |
    | modification, are permitted provided that the conditions mentioned |
    | in the accompanying LICENSE file are met.                          |
    +--------------------------------------------------------------------+
    | Copyright (c) 2004-2011, Michael Wallner <mike@php.net>            |
    +--------------------------------------------------------------------+
*/

#include "php_http_api.h"

#define PHP_HTTP_BEGIN_ARGS(method, req_args) 			PHP_HTTP_BEGIN_ARGS_EX(HttpClientRequest, method, 0, req_args)
#define PHP_HTTP_EMPTY_ARGS(method)						PHP_HTTP_EMPTY_ARGS_EX(HttpClientRequest, method, 0)
#define PHP_HTTP_CLIENT_REQUEST_ME(method, visibility)	PHP_ME(HttpClientRequest, method, PHP_HTTP_ARGS(HttpClientRequest, method), visibility)
#define PHP_HTTP_CLIENT_REQUEST_ALIAS(method, func)		PHP_HTTP_STATIC_ME_ALIAS(method, func, PHP_HTTP_ARGS(HttpClientRequest, method))
#define PHP_HTTP_CLIENT_REQUEST_MALIAS(me, al, vis)		ZEND_FENTRY(me, ZEND_MN(HttpClientRequest_##al), PHP_HTTP_ARGS(HttpClientRequest, al), vis)

PHP_HTTP_BEGIN_ARGS(__construct, 0)
	PHP_HTTP_ARG_VAL(method, 0)
	PHP_HTTP_ARG_VAL(url, 0)
	PHP_HTTP_ARG_ARR(headers, 1, 0)
	PHP_HTTP_ARG_OBJ(http\\Message\\Body, body, 1)
PHP_HTTP_END_ARGS;

PHP_HTTP_EMPTY_ARGS(getContentType);
PHP_HTTP_BEGIN_ARGS(setContentType, 1)
	PHP_HTTP_ARG_VAL(content_type, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_EMPTY_ARGS(getQuery);
PHP_HTTP_BEGIN_ARGS(setQuery, 0)
	PHP_HTTP_ARG_VAL(query_data, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_BEGIN_ARGS(addQuery, 1)
	PHP_HTTP_ARG_VAL(query_data, 0)
PHP_HTTP_END_ARGS;


PHP_METHOD(HttpClientRequest, __construct)
{
	char *meth_str = NULL, *url_str = NULL;
	int meth_len = 0, url_len = 0;
	zval *zheaders = NULL, *zbody = NULL;

	with_error_handling(EH_THROW, php_http_exception_class_entry) {
		if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|s!s!a!O!", &meth_str, &meth_len, &url_str, &url_len, &zheaders, &zbody, php_http_message_body_class_entry)) {
			php_http_message_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

			if (obj->message) {
				php_http_message_set_type(obj->message, PHP_HTTP_REQUEST);
			} else {
				obj->message = php_http_message_init(NULL, PHP_HTTP_REQUEST TSRMLS_CC);
			}

			if (meth_str && meth_len) {
				PHP_HTTP_INFO(obj->message).request.method = estrndup(meth_str, meth_len);
			}
			if (url_str && url_len) {
				PHP_HTTP_INFO(obj->message).request.url = estrndup(url_str, url_len);
			}
			if (zheaders) {
				array_copy(Z_ARRVAL_P(zheaders), &obj->message->hdrs);
			}
			if (zbody) {
				php_http_message_body_object_t *body_obj = zend_object_store_get_object(zbody TSRMLS_CC);

				php_http_message_body_dtor(&obj->message->body);
				php_http_message_body_copy(body_obj->body, &obj->message->body, 0);
				Z_OBJ_ADDREF_P(zbody);
				obj->body = Z_OBJVAL_P(zbody);
			}
		}
	} end_error_handling();
}


PHP_METHOD(HttpClientRequest, setContentType)
{
	char *ctype;
	int ct_len;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &ctype, &ct_len)) {
		int invalid = 0;

		if (ct_len) {
			PHP_HTTP_CHECK_CONTENT_TYPE(ctype, invalid = 1);
		}

		if (!invalid) {
			zend_update_property_stringl(php_http_client_request_class_entry, getThis(), ZEND_STRL("contentType"), ctype, ct_len TSRMLS_CC);
		}
	}
	RETVAL_ZVAL(getThis(), 1, 0);
}

PHP_METHOD(HttpClientRequest, getContentType)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		RETURN_PROP(php_http_client_request_class_entry, "contentType");
	}
	RETURN_FALSE;
}

PHP_METHOD(HttpClientRequest, setQuery)
{
	zval *qdata = NULL;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z!", &qdata)) {
		if ((!qdata) || Z_TYPE_P(qdata) == IS_NULL) {
			zend_update_property_stringl(php_http_client_request_class_entry, getThis(), ZEND_STRL("query"), "", 0 TSRMLS_CC);
		} else if ((Z_TYPE_P(qdata) == IS_ARRAY) || (Z_TYPE_P(qdata) == IS_OBJECT)) {
			char *query_data_str = NULL;
			size_t query_data_len;

			if (SUCCESS == php_http_url_encode_hash(HASH_OF(qdata), NULL, 0, &query_data_str, &query_data_len TSRMLS_CC)) {
				zend_update_property_stringl(php_http_client_request_class_entry, getThis(), ZEND_STRL("query"), query_data_str, query_data_len TSRMLS_CC);
				efree(query_data_str);
			}
		} else {
			zval *data = php_http_ztyp(IS_STRING, qdata);

			zend_update_property_stringl(php_http_client_request_class_entry, getThis(), ZEND_STRL("query"), Z_STRVAL_P(data), Z_STRLEN_P(data) TSRMLS_CC);
			zval_ptr_dtor(&data);
		}
	}
	RETVAL_ZVAL(getThis(), 1, 0);
}

PHP_METHOD(HttpClientRequest, getQuery)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		RETURN_PROP(php_http_client_request_class_entry, "query");
	}
	RETURN_FALSE;
}

PHP_METHOD(HttpClientRequest, addQuery)
{
	zval *qdata;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "a/", &qdata)) {
		char *query_data_str = NULL;
		size_t query_data_len = 0;
		zval *old_qdata = php_http_ztyp(IS_STRING, zend_read_property(php_http_client_request_class_entry, getThis(), ZEND_STRL("query"), 0 TSRMLS_CC));

		if (SUCCESS == php_http_url_encode_hash(Z_ARRVAL_P(qdata), Z_STRVAL_P(old_qdata), Z_STRLEN_P(old_qdata), &query_data_str, &query_data_len TSRMLS_CC)) {
			zend_update_property_stringl(php_http_client_request_class_entry, getThis(), ZEND_STRL("query"), query_data_str, query_data_len TSRMLS_CC);
			efree(query_data_str);
		}

		zval_ptr_dtor(&old_qdata);
	}
	RETVAL_ZVAL(getThis(), 1, 0);

}


zend_class_entry *php_http_client_request_class_entry;
zend_function_entry php_http_client_request_method_entry[] = {
	PHP_HTTP_CLIENT_REQUEST_ME(__construct, ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
	PHP_HTTP_CLIENT_REQUEST_ME(getQuery, ZEND_ACC_PUBLIC)
	PHP_HTTP_CLIENT_REQUEST_ME(setQuery, ZEND_ACC_PUBLIC)
	PHP_HTTP_CLIENT_REQUEST_ME(addQuery, ZEND_ACC_PUBLIC)
	PHP_HTTP_CLIENT_REQUEST_ME(getContentType, ZEND_ACC_PUBLIC)
	PHP_HTTP_CLIENT_REQUEST_ME(setContentType, ZEND_ACC_PUBLIC)
	EMPTY_FUNCTION_ENTRY
};

PHP_MINIT_FUNCTION(http_client_request)
{
	PHP_HTTP_REGISTER_CLASS(http\\Client, Request, http_client_request, php_http_message_class_entry, 0);

	zend_declare_property_string(php_http_client_request_class_entry, ZEND_STRL("query"), "", ZEND_ACC_PROTECTED TSRMLS_CC);
	zend_declare_property_string(php_http_client_request_class_entry, ZEND_STRL("contentType"), "", ZEND_ACC_PROTECTED TSRMLS_CC);

	return SUCCESS;
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

