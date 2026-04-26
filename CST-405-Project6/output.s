.data

.text
.globl main
main:
    # Allocate stack space
    addi $sp, $sp, -400
    j main_start       # Jump to main function


# Function: __mod
func___mod:
    # Save return address and allocate local stack frame
    # Parameter 'a' at offset 0
    sw $a0, 0($sp)     # Store parameter 'a'
    # Parameter 'b' at offset 4
    sw $a1, 4($sp)     # Store parameter 'b'
    # Declared 'q' at offset 8
    lw $t0, 0($sp)     # Load variable 'a'
    lw $t1, 4($sp)     # Load variable 'b'
    div $t0, $t1         # a / b
    mflo $t2              # t0 = quotient
    move $t0, $t2       # q = t0
    sw $t0, 8($sp)     # Store to 'q'
    lw $t1, 4($sp)     # Load variable 'b'
    mul $t2, $t0, $t1   # t0 = q * b
    lw $t0, 0($sp)     # Load variable 'a'
    sub $t1, $t0, $t2   # t1 = a - t0
    move $v0, $t1       # Move to return register
    # Return from function
    # End of function __mod
    # Restore and return
    jr $ra

# Function: __and
func___and:
    # Save return address and allocate local stack frame
    # Parameter 'a' at offset 0
    sw $a0, 0($sp)     # Store parameter 'a'
    # Parameter 'b' at offset 4
    sw $a1, 4($sp)     # Store parameter 'b'
    lw $t0, 0($sp)     # Load condition 'a'
    beq $t0, $zero, L1   # if !a goto L1
    lw $t1, 4($sp)     # Load condition 'b'
    beq $t1, $zero, L3   # if !b goto L3
    li $t2, 1         # Load return value
    move $v0, $t2       # Move to return register
    # Return from function
L3:
L1:
    li $t2, 0         # Load return value
    move $v0, $t2       # Move to return register
    # Return from function
    # End of function __and
    # Restore and return
    jr $ra

# Function: __or
func___or:
    # Save return address and allocate local stack frame
    # Parameter 'a' at offset 0
    sw $a0, 0($sp)     # Store parameter 'a'
    # Parameter 'b' at offset 4
    sw $a1, 4($sp)     # Store parameter 'b'
    lw $t0, 0($sp)     # Load condition 'a'
    beq $t0, $zero, L5   # if !a goto L5
    li $t1, 1         # Load return value
    move $v0, $t1       # Move to return register
    # Return from function
L5:
    lw $t1, 4($sp)     # Load condition 'b'
    beq $t1, $zero, L7   # if !b goto L7
    li $t2, 1         # Load return value
    move $v0, $t2       # Move to return register
    # Return from function
L7:
    li $t2, 0         # Load return value
    move $v0, $t2       # Move to return register
    # Return from function
    # End of function __or
    # Restore and return
    jr $ra

# Function: __not
func___not:
    # Save return address and allocate local stack frame
    # Parameter 'a' at offset 0
    sw $a0, 0($sp)     # Store parameter 'a'
    lw $t0, 0($sp)     # Load condition 'a'
    beq $t0, $zero, L9   # if !a goto L9
    li $t1, 0         # Load return value
    move $v0, $t1       # Move to return register
    # Return from function
L9:
    li $t1, 1         # Load return value
    move $v0, $t1       # Move to return register
    # Return from function
    # End of function __not
    # Restore and return
    jr $ra

# Function: main
main_start:
    # Declared 'x' at offset 0
    # Declared 'y' at offset 4
    # Declared 'result' at offset 8
    li $t0, 111         # Load constant 111 for print
    # Print integer
    move $a0, $t0
    li $v0, 1
    syscall
    # Print newline
    li $v0, 11
    li $a0, 10
    syscall
    li $t0, 5         # x = 5 (constant)
    sw $t0, 0($sp)     # Store to 'x'
    li $t1, 3         # Load constant 3
    sgt $t2, $t0, $t1   # t1 = x > 3
    beq $t2, $zero, L11   # if !t1 goto L11
    li $t0, 1         # Load constant 1 for print
    # Print integer
    move $a0, $t0
    li $v0, 1
    syscall
    # Print newline
    li $v0, 11
    li $a0, 10
    syscall
