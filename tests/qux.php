<?php
function qux(int $a, ?int $b) : int {
	return $b - $a;
}

function qux_original(int $a, ?int $b) : int {
    return $b - $a;
}

?>
