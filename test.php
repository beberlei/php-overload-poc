<?php
include "foo.php";

var_dump(foo(1, 2));

include "bar.php";

var_dump(bar(3, 4));

include "qux.php";

var_dump(qux(5,6));
?>