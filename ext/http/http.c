/*
   +----------------------------------------------------------------------+
   | PECL :: http                                                         |
   +----------------------------------------------------------------------+
   | This source file is subject to version 3.0 of the PHP license, that  |
   | is bundled with this package in the file LICENSE, and is available   |
   | through the world-wide-web at http://www.php.net/license/3_0.txt.    |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
   | Copyright (c) 2004-2005 Michael Wallner <mike@php.net>               |
   +----------------------------------------------------------------------+
*/

/* $Id$ */

#define _WINSOCKAPI_
#define ZEND_INCLUDE_FULL_WINDOWS_HEADERS

#ifdef HAVE_CONFIG_H
#	include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "snprintf.h"
#include "ext/standard/info.h"
#include "ext/session/php_session.h"
#include "ext/standard/php_string.h"
#include "ext/standard/php_smart_str.h"

#include "SAPI.h"

#include "php_http.h"
#include "php_http_api.h"

#ifdef ZEND_ENGINE_2
#	include "ext/standard/php_http.h"
#endif

#ifdef HTTP_HAVE_CURL

#	ifdef PHP_WIN32
#		include <winsock2.h>
#		include <sys/types.h>
#	endif

#	include <curl/curl.h>

/* {{{ ARG_INFO */
#	ifdef ZEND_BEGIN_ARG_INFO
ZEND_BEGIN_ARG_INFO(http_request_info_ref_3, 0)
	ZEND_ARG_PASS_INFO(0)
	ZEND_ARG_PASS_INFO(0)
	ZEND_ARG_PASS_INFO(1)
ZEND_END_ARG_INFO();

ZEND_BEGIN_ARG_INFO(http_request_info_ref_4, 0)
	ZEND_ARG_PASS_INFO(0)
	ZEND_ARG_PASS_INFO(0)
	ZEND_ARG_PASS_INFO(0)
	ZEND_ARG_PASS_INFO(1)
ZEND_END_ARG_INFO();
#	else
static unsigned char http_request_info_ref_3[] = {3, BYREF_NONE, BYREF_NONE, BYREF_FORCE};
static unsigned char http_request_info_ref_4[] = {4, BYREF_NONE, BYREF_NONE, BYREF_NONE, BYREF_FORCE};
#	endif
/* }}} ARG_INFO */

#endif /* HTTP_HAVE_CURL */

ZEND_DECLARE_MODULE_GLOBALS(http)

#ifdef COMPILE_DL_HTTP
ZEND_GET_MODULE(http)
#endif

/* {{{ http_functions[] */
function_entry http_functions[] = {
	PHP_FE(http_date, NULL)
	PHP_FE(http_absolute_uri, NULL)
	PHP_FE(http_negotiate_language, NULL)
	PHP_FE(http_negotiate_charset, NULL)
	PHP_FE(http_redirect, NULL)
	PHP_FE(http_send_status, NULL)
	PHP_FE(http_send_last_modified, NULL)
	PHP_FE(http_match_modified, NULL)
	PHP_FE(http_match_etag, NULL)
	PHP_FE(http_cache_last_modified, NULL)
	PHP_FE(http_cache_etag, NULL)
	PHP_FE(http_content_type, NULL)
	PHP_FE(http_content_disposition, NULL)
	PHP_FE(http_send_data, NULL)
	PHP_FE(http_send_file, NULL)
	PHP_FE(http_send_stream, NULL)
	PHP_FE(http_chunked_decode, NULL)
	PHP_FE(http_split_response, NULL)
	PHP_FE(http_parse_headers, NULL)
	PHP_FE(http_get_request_headers, NULL)
#ifdef HTTP_HAVE_CURL
	PHP_FE(http_get, http_request_info_ref_3)
	PHP_FE(http_head, http_request_info_ref_3)
	PHP_FE(http_post_data, http_request_info_ref_4)
	PHP_FE(http_post_array, http_request_info_ref_4)
#endif
	PHP_FE(http_auth_basic, NULL)
	PHP_FE(http_auth_basic_cb, NULL)
#ifndef ZEND_ENGINE_2
	PHP_FE(http_build_query, NULL)
#endif
	PHP_FE(ob_httpetaghandler, NULL)
	{NULL, NULL, NULL}
};
/* }}} */

#define RETURN_SUCCESS(v) RETURN_BOOL(SUCCESS == (v))
#define HASH_ORNULL(z) ((z) ? Z_ARRVAL_P(z) : NULL)

#ifdef ZEND_ENGINE_2