L11:
    li $t0, 222         # Load constant 222 for print
    # Print integer
    move $a0, $t0
    li $v0, 1
    syscall
    # Print newline
    li $v0, 11
    li $a0, 10
    syscall
    li $t0, 2         # x = 2 (constant)
    sw $t0, 0($sp)     # Store to 'x'
    li $t1, 5         # Load constant 5
    sgt $t2, $t0, $t1   # t1 = x > 5
    beq $t2, $zero, L12   # if !t1 goto L12
    li $t0, 0         # Load constant 0 for print
    # Print integer
    move $a0, $t0
    li $v0, 1
    syscall
    # Print newline
    li $v0, 11
    li $a0, 10
    syscall
    j L13        # Unconditional jump
L12:
    li $t0, 1         # Load constant 1 for print
    # Print integer
    move $a0, $t0
    li $v0, 1
    syscall
    # Print newline
    li $v0, 11
    li $a0, 10
    syscall
L13:
    li $t0, 333         # Load constant 333 for print
    # Print integer
    move $a0, $t0
    li $v0, 1
    syscall
    # Print newline
    li $v0, 11
    li $a0, 10
    syscall
    li $t0, 7         # x = 7 (constant)
    sw $t0, 0($sp)     # Store to 'x'
    li $t1, 3         # y = 3 (constant)
    sw $t1, 4($sp)     # Store to 'y'
    li $t3, 5         # Load constant 5
    sgt $t2, $t0, $t3   # t1 = x > 5
    beq $t2, $zero, L14   # if !t1 goto L14
    li $t0, 5         # Load constant 5
    slt $t2, $t1, $t0   # t1 = y < 5
    beq $t2, $zero, L16   # if !t1 goto L16
    li $t0, 1         # Load constant 1 for print
    # Print integer
    move $a0, $t0
    li $v0, 1
    syscall
    # Print newline
    li $v0, 11
    li $a0, 10
    syscall
    j L17        # Unconditional jump
L16:
    li $t0, 0         # Load constant 0 for print
    # Print integer
    move $a0, $t0
    li $v0, 1
    syscall
    # Print newline
    li $v0, 11
    li $a0, 10
    syscall
L17:
    j L15        # Unconditional jump
L14:
    li $t0, 0         # Load constant 0 for print
    # Print integer
    move $a0, $t0
    li $v0, 1
    syscall
    # Print newline
    li $v0, 11
    li $a0, 10
    syscall
L15:
    li $t0, 444         # Load constant 444 for print
    # Print integer
    move $a0, $t0
    li $v0, 1
    syscall
    # Print newline
    li $v0, 11
    li $a0, 10
    syscall
    li $t0, 2         # x = 2 (constant)
    sw $t0, 0($sp)     # Store to 'x'
    # Declared '_sw0' at offset 12
    move $t1, $t0       # _sw0 = x
    sw $t1, 12($sp)     # Store to '_sw0'
    li $t3, 1         # Load constant 1
    seq $t2, $t1, $t3   # t1 = _sw0 == 1
    beq $t2, $zero, L18   # if !t1 goto L18
    li $t1, 10         # Load constant 10 for print
    # Print integer
    move $a0, $t1
    li $v0, 1
    syscall
    # Print newline
    li $v0, 11
    li $a0, 10
    syscall
    j L19        # Unconditional jump
