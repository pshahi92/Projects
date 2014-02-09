	.file	"handle.c"
	.section	.rodata
.LC0:
	.string	"exiting\n"
.LC1:
	.string	"Nice try.\n"
	.text
	.globl	handler
	.type	handler, @function
handler:
.LFB0:
	.cfi_startproc
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset 6, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register 6
	subq	$32, %rsp
	movl	%edi, -20(%rbp)
	movl	$1, -4(%rbp)
	cmpl	$10, -20(%rbp)
	jne	.L2
	movl	-4(%rbp), %eax
	movl	$8, %edx
	movl	$.LC0, %esi
	movl	%eax, %edi
	call	write
	movq	%rax, -16(%rbp)
	movl	$1, %edi
	call	exit
.L2:
	cmpl	$2, -20(%rbp)
	jne	.L3
	movl	-4(%rbp), %eax
	movl	$10, %edx
	movl	$.LC1, %esi
	movl	%eax, %edi
	call	write
	movq	%rax, -16(%rbp)
	jmp	.L1
.L3:
	cmpq	$10, -16(%rbp)
	je	.L1
	movl	$-999, %edi
	call	exit
.L1:
	leave
	.cfi_def_cfa 7, 8
	ret
	.cfi_endproc
.LFE0:
	.size	handler, .-handler
	.section	.rodata
.LC2:
	.string	"%d\n"
.LC3:
	.string	"Still here"
	.text
	.globl	main
	.type	main, @function
main:
.LFB1:
	.cfi_startproc
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset 6, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register 6
	subq	$48, %rsp
	movl	%edi, -36(%rbp)
	movq	%rsi, -48(%rbp)
	movl	$handler, %esi
	movl	$2, %edi
	call	Signal
	movl	$handler, %esi
	movl	$10, %edi
	call	Signal
	call	getpid
	movl	%eax, -8(%rbp)
	movl	$.LC2, %eax
	movl	-8(%rbp), %edx
	movl	%edx, %esi
	movq	%rax, %rdi
	movl	$0, %eax
	call	printf
	movl	$1, -4(%rbp)
	movq	$3, -32(%rbp)
	movq	$0, -24(%rbp)
	jmp	.L6
.L7:
	movl	$.LC3, %edi
	call	puts
	leaq	-32(%rbp), %rax
	movl	$0, %esi
	movq	%rax, %rdi
	call	nanosleep
.L6:
	cmpl	$0, -4(%rbp)
	jne	.L7
	movl	$0, %eax
	leave
	.cfi_def_cfa 7, 8
	ret
	.cfi_endproc
.LFE1:
	.size	main, .-main
	.ident	"GCC: (Ubuntu/Linaro 4.6.3-1ubuntu5) 4.6.3"
	.section	.note.GNU-stack,"",@progbits
