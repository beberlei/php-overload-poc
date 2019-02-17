/* overload extension for PHP */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "php.h"
#include "ext/standard/info.h"
#include "php_overload.h"

#define OLG(e) php_overload_globals.e

static HashTable php_overload_targets;

typedef struct _php_overload_globals_t {
    HashTable overloads;
    HashTable targets;
    zend_function *instrument;
} php_overload_globals_t;

ZEND_TLS php_overload_globals_t php_overload_globals;

zend_op_array* (*zend_compile_file_function)(zend_file_handle*, int) = NULL;

zend_op_array* php_overload_compile(zend_file_handle *fh, int type) {
    zend_string *target;
    zend_function *function;
    zend_op_array *ops = zend_compile_file_function(fh, type);

    ZEND_HASH_FOREACH_STR_KEY(&OLG(targets), target) {
        if ((function = zend_hash_find_ptr(CG(function_table), target))) {
            if (function->type == ZEND_INTERNAL_FUNCTION) {
                continue;
            }

            zend_function *overload = zend_arena_alloc(
                &CG(arena), sizeof(zend_internal_function));

            zend_hash_add_ptr(&OLG(overloads), target, function);
            function_add_ref(function);

            memcpy(overload, OLG(instrument), sizeof(zend_internal_function));
            overload->common.function_name =
                zend_string_copy(function->common.function_name);
            overload->common.num_args = function->common.num_args;
            overload->common.required_num_args = function->common.required_num_args;

            overload->common.arg_info = safe_emalloc(sizeof(zend_internal_arg_info), function->common.num_args, 0);

            for (int i = 0; i < function->common.num_args; i++) {
                zend_arg_info *arg_info = &function->common.arg_info[i];

                // convert from zend_arg_info to zend_internal_arg_info, zend_string => char
                overload->common.arg_info[i].name = estrdup(ZSTR_VAL(arg_info->name));

                overload->common.arg_info[i].is_variadic = arg_info->is_variadic;
                overload->common.arg_info[i].pass_by_reference = arg_info->pass_by_reference;
#if PHP_VERSION_ID > 70200
                overload->common.arg_info[i].type = arg_info->type;
#elif PHP_VERSION_ID > 70100
                overload->common.arg_info[i].type_hint = arg_info->type_hint;
                overload->common.arg_info[i].allow_null = arg_info->allow_null;
#endif
            }

            zend_hash_update_ptr(CG(function_table), target, overload);
            zend_hash_del(&OLG(targets), target);
        }
    } ZEND_HASH_FOREACH_END();

    return ops;
}

static zend_always_inline int php_overload_target_add(const char *function_name, size_t function_name_len) {
    zend_string *name = zend_string_init(function_name, function_name_len, 1);

    if (!zend_hash_add_empty_element(&php_overload_targets, name)) {
        zend_string_release(name);
        return FAILURE;
    }

    zend_string_release(name);
    return SUCCESS;
}

static zend_always_inline zend_function* php_overload_find(zend_string *name) {
    zend_string *key = zend_string_tolower(name);
    zend_function *function =
        (zend_function*)
            zend_hash_find_ptr(&OLG(overloads), key);
    zend_string_release(key);
    return function;
}

PHP_MINIT_FUNCTION(overload)
{
    zend_hash_init(&php_overload_targets, 64, NULL, NULL, 1);

    /* add all target names here */
    php_overload_target_add(ZEND_STRL("foo"));
    php_overload_target_add(ZEND_STRL("bar"));
    php_overload_target_add(ZEND_STRL("qux"));

    return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(overload)
{
    zend_hash_destroy(&php_overload_targets);

    return SUCCESS;
}

PHP_RINIT_FUNCTION(overload)
{
#if defined(ZTS) && defined(COMPILE_DL_OVERLOAD)
    ZEND_TSRMLS_CACHE_UPDATE();
#endif

    if (zend_compile_file_function == NULL) {
        zend_compile_file_function = zend_compile_file;
        zend_compile_file = php_overload_compile;
    }

    zend_hash_init(&OLG(overloads), zend_hash_num_elements(&php_overload_targets), NULL, zend_function_dtor, 0);
    zend_hash_init(&OLG(targets),
        zend_hash_num_elements(&php_overload_targets), NULL, NULL, 0);
    zend_hash_copy(&OLG(targets), &php_overload_targets, NULL);

    OLG(instrument) = zend_hash_str_find_ptr(
        EG(function_table), ZEND_STRL("php_overload_instrument"));

    return SUCCESS;
}

PHP_RSHUTDOWN_FUNCTION(overload)
{
    if (zend_compile_file == php_overload_compile) {
        zend_compile_file = zend_compile_file_function;
    }

    zend_hash_destroy(&OLG(overloads));
    zend_hash_destroy(&OLG(targets));

    return SUCCESS;
}

PHP_MINFO_FUNCTION(overload)
{
    php_info_print_table_start();
    php_info_print_table_header(2, "overload support", "enabled");
    php_info_print_table_end();
}

PHP_FUNCTION(php_overload_instrument)
{
    zend_fcall_info fci = empty_fcall_info;
    zend_fcall_info_cache fcc = empty_fcall_info_cache;
    zend_function *function =
        php_overload_find(EX(func)->common.function_name);

    if (!function) {
        zend_throw_error(NULL,
            "php_overload_instrument should not be called directly");
        return;
    }

    fci.size = sizeof(zend_fcall_info);
#if PHP_VERSION_ID < 70300
    fcc.initialized = 1;
#endif
    fcc.function_handler = function;

    /* work with arguments here */

    zend_fcall_info_argp(&fci, ZEND_NUM_ARGS(), ZEND_CALL_ARG(execute_data, 1));

    if (zend_fcall_info_call(&fci, &fcc, return_value, NULL) != SUCCESS) {
        zend_throw_error(NULL,
            "overload of %s could not be invoked",
            ZSTR_VAL(EX(func)->common.function_name));
    }

    /* work with return value here */

    zend_fcall_info_args_clear(&fci, 1);
}

zend_function_entry php_overload_functions[] = {
    PHP_FE(php_overload_instrument, NULL)
    PHP_FE_END
};

zend_module_entry overload_module_entry = {
    STANDARD_MODULE_HEADER,
    "overload",
    php_overload_functions,
    PHP_MINIT(overload),
    PHP_MSHUTDOWN(overload),
    PHP_RINIT(overload),
    PHP_RSHUTDOWN(overload),
    PHP_MINFO(overload),
    PHP_OVERLOAD_VERSION,
    STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_OVERLOAD
# ifdef ZTS
ZEND_TSRMLS_CACHE_DEFINE()
# endif
ZEND_GET_MODULE(overload)
#endif
