	.section .stub, "ax"
	.globl __nand_stub
__nand_stub:
	mfmsr	3
	rlwinm	3,3,0,17,15
	rlwinm	3,3,0,28,25
	mtmsr	3
	isync

	mfspr	3,1011
	oris	3,3,0x0200
	mtspr	1011,3
	isync

	li	0,0
	mtspr	528,0
	mtspr	529,0
	mtspr	530,0
	mtspr	531,0
	mtspr	532,0
	mtspr	533,0
	mtspr	534,0
	mtspr	535,0
	mtspr	536,0
	mtspr	537,0
	mtspr	538,0
	mtspr	539,0
	mtspr	540,0
	mtspr	541,0
	mtspr	542,0
	mtspr	543,0
	mtspr	560,0
	mtspr	561,0
	mtspr	562,0
	mtspr	563,0
	mtspr	564,0
	mtspr	565,0
	mtspr	566,0
	mtspr	567,0
	mtspr	568,0
	mtspr	569,0
	mtspr	570,0
	mtspr	571,0
	mtspr	572,0
	mtspr	573,0
	mtspr	574,0
	mtspr	575,0
	isync

	lis	3,0x8000
	ori	3,3,0x1fff
	li	4,0x0002
	mtspr	528,3
	mtspr	529,4
	mtspr	536,3
	mtspr	537,4

	lis	3,0xc000
	ori	3,3,0x1fff
	li	4,0x002a
	mtspr	538,3
	mtspr	539,4

	lis	3,0x9000
	ori	3,3,0x1fff
	lis	4,0x1000
	ori	4,4,0x0002
	mtspr	560,3
	mtspr	561,4
	mtspr	568,3
	mtspr	569,4

	lis	3,0xd000
	ori	3,3,0x1fff
	lis	4,0x1000
	ori	4,4,0x002a
	mtspr	570,3
	mtspr	571,4
	isync

	lis	3,__app_start@h
	ori	3,3,__app_start@l
	mtsrr0	3
	mfmsr	3
	ori	3,3,0x0030
	mtsrr1	3
	rfi
