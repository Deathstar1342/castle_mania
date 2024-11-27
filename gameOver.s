@ gameOver.s

/* function which checks if the game has ended */
.global gameOver
gameOver:
    cmp r0, #0
    ble .lose
    cmp r1, #0
    ble .win
    mov r0, #0
    b .done
.lose:
    mov r0, #1
    b .done
.win:
    mov r0, #2
    b .done
.done:
    mov pc, lr
