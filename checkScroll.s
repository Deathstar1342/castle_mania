@ checkScroll.s

/* function to check how far we have scrolled */
.global checkScroll
checkScroll:
    cmp r0, #1000
    bge .bossTime
    mov r0, #0
    b .done
.bossTime:
    mov r0, #1
.done:
    mov pc, lr
