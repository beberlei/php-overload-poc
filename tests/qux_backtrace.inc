<?php
function qux($a, $b) {
    var_dump(php_overload_backtrace());

    return $b - $a;
}
