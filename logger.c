/*
  +----------------------------------------------------------------------+
  | PHP Version 5                                                        |
  +----------------------------------------------------------------------+
  | Copyright (c) 1997-2013 The PHP Group                                |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author:                                                              |
  +----------------------------------------------------------------------+
*/

/* $Id$ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "php_logger.h"
#include "time.h"
#include "stdio.h"

/* If you declare any globals in php_logger.h uncomment this:
ZEND_DECLARE_MODULE_GLOBALS(logger)
*/

/* True global resources - no need for thread safety here */
static int le_logger;

/* {{{ logger_functions[]
 *
 * Every user visible function must have an entry in logger_functions[].
 */
const zend_function_entry logger_functions[] = {
	PHP_FE(confirm_logger_compiled,	NULL)		/* For testing, remove later. */
	PHP_FE_END	/* Must be the last line in logger_functions[] */
};
/* }}} */

/* {{{ logger_module_entry
 */
zend_module_entry logger_module_entry = {
#if ZEND_MODULE_API_NO >= 20010901
	STANDARD_MODULE_HEADER,
#endif
	"logger",
	logger_functions,
	PHP_MINIT(logger),
	PHP_MSHUTDOWN(logger),
	PHP_RINIT(logger),		/* Replace with NULL if there's nothing to do at request start */
	PHP_RSHUTDOWN(logger),	/* Replace with NULL if there's nothing to do at request end */
	PHP_MINFO(logger),
#if ZEND_MODULE_API_NO >= 20010901
	"0.1", /* Replace with version number for your extension */
#endif
	STANDARD_MODULE_PROPERTIES
};
/* }}} */

#ifdef COMPILE_DL_LOGGER
ZEND_GET_MODULE(logger)
#endif

/* {{{ PHP_INI
 */
/* Remove comments and fill if you need to have entries in php.ini
PHP_INI_BEGIN()
    STD_PHP_INI_ENTRY("logger.global_value",      "42", PHP_INI_ALL, OnUpdateLong, global_value, zend_logger_globals, logger_globals)
    STD_PHP_INI_ENTRY("logger.global_string", "foobar", PHP_INI_ALL, OnUpdateString, global_string, zend_logger_globals, logger_globals)
PHP_INI_END()
*/
/* }}} */

/* {{{ php_logger_init_globals
 */
/* Uncomment this function if you have INI entries
static void php_logger_init_globals(zend_logger_globals *logger_globals)
{
	logger_globals->global_value = 0;
	logger_globals->global_string = NULL;
}
*/
/* }}} */
ZEND_BEGIN_ARG_INFO_EX(arginfo_logger_callStatic, 0, 0, 2)
        ZEND_ARG_INFO(0, method)
        ZEND_ARG_ARRAY_INFO(0, argv, 1)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_logger_init, 0, 0 ,2)
        ZEND_ARG_INFO(0, app)
        ZEND_ARG_INFO(0, path)
ZEND_END_ARG_INFO()


zend_class_entry *logger_ce;

int logger_mkdir(char *path)
{
        char *tmp, *pos;
        int  sLen, i;
        tmp = estrdup(path);
        pos = tmp;
        sLen = strlen(path);
        if (strncmp(pos, "/", 1) == 0) {
                pos += 1;
        } else if(strncmp(pos, "./", 2) == 0) {
                pos += 2;
        }
        for (; *pos != '\0'; ++pos) {
                if (*pos == '/') {
                        *pos = '\0';
                        
                        if (access(tmp, F_OK) == 0) {
                                *pos = '/';
                                continue;
                        }
                        
                        if (mkdir(tmp, 0755) != 0) {
                                free(tmp);
                                return -1;
                        }
                        *pos = '/';
                }
        }
        if (*(pos - 1) != "/" && access(tmp, F_OK) != 0) {
                if (mkdir(tmp, 0755) != 0) {
                        free(tmp);
                        return -1;
                }
        }
        free(tmp);                                                                                                                                                     
        return 0;
}
char * logger_get_server(char *type, uint len)
{
        char *url;
        zval **carrier = NULL, **ret;
#if (PHP_MAJOR_VERSION == 5) && (PHP_MINOR_VERSION < 4)
        zend_bool       jit_initialization = (PG(auto_globals_jit) && !PG(register_globals) && !PG(register_long_arrays));
#else
        zend_bool       jit_initialization = PG(auto_globals_jit);
#endif
        if (jit_initialization) {
                zend_is_auto_global(ZEND_STRL("_SERVER") TSRMLS_CC);
        }
        //(void)zend_hash_find(&EG(symbol_table), ZEND_STRS("_SERVER"), (void **)&carrier);
        carrier = &PG(http_globals)[TRACK_VARS_SERVER];
        if (!carrier || !(*carrier)) {
                return "";
        }
        //php_printf("sizeof : %d, str : %s", sizeof(type), type);
        if (zend_hash_find(Z_ARRVAL_PP(carrier), type, len + 1, (void **)&ret) == FAILURE) {
                return "";
        }
        return Z_STRVAL_PP(ret);
}

