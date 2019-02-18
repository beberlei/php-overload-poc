--TEST--
overload: Backtrace
--FILE--
<?php
require "qux_backtrace.inc";

var_dump(qux(6, 5));
?>
--EXPECT--
array(1) {
  [0]=>
  array(2) {
    ["file"]=>
    string(3) "qux"
    ["line"]=>
    int(3)
  }
}
int(-1)