#	define HTTP_REGISTER_CLASS_EX(classname, name, parent, flags) \
	{ \
		zend_class_entry ce; \
		INIT_CLASS_ENTRY(ce, #classname, name## _class_methods); \
		ce.create_object = name## _new_object; \
		name## _ce = zend_register_internal_class_ex(&ce, parent, NULL TSRMLS_CC); \
		name## _ce->ce_flags |= flags;  \
		memcpy(& name## _object_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers)); \
		name## _object_handlers.clone_obj = NULL; \
		name## _declare_default_properties(name## _ce); \
	}

#	define HTTP_REGISTER_CLASS(classname, name, parent, flags) \
	{ \
		zend_class_entry ce; \
		INIT_CLASS_ENTRY(ce, #classname, name## _class_methods); \
		ce.create_object = NULL; \
		name## _ce = zend_register_internal_class_ex(&ce, parent, NULL TSRMLS_CC); \
		name## _ce->ce_flags |= flags;  \
	}

#	define getObject(t, o) t * o = ((t *) zend_object_store_get_object(getThis() TSRMLS_CC))
#	define OBJ_PROP(o) o->zo.properties
#	define DCL_PROP(a, t, n, v) zend_declare_property_ ##t(ce, (#n), sizeof(#n), (v), (ZEND_ACC_ ##a) TSRMLS_CC)
#	define UPD_PROP(o, t, n, v) zend_update_property_ ##t(o->zo.ce, getThis(), (#n), sizeof(#n), (v) TSRMLS_CC)
#	define SET_PROP(o, n, z) zend_update_property(o->zo.ce, getThis(), (#n), sizeof(#n), (z) TSRMLS_CC)
#	define GET_PROP(o, n) zend_read_property(o->zo.ce, getThis(), (#n), sizeof(#n), 0 TSRMLS_CC)

/* {{{ HTTPi */

zend_class_entry *httpi_ce;

#define HTTPi_ME(me, al, ai) ZEND_FENTRY(me, ZEND_FN(al), ai, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)

zend_function_entry httpi_class_methods[] = {
	HTTPi_ME(date, http_date, NULL)
	HTTPi_ME(absoluteURI, http_absolute_uri, NULL)
	HTTPi_ME(negotiateLanguage, http_negotiate_language, NULL)
	HTTPi_ME(negotiateCharset, http_negotiate_charset, NULL)
	HTTPi_ME(redirect, http_redirect, NULL)
	HTTPi_ME(sendStatus, http_send_status, NULL)
	HTTPi_ME(sendLastModified, http_send_last_modified, NULL)
	HTTPi_ME(matchModified, http_match_modified, NULL)
	HTTPi_ME(matchEtag, http_match_etag, NULL)
	HTTPi_ME(cacheLastModified, http_cache_last_modified, NULL)
	HTTPi_ME(cacheEtag, http_cache_etag, NULL)
	HTTPi_ME(chunkedDecode, http_chunked_decode, NULL)
	HTTPi_ME(splitResponse, http_split_response, NULL)
	HTTPi_ME(parseHeaders, http_parse_headers, NULL)
	HTTPi_ME(getRequestHeaders, http_get_request_headers, NULL)
#ifdef HTTP_HAVE_CURL
	HTTPi_ME(get, http_get, http_request_info_ref_3)
	HTTPi_ME(head, http_head, http_request_info_ref_3)
	HTTPi_ME(postData, http_post_data, http_request_info_ref_4)
	HTTPi_ME(postArray, http_post_array, http_request_info_ref_4)
#endif
	HTTPi_ME(authBasic, http_auth_basic, NULL)
	HTTPi_ME(authBasicCallback, http_auth_basic_cb, NULL)
	{NULL, NULL, NULL}
};
/* }}} HTTPi */

/* {{{ HTTPi_Response */

zend_class_entry *httpi_response_ce;
static zend_object_handlers httpi_response_object_handlers;

typedef struct {
	zend_object	zo;
} httpi_response_object;

#define httpi_response_declare_default_properties(ce) _httpi_response_declare_default_properties(ce TSRMLS_CC)
static inline void _httpi_response_declare_default_properties(zend_class_entry *ce TSRMLS_DC)
{
	DCL_PROP(PROTECTED, string, contentType, "application/x-octetstream");
	DCL_PROP(PROTECTED, string, eTag, "");
	DCL_PROP(PROTECTED, string, dispoFile, "");
	DCL_PROP(PROTECTED, string, cacheControl, "public");
	DCL_PROP(PROTECTED, string, data, "");
	DCL_PROP(PROTECTED, string, file, "");
	DCL_PROP(PROTECTED, long, stream, 0);
	DCL_PROP(PROTECTED, long, size, 0);
	DCL_PROP(PROTECTED, long, lastModified, 0);
	DCL_PROP(PROTECTED, long, dispoInline, 0);
	DCL_PROP(PROTECTED, long, cache, 0);
	DCL_PROP(PROTECTED, long, gzip, 0);

	DCL_PROP(PRIVATE, long, raw_cache_header, 0);
	DCL_PROP(PRIVATE, long, send_mode, -1);
}

#define httpi_response_destroy_object _httpi_response_destroy_object
void _httpi_response_destroy_object(void *object, zend_object_handle handle TSRMLS_DC)
{
	httpi_response_object *o = object;
	if (OBJ_PROP(o)) {
		zend_hash_destroy(OBJ_PROP(o));
		FREE_HASHTABLE(OBJ_PROP(o));
	}
	efree(o);
}

#define httpi_response_new_object _httpi_response_new_object
zend_object_value _httpi_response_new_object(zend_class_entry *ce TSRMLS_DC)
{
	zend_object_value ov;
	httpi_response_object *o;

	o = ecalloc(sizeof(httpi_response_object), 1);
	o->zo.ce = ce;

	ALLOC_HASHTABLE(OBJ_PROP(o));
	zend_hash_init(OBJ_PROP(o), 0, NULL, ZVAL_PTR_DTOR, 0);
	zend_hash_copy(OBJ_PROP(o), &ce->default_properties, (copy_ctor_func_t) zval_add_ref, NULL, sizeof(zval *));
	
	ov.handle = zend_objects_store_put(o, httpi_response_destroy_object, NULL, NULL TSRMLS_CC);
	ov.handlers = &httpi_response_object_handlers;

	return ov;
}

zend_function_entry httpi_response_class_methods[] = {
	PHP_ME(HTTPi_Response, __construct, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
/*	PHP_ME(HTTPi_Response, __destruct, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_DTOR)
*/
	PHP_ME(HTTPi_Response, setETag, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(HTTPi_Response, getETag, NULL, ZEND_ACC_PUBLIC)

	PHP_ME(HTTPi_Response, setContentDisposition, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(HTTPi_Response, getContentDisposition, NULL, ZEND_ACC_PUBLIC)

	PHP_ME(HTTPi_Response, setContentType, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(HTTPi_Response, getContentType, NULL, ZEND_ACC_PUBLIC)

	PHP_ME(HTTPi_Response, setCache, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(HTTPi_Response, getCache, NULL, ZEND_ACC_PUBLIC)

	PHP_ME(HTTPi_Response, setCacheControl, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(HTTPi_Response, getCacheControl, NULL, ZEND_ACC_PUBLIC)

	PHP_ME(HTTPi_Response, setGzip, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(HTTPi_Response, getGzip, NULL, ZEND_ACC_PUBLIC)

	PHP_ME(HTTPi_Response, setData, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(HTTPi_Response, getData, NULL, ZEND_ACC_PUBLIC)

	PHP_ME(HTTPi_Response, setFile, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(HTTPi_Response, getFile, NULL, ZEND_ACC_PUBLIC)

	PHP_ME(HTTPi_Response, setStream, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(HTTPi_Response, getStream, NULL, ZEND_ACC_PUBLIC)

	PHP_ME(HTTPi_Response, send, NULL, ZEND_ACC_PUBLIC)
	
	PHP_ME(HTTPi_Response, getSize, NULL, ZEND_ACC_PUBLIC)

	{NULL, NULL, NULL}
};

/* {{{ proto void HTTPi_Response::__construct(bool cache, bool gzip)
 *
 */
PHP_METHOD(HTTPi_Response, __construct)
{
	zend_bool do_cache = 0, do_gzip = 0;
	getObject(httpi_response_object, obj);

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|bb", &do_cache, &do_gzip)) {
		// throw exception
		return;
	}

	UPD_PROP(obj, long, cache, do_cache);
	UPD_PROP(obj, long, gzip, do_gzip);
}
/* }}} */

/* {{{ proto bool HTTPi_Response::setCache(bool cache)
 *
 */
PHP_METHOD(HTTPi_Response, setCache)
{
	zend_bool do_cache = 0;
	getObject(httpi_response_object, obj);

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "b", &do_cache)) {
		RETURN_FALSE;
	}

	UPD_PROP(obj, long, cache, do_cache);
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool HTTPi_Response::getCache()
 *
 */
PHP_METHOD(HTTPi_Response, getCache)
{
	zval *do_cache = NULL;
	getObject(httpi_response_object, obj);

	if (ZEND_NUM_ARGS()) {
		WRONG_PARAM_COUNT;
	}

	do_cache = GET_PROP(obj, cache);
	RETURN_BOOL(Z_LVAL_P(do_cache));
}
/* }}}*/

/* {{{ proto bool HTTPi_Response::setGzip(bool gzip)
 *
 */
PHP_METHOD(HTTPi_Response, setGzip)
{
	zend_bool do_gzip = 0;
	getObject(httpi_response_object, obj);

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "b", &do_gzip)) {
		RETURN_FALSE;
	}

	UPD_PROP(obj, long, gzip, do_gzip);
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool HTTPi_Response::getGzip()
 *
 */
PHP_METHOD(HTTPi_Response, getGzip)
{
	zval *do_gzip = NULL;
	getObject(httpi_response_object, obj);

	if (ZEND_NUM_ARGS()) {
		WRONG_PARAM_COUNT;
	}

	do_gzip = GET_PROP(obj, gzip);
	RETURN_BOOL(Z_LVAL_P(do_gzip));
}
/* }}} */

/* {{{ proto bool HTTPi_Response::setCacheControl(string control[, bool raw = false])
 *
 */
PHP_METHOD(HTTPi_Response, setCacheControl)
{
	char *ccontrol;
	int cc_len;
	zend_bool raw = 0;
	getObject(httpi_response_object, obj);

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|b", &ccontrol, &cc_len, &raw)) {
		RETURN_FALSE;
	}

	if ((!raw) && (strcmp(ccontrol, "public") && strcmp(ccontrol, "private") && strcmp(ccontrol, "no-cache"))) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Cache-Control '%s' doesn't match public, private or no-cache", ccontrol);
		RETURN_FALSE;
	}

	UPD_PROP(obj, long, raw_cache_header, raw);
	UPD_PROP(obj, string, cacheControl, ccontrol);
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto string HTTPi_Response::getCacheControl()
 *
 */
PHP_METHOD(HTTPi_Response, getCacheControl)
{
	zval *ccontrol;
	getObject(httpi_response_object, obj);

	if (ZEND_NUM_ARGS()) {
		WRONG_PARAM_COUNT;
	}

	ccontrol = GET_PROP(obj, cacheControl);
	RETURN_STRINGL(Z_STRVAL_P(ccontrol), Z_STRLEN_P(ccontrol), 1);
}
/* }}} */

/* {{{ proto bool HTTPi::setContentType(string content_type)
 *
 */
PHP_METHOD(HTTPi_Response, setContentType)
{
	char *ctype;
	int ctype_len;
	getObject(httpi_response_object, obj);

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &ctype, &ctype_len)) {
		RETURN_FALSE;
	}

	if (!strchr(ctype, '/')) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Content type '%s' doesn't seem to contain a primary and secondary part", ctype);
		RETURN_FALSE;
	}

	UPD_PROP(obj, string, contentType, ctype);
	if (HTTP_G(ctype)) {
		efree(HTTP_G(ctype));
	}
	HTTP_G(ctype) = estrndup(ctype, ctype_len);
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto string HTTPi_Response::getContentType()
 *
 */
PHP_METHOD(HTTPi_Response, getContentType)
{
	zval *ctype;
	getObject(httpi_response_object, obj);

	if (ZEND_NUM_ARGS()) {
		WRONG_PARAM_COUNT;
	}

	ctype = GET_PROP(obj, contentType);
	RETURN_STRINGL(Z_STRVAL_P(ctype), Z_STRLEN_P(ctype), 1);
}
/* }}} */

/* {{{ proto bool HTTPi_Response::setContentDisposition(string filename[, bool inline = false])
 *
 */
PHP_METHOD(HTTPi_Response, setContentDisposition)
{
	char *file;
	int file_len;
	zend_bool is_inline = 0;
	getObject(httpi_response_object, obj);

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|b", &file, &file_len, &is_inline)) {
		RETURN_FALSE;
	}

	UPD_PROP(obj, string, dispoFile, file);
	UPD_PROP(obj, long, dispoInline, is_inline);
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto array HTTPi_Response::getContentDisposition()
 *
 */
PHP_METHOD(HTTPi_Response, getContentDisposition)
{
	zval *file;
	zval *is_inline;
	getObject(httpi_response_object, obj);

	if (ZEND_NUM_ARGS()) {
		WRONG_PARAM_COUNT;
	}

	file = GET_PROP(obj, dispoFile);
	is_inline = GET_PROP(obj, dispoInline);

	array_init(return_value);
	add_assoc_stringl(return_value, "filename", Z_STRVAL_P(file), Z_STRLEN_P(file), 1);
	add_assoc_bool(return_value, "inline", Z_LVAL_P(is_inline));
}
/* }}} */

/* {{{ proto bool HTTPi_Response::setETag(string etag)
 *
 */
PHP_METHOD(HTTPi_Response, setETag)
{
	char *etag;
	int etag_len;
	getObject(httpi_response_object, obj);

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &etag, &etag_len)) {
		RETURN_FALSE;
	}

	UPD_PROP(obj, string, eTag, etag);
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto string HTTPi_Response::getETag()
 *
 */
PHP_METHOD(HTTPi_Response, getETag)
{
	zval *etag;
	getObject(httpi_response_object, obj);

	if (ZEND_NUM_ARGS()) {
		WRONG_PARAM_COUNT;
	}

	etag = GET_PROP(obj, eTag);
	RETURN_STRINGL(Z_STRVAL_P(etag), Z_STRLEN_P(etag), 1);
}
/* }}} */

/* {{{ proto bool HTTPi_Response::setData(string data)
 *
 */
PHP_METHOD(HTTPi_Response, setData)
{
	zval *the_data;
	char *etag;
	getObject(httpi_response_object, obj);
	
	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &the_data)) {
		RETURN_FALSE;
	}
	
	convert_to_string_ex(&the_data);
	etag = http_etag(Z_STRVAL_P(the_data), Z_STRLEN_P(the_data), SEND_DATA);
	SET_PROP(obj, data, the_data);
	UPD_PROP(obj, string, eTag, etag);
	UPD_PROP(obj, long, size, Z_STRLEN_P(the_data));
	UPD_PROP(obj, long, lastModified, time(NULL));
	UPD_PROP(obj, long, send_mode, SEND_DATA);
	efree(etag);
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto string HTTPi_Response::getData()
 *
 */
PHP_METHOD(HTTPi_Response, getData)
{
	zval *the_data;
	getObject(httpi_response_object, obj);
	
	if (ZEND_NUM_ARGS()) {
		WRONG_PARAM_COUNT;
	}
	
	the_data = GET_PROP(obj, data);
	RETURN_STRINGL(Z_STRVAL_P(the_data), Z_STRLEN_P(the_data), 1);
}
/* }}} */

/* {{{ proto bool HTTPi_Response::setStream(resource stream)
 *
 */
PHP_METHOD(HTTPi_Response, setStream)
{
	zval *the_stream;
	php_stream *the_real_stream;
	char *etag;
	getObject(httpi_response_object, obj);
	
	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r", &the_stream)) {
		RETURN_FALSE;
	}
	
	php_stream_from_zval(the_real_stream, &the_stream);
	etag = http_etag(the_real_stream, 0, SEND_RSRC);
	
	SET_PROP(obj, stream, the_stream);
	UPD_PROP(obj, string, eTag, etag);
	UPD_PROP(obj, long, size, HTTP_G(ssb).sb.st_size);
	UPD_PROP(obj, long, lastModified, HTTP_G(ssb).sb.st_mtime);
	UPD_PROP(obj, long, send_mode, SEND_RSRC);
	efree(etag);
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto resource HTTPi_Response::getStream()
 *
 */
PHP_METHOD(HTTPi_Response, getStream)
{
	zval *the_stream;
	getObject(httpi_response_object, obj);
	
	if (ZEND_NUM_ARGS()) {
		WRONG_PARAM_COUNT;
	}
	
	the_stream = GET_PROP(obj, stream);
	RETURN_RESOURCE(Z_LVAL_P(the_stream));
}
/* }}} */

/* {{{ proto bool HTTPi_Response::setFile(string file)
 *
 */
PHP_METHOD(HTTPi_Response, setFile)
{
	char *the_file;
	int file_len;
	zval fmtime, fsize;
	getObject(httpi_response_object, obj);
	
	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &the_file, &file_len)) {
		RETURN_FALSE;
	}
	
	php_stat(the_file, file_len, FS_SIZE, &fsize TSRMLS_CC);
	php_stat(the_file, file_len, FS_MTIME, &fmtime TSRMLS_CC);
	
	UPD_PROP(obj, string, file, the_file);
	UPD_PROP(obj, long, size, Z_LVAL(fsize));
	UPD_PROP(obj, long, lastModified, Z_LVAL(fmtime));
	UPD_PROP(obj, long, send_mode, -1);
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto string HTTPi_Response::getFile()
 *
 */
PHP_METHOD(HTTPi_Response, getFile)
{
	zval *the_file;
	getObject(httpi_response_object, obj);
	
	if (ZEND_NUM_ARGS()) {
		WRONG_PARAM_COUNT;
	}
	
	the_file = GET_PROP(obj, file);
	RETURN_STRINGL(Z_STRVAL_P(the_file), Z_STRLEN_P(the_file), 1);
}
/* }}} */

/* {{{ proto int HTTPi_Response::getSize()
 *
 */
PHP_METHOD(HTTPi_Response, getSize)
{
	zval *the_size;
	getObject(httpi_response_object, obj);
	
	if (ZEND_NUM_ARGS()) {
		WRONG_PARAM_COUNT;
	}
	
	the_size = GET_PROP(obj, size);
	RETURN_LONG(Z_LVAL_P(the_size));
}
/* }}} */

PHP_METHOD(HTTPi_Response, send)
{
	zval *do_cache;
	getObject(httpi_response_object, obj);
	
	do_cache = GET_PROP(obj, cache);
	
	/* caching */
	if (Z_LVAL_P(do_cache)) {
		/* cache header */
		{
			zval *ccontrol = GET_PROP(obj, cacheControl);
			if (Z_STRLEN_P(ccontrol)) {
				char *cc_header;
				zval *cc_raw = GET_PROP(obj, raw_cache_header);
				if (Z_LVAL_P(cc_raw)) {
					cc_header = ecalloc(sizeof("Cache-Control: ") + Z_STRLEN_P(ccontrol), 1);
					sprintf(cc_header, "Cache-Control: %s", Z_STRVAL_P(ccontrol));
				} else {
					cc_header = ecalloc(sizeof("Cache-Control: , must-revalidate, max-age=0") + Z_STRLEN_P(ccontrol), 1);
					sprintf(cc_header, "Cache-Control: %s, must-revalidate, max-age=0", Z_STRVAL_P(ccontrol));
				}
				http_send_header(cc_header);
				efree(cc_header);
			} else {
				http_send_header("Cache-Control: public, must-revalidate, max-age=0");
			}
		}
		/* etag */
		{
			zval *etag = GET_PROP(obj, eTag);
			if (Z_STRLEN_P(etag)) {
				if ((!http_is_range_request()) && http_etag_match("HTTP_IF_NONE_MATCH", Z_STRVAL_P(etag))) {
					http_send_status(304);
					RETURN_TRUE;
				}
				http_send_etag(Z_STRVAL_P(etag), Z_STRLEN_P(etag));
				if (HTTP_G(etag)) {
					efree(HTTP_G(etag));
				}
				HTTP_G(etag) = estrndup(Z_STRVAL_P(etag), Z_STRLEN_P(etag));
			}
		}
		/* last modified */
		{
			zval *lmod = GET_PROP(obj, lastModified);
			if (Z_LVAL_P(lmod)) {
				if (http_modified_match("HTTP_IF_MODIFIED_SINCE", Z_LVAL_P(lmod))) {
					http_send_status(304);
					RETURN_TRUE;
				}
				http_send_last_modified(Z_LVAL_P(lmod));
				HTTP_G(lmod) = Z_LVAL_P(lmod);
			} else {
				time_t t = time(NULL);
				http_send_last_modified(t);
				HTTP_G(lmod) = t;
			}
		}
	} /* caching done */
	
	/* gzip */
	/* ... */
	
	/* content type */
	{
		zval *ctype = GET_PROP(obj, contentType);
		if (Z_STRLEN_P(ctype)) {
			char *ctype_header = ecalloc(sizeof("Content-Type: ") + Z_STRLEN_P(ctype), 1);
			sprintf(ctype_header, "Content-Type: %s", Z_STRVAL_P(ctype));
			http_send_header(ctype_header);
			efree(ctype_header);
			if (HTTP_G(ctype)) {
				efree(HTTP_G(ctype));
			}
			HTTP_G(ctype) = estrndup(Z_STRVAL_P(ctype), Z_STRLEN_P(ctype));
		} else {
			http_send_header("Content-Type: application/x-octetstream");
			if (HTTP_G(ctype)) {
				efree(HTTP_G(ctype));
			}
			HTTP_G(ctype) = estrdup("application/x-octetstream");
		}
	}
	
	/* content disposition */
	{
		zval *dispo_file = GET_PROP(obj, dispoFile);
		if (Z_STRLEN_P(dispo_file)) {
			char *dispo_header;
			zval *dispo_inline = GET_PROP(obj, dispoInline);
			if (Z_LVAL_P(dispo_inline)) {
				dispo_header = ecalloc(sizeof("Content-Disposition: inline; filename=\"\"") + Z_STRLEN_P(dispo_file), 1);
				sprintf(dispo_header, "Content-Disposition: inline; filename=\"%s\"", Z_STRVAL_P(dispo_file));
			} else {
				dispo_header = ecalloc(sizeof("Content-Disposition: attachment; filename=\"\"") + Z_STRLEN_P(dispo_file), 1);
				sprintf(dispo_header, "Content-Disposition: attachment; filename=\"%s\"", Z_STRVAL_P(dispo_file));
			}
			http_send_header(dispo_header);
			efree(dispo_header);
		}
	}
	
	/* send */
	{
		zval *send_mode = GET_PROP(obj, send_mode);
		switch (Z_LVAL_P(send_mode))
		{
			case SEND_DATA:
			{
				RETURN_SUCCESS(http_send_data(GET_PROP(obj, data)));
			}
			
			case SEND_RSRC:
			{
				php_stream *the_real_stream;
				zval *the_stream = GET_PROP(obj, stream);
				php_stream_from_zval(the_real_stream, &the_stream);
				RETURN_SUCCESS(http_send_stream(the_real_stream));
			}
			
			default:
			{
				RETURN_SUCCESS(http_send_file(GET_PROP(obj, file)));
			}
		}
	}
}

#endif /* ZEND_ENGINE_2 */

/* {{{ http_module_entry */
zend_module_entry http_module_entry = {
#if ZEND_MODULE_API_NO >= 20010901
	STANDARD_MODULE_HEADER,
#endif
	"http",
	http_functions,
	PHP_MINIT(http),
	PHP_MSHUTDOWN(http),
	PHP_RINIT(http),
	PHP_RSHUTDOWN(http),
	PHP_MINFO(http),
#if ZEND_MODULE_API_NO >= 20010901
	PHP_EXT_HTTP_VERSION,
#endif
	STANDARD_MODULE_PROPERTIES
};
/* }}} */

/* {{{ proto string http_date([int timestamp])
 *
 * This function returns a valid HTTP date regarding RFC 822/1123
 * looking like: "Wed, 22 Dec 2004 11:34:47 GMT"
 *
 */
PHP_FUNCTION(http_date)
{
	long t = -1;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|l", &t) != SUCCESS) {
		RETURN_FALSE;
	}

	if (t == -1) {
		t = (long) time(NULL);
	}

	RETURN_STRING(http_date(t), 0);
}
/* }}} */

/* {{{ proto string http_absolute_uri(string url[, string proto])
 *
 * This function returns an absolute URI constructed from url.
 * If the url is already abolute but a different proto was supplied,
 * only the proto part of the URI will be updated.  If url has no
 * path specified, the path of the current REQUEST_URI will be taken.
 * The host will be taken either from the Host HTTP header of the client
 * the SERVER_NAME or just localhost if prior are not available.
 *
 * Some examples:
 * <pre>
 *  url = "page.php"                    => http://www.example.com/current/path/page.php
 *  url = "/page.php"                   => http://www.example.com/page.php
 *  url = "/page.php", proto = "https"  => https://www.example.com/page.php
 * </pre>
 *
 */
PHP_FUNCTION(http_absolute_uri)
{
	char *url = NULL, *proto = NULL;
	int url_len = 0, proto_len = 0;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|s", &url, &url_len, &proto, &proto_len) != SUCCESS) {
		RETURN_FALSE;
	}

	RETURN_STRING(http_absolute_uri(url, proto), 0);
}
/* }}} */

/* {{{ proto string http_negotiate_language(array supported[, string default = 'en-US'])
 *
 * This function negotiates the clients preferred language based on its
 * Accept-Language HTTP header.  It returns the negotiated language or
 * the default language if none match.
 *
 * The qualifier is recognized and languages without qualifier are rated highest.
 *
 * The supported parameter is expected to be an array having
 * the supported languages as array values.
 *
 * Example:
 * <pre>
 * <?php
 * $langs = array(
 * 		'en-US',// default
 * 		'fr',
 * 		'fr-FR',
 * 		'de',
 * 		'de-DE',
 * 		'de-AT',
 * 		'de-CH',
 * );
 * include './langs/'. http_negotiate_language($langs) .'.php';
 * ?>
 * </pre>
 *
 */
PHP_FUNCTION(http_negotiate_language)
{
	zval *supported;
	char *def = NULL;
	int def_len = 0;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "a|s", &supported, &def, &def_len) != SUCCESS) {
		RETURN_FALSE;
	}

	if (!def) {
		def = "en-US";
	}

	RETURN_STRING(http_negotiate_language(supported, def), 0);
}
/* }}} */