L18:
    lw $t1, 12($sp)     # Load variable '_sw0'
    li $t3, 2         # Load constant 2
    seq $t2, $t1, $t3   # t1 = _sw0 == 2
    beq $t2, $zero, L20   # if !t1 goto L20
    li $t1, 20         # Load constant 20 for print
    # Print integer
    move $a0, $t1
    li $v0, 1
    syscall
    # Print newline
    li $v0, 11
    li $a0, 10
    syscall
    j L21        # Unconditional jump
L20:
    lw $t1, 12($sp)     # Load variable '_sw0'
    li $t3, 3         # Load constant 3
    seq $t2, $t1, $t3   # t1 = _sw0 == 3
    beq $t2, $zero, L22   # if !t1 goto L22
    li $t1, 30         # Load constant 30 for print
    # Print integer
    move $a0, $t1
    li $v0, 1
    syscall
    # Print newline
    li $v0, 11
    li $a0, 10
    syscall
    j L23        # Unconditional jump
L22:
    li $t1, 99         # Load constant 99 for print
    # Print integer
    move $a0, $t1
    li $v0, 1
    syscall
    # Print newline
    li $v0, 11
    li $a0, 10
    syscall
L23:
L21:
L19:
    li $t1, 555         # Load constant 555 for print
    # Print integer
    move $a0, $t1
    li $v0, 1
    syscall
    # Print newline
    li $v0, 11
    li $a0, 10
    syscall
    li $t0, 9         # x = 9 (constant)
    sw $t0, 0($sp)     # Store to 'x'
    # Declared '_sw1' at offset 16
    move $t1, $t0       # _sw1 = x
    sw $t1, 16($sp)     # Store to '_sw1'
    li $t3, 1         # Load constant 1
    seq $t2, $t1, $t3   # t1 = _sw1 == 1
    beq $t2, $zero, L24   # if !t1 goto L24
    li $t1, 10         # Load constant 10 for print
    # Print integer
    move $a0, $t1
    li $v0, 1
    syscall
    # Print newline
    li $v0, 11
    li $a0, 10
    syscall
    j L25        # Unconditional jump
L24:
    lw $t1, 16($sp)     # Load variable '_sw1'
    li $t3, 2         # Load constant 2
    seq $t2, $t1, $t3   # t1 = _sw1 == 2
    beq $t2, $zero, L26   # if !t1 goto L26
    li $t1, 20         # Load constant 20 for print
    # Print integer
    move $a0, $t1
    li $v0, 1
    syscall
    # Print newline
    li $v0, 11
    li $a0, 10
    syscall
    j L27        # Unconditional jump
L26:
    li $t1, 99         # Load constant 99 for print
    # Print integer
    move $a0, $t1
    li $v0, 1
    syscall
    # Print newline
    li $v0, 11
    li $a0, 10
    syscall
L27:
L25:
    li $t1, 666         # Load constant 666 for print
    # Print integer
    move $a0, $t1
    li $v0, 1
    syscall
    # Print newline
    li $v0, 11
    li $a0, 10
    syscall
    li $t0, 5         # x = 5 (constant)
    sw $t0, 0($sp)     # Store to 'x'
    li $t1, 3         # y = 3 (constant)
    sw $t1, 4($sp)     # Store to 'y'
    li $t3, 3         # Load constant 3
    sgt $t2, $t0, $t3   # t1 = x > 3
    # Argument: t1
    li $t0, 5         # Load constant 5
    slt $t3, $t1, $t0   # t0 = y < 5
    # Argument: t0
    # Call function __and with 2 arguments
    move $a0, $t2      # Pass argument 't1'
    move $a1, $t3      # Pass argument 't0'
    jal func___and
    move $t0, $v0      # Get return value
    beq $t0, $zero, L28   # if !t2 goto L28
    li $t1, 1         # Load constant 1 for print
    # Print integer
    move $a0, $t1
    li $v0, 1
    syscall
    # Print newline
    li $v0, 11
    li $a0, 10
    syscall
    j L29        # Unconditional jump
L28:
    li $t1, 0         # Load constant 0 for print
    # Print integer
    move $a0, $t1
    li $v0, 1
    syscall
    # Print newline
    li $v0, 11
    li $a0, 10
    syscall
