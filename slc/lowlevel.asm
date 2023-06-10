	.area _CODE
	
	.globl ___sdcc_call_hl
	.globl __mulint
	.globl _multiply
	.globl ___sdcc_call_iy
	.globl ___sdcc_enter_ix

___sdcc_call_hl:
	jp	(hl)

__mulint:
	jp	_multiply

___sdcc_call_iy:
	jp	(iy)
	
___sdcc_enter_ix:
   pop hl
   push ix
   ld ix,#0
   add ix,sp
   jp (hl)