/* {{{ proto string http_negotiate_charset(array supported[, string default = 'iso-8859-1'])
 *
 * This function negotiates the clients preferred charset based on its
 * Accept-Charset HTTP header.  It returns the negotiated charset or
 * the default charset if none match.
 *
 * The qualifier is recognized and charset without qualifier are rated highest.
 *
 * The supported parameter is expected to be an array having
 * the supported charsets as array values.
 *
 * Example:
 * <pre>
 * <?php
 * $charsets = array(
 * 		'iso-8859-1', // default
 * 		'iso-8859-2',
 * 		'iso-8859-15',
 * 		'utf-8'
 * );
 * $pref = http_negotiate_charset($charsets);
 * if (!strcmp($pref, 'iso-8859-1')) {
 * 		iconv_set_encoding('internal_encoding', 'iso-8859-1');
 * 		iconv_set_encoding('output_encoding', $pref);
 * 		ob_start('ob_iconv_handler');
 * }
 * ?>
 * </pre>
 */
PHP_FUNCTION(http_negotiate_charset)
{
	zval *supported;
	char *def = NULL;
	int def_len = 0;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "a|s", &supported, &def, &def_len) != SUCCESS) {
		RETURN_FALSE;
	}

	if (!def) {
		def = "iso-8859-1";
	}

	RETURN_STRING(http_negotiate_charset(supported, def), 0);
}
/* }}} */

