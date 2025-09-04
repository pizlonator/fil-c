.text : {
    *(.text.yyy)
    *(.text.b?r)
    *(.text)
    *(.text.xxx .text.foo)
}

.data : {
    *(.data.small)
    *(.big*)
    *(.bar .baz*)
    *(.data.ccc)
}

