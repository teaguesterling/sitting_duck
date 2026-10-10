<?php
// `float` and `string` here are the anonymous type keywords; the named rules of
// the same name are the literals.
function f(float $x): string {
    return "s";
}

function g(): ?float {
    return null;
}

$c = null;
