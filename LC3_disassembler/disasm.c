/* Assignment #11
 * Name: Yu Xiang, Zheng
 * Nickname: Slighten
 * ID: 101021120
 * Due date: 2013-12-16 12:30
 * Filename: disasm.c
 *
 * Usage:
 *   This is an simple disassembler, which is able to convert an LC3 object file (.obj) 
 * back into an assembly language program (.asm).
 *
 * Restriction:
 *   This simple disassembler can only handle the LC3 assembly language which 
 * put ".FILL" in the bottom of the code and without ".BLKW", ".STRINGZ".
 * 
 * Status: 
 *   Compile, execute, and prompt impeccably. 
 */

#include <stdio.h>
#include <stdlib.h>

/* opcode */
char *op[ ] = {
	"BR", "ADD", "LD", "ST", "JSR", "AND", "LDR", "STR",
	"RTI", "NOT", "LDI", "STI", "JMP", "RESERVED", "LEA", "TRAP"
};
/* pseudo instruction */
char *pseudo[ ]= {
	"GETC", "OUT", "PUTS", "IN", "" ,"HALT"
};

int PC, progSize, progStart, progEnd;
size_t fileSize;


int SignExtend(int val, int nBits) {
/* return the sign-extended value for the n-bit val
 * for example,
 * val=0x123, nBits=9, => return 0xffffff23
 * val=0x045, nBits=9, => return 0x00000045
 * for 32-bit int, one easy way is to shift left by (32-9=23) bits
 * so that the sign bit of val lines up with the sign bit of the 32-bit int,
 * and then shift right by 23 bits.
 */ 
	int temp;
	temp = val << (32 - nBits);
	temp = temp >> (32 - nBits);
	return temp;
}

	
 
/* This function disassembles one instruction by decoding the fields. */
void disasm(short IR) {
	/* declare variables to extract different fields of the IR */
	int opcode = (IR >> 12) & 0xf; /* extract IR bits 15:12 */
	char *mnemonic = *(op + opcode); /* map the 4-bit opcode to the string */
	int dst = (IR >> 9) & 0x7; /* extract IR bits 11:9 */
	int src1 = (IR >> 6) & 0x7; /* extract IR bits 8:6 */
	int src2 = IR & 0x7; /* extract IR bits 2:0 */
	int imm = (IR >> 5) & 0x1 ;  /* extract IR bit 5 */
	int imm5 = SignExtend(IR & 0x1f, 5); /* extract IR bit 4:0, then sign extend */
	int PCoffset9 = SignExtend(IR & 0x1ff, 9); /* extract IR bits 8:0, then sign extend */
	int PCoffset11 = SignExtend(IR & 0x7ff, 11); /* extract IR bits 10:0, then sign extend */
	int offset6 = SignExtend(IR & 0x3f, 6); /* extract IR bits 5:0, then sign extend */
	int jsrBit = (IR >> 11) & 0x1; /* extract IR bit 11 */
	int baseReg = (IR >> 6) & 0x7; /* extract IR bit 8:6 */
	int trapvect8 = IR & 0xff; /* extract IR bits 7:0 */
	char *trapcode = *(pseudo + trapvect8 - 0x20); /* map trapvect8 to the pseudo-instruction */
	int n = (IR >> 11) & 0x1; /* extract IR bit 11 */
	int z = (IR >> 10) & 0x1; /* extract IR bit 10 */
	int p = (IR >> 9) & 0x1; /* extract IR bit 9 */
	static int genData = 0; /* this flag indicates if we started .FILL */
	/* if we already started .FILL, then treat the rest of the memory image
	 * as .FILL here.
	 * Otherwise, try to disassemble as an instruction. */
	if(genData){
		printf("\t.FILL x%hx", IR);
		return;
	}
	switch (opcode) {
		case 0: /* "BR"
			 * The format is BR[n][z][p]  label
			 * where the label is xx3000, xx3001, etc
			 * derived from PC + 1 + PCoffset9
			 * 
			 * Check if target address is outside the range of
			 * progStart ... progEnd.
			 * If so, then treat it as data, rather than
			 * instruction, and set the flag.
			 * Otherwise, generate as a BR instruction 
			 */
			/* check it be in reasonable range */
			if( (PC + 1 + PCoffset9) - progEnd  <= 0 && 
					(PC + 1 + PCoffset9) - progStart >=0){
			/* determine [n][z][p], and puts fake comment */
				printf("%s%s\txx%hx\t\t;;PCoffset9 %hd" , mnemonic ,
					(n ?( z ? (p ? "nzp" : "nz") : (p ? "np" : "n")) : 
	     	     		             (z ? (p ? "zp" : "z") : (p ? "p" : ""))), 
						 PC + 1 + PCoffset9, PCoffset9);
			}
			/* if out of reasonable range, treat it as an .FILL address */
			else{
				printf("\t.FILL x%hx", IR);
				genData = 1;
			}
			break;
		case 1: /* ADD */
		case 5: /* AND */
			/* these two conform to the format of
			 * [opcode][dst][src1][imm][src2 or imm5]
			 * depending on the imm bit.
			 * The assembly instruction has the format of
			 * mnemonic  Rdst, Rsrc1, src2 
			 *    or
			 * mnemonic  Rdst, Rsrc1, #imm5 
			 */
			/* immediate mode */
			if(imm) 
				printf("%s\tR%d, R%d, #%d", mnemonic, dst, src1, imm5);
			else
				printf("%s\tR%d, R%d, R%d", mnemonic, dst, src1, src2);
			break;
		case 2: /* LD */
		case 3: /* ST */
		case 0xA: /* LDI */
		case 0xB: /* STI */
		case 0xE: /* LEA */
			/* these instructions have the format of
			 * [opcode][dst][PCoffset9]
			 * the assembly instruction has the format of
			 * mnemonic  Rn, label
			 * where label has the format of xxTargetAddress
			 * (e.g., xx3000)
			 * where xxTargetAddress is dervied from PC + 1 + PCoffset9
			 * 
			 * Be sure to  check if target address is within range
			 * (similar to BR above) 
			 */
			/* check it be in reasonable range */
			if( PC + 1 + PCoffset9 <= progEnd && PC + 1 + PCoffset9 >= progStart)
				/* puts fake comment */
				printf("%s\tR%d, xx%hx\t;;PCoffset9 %hd", mnemonic, 
					dst, PC + 1 + PCoffset9, PCoffset9);
			/* if out of reasonable range, treat it as an .FILL address */
			else{
				printf("\t.FILL x%hx", IR);
				genData = 1;
			}
			break;
		case 4: /* JSR */
			/* This instruction has the format of
			 * [opcode][jsrBit][PCoffset11] => this is JSR label
			 *   or
			 * [opcode][jsrBit][baseReg]  => this is JSRR  Rn
			 *
			 * Be sure to check if target address (JSR, not JSRR) is
			 * within range (similar to above) 
			 */
			/* JSR */
			if(jsrBit){
				/* check it be in reasonable range */
				if( PC + 1 + PCoffset11 < progEnd && PC + 1 + PCoffset11 >= progStart )
					/* puts fake comment */
					printf("%s\txx%hx\t;;PCoffset11 %hd", mnemonic, 
						PC + 1 + PCoffset11, PCoffset11);
				/* if out of reasonable range, treat it as an .FILL address */
				else{
					printf("\t.FILL x%hx", IR);
					genData = 1;
				}
			}
			/* JSRR */
			else
				printf("%s\tR%d", mnemonic, baseReg);
			break;
		case 6: /* LDR */
		case 7: /* STR */
			/* these instructions have the format of
			 * [opcode][dst][baseReg][#offset6] */
			printf("%s\tR%d, R%d, #%d", mnemonic, dst, baseReg, offset6);
			break;
		case 8: /* RTI */
			/* just RTI without operands */
			printf("%s", mnemonic);
			break;
		case 9: /* NOT */
			/* [opcode][dst][src1] */
			printf("%s\tR%d, R%d", mnemonic, dst, src1);
			break;
		case 0xC: /* JMP */
			/* [opcode][baseReg] */
			printf("%s\tR%d", mnemonic, baseReg);
			break;
		case 0xD: /* reserved */
			break;
		case 0xF: /* TRAP */
			/* [opcode][trapvect8] */
			/* also puts pseudo trap codes to the comment */
			printf("%s\tx%hx\t\t;;%s", mnemonic, trapvect8, trapcode);
			break;
		}
}