/* {{{ proto bool http_send_status(int status)
 *
 * Send HTTP status code.
 *
 */
PHP_FUNCTION(http_send_status)
{
	int status = 0;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &status) != SUCCESS) {
		RETURN_FALSE;
	}
	if (status < 100 || status > 510) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Invalid HTTP status code (100-510): %d", status);
		RETURN_FALSE;
	}

	RETURN_SUCCESS(http_send_status(status));
}
/* }}} */

/* {{{ proto bool http_send_last_modified([int timestamp])
 *
 * This converts the given timestamp to a valid HTTP date and
 * sends it as "Last-Modified" HTTP header.  If timestamp is
 * omitted, current time is sent.
 *
 */
PHP_FUNCTION(http_send_last_modified)
{
	long t = -1;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|l", &t) != SUCCESS) {
		RETURN_FALSE;
	}

	if (t == -1) {
		t = (long) time(NULL);
	}

	RETURN_SUCCESS(http_send_last_modified(t));
}
/* }}} */

/* {{{ proto bool http_match_modified([int timestamp])
 *
 * Matches the given timestamp against the clients "If-Modified-Since" resp.
 * "If-Unmodified-Since" HTTP headers.
 *
 */
PHP_FUNCTION(http_match_modified)
{
	long t = -1;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|l", &t) != SUCCESS) {
		RETURN_FALSE;
	}

	// current time if not supplied (senseless though)
	if (t == -1) {
		t = (long) time(NULL);
	}

	RETURN_BOOL(http_modified_match("HTTP_IF_MODIFIED_SINCE", t) || http_modified_match("HTTP_IF_UNMODIFIED_SINCE", t));
}
/* }}} */

