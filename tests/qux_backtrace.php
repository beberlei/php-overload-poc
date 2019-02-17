<?php

function qux($a, $b) {
    var_dump(array_map(function ($item) {
        return $item['function'];
    }, debug_backtrace()));
    return $b - $a;
}
