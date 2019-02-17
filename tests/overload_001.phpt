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
var_dump($params = $reflectionFunction->getParameters());

$reflectionFunction = new ReflectionFunction('qux_original');
$original = $reflectionFunction->getParameters();

for ($i = 0; $i < count($params); $i++) {
    if ($params[$i]->allowsNull() !== $original[$i]->allowsNull()) {
        echo $params[$i]->getName() . " allowsNull missmatch.\n";
    }

    if ($params[$i]->canBePassedByValue() !== $original[$i]->canBePassedByValue()) {
        echo $params[$i]->getName() . " canBePassedByValue missmatch.\n";
    }

    if ($params[$i]->getDefaultValue() !== $original[$i]->getDefaultValue()) {
        echo $params[$i]->getName() . " getDefaultValue() missmatch.\n";
    }

    if ($params[$i]->isOptional() !== $original[$i]->isOptional()) {
        echo $params[$i]->getName() . " isOptional() missmatch.\n";
    }
}

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
