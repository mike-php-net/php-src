/*
    +--------------------------------------------------------------------+
    | PECL :: http                                                       |
    +--------------------------------------------------------------------+
    | Redistribution and use in source and binary forms, with or without |
    | modification, are permitted provided that the conditions mentioned |
    | in the accompanying LICENSE file are met.                          |
    +--------------------------------------------------------------------+
    | Copyright (c) 2004-2005, Michael Wallner <mike@php.net>            |
    +--------------------------------------------------------------------+
*/

/* $Id$ */

#ifndef PHP_HTTP_URL_API_H
#define PHP_HTTP_URL_API_H

#include "php_http_std_defs.h"

#define http_absolute_uri(url) http_absolute_url(url)
#define http_absolute_uri_ex(url, url_len, proto, proto_len, host, host_len, port) _http_absolute_url_ex((url), (url_len), (proto), (proto_len), (host), (host_len), (port) TSRMLS_CC)
#define http_absolute_url(url) http_absolute_url_ex((url), strlen(url), NULL, 0, NULL, 0, 0)
#define http_absolute_url_ex(url, url_len, proto, proto_len, host, host_len, port) _http_absolute_url_ex((url), (url_len), (proto), (proto_len), (host), (host_len), (port) TSRMLS_CC)
PHP_HTTP_API char *_http_absolute_url_ex(const char *url, size_t url_len, const char *proto, size_t proto_len, const char *host, size_t host_len, unsigned port TSRMLS_DC);

#define http_urlencode_hash(h, q) _http_urlencode_hash_ex((h), 1, NULL, 0, (q), NULL TSRMLS_CC)
#define http_urlencode_hash_ex(h, o, p, pl, q, ql) _http_urlencode_hash_ex((h), (o), (p), (pl), (q), (ql) TSRMLS_CC)
PHP_HTTP_API STATUS _http_urlencode_hash_ex(HashTable *hash, zend_bool override_argsep, char *pre_encoded_data, size_t pre_encoded_len, char **encoded_data, size_t *encoded_len TSRMLS_DC);

#define http_urlencode_hash_recursive(ht, s, as, al, pr, pl) _http_urlencode_hash_recursive((ht), (s), (as), (al), (pr), (pl) TSRMLS_CC)
PHP_HTTP_API STATUS _http_urlencode_hash_recursive(HashTable *ht, phpstr *str, const char *arg_sep, size_t arg_sep_len, const char *prefix, size_t prefix_len TSRMLS_DC);

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