/* {{{ proto bool http_match_etag(string etag)
 *
 * This matches the given ETag against the clients
 * "If-Match" resp. "If-None-Match" HTTP headers.
 *
 */
PHP_FUNCTION(http_match_etag)
{
	int etag_len;
	char *etag;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &etag, &etag_len) != SUCCESS) {
		RETURN_FALSE;
	}

	RETURN_BOOL(http_etag_match("HTTP_IF_NONE_MATCH", etag) || http_etag_match("HTTP_IF_MATCH", etag));
}
/* }}} */

/* {{{ proto bool http_cache_last_modified([int timestamp_or_expires]])
 *
 * If timestamp_or_exires is greater than 0, it is handled as timestamp
 * and will be sent as date of last modification.  If it is 0 or omitted,
 * the current time will be sent as Last-Modified date.  If it's negative,
 * it is handled as expiration time in seconds, which means that if the
 * requested last modification date is not between the calculated timespan,
 * the Last-Modified header is updated and the actual body will be sent.
 *
 */
PHP_FUNCTION(http_cache_last_modified)
{
	long last_modified = 0, send_modified = 0, t;
	zval *zlm;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|l", &last_modified) != SUCCESS) {
		RETURN_FALSE;
	}

	t = (long) time(NULL);

	/* 0 or omitted */
	if (!last_modified) {
		/* does the client have? (att: caching "forever") */
		if (zlm = http_get_server_var("HTTP_IF_MODIFIED_SINCE")) {
			last_modified = send_modified = http_parse_date(Z_STRVAL_P(zlm));
		/* send current time */
		} else {
			send_modified = t;
		}
	/* negative value is supposed to be expiration time */
	} else if (last_modified < 0) {
		last_modified += t;
		send_modified  = t;
	/* send supplied time explicitly */
	} else {
		send_modified = last_modified;
	}

	http_send_header("Cache-Control: private, must-revalidate, max-age=0");

	if (http_modified_match("HTTP_IF_MODIFIED_SINCE", last_modified)) {
		if (SUCCESS == http_send_status(304)) {
			zend_bailout();
		} else {
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "Could not send 304 Not Modified");
			RETURN_FALSE;
		}
	}
	RETURN_SUCCESS(http_send_last_modified(send_modified));
}
/* }}} */

/* {{{ proto bool http_cache_etag([string etag])
 *
 * This function attempts to cache the HTTP body based on an ETag,
 * either supplied or generated through calculation of the MD5
 * checksum of the output (uses output buffering).
 *
 * If clients "If-None-Match" header matches the supplied/calculated
 * ETag, the body is considered cached on the clients side and
 * a "304 Not Modified" status code is issued.
 *
 */
PHP_FUNCTION(http_cache_etag)
{
	char *etag;
	int etag_len = 0;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|s", &etag, &etag_len) != SUCCESS) {
		RETURN_FALSE;
	}

	http_send_header("Cache-Control: private, must-revalidate, max-age=0");

	if (etag_len) {
		http_send_etag(etag, etag_len);
		if (http_etag_match("HTTP_IF_NONE_MATCH", etag)) {
			if (SUCCESS == http_send_status(304)) {
				zend_bailout();
			} else {
				php_error_docref(NULL TSRMLS_CC, E_WARNING, "Could not send 304 Not Modified");
				RETURN_FALSE;
			}
		}
	}

	/* if no etag is given and we didn't already start ob_etaghandler -- start it */
	if (!HTTP_G(etag_started)) {
		RETURN_BOOL(HTTP_G(etag_started) = (SUCCESS == http_start_ob_handler(_http_ob_etaghandler, "ob_etaghandler", 4096, 1)));
	}
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto string ob_httpetaghandler(string data, int mode)
 *
 * For use with ob_start().
 * Note that this has to be started as first output buffer.
 * WARNING: Don't use with http_send_*().
 */
