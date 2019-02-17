--TEST--
overload: Backtrace
--FILE--
<?php

require "qux_backtrace.php";

var_dump(qux(6, 5));
--EXPECTF--
array(2) {
  [0]=>
  string(3) "qux"
  [1]=>
  string(3) "qux"
}
int(-1)