L29:
    li $t1, 777         # Load constant 777 for print
    # Print integer
    move $a0, $t1
    li $v0, 1
    syscall
    # Print newline
    li $v0, 11
    li $a0, 10
    syscall
    li $t1, 1         # x = 1 (constant)
    sw $t1, 0($sp)     # Store to 'x'
    li $t4, 10         # y = 10 (constant)
    sw $t4, 4($sp)     # Store to 'y'
    li $t5, 5         # Load constant 5
    sgt $t0, $t1, $t5   # t2 = x > 5
    # Argument: t2
    li $t1, 5         # Load constant 5
    sgt $t5, $t4, $t1   # t3 = y > 5
    # Argument: t3
    # Call function __or with 2 arguments
    move $a0, $t0      # Pass argument 't2'
    move $a1, $t5      # Pass argument 't3'
    jal func___or
    move $t1, $v0      # Get return value
    beq $t1, $zero, L30   # if !t4 goto L30
    li $t4, 1         # Load constant 1 for print
    # Print integer
    move $a0, $t4
    li $v0, 1
    syscall
    # Print newline
    li $v0, 11
    li $a0, 10
    syscall
    j L31        # Unconditional jump
L30:
    li $t4, 0         # Load constant 0 for print
    # Print integer
    move $a0, $t4
    li $v0, 1
    syscall
    # Print newline
    li $v0, 11
    li $a0, 10
    syscall
L31:
    li $t4, 888         # Load constant 888 for print
    # Print integer
    move $a0, $t4
    li $v0, 1
    syscall
    # Print newline
    li $v0, 11
    li $a0, 10
    syscall
    li $t4, 0         # x = 0 (constant)
    sw $t4, 0($sp)     # Store to 'x'
    # Argument: x
    # Call function __not with 1 arguments
    move $a0, $t4      # Pass argument 'x'
    jal func___not
    move $t1, $v0      # Get return value
    beq $t1, $zero, L32   # if !t4 goto L32
    li $t6, 1         # Load constant 1 for print
    # Print integer
    move $a0, $t6
    li $v0, 1
    syscall
    # Print newline
    li $v0, 11
    li $a0, 10
    syscall
    j L33        # Unconditional jump
L32:
    li $t6, 0         # Load constant 0 for print
    # Print integer
    move $a0, $t6
    li $v0, 1
    syscall
    # Print newline
    li $v0, 11
    li $a0, 10
    syscall
L33:
    li $t6, 999         # Load constant 999 for print
    # Print integer
    move $a0, $t6
    li $v0, 1
    syscall
    # Print newline
    li $v0, 11
    li $a0, 10
    syscall
    li $t4, 4         # x = 4 (constant)
    sw $t4, 0($sp)     # Store to 'x'
    li $t6, 6         # y = 6 (constant)
    sw $t6, 4($sp)     # Store to 'y'
    li $t7, 2         # Load constant 2
    sgt $t1, $t4, $t7   # t4 = x > 2
    # Argument: t4
    li $t4, 4         # Load constant 4
    sgt $t7, $t6, $t4   # t5 = y > 4
    # Argument: t5
    # Call function __and with 2 arguments
    move $a0, $t1      # Pass argument 't4'
    move $a1, $t7      # Pass argument 't5'
    jal func___and
    move $t4, $v0      # Get return value
    move $t6, $t4       # result = t6
    sw $t6, 8($sp)     # Store to 'result'
    # Print integer
    move $a0, $t6
    li $v0, 1
    syscall
    # Print newline
    li $v0, 11
    li $a0, 10
    syscall
    li $t6, 0         # Load return value
    move $v0, $t6       # Move to return register
    # Return from function
    # End of function main

    # Spill any remaining registers

    # Exit program
    addi $sp, $sp, 400
    li $v0, 10
    syscall