PHP_FUNCTION(ob_httpetaghandler)
{
	char *data;
	int data_len;
	long mode;

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sl", &data, &data_len, &mode)) {
		RETURN_FALSE;
	}

	if (mode & PHP_OUTPUT_HANDLER_START) {
		if (HTTP_G(etag_started)) {
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "ob_httpetaghandler can only be used once");
			RETURN_STRINGL(data, data_len, 1);
		}
		http_send_header("Cache-Control: private, must-revalidate, max-age=0");
		HTTP_G(etag_started) = 1;
	}

    if (OG(ob_nesting_level) > 1) {
        php_error_docref(NULL TSRMLS_CC, E_WARNING, "ob_httpetaghandler must be started prior to other output buffers");
        RETURN_STRINGL(data, data_len, 1);
    }

	Z_TYPE_P(return_value) = IS_STRING;
	http_ob_etaghandler(data, data_len, &Z_STRVAL_P(return_value), &Z_STRLEN_P(return_value), mode);
}
/* }}} */

/* {{{ proto void http_redirect([string url[, array params[, bool session,[ bool permanent]]]])
 *
 * Redirect to a given url.
 * The supplied url will be expanded with http_absolute_uri(), the params array will
 * be treated with http_build_query() and the session identification will be appended
 * if session is true.
 *
 * Depending on permanent the redirection will be issued with a permanent
 * ("301 Moved Permanently") or a temporary ("302 Found") redirection
 * status code.
 *
 * To be RFC compliant, "Redirecting to <a>URI</a>." will be displayed,
 * if the client doesn't redirect immediatly.
 */
PHP_FUNCTION(http_redirect)
{
	int url_len;
	zend_bool session = 0, permanent = 0;
	zval *params = NULL;
	smart_str qstr = {0};
	char *url, *URI, LOC[HTTP_URI_MAXLEN + 9], RED[HTTP_URI_MAXLEN * 2 + 34];

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|sa!/bb", &url, &url_len, &params, &session, &permanent) != SUCCESS) {
		RETURN_FALSE;
	}

	/* append session info */
	if (session && (PS(session_status) == php_session_active)) {
		if (!params) {
			MAKE_STD_ZVAL(params);
			array_init(params);
		}
		if (add_assoc_string(params, PS(session_name), PS(id), 1) != SUCCESS) {
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "Could not append session information");
		}
	}

	/* treat params array with http_build_query() */
	if (params) {
		if (php_url_encode_hash_ex(Z_ARRVAL_P(params), &qstr, NULL,0,NULL,0,NULL,0,NULL TSRMLS_CC) != SUCCESS) {
			if (qstr.c) {
				efree(qstr.c);
			}
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "Could not encode query parameters");
			RETURN_FALSE;
		}
		smart_str_0(&qstr);
	}

	URI = http_absolute_uri(url, NULL);
	if (qstr.c) {
		snprintf(LOC, HTTP_URI_MAXLEN + strlen("Location: "), "Location: %s?%s", URI, qstr.c);
		sprintf(RED, "Redirecting to <a href=\"%s?%s\">%s?%s</a>.\n", URI, qstr.c, URI, qstr.c);
		efree(qstr.c);
	} else {
		snprintf(LOC, HTTP_URI_MAXLEN + strlen("Location: "), "Location: %s", URI);
		sprintf(RED, "Redirecting to <a href=\"%s\">%s</a>.\n", URI, URI);
	}
	efree(URI);

	if ((SUCCESS == http_send_header(LOC)) && (SUCCESS == http_send_status((permanent ? 301 : 302)))) {
		php_body_write(RED, strlen(RED) TSRMLS_CC);
		RETURN_TRUE;
	}
	RETURN_FALSE;
}
/* }}} */

/* {{{ proto bool http_send_data(string data)
 *
 * Sends raw data with support for (multiple) range requests.
 *
 */
PHP_FUNCTION(http_send_data)
{
	zval *zdata;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &zdata) != SUCCESS) {
		RETURN_FALSE;
	}

	convert_to_string_ex(&zdata);
	http_send_header("Accept-Ranges: bytes");
	RETURN_SUCCESS(http_send_data(zdata));
}
/* }}} */

/* {{{ proto bool http_send_file(string file)
 *
 * Sends a file with support for (multiple) range requests.
 *
 */
PHP_FUNCTION(http_send_file)
{
	zval *zfile;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &zfile) != SUCCESS) {
		RETURN_FALSE;
	}

	convert_to_string_ex(&zfile);
	http_send_header("Accept-Ranges: bytes");
	RETURN_SUCCESS(http_send_file(zfile));
}
/* }}} */

/* {{{ proto bool http_send_stream(resource stream)
 *
 * Sends an already opened stream with support for (multiple) range requests.
 *
 */
PHP_FUNCTION(http_send_stream)
{
	zval *zstream;
	php_stream *file;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r", &zstream) != SUCCESS) {
		RETURN_FALSE;
	}

	php_stream_from_zval(file, &zstream);
	http_send_header("Accept-Ranges: bytes");
	RETURN_SUCCESS(http_send_stream(file));
}
/* }}} */

/* {{{ proto bool http_content_type([string content_type = 'application/x-octetstream'])
 *
 * Sets the content type.
 *
 */
PHP_FUNCTION(http_content_type)
{
	char *ct, *content_type;
	int ct_len = 0;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|s", &ct, &ct_len) != SUCCESS) {
		RETURN_FALSE;
	}

	if (!ct_len) {
		RETURN_SUCCESS(http_send_header("Content-Type: application/x-octetstream"));
	}

	/* remember for multiple ranges */
	if (HTTP_G(ctype)) {
		efree(HTTP_G(ctype));
	}
	HTTP_G(ctype) = estrndup(ct, ct_len);

	content_type = (char *) emalloc(strlen("Content-Type: ") + ct_len + 1);
	sprintf(content_type, "Content-Type: %s", ct);

	RETVAL_BOOL(SUCCESS == http_send_header(content_type));
	efree(content_type);
}
/* }}} */

/* {{{ proto bool http_content_disposition(string filename[, bool inline = false])
 *
 * Set the Content Disposition.  The Content-Disposition header is very useful
 * if the data actually sent came from a file or something similar, that should
 * be "saved" by the client/user (i.e. by browsers "Save as..." popup window).
 *
 */
PHP_FUNCTION(http_content_disposition)
{
	char *filename, *header;
	int f_len;
	zend_bool send_inline = 0;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|b", &filename, &f_len, &send_inline) != SUCCESS) {
		RETURN_FALSE;
	}

	if (send_inline) {
		header = (char *) emalloc(strlen("Content-Disposition: inline; filename=\"\"") + f_len + 1);
		sprintf(header, "Content-Disposition: inline; filename=\"%s\"", filename);
	} else {
		header = (char *) emalloc(strlen("Content-Disposition: attachment; filename=\"\"") + f_len + 1);
		sprintf(header, "Content-Disposition: attachment; filename=\"%s\"", filename);
	}

	RETVAL_BOOL(SUCCESS == http_send_header(header));
	efree(header);
}
/* }}} */

/* {{{ proto string http_chunked_decode(string encoded)
 *
 * This function decodes a string that was HTTP-chunked encoded.
 * Returns false on failure.
 */
PHP_FUNCTION(http_chunked_decode)
{
	char *encoded = NULL, *decoded = NULL;
	int encoded_len = 0, decoded_len = 0;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &encoded, &encoded_len) != SUCCESS) {
		RETURN_FALSE;
	}

	if (SUCCESS == http_chunked_decode(encoded, encoded_len, &decoded, &decoded_len)) {
		RETURN_STRINGL(decoded, decoded_len, 0);
	} else {
		RETURN_FALSE;
	}
}
/* }}} */

/* {{{ proto array http_split_response(string http_response)
 *
 * This function splits an HTTP response into an array with headers and the
 * content body. The returned array may look simliar to the following example:
 *
 * <pre>
 * <?php
 * array(
 *     0 => array(
 *         'Status' => '200 Ok',
 *         'Content-Type' => 'text/plain',
 *         'Content-Language' => 'en-US'
 *     ),
 *     1 => "Hello World!"
 * );
 * ?>
 * </pre>
 */
