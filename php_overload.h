/* overload extension for PHP */

#ifndef PHP_OVERLOAD_H
# define PHP_OVERLOAD_H

extern zend_module_entry overload_module_entry;
# define phpext_overload_ptr &overload_module_entry

# define PHP_OVERLOAD_VERSION "0.0.1"

# if defined(ZTS) && defined(COMPILE_DL_OVERLOAD)
ZEND_TSRMLS_CACHE_EXTERN()
# endif

#endif	/* PHP_OVERLOAD_H */