void Disassemble(short mem[]) {
	/* - initialize progStart and PC to the .ORIG address
	 * - initialize progEnd before call disasm()
	 * - print the .ORIG directive
	 * - loop over each instruction word corresponding to each PC value
	 *   - print its label for each line based on its memory address
	 *     (i.e., value of PC) in the format of xx3000, xx3001, xx3002, etc.
	 *   - invoke disasm() by passing the current instruction word
	 * - print the .END directive */
	int i = 0;
	progStart = *mem;
	/* count progEnd */
	while (*(mem + ++i)){
	}
	progEnd = progStart + i;
	/* .ORIG */
	printf("\t.ORIG x%x\n", progStart);
	/* initialize used i */
	i = 0;
	/* go through each word */
	while (*(mem + i + 1)){
		PC = *mem + i;
		printf("xx%x\t", PC);
		/* the content of x3000 is at x3001 
		 * differ PC by 1 */ 
		disasm(*(mem + ++i));
		printf("\n");
	}
	/* .END */
	printf("\t.END\n");
}

/*
 * This function opens and reads the object file name, allocates memory,
 * and returns the address to the binary image corresponding to the object file.
 * In case of errors, it prints the error and exits.
 * It is ready to use. You don't have to modify it.
 */
short *ReadFile(char *filename) {
	short *mem;
	FILE *fh;
	fh = fopen(filename, "rb"); /* open as binary */
	if (fh == NULL) {
		fprintf(stderr, "cannot open %s\n", filename);
		exit(2);
	}
	/* find the file size */
	fseek(fh, 0L, SEEK_END); /* go to the end of file */
	fileSize = ftell(fh);    /* find where it is */
	fseek(fh, 0L, SEEK_SET); /* go to the beginning */
	/* now allocate a buffer of the size */
	mem = malloc(fileSize);
	if (mem == NULL) {
		fprintf(stderr, "cannot allocate memory\n");
		exit(3);
	}
	fread(mem, fileSize, 1, fh);
	fclose(fh);
	/* check if file is empty */
	if (fileSize == 0) {
		return mem;
	}
	/* check if file size is odd */
	if (fileSize % 2) {
		fprintf(stderr, "file size is odd\n");
		exit(5);
	}
	/* convert from big endian to little endian format */
	progSize = fileSize / 2;
	{
		int i;
		union {
			int i;
			char buf[4];
		} u;
		u.i = 0x12345678;
		if (u.buf[0] == 0x78) { /* little endian */
			for ( i = 0; i < progSize; i++) {
				mem[i] = mem[i] << 8 | ((mem[i] >> 8) & 0xff);
			}
		}
	}
	return mem;
}


/*
 * main file to open and call the disassembler function.
 * it is ready to use. you don't have to make changes.
 */
int main(int argc, char **argv) {
	if (argc != 2) {
		fprintf(stderr, "usage: %s file.obj\n", argv[0]);
		exit(1);
	}
	Disassemble(ReadFile(argv[1]));
}
