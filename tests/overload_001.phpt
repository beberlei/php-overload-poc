--TEST--
OVerload functions
--FILE--
<?php
include "foo.php";

var_dump(foo(1, 2));

include "bar.php";

var_dump(bar(3, 4));

include "qux.php";

var_dump(qux(5,6));

$reflectionFunction = new ReflectionFunction('qux');
var_dump($reflectionFunction);
var_dump($reflectionFunction->invokeArgs([5, 6]));
var_dump($reflectionFunction->getNumberOfParameters());
var_dump($reflectionFunction->getNumberOfRequiredParameters());
var_dump($reflectionFunction->getParameters());
--EXPECTF--
int(3)
int(12)
int(1)
object(ReflectionFunction)#1 (1) {
  ["name"]=>
  string(3) "qux"
}
int(1)
int(2)
int(2)
array(2) {
  [0]=>
  object(ReflectionParameter)#2 (1) {
    ["name"]=>
    string(1) "a"
  }
  [1]=>
  object(ReflectionParameter)#3 (1) {
    ["name"]=>
    string(1) "b"
  }
}
