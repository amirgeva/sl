	.area _CODE
	
	.globl ___sdcc_call_hl
	.globl __mulint
	.globl _multiply

___sdcc_call_hl:
	jp	(hl)

__mulint:
	jp	_multiply
