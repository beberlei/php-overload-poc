/* overload extension for PHP */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "php.h"
#include "ext/standard/info.h"

#include "zend_extensions.h"

#include "php_overload.h"

#define OLG(e) php_overload_globals.e

static HashTable php_overload_targets;

static int php_overload_resource_id;

typedef struct _php_overload_globals_t {
    HashTable overloads;
    HashTable targets;
    zend_function *instrument;
    zend_function *prototype;
} php_overload_globals_t;

ZEND_TLS php_overload_globals_t php_overload_globals;

zend_op_array* (*zend_compile_file_function)(zend_file_handle*, int) = NULL;

typedef enum _php_reflection_type_t {
    REF_TYPE_OTHER = 1,
} php_reflection_type_t;

typedef struct _php_reflection_object_t {
#if PHP_VERSION_ID <= 70400
    zval dummy;
#endif
    zval obj;
    void *ptr;
    zend_class_entry *ce;
    php_reflection_type_t type;
    unsigned int ignore_visibility:1;
    zend_object std;
} php_reflection_object_t;

static zend_always_inline php_reflection_object_t* php_reflection_object_fetch(zend_object *o) {
	return (php_reflection_object_t*) (((char*) o) - XtOffsetOf(php_reflection_object_t, std));
}

static zend_always_inline php_reflection_object_t* php_reflection_object_from(zval *zv) {
    return php_reflection_object_fetch(Z_OBJ_P(zv));
}


static zend_always_inline void php_overload_arginfo(zend_function *overload, zend_function *function, zend_arg_info *info, uint32_t end) {
	zend_internal_arg_info *copy;
	uint32_t it = 0;

	if (function->common.fn_flags & ZEND_ACC_HAS_RETURN_TYPE) {
		info--;
		end++;
	}

	if (function->common.fn_flags & ZEND_ACC_VARIADIC) {
		end++;
	}

	copy = (zend_internal_arg_info*) pecalloc(end, sizeof(zend_internal_arg_info), 1);

	while (it < end) {
		if (info[it].name) {
			copy[it].name = strndup(ZSTR_VAL(info[it].name), ZSTR_LEN(info[it].name));
		}

		copy[it].is_variadic = info[it].is_variadic;
		copy[it].pass_by_reference = info[it].pass_by_reference;
#if PHP_VERSION_ID >= 70200
		copy[it].type              = info[it].type;
		if (ZEND_TYPE_IS_SET(info[it].type) && ZEND_TYPE_IS_CLASS(info[it].type)) {
			zend_string *interned = zend_string_init_interned(
				ZSTR_VAL(ZEND_TYPE_NAME(info[it].type)), 
				ZSTR_LEN(ZEND_TYPE_NAME(info[it].type)), 1);

                        copy[it].type = ZEND_TYPE_ENCODE_CLASS(
				interned, ZEND_TYPE_ALLOW_NULL(info[it].type));
		}
#elif
		copy[it].type_hint         = info[it].type_hint;
		copy[it].allow_null        = info[it].allow_null;
#endif
		it++;
	}

	if (function->common.fn_flags & ZEND_ACC_HAS_RETURN_TYPE) {
		copy++;
	}

	overload->internal_function.arg_info          = copy;
	overload->internal_function.num_args          = function->common.num_args;
	overload->internal_function.required_num_args = function->common.required_num_args;
}

zend_op_array* php_overload_compile(zend_file_handle *fh, int type) {
    zend_string *target;
    zend_function *function;
    zend_op_array *ops = zend_compile_file_function(fh, type);

    ZEND_HASH_FOREACH_STR_KEY(&OLG(targets), target) {
        if ((function = zend_hash_find_ptr(CG(function_table), target))) {
            if (function->type == ZEND_INTERNAL_FUNCTION) {
                continue;
            }

            zend_function *overload = calloc(1, sizeof(zend_internal_function));

            zend_hash_add_ptr(&OLG(overloads), target, function);
            function_add_ref(function);

            memcpy(overload, OLG(instrument), sizeof(zend_internal_function));
            overload->common.function_name =
                zend_string_dup(function->common.function_name, 1);
            overload->common.fn_flags &= ~ZEND_ACC_ARENA_ALLOCATED;

	    php_overload_arginfo(overload, function, 
                function->common.arg_info, function->common.num_args);

            overload->internal_function.reserved[php_overload_resource_id] = function;

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
    zend_extension dummy;

    zend_hash_init(&php_overload_targets, 64, NULL, NULL, 1);

    /* add all target names here */
    php_overload_target_add(ZEND_STRL("foo"));
    php_overload_target_add(ZEND_STRL("bar"));
    php_overload_target_add(ZEND_STRL("qux"));

    php_overload_resource_id = zend_get_resource_handle(&dummy);

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
    OLG(prototype)  = zend_hash_str_find_ptr(
	EG(function_table), ZEND_STRL("php_overload_prototype"));

    {
        zend_class_entry *reflection = zend_hash_str_find_ptr(EG(class_table), ZEND_STRL("reflectionfunction"));

        if (reflection && !zend_hash_str_exists(&reflection->function_table, ZEND_STRL("getoverload"))) {
            zend_function *insert = zend_hash_str_add_mem(
                &reflection->function_table, 
                ZEND_STRL("getoverload"), 
                OLG(prototype), sizeof(zend_internal_function));
            insert->internal_function.scope = reflection;

            function_add_ref(OLG(prototype));
        }
    }
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

PHP_FUNCTION(php_overload_prototype)
{
    php_reflection_object_t *this = php_reflection_object_from(getThis());
    php_reflection_object_t *object;
    zend_function *parent;

    object_init_ex(return_value, EX(func)->common.scope);

    object = php_reflection_object_from(return_value);
    object->type = REF_TYPE_OTHER;

    parent = this->ptr;    

    object->ptr = parent->internal_function.reserved[php_overload_resource_id];    
}

zend_function_entry php_overload_functions[] = {
    PHP_FE(php_overload_prototype, NULL)
    PHP_FE(php_overload_instrument, NULL)
    PHP_FE_END
};

static const zend_module_dep php_overload_deps[] = {
	ZEND_MOD_REQUIRED("Reflection")
        ZEND_MOD_END
};

zend_module_entry overload_module_entry = {
    STANDARD_MODULE_HEADER_EX,
    NULL,
    php_overload_deps,
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