PHP_FUNCTION(http_split_response)
{
	zval *zresponse, *zbody, *zheaders;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &zresponse) != SUCCESS) {
		RETURN_FALSE;
	}

	convert_to_string_ex(&zresponse);

	MAKE_STD_ZVAL(zbody);
	MAKE_STD_ZVAL(zheaders);
	array_init(zheaders);

	if (SUCCESS != http_split_response(zresponse, zheaders, zbody)) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Could not parse HTTP response");
		RETURN_FALSE;
	}

	array_init(return_value);
	add_index_zval(return_value, 0, zheaders);
	add_index_zval(return_value, 1, zbody);
}
/* }}} */

/* {{{ proto array http_parse_headers(string header)
 *
 */
PHP_FUNCTION(http_parse_headers)
{
	char *header, *rnrn;
	int header_len;

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &header, &header_len)) {
		RETURN_FALSE;
	}

	array_init(return_value);

	if (rnrn = strstr(header, HTTP_CRLF HTTP_CRLF)) {
		header_len = rnrn - header + 2;
	}
	if (SUCCESS != http_parse_headers(header, header_len, return_value)) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Could not parse HTTP header");
		zval_dtor(return_value);
		RETURN_FALSE;
	}
}
/* }}}*/

/* {{{ proto array http_get_request_headers(void)
 *
 */
PHP_FUNCTION(http_get_request_headers)
{
	if (ZEND_NUM_ARGS()) {
		WRONG_PARAM_COUNT;
	}

	array_init(return_value);
	http_get_request_headers(return_value);
}
/* }}} */

/* {{{ HAVE_CURL */
#ifdef HTTP_HAVE_CURL

/* {{{ proto string http_get(string url[, array options[, array &info]])
 *
 * Performs an HTTP GET request on the supplied url.
 *
 * The second parameter is expected to be an associative
 * array where the following keys will be recognized:
 * <pre>
 *  - redirect:         int, whether and how many redirects to follow
 *  - unrestrictedauth: bool, whether to continue sending credentials on
 *                      redirects to a different host
 *  - proxyhost:        string, proxy host in "host[:port]" format
 *  - proxyport:        int, use another proxy port as specified in proxyhost
 *  - proxyauth:        string, proxy credentials in "user:pass" format
 *  - proxyauthtype:    int, HTTP_AUTH_BASIC and/or HTTP_AUTH_NTLM
 *  - httpauth:         string, http credentials in "user:pass" format
 *  - httpauthtype:     int, HTTP_AUTH_BASIC, DIGEST and/or NTLM
 *  - compress:         bool, whether to allow gzip/deflate content encoding
 *                      (defaults to true)
 *  - port:             int, use another port as specified in the url
 *  - referer:          string, the referer to sends
 *  - useragent:        string, the user agent to send
 *                      (defaults to PECL::HTTP/version (PHP/version)))
 *  - headers:          array, list of custom headers as associative array
 *                      like array("header" => "value")
 *  - cookies:          array, list of cookies as associative array
 *                      like array("cookie" => "value")
 *  - cookiestore:      string, path to a file where cookies are/will be stored
 * </pre>
 *
 * The optional third parameter will be filled with some additional information
 * in form af an associative array, if supplied, like the following example:
 * <pre>
 * <?php
 * array (
 *     'effective_url' => 'http://localhost',
 *     'response_code' => 403,
 *     'total_time' => 0.017,
 *     'namelookup_time' => 0.013,
 *     'connect_time' => 0.014,
 *     'pretransfer_time' => 0.014,
 *     'size_upload' => 0,
 *     'size_download' => 202,
 *     'speed_download' => 11882,
 *     'speed_upload' => 0,
 *     'header_size' => 145,
 *     'request_size' => 62,
 *     'ssl_verifyresult' => 0,
 *     'filetime' => -1,
 *     'content_length_download' => 202,
 *     'content_length_upload' => 0,
 *     'starttransfer_time' => 0.017,
 *     'content_type' => 'text/html; charset=iso-8859-1',
 *     'redirect_time' => 0,
 *     'redirect_count' => 0,
 *     'private' => '',
 *     'http_connectcode' => 0,
 *     'httpauth_avail' => 0,
 *     'proxyauth_avail' => 0,
 * )
 * ?>
 * </pre>
 */
PHP_FUNCTION(http_get)
{
	char *URL, *data = NULL;
	size_t data_len = 0;
	int URL_len;
	zval *options = NULL, *info = NULL;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|a/!z", &URL, &URL_len, &options, &info) != SUCCESS) {
		RETURN_FALSE;
	}

	if (info) {
		zval_dtor(info);
		array_init(info);
	}

	if (SUCCESS == http_get(URL, HASH_ORNULL(options), HASH_ORNULL(info), &data, &data_len)) {
		RETURN_STRINGL(data, data_len, 0);
	} else {
		RETURN_FALSE;
	}
}
/* }}} */

/* {{{ proto string http_head(string url[, array options[, array &info]])
 *
 * Performs an HTTP HEAD request on the suppied url.
 * Returns the HTTP response as string.
 * See http_get() for a full list of available options.
 */
PHP_FUNCTION(http_head)
{
	char *URL, *data = NULL;
	size_t data_len = 0;
	int URL_len;
	zval *options = NULL, *info = NULL;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|a/!z", &URL, &URL_len, &options, &info) != SUCCESS) {
		RETURN_FALSE;
	}

	if (info) {
		zval_dtor(info);
		array_init(info);
	}

	if (SUCCESS == http_head(URL, HASH_ORNULL(options), HASH_ORNULL(info), &data, &data_len)) {
		RETURN_STRINGL(data, data_len, 0);
	} else {
		RETURN_FALSE;
	}
}
/* }}} */

/* {{{ proto string http_post_data(string url, string data[, array options[, &info]])
 *
 * Performs an HTTP POST request, posting data.
 * Returns the HTTP response as string.
 * See http_get() for a full list of available options.
 */
PHP_FUNCTION(http_post_data)
{
	char *URL, *postdata, *data = NULL;
	size_t data_len = 0;
	int postdata_len, URL_len;
	zval *options = NULL, *info = NULL;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ss|a/!z", &URL, &URL_len, &postdata, &postdata_len, &options, &info) != SUCCESS) {
		RETURN_FALSE;
	}

	if (info) {
		zval_dtor(info);
		array_init(info);
	}

	if (SUCCESS == http_post_data(URL, postdata, (size_t) postdata_len, HASH_ORNULL(options), HASH_ORNULL(info), &data, &data_len)) {
		RETURN_STRINGL(data, data_len, 0);
	} else {
		RETURN_FALSE;
	}
}
/* }}} */

/* {{{ proto string http_post_array(string url, array data[, array options[, array &info]])
 *
 * Performs an HTTP POST request, posting www-form-urlencoded array data.
 * Returns the HTTP response as string.
 * See http_get() for a full list of available options.
 */
PHP_FUNCTION(http_post_array)
{
	char *URL, *data = NULL;
	size_t data_len = 0;
	int URL_len;
	zval *options = NULL, *info = NULL, *postdata;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sa|a/!z", &URL, &URL_len, &postdata, &options, &info) != SUCCESS) {
		RETURN_FALSE;
	}

	if (info) {
		zval_dtor(info);
		array_init(info);
	}

	if (SUCCESS == http_post_array(URL, Z_ARRVAL_P(postdata), HASH_ORNULL(options), HASH_ORNULL(info), &data, &data_len)) {
		RETURN_STRINGL(data, data_len, 0);
	} else {
		RETURN_FALSE;
	}
}
/* }}} */

#endif
/* }}} HAVE_CURL */


