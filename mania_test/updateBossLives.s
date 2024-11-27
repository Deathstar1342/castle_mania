@updateBossLives.s

/*function to update the lives of chocula */
.global updateBossLives
updateBossLives:
    cmp r1, #200
    bge .attacked
    b .done
.attacked:
   sub r0, r0, #1
   b .done
.done:
    mov pc, lr 
