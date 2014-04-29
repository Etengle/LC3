	.ORIG x3000
xx3000	AND	R2, R2, #0
xx3001	LD	R3, xx3013	;;PCoffset9 x11
xx3002	TRAP	x20		;;GETC
xx3003	LDR	R1, R3, #0
xx3004	ADD	R4, R1, #-4
xx3005	BRz	xx300e		;;PCoffset9 x8
xx3006	NOT	R1, R1
xx3007	ADD	R1, R1, R0
xx3008	NOT	R1, R1
xx3009	BRnp	xx300b		;;PCoffset9 x1
xx300a	ADD	R2, R2, #1
xx300b	ADD	R3, R3, #1
xx300c	LDR	R1, R3, #0
xx300d	BRnzp	xx3004		;;PCoffset9 xfff6
xx300e	LD	R0, xx3012	;;PCoffset9 x3
xx300f	ADD	R0, R0, R2
xx3010	TRAP	x21		;;OUT
xx3011	TRAP	x25		;;HALT
xx3012		.FILL x30
xx3013		.FILL x4000
	.END
