<?php
function helper($x) {
    return $x + 1;
}

class Widget {
    public function render($data) {
        return strtoupper($data);
    }
}

$w = new Widget();
helper(1);
$w->render("a");
Widget::render("b");