char * logger_get_method()
{
        return logger_get_server(ZEND_STRL("REQUEST_METHOD"));
}

char * logger_get_url()
{
        return logger_get_server(ZEND_STRL("REQUEST_URI"));
}

char * logger_get_file_name()
{
        char *filename;
        if (zend_is_compiling(TSRMLS_C)) {   
                filename = zend_get_compiled_filename(TSRMLS_C);
        } else if (zend_is_executing(TSRMLS_C)) {
                       filename = zend_get_executed_filename(TSRMLS_C);
        } else {
                filename = NULL;
        }
        return filename;
}

long get_memery_usage()
{
        zval *memoryusage, *funname;
        MAKE_STD_ZVAL(funname);  
        ZVAL_STRING(funname, "memory_get_usage", 1);
        call_user_function_ex(EG(function_table), NULL, funname, &memoryusage, 0, NULL, 0, NULL TSRMLS_CC);
        return Z_LVAL_PP(&memoryusage);
}
int write_logger(char* level, char *content)
{
        zval *app, *path;
        char *allPath, *date[10];
        FILE *fp;
        app  = zend_read_static_property(logger_ce, ZEND_STRL("app"), 1 TSRMLS_CC);
        path = zend_read_static_property(logger_ce, ZEND_STRL("path"), 1 TSRMLS_CC);
        if (Z_LVAL_PP(&app) == NULL || Z_LVAL_PP(&path) == NULL || content == NULL) {
                return 0;
        }
        
        time_t rawtime;
        struct tm * timeinfo;
        time(&rawtime);
        timeinfo = localtime(&rawtime);
        timeinfo->tm_year += 1900;
        timeinfo->tm_mon  += 1;
        allPath = emalloc(strlen(Z_STRVAL_PP(&path) +strlen(Z_STRVAL_PP(&app))+ 14 + 1));
        sprintf(allPath, "%s/%s/log_%d%d%d.log",Z_STRVAL_PP(&path), Z_STRVAL_PP(&app),timeinfo->tm_year, timeinfo->tm_mon, timeinfo->tm_mday);
        /*
        if (access(allPath, 0) == 0 && access(allPath, 6) == -1){
                efree(allPath);
                return 0;
        }*/
        //php_printf("asaswe34qqqqqqqqqqqq path:%s", allPath); 
        if ((fp = fopen(allPath, "a")) == NULL){
                efree(allPath);
                return 0;
        }
        sprintf(date, "%d-%d-%d %d:%d:%d", timeinfo->tm_year, timeinfo->tm_mon, timeinfo->tm_mday, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
        fprintf(fp, "[%s][%s][%s][%d][%s %s] %s\n",level, date, logger_get_file_name(), get_memery_usage(),logger_get_method(), logger_get_url(), content);
        efree(allPath);
        fclose(fp);
        return 1;
}

ZEND_METHOD(logger, __callStatic)
{
        char *method;
        int  method_len;
        zval *argv, **v, *funname, *logstr;
        HashTable *argvHt;
        zval **param[2];
        if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sa",&method, &method_len, &argv) == FAILURE) {
                RETURN_NULL();
        }
        argvHt = Z_ARRVAL_P(argv);
        if (zend_hash_index_find(argvHt, 0, (void **) &v) == SUCCESS)
        {
                convert_to_string(*v);
                zend_hash_index_del(argvHt, 0);
        }
        MAKE_STD_ZVAL(funname);  
        ZVAL_STRING(funname, "vsprintf", 1);
        param[0] = v;
        param[1] = &argv;
        call_user_function_ex(EG(function_table), NULL, funname, &logstr, 2, param, 0, NULL TSRMLS_CC);
        write_logger(method, Z_STRVAL_PP(&logstr));
        return;
}