/* {{{ proto bool http_auth_basic(string user, string pass[, string realm = "Restricted"])
 *
 * Example:
 * <pre>
 * <?php
 * if (!http_auth_basic('mike', 's3c|r3t')) {
 *     die('<h1>Authorization failed!</h1>');
 * }
 * ?>
 * </pre>
 */
PHP_FUNCTION(http_auth_basic)
{
	char *realm = NULL, *user, *pass, *suser, *spass;
	int r_len, u_len, p_len;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ss|s", &user, &u_len, &pass, &p_len, &realm, &r_len) != SUCCESS) {
		RETURN_FALSE;
	}

	if (!realm) {
		realm = "Restricted";
	}

	if (SUCCESS != http_auth_credentials(&suser, &spass)) {
		http_auth_header("Basic", realm);
		RETURN_FALSE;
	}

	if (strcasecmp(suser, user)) {
		http_auth_header("Basic", realm);
		RETURN_FALSE;
	}

	if (strcmp(spass, pass)) {
		http_auth_header("Basic", realm);
		RETURN_FALSE;
	}

	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool http_auth_basic_cb(mixed callback[, string realm = "Restricted"])
 *
 * Example:
 * <pre>
 * <?php
 * function auth_cb($user, $pass)
 * {
 *     global $db;
 *     $query = 'SELECT pass FROM users WHERE user='. $db->quoteSmart($user);
 *     if (strlen($realpass = $db->getOne($query)) {
 *         return $pass === $realpass;
 *     }
 *     return false;
 * }
 *
 * if (!http_auth_basic_cb('auth_cb')) {
 *     die('<h1>Authorization failed</h1>');
 * }
 * ?>
 * </pre>
 */
PHP_FUNCTION(http_auth_basic_cb)
{
	zval *cb;
	char *realm = NULL, *user, *pass;
	int r_len;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z|s", &cb, &realm, &r_len) != SUCCESS) {
		RETURN_FALSE;
	}

	if (!realm) {
		realm = "Restricted";
	}

	if (SUCCESS != http_auth_credentials(&user, &pass)) {
		http_auth_header("Basic", realm);
		RETURN_FALSE;
	}
	{
		zval *zparams[2] = {NULL, NULL}, retval;
		int result = 0;

		MAKE_STD_ZVAL(zparams[0]);
		MAKE_STD_ZVAL(zparams[1]);
		ZVAL_STRING(zparams[0], user, 0);
		ZVAL_STRING(zparams[1], pass, 0);

		if (SUCCESS == call_user_function(EG(function_table), NULL, cb,
				&retval, 2, zparams TSRMLS_CC)) {
			result = Z_LVAL(retval);
		}

		efree(user);
		efree(pass);
		efree(zparams[0]);
		efree(zparams[1]);

		if (!result) {
			http_auth_header("Basic", realm);
		}

		RETURN_BOOL(result);
	}
}
/* }}}*/


/* {{{ php_http_init_globals(zend_http_globals *) */
static void php_http_init_globals(zend_http_globals *http_globals)
{
	http_globals->etag_started = 0;
	http_globals->ctype = NULL;
	http_globals->etag  = NULL;
	http_globals->lmod  = 0;
#ifdef HTTP_HAVE_CURL
	http_globals->curlbuf.body.data = NULL;
	http_globals->curlbuf.body.used = 0;
	http_globals->curlbuf.body.free = 0;
	http_globals->curlbuf.hdrs.data = NULL;
	http_globals->curlbuf.hdrs.used = 0;
	http_globals->curlbuf.hdrs.free = 0;
#endif
	http_globals->allowed_methods = NULL;
}
/* }}} */

/* {{{ static inline STATUS http_check_allowed_methods(char *, int) */
#define http_check_allowed_methods(m, l) _http_check_allowed_methods((m), (l) TSRMLS_CC)
static inline void _http_check_allowed_methods(char *methods, int length TSRMLS_DC)
{
	if (length && SG(request_info).request_method && (!strstr(methods, SG(request_info).request_method))) {
		char *allow_header = emalloc(length + sizeof("Allow: "));
		sprintf(allow_header, "Allow: %s", methods);
		http_send_header(allow_header);
		efree(allow_header);
		http_send_status(405);
		zend_bailout();
	}
}
/* }}} */

/* {{{ PHP_INI */
PHP_INI_MH(update_allowed_methods)
{
	http_check_allowed_methods(new_value, new_value_length);
	return OnUpdateString(entry, new_value, new_value_length, mh_arg1, mh_arg2, mh_arg3, stage TSRMLS_CC);
}

PHP_INI_BEGIN()
	STD_PHP_INI_ENTRY("http.allowed_methods", "OPTIONS,GET,HEAD,POST,PUT,DELETE,TRACE,CONNECT", PHP_INI_ALL, update_allowed_methods, allowed_methods, zend_http_globals, http_globals)
PHP_INI_END()
/* }}} */

/* {{{ PHP_MINIT_FUNCTION */
PHP_MINIT_FUNCTION(http)
{
	ZEND_INIT_MODULE_GLOBALS(http, php_http_init_globals, NULL);
	REGISTER_INI_ENTRIES();

#ifdef HTTP_HAVE_CURL
	REGISTER_LONG_CONSTANT("HTTP_AUTH_BASIC", CURLAUTH_BASIC, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("HTTP_AUTH_DIGEST", CURLAUTH_DIGEST, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("HTTP_AUTH_NTLM", CURLAUTH_NTLM, CONST_CS | CONST_PERSISTENT);
#endif

#ifdef ZEND_ENGINE_2
	/*
	REGISTER_LONG_CONSTANT("HTTP_GET", HTTP_GET, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("HTTP_HEAD", HTTP_HEAD, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("HTTP_POST", HTTP_POST, CONST_CS | CONST_PERSISTENT);
	*/
	HTTP_REGISTER_CLASS(HTTPi, httpi, NULL, ZEND_ACC_FINAL_CLASS);
	HTTP_REGISTER_CLASS_EX(HTTPi_Response, httpi_response, NULL, 0);
#endif
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MSHUTDOWN_FUNCTION */
PHP_MSHUTDOWN_FUNCTION(http)
{
	UNREGISTER_INI_ENTRIES();
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_RINIT_FUNCTION */
PHP_RINIT_FUNCTION(http)
{
	char *allowed_methods = INI_STR("http.allowed_methods");
	http_check_allowed_methods(allowed_methods, strlen(allowed_methods));
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_RSHUTDOWN_FUNCTION */
PHP_RSHUTDOWN_FUNCTION(http)
{
	HTTP_G(etag_started) = 0;
	HTTP_G(lmod) = 0;

	if (HTTP_G(etag)) {
		efree(HTTP_G(etag));
		HTTP_G(etag) = NULL;
	}

	if (HTTP_G(ctype)) {
		efree(HTTP_G(ctype));
		HTTP_G(ctype) = NULL;
	}
#ifdef HTTP_HAVE_CURL
	if (HTTP_G(curlbuf).body.data) {
		efree(HTTP_G(curlbuf).body.data);
		HTTP_G(curlbuf).body.data = NULL;
	}
	if (HTTP_G(curlbuf).hdrs.data) {
		efree(HTTP_G(curlbuf).hdrs.data);
		HTTP_G(curlbuf).hdrs.data = NULL;
	}
#endif
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MINFO_FUNCTION */
PHP_MINFO_FUNCTION(http)
{
	php_info_print_table_start();
	php_info_print_table_header(2, "Extended HTTP support", "enabled");
	php_info_print_table_row(2, "Version:", PHP_EXT_HTTP_VERSION);
	php_info_print_table_row(2, "cURL convenience functions:",
#ifdef HTTP_HAVE_CURL
			"enabled"
#else
			"disabled"
#endif
	);
	php_info_print_table_end();

	DISPLAY_INI_ENTRIES();
}
/* }}} */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

