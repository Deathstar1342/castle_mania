@updateBossLives.s

/*function to update the lives of chocula */
.global updateBossLives
updateBossLives:
    cmp r0, #1
    beq .attacking
    mov r3, #0
    b .done
.attacking:
    cmp r2, #200
    bge .attacked
    mov r3, #0
    b .done
.attacked:
    sub r3, r3, #1
.done:
    mov r0, r3
    mov pc, lr