ZEND_METHOD(logger, init)
{
        char *app, *path;
        int appLen, pathLen;
        zval *logApp, *logPath;
        if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ss",&app, &appLen, &path, &pathLen) == FAILURE) {
                RETURN_NULL();
        }
        zend_update_static_property_string(logger_ce, ZEND_STRL("app"), app TSRMLS_CC);
        zend_update_static_property_string(logger_ce, ZEND_STRL("path"), path TSRMLS_CC);
        return;
}


static zend_function_entry logger_method[]=
{
        ZEND_ME(logger, __callStatic , arginfo_logger_callStatic, ZEND_ACC_STATIC|ZEND_ACC_PUBLIC|ZEND_ACC_CALL_VIA_HANDLER)
        ZEND_ME(logger, init, arginfo_logger_init, ZEND_ACC_STATIC|ZEND_ACC_PUBLIC)
        {NULL, NULL, NULL}
};

/* {{{ PHP_MINIT_FUNCTION
 */
PHP_MINIT_FUNCTION(logger)
{
	/* If you have INI entries, uncomment these lines 
	REGISTER_INI_ENTRIES();
	*/
        zend_class_entry ce;
        INIT_CLASS_ENTRY(ce, "logger", logger_method);
        logger_ce = zend_register_internal_class(&ce TSRMLS_CC);
        zend_declare_property_null(logger_ce,  ZEND_STRL("app"), ZEND_ACC_STATIC|ZEND_ACC_PUBLIC TSRMLS_CC);
        zend_declare_property_null(logger_ce,  ZEND_STRL("path"), ZEND_ACC_STATIC|ZEND_ACC_PUBLIC TSRMLS_CC);
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MSHUTDOWN_FUNCTION
 */
PHP_MSHUTDOWN_FUNCTION(logger)
{
	/* uncomment this line if you have INI entries
	UNREGISTER_INI_ENTRIES();
	*/
	return SUCCESS;
}
/* }}} */

/* Remove if there's nothing to do at request start */
/* {{{ PHP_RINIT_FUNCTION
 */
PHP_RINIT_FUNCTION(logger)
{
	return SUCCESS;
}
/* }}} */

/* Remove if there's nothing to do at request end */
/* {{{ PHP_RSHUTDOWN_FUNCTION
 */
PHP_RSHUTDOWN_FUNCTION(logger)
{
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MINFO_FUNCTION
 */
PHP_MINFO_FUNCTION(logger)
{
	php_info_print_table_start();
	php_info_print_table_header(2, "logger support", "enabled");
	php_info_print_table_end();

	/* Remove comments if you have entries in php.ini
	DISPLAY_INI_ENTRIES();
	*/
}
/* }}} */


/* Remove the following function when you have succesfully modified config.m4
   so that your module can be compiled into PHP, it exists only for testing
   purposes. */

/* Every user-visible function in PHP should document itself in the source */
/* {{{ proto string confirm_logger_compiled(string arg)
   Return a string to confirm that the module is compiled in */
PHP_FUNCTION(confirm_logger_compiled)
{
	char *arg = NULL;
	int arg_len, len;
	char *strg;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &arg, &arg_len) == FAILURE) {
		return;
	}

	len = spprintf(&strg, 0, "Congratulations! You have successfully modified ext/%.78s/config.m4. Module %.78s is now compiled into PHP.", "logger", arg);
	RETURN_STRINGL(strg, len, 0);
}
/* }}} */
/* The previous line is meant for vim and emacs, so it can correctly fold and 
   unfold functions in source code. See the corresponding marks just before 
   function definition, where the functions purpose is also documented. Please 
   follow this convention for the convenience of others editing your code.
*/


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
