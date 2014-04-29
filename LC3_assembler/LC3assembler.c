/* Programming project
 * Name: Yu Xiang, Zheng
 * Nickname: Slighten
 * ID: 101021120
 * Due date: 2014-01-20 23:59
 * Filename: LC3assembler.c
 * Enunciation: 
 * 	This is a simple LC3 assembler, which works nearly like the Windows version
 * 	LC3 assembler downloaded on 
 * 	http://highered.mcgraw-hill.com/sites/0072467509/student_view0/lc-3_simulator.html
 * 	The remain informations can be seen in Readme.txt
 * Status:
 *  Compile, execute successfully and correctly.	
 */  
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#define BUFLEN 100

/*****************variables declaration************************/
//opcode
static char *op[] = {
	"BR", "ADD", "LD", "ST", "JSR", "AND", "LDR", "STR",
	"RTI", "NOT", "LDI", "STI", "JMP", "RESERVED", "LEA", "TRAP"
};

//directive
static char *dir[] = {
	".ORIG", ".END", ".BLKW", ".STRINGZ", ".FILL"
};

//pseudo
char *pseudo[] = {
	"HALT", "IN", "OUT", "GETC", "PUTS"
};

static int opcode, JSRflag, address, PC, dst, src1, src2, basereg, imm5, trapvect8, IR, Progstart, Progend, blkw, Recordnum;

static short mem[65536];
static char *ptr[65536];

static char str[65536], oristr[65536], buf[65536], oribuf[65536], test[65536], stringz[65536], labelbuf[65536][BUFLEN]; 

static struct LABEL{
	char label[BUFLEN];	//name
	short address;		//left label's PC
	short mapping[BUFLEN];	//opcode called label's PC
	int redefine;		//test if duplicated define
	short differ[BUFLEN];	//pass 2 - count add & map difference
	int linenumber[BUFLEN];	//opcode called label's line
	int linerecord;		//left label's line
	int length;		//label's length
	int labelcount; //count nth times opcode call the same label 
	int PCoffset11[BUFLEN]; //1 or 0
} labelstruct[65536];

static int argoffset = 0, arg1, arg2, arg3, labelamount, errornum = 0, errornum2 = 0, linenum = 0, argsave = 0, remainarg = 0, foundORIGflag = 0, NeedaLabel = 0, doublelabel = 0, checkin = 0;

static struct CMPSTRUCT {
	short opnum;
	short dirnum;	
} cmpstruct,oristruct;

static struct CMPSTRUCT oristruct = {-1,-1};

/*******************functions**********************/
int FindMaxLength(){
	int i, maxindex = -1;
	for (i = 0 ; i < 65536 ; i++){
		if(labelstruct[i].label[0] && labelstruct[i].address && (maxindex < 0 || labelstruct[i].length > labelstruct[maxindex].length)){
			maxindex = i;
		}
	}
	return labelstruct[maxindex].length;
}
				
void WriteSymbolFile(char *name){
	int i = 0, max, cmp;
	name[strlen(name)-3] = '\0';
	strcat(name, "sym");
	FILE *fhsym = fopen(name, "w");
	max = FindMaxLength();
	cmp = (max < 12) ? (12 + 5) : (max + 5);
	//do this in case label is too long
	fprintf(fhsym, "// Symbol table\n"
		       "// Scope level 0:\n"
		       "//      %-*s  Page Address\n"
		       "//      ", cmp, "Symbol table");
	for (i = 0 ; i < cmp ; i++){
		fprintf(fhsym,"-");
	}
	fprintf(fhsym, "  ------------\n");
	for (i = 0 ; i < 65536 ; i++){
		if(labelstruct[i].label[0] && labelstruct[i].address)
			fprintf(fhsym,"//      %-*s  %04hX\n", cmp, labelstruct[i].label, labelstruct[i].address);
	}
	fclose(fhsym);
}



void Saveargoffset(int arg){
	argsave = arg;
}

int CheckisLabelForm(char *lb){
	//label has to start with letter or _
	if ((*lb >= 'a' && *lb <= 'z') || (*lb >= 'A' && *lb <= 'Z' ) || (*lb == '_')){
		return 1;
	}
	return 0;
}

void RecordLabel(int address, int mapping, char *lb){
	int i = 0;
	static struct LABEL temp;//for swap 
	while (labelstruct[i].label[0] && strcmp(labelstruct[i].label, lb)){
		i++;
	}
	strcpy(labelstruct[i].label, lb);
	//if find the label on left side but not duplicated
	if(address && !labelstruct[i].redefine){
		labelstruct[i].redefine = 1;
		labelstruct[i].length = strlen(labelstruct[i].label);
		labelstruct[i].linerecord = linenum;
		labelstruct[i].address = address;
		//swap in order to sort as the order of found on left side
		temp = labelstruct[Recordnum];
		labelstruct[Recordnum] = labelstruct[i];
		labelstruct[i] = temp;
		Recordnum++;
	}
	//if found after opcode
	else if(mapping){
			if (JSRflag){
					labelstruct[i].PCoffset11[labelstruct[i].labelcount] = 1;
					JSRflag = 0;
			}
		labelstruct[i].mapping[labelstruct[i].labelcount] = mapping;
		labelstruct[i].linenumber[labelstruct[i].labelcount++] = linenum;
	}
	//duplicated define
	else{
		fprintf(stderr, "Line %d: Duplicate label '%s' found with label on line %d\n", linenum, lb, labelstruct[i].linerecord);
		errornum++;
	}
}



void UpperCase(char buf[]){
	int i = 0;
	while (*(buf + i)){
		buf[i] = ('a' <= buf[i] && buf[i] <= 'z') ? 
			  (buf[i] - 'a' + 'A') : (buf[i]);
		i++;
	};
}

void Initialtable(){
	cmpstruct = oristruct;
	arg1 = arg3 = arg2 = src1 = src2 = imm5 = dst = trapvect8 = basereg = 0;
}

void errormessage(int code, int line, char *str){
	errornum++;
	switch (code){
		case 1:
			fprintf(stderr, "Line %d: Expected 16-bit value, but found '%s' instead\n", line, str);
			break;
		case 2:
			fprintf(stderr, "Line %d: Expected .ORIG, but found '%s' instead\n", line, str);
			break;
		case 3:
			fprintf(stderr, "Line %d: Invalid label '%s'\n", line, str);
			break;
		case 4:
			fprintf(stderr, "Line %d: Already have .ORIG before, but found another .ORIG here\n", line);
			break;
		case 5:
			fprintf(stderr, "Line %d: Expected string constant, but found '%s' instead\n", line, str);
			break;
		case 6:
			fprintf(stderr, "Line %d: Expected register operand, but found '%s' instead\n", line, str);
			break;
		case 7: 
			fprintf(stderr, "Line %d: Expected label, but found '%s' instead\n", line, str);
			break;
		case 8:
			fprintf(stderr, "Line %d: Expected .END at the end of file\n", line);
			break;
		case 9:
			fprintf(stderr, "Line %d: Instruction references label '%s' that cannot be represented in a 9 bit signed PC offset\n", line, str);
			errornum--;
			errornum2++;
			break;
		case 10:
			fprintf(stderr, "Line %d: Instruction uses memory beyond memory location xFFFF\n", line);
			break;
		case 11:
			fprintf(stderr, "Line %d: Expected 8 bit non-negative trap vector, but found '%s' instead\n", line, str);
			break;
		case 12:
			fprintf(stderr, "Line %d: Expected register or immediate value, but found '%s' instead\n", line, str);
			break;
	}
}

void errormessage2(int code, int line, int num){
	errornum++;
	switch (code){
		case 1:
			fprintf(stderr, "Line %d: %d can not be represented as a signed number in 5 bits\n", line, num);
			break;
		case 2:
			fprintf(stderr, "Line %d: Register %d does not exist\n", line, num);
			break;
		case 3: 
			fprintf(stderr, "Line %d: %x can not be represented as an 8 bit trap vector\n", line, num);
			break;
		case 4:
			fprintf(stderr, "Line %d: %d can not be represented as a signed number in 9 bits\n", line, num);
			break;
		case 5:
			fprintf(stderr, "Line %d: %d can not be represented as an unsigned number in 16 bits\n", line, num);
			break;
		case 6:
			fprintf(stderr, "Line %d: %d can not be represented as a signed number in 6 bits\n", line, num);
			break;
		case 7:
			fprintf(stderr, "Line %d: %d can not be represented as a signed number in 11 bits\n", line, num);
			break;
	}
}

void errormessage3(int code, int line, char *name){
	errornum2++;
	switch (code){
		case 1:
			fprintf(stderr, "Line %d: Instruction references undefined label '%s'\n", line, name);
			break;
		case 2:
			fprintf(stderr, "Line %d: Instruction references non-address memory location -1\n", line);
			break;
	}
}
void EnterInstruction(int address, short IR, char *lb){
	ptr[address] = lb;
	mem[address] = IR;
	PC++;
	if (PC > 0xFFFF){
		errormessage(10, linenum, NULL);
		fprintf(stderr, "Pass 1 - 1 error(s)\n");
		exit(8);
	}
}

void EnterData(int address, short data){
	mem[address] = data;
	PC++;
	if (PC > 0xFFFF){
		errormessage(10, linenum, NULL);
		fprintf(stderr, "Pass 1 - 1 error(s)\n");
		exit(8);
	}
}

void EnterReg2onlyInstr(int address, int opcode, int dst, int src){
	EnterInstruction(address, (opcode << 12) | (dst << 9) | (src << 6) | 63, NULL);
}

void EnterReg2Imm5Instr(int address, int opcode, int dst, int src1, int imm5){
	EnterInstruction(address, (opcode << 12) | (dst << 9) | (src1 << 6) | (1 << 5) | (imm5 & 0x1F), NULL);
}

void EnterReg3Instr(int address, int opcode, int dst, int src1, int src2){
	EnterInstruction(address, (opcode << 12) | (dst << 9) | (src1 << 6) | (0 << 3) | src2, NULL);
}

void EnterReg2offset6Instr(int address, int opcode, int dst, int base, int offset6){
	EnterInstruction(address, (opcode << 12) | (dst << 9) | (base << 6) | (offset6 & 0x3F), NULL);
}
//check if fgets too many words
void CheckIfTooManyWords(char *buffer, int line){
	if(buffer[strlen(buffer)] != '\0'){
		fprintf(stderr, "Line %d: This line has too many words!\n"
				"Pass 1 - 1 error(s)\n", line);
		errornum++;
		exit(7);
	}
	return;
}

//skip whitespace, newline, comment, and line started with ','
void SkipSpace(int *arg, FILE *fh){
	//start with these, get new line directly
	if(buf[*arg] == '\0' || buf[*arg] == '\n' || buf[*arg] == ';'){
		//same as first several line of FirstPass()
		fgets(buf, 65536, fh);
		linenum++;
		CheckIfTooManyWords(buf, linenum);
		strcpy(oribuf, buf);
		UpperCase(buf);
		argoffset = argsave = 0;
		//recursively check if confirm
		SkipSpace(arg, fh);
	}
	//skip space
	else{
		for(;buf[*arg] && buf[*arg] <= ' ' ; (*arg)++){
		}
		if(buf[*arg] == '\0' || buf[*arg] == '\n' || buf[*arg] == ';')
			SkipSpace(arg, fh);
		if(buf[*arg] == ','){
			(*arg) += 1;
			SkipSpace(arg, fh);
		}
	}
}

void Checkifin16bits(){
	if (address < 65536 && address > 0)
		EnterData(PC, address);
	else
		errormessage2(5, linenum, address);
}

void CheckifinPCoffset9(){
	if (address <= 255 && address >= -256)
		EnterInstruction(PC, IR|(address & 0X1ff), NULL);
	else 
		errormessage2(4 , linenum, address);
}

void CheckifinPCoffset11(){
	if (address <= 1023 && address >= -1024)
		EnterInstruction(PC, IR|(address & 0X7ff), NULL);
	else 
		errormessage2(7 , linenum, address);
}

void ScanAnIntRoutione(){
	sscanf(buf + argoffset, "#%n", &arg1);//decimal
	sscanf(buf + argoffset, "X%n", &arg3);//hex
	argoffset += arg1 + arg3;
	sscanf(buf + argoffset, "0X%n", &arg2);//hex
}

void ScanIntorLabelRoutine(int i){
	ScanAnIntRoutione();
	//if not num, may need a label
	if ( (arg3 && arg2) || (sscanf(buf + argoffset , (arg2 || arg3) ? "%X%n" : "%d%n", &address, &arg2) != 1) ){ 
		NeedaLabel = 1; 
		argoffset -= arg1 + arg3;
		arg1 = arg2 = arg3 = 0;
	}
	else{
		switch (i){
			case 1:
				Checkifin16bits();
				break;
			case 2:
				CheckifinPCoffset9();
				break;
			case 3: 
				CheckifinPCoffset11();
				break;
		}
	}
}

int FirstPass(FILE *fh, char *name){
	int i = 0;
	//get new line
	while (fgets(buf, 65536, fh)){
		linenum++;
		CheckIfTooManyWords(buf, linenum);
		strcpy(oribuf, buf);	//save non-uppercased line
		UpperCase(buf);
		argoffset = argsave = 0;	//initialize argoffset
		//get new words
		while (sscanf(oribuf + argoffset, "%99s%n", str, &argoffset) == 1){	
			argoffset += argsave;	//plus saved last time
			if (*str == ';'){	//comment
				break;
			}
			strcpy(oristr, str);	//save non-uppercased words for labels
			UpperCase(str);
			Initialtable();
			
			//test if match any of directive, opcode, or pseudos
			for(i = 0 ; i < 5 ; i++){
				if (!strcmp(dir[i], str)){
					cmpstruct.dirnum = i; 
					break;
				}
			}
			for(i = 0 ; i < 16 ; i++){
				if (!strcmp(str, "RET")){
					cmpstruct.opnum = 12;	//same as JMP R7
					break;
				}
				if (!strcmp(str, "JSRR")){
					cmpstruct.opnum = 4;	//same as JSR
					break;
				}	
				if ((!strcmp(op[i], str) || !strcmp(str, "BRN") || !strcmp(str, "BRZ") || !strcmp(str, "BRP") || !strcmp(str, "BRNZ") || !strcmp(str, "BRNP")|| !strcmp(str, "BRZP") || !strcmp(str, "BRNZP"))){
					cmpstruct.opnum = i;
					break;
				}
			}
			for(i = 0 ; i < 5 ; i++){
				if (!strcmp(pseudo[i], str)){	
					cmpstruct.opnum = 15;	 //one of Trap
					break;
				}
			}
			
/*start*/
			//if not found .ORIG
			if(!foundORIGflag){
				//if is .ORIG
				SkipSpace(&argoffset, fh);
				if (cmpstruct.dirnum == 0){
					ScanAnIntRoutione();
					//if not hexadecimal or decimal num 
					if ( (arg3 && arg2) || (sscanf(buf + argoffset, (arg2 || arg3) ? "%X%n" : "%d%n", &address, &arg2) != 1 ) ){ 
						if (arg3 && arg2)
							foundORIGflag = 1;
						argoffset -= arg1 + arg3;
						sscanf(buf + argoffset, "%99s%n", test, &arg2);
						errormessage(1, linenum, test);
					}
					//if is, then set PC, set flag, and record in mem
					else if (address < 65536 && address > 0){
						Progstart = PC = address - 1;
						foundORIGflag = 1;
						EnterData(PC, address);
					}
					//out of 16 bits range
					else
						errormessage2(5, linenum, address);
				}
				//else not .ORIG
				else {
					errormessage(2, linenum, oristr);
					break;
				}
			}
			//if is label(no match opcode or directive or pseudo)
			else if (cmpstruct.opnum == -1 && cmpstruct.dirnum == -1){
				//if confirm label's form
				if (CheckisLabelForm(oristr)){
					//if is after opcode
					if(NeedaLabel){
						//do this since ptr is a pointer
						//if point to oristr, then when oristr changes,
						//ptr changes.
						strcpy(labelbuf[checkin], oristr);
						RecordLabel(0, PC, labelbuf[checkin]);
						EnterInstruction(PC, IR, labelbuf[checkin]);
						NeedaLabel = 0;
						IR = 0;
						checkin++;
					}
					//left-sided label
					else{
						RecordLabel(PC, 0, oristr);
					}
				}
				//not label's form
				else{
					errormessage(3, linenum, oristr);
				}
			}
			
			//if no need a label & is one of opcode, directive, or pseudo
			else if (!NeedaLabel && (cmpstruct.opnum != -1 || cmpstruct.dirnum != -1)){
				//if is one of directive
				 SkipSpace(&argoffset, fh);
				 if (cmpstruct.dirnum != -1){
					switch (cmpstruct.dirnum){
						case 0://.ORIG(have found one before)
							errormessage(4, linenum, NULL);
							break;
						case 1://.END
							Progend = PC;
							if(!errornum)	//if no error
								WriteSymbolFile(name);
							return errornum;
						case 2://.BLKW
							ScanAnIntRoutione();
							//if not num
							if ( (arg3 && arg2) || (sscanf(buf + argoffset, (arg2 || arg3)? "%X%n" : "%d%n", &blkw, &arg2) != 1) ){ 
								argoffset -= arg1 + arg3;
								sscanf(buf + argoffset, "%99s%n", test, &arg2);
								errormessage(1, linenum, test);
							}
							else if (blkw >= 65536 || blkw < 0)
								errormessage2(5, linenum, blkw);
							else if (blkw + PC > 0XFFFF)
								errormessage(10, linenum, NULL);
							else
								PC += blkw;
							break;
						case 3://.STRINGZ
							//if the form is "..." 
							{int i = argoffset + 1;
								while (i < strlen(oribuf)){
									if(oribuf[i] == '"'){
											arg2 = i - argoffset + 1;
											break;
									}
									i++;
								}
							}
							if (arg2 && *(oribuf + argoffset) == '"'){
								int i = 0;
								for (i = argoffset + 1 ; i < argoffset + arg2 - 1; i++){
									EnterData(PC, oribuf[i]);
								}
								EnterData(PC, 0);
							}
							else{
								errormessage(5, linenum, stringz);
							}
							break;
						case 4://.FILL
							IR = 0;
							ScanIntorLabelRoutine(1);
							break;
						}
				 }
				//if one of opcode
				else if (cmpstruct.opnum != -1){
					opcode = cmpstruct.opnum;
					switch (cmpstruct.opnum){
						case 0://BR
							IR = opcode << 12;
							if (strchr(str, 'N'))
								IR = IR | (1 << 11);
							if (strchr(str, 'Z'))
								IR = IR | (1 << 10);
							if (strchr(str, 'P'))
								IR = IR | (1 << 9);
							ScanIntorLabelRoutine(2);
							break;
						case 1://ADD
						case 5://AND
						case 6://LDR
						case 7://STR
						case 9://NOT
							if(sscanf(buf + argoffset, "R%d%n", &dst, &arg2) == 1){ 
								if (dst>7||dst<0)//R1~R7
									errormessage2(2, linenum, dst);
								argoffset += arg2;
								SkipSpace(&argoffset, fh);
								if(sscanf(buf + argoffset,"R%d%n", &src1, &arg2) == 1){ 
									if(src1>7||src1<0)
										errormessage2(2, linenum, src1);
									if(opcode == 9){//NOT instr
										EnterReg2onlyInstr(PC, opcode, dst, src1);
										break;
									}
									argoffset += arg2;
									SkipSpace(&argoffset, fh);
									if(sscanf(buf + argoffset,"R%d%n", &src2, &arg2) == 1){
										if(src2>7||src2<0)
											errormessage2(2, linenum, src2);
										EnterReg3Instr(PC, opcode, dst, src1, src2);
									}
									else if((sscanf(buf + argoffset, "#%d%n", &imm5, &arg2) == 1 || sscanf(buf + argoffset, "X%X%n", &imm5, &arg2) == 1 || sscanf(buf + argoffset, "0X%X%n", &imm5, &arg2) == 1 || sscanf(buf + argoffset, "%d%n", &imm5, &arg2) == 1) && (!strstr(buf + argoffset + 1, "0X"))){
										if (opcode < 6){//AND,ADD
											if (imm5 > 15 || imm5 < -16)
										errormessage2(1, linenum, imm5);	
											EnterReg2Imm5Instr(PC, opcode, dst, src1, imm5);
										}
										else{//STR,LDR
											if (imm5 > 31 || imm5 < -32)
										errormessage2(6, linenum, imm5);	
											EnterReg2offset6Instr(PC, opcode, dst, src1, imm5);
										}
									}
									else{
										sscanf(oribuf + argoffset, "%s%n", test, &arg2);
										errormessage(12, linenum, test);
									}
								}
								else{
									sscanf(oribuf + argoffset, "%s%n", test, &arg2);
									if (test[strlen(test) - 1] == ',')
										test[strlen(test) - 1] = '\0';
									errormessage(6, linenum, test);
								}
							}
							else{
								sscanf(oribuf + argoffset, "%s%n", test, &arg2);
								if (test[strlen(test) - 1] == ',')
									test[strlen(test) - 1] = '\0';
								errormessage(6, linenum, test);
							}
							break;
						 case 2: /* LD */
						 case 3: /* ST */
						 case 0xA: /* LDI */
						 case 0xB: /* STI */
					     case 0xE: /* LEA */
							if (sscanf(buf + argoffset, "R%d%n", &dst, &arg2) == 1){
								if(dst>7||dst<0)
									errormessage2(2, linenum, dst);
								argoffset += arg2;
								SkipSpace(&argoffset, fh);//skip ','
								arg2 = 0;
								//if a num
								IR = (opcode << 12) | (dst << 9);
								ScanIntorLabelRoutine(2);
							}
							//not R1~R7
							else{
								sscanf(oribuf + argoffset, "%s%n", test, &arg2);
								if (test[strlen(test) - 1] == ',')
									test[strlen(test) - 1] = '\0';
								errormessage(6, linenum, test);
							}
							break;
						case 4://JSR
							if (strstr(str,"JSRR")){
								if (sscanf(buf + argoffset, "R%d%n", &basereg, &arg2) == 1){ 
									if(basereg>7||basereg<0)
										errormessage2(2, linenum, basereg);
									EnterData(PC, (opcode << 12) | (basereg << 6));
								}
								else{
									sscanf(oribuf + argoffset, "%s%n", test, &arg2);
									if (test[strlen(test) - 1] == ',')
										test[strlen(test) - 1] = '\0';
									errormessage(6, linenum, test);
								}
							}
							else{
								IR = (opcode << 12) | (1 << 11);
								JSRflag = 1;
								ScanIntorLabelRoutine(3);
							}
							break;
						case 8://RTI
							EnterData(PC, (opcode << 12));
							break;
						case 0xC://JMP
							if (strstr(str, "RET"))
								EnterData(PC, (opcode << 12) | (7 << 6));
							else{ 
								if (sscanf(buf + argoffset, "R%d%n", &basereg, &arg2) == 1){
									if(basereg>7||basereg<0)
										errormessage2(2, linenum, basereg);
									EnterData(PC, (opcode << 12) | (basereg << 6));
								}
								else{
									sscanf(oribuf + argoffset, "%s%n", test, &arg2);
									if (test[strlen(test) - 1] == ',')
										test[strlen(test) - 1] = '\0';
									errormessage(6, linenum, test);
								}
							}
							break;	
						case 0xD://RESERVED
							break;
						case 0xF://TRAP
							if((sscanf(buf + argoffset, "X%X%n", &trapvect8, &arg2) == 1 || sscanf(buf + argoffset, "0X%X%n", &trapvect8, &arg2) == 1|| sscanf(buf + argoffset, "#%d%n", &trapvect8, &arg2) == 1 || sscanf(buf + argoffset, "%d%n", &trapvect8, &arg2) == 1) && !strstr(buf + argoffset + 1, "0X")){
								if(trapvect8>255||trapvect8<0)
									errormessage2(3, linenum, trapvect8);
									EnterInstruction(PC, (opcode << 12) | trapvect8, NULL);
							}
							else if(strstr(str, "HALT"))
								EnterInstruction(PC, (opcode << 12) | 0x25, NULL);
							else if(strstr(str, "IN"))
								EnterInstruction(PC, (opcode << 12) | 0x23, NULL);
							else if(strstr(str, "OUT"))
								EnterInstruction(PC, (opcode << 12) | 0x21, NULL);
							else if(strstr(str, "GETC"))
								EnterInstruction(PC, (opcode << 12) | 0x20, NULL);
							else if(strstr(str, "PUTS"))
								EnterInstruction(PC, (opcode << 12) | 0x22, NULL);
							else{
								sscanf(oribuf + argoffset, "%s%n", test, &arg2);
								errormessage(11, linenum, test);
							}
							break;
					}
				}
			}
			else if(!NeedaLabel && (cmpstruct.opnum != -1 || cmpstruct.dirnum != -1)){
				errormessage(7, linenum, oristr);
			}
			argoffset += arg2;
			Saveargoffset(argoffset);
		}
	}
	errormessage(8, linenum, NULL);
	return errornum;
}

							
void WriteObjectFile(char *name){
	int i = 0,j = 0;
	short data[Progend-Progstart];
	//extract existng data
	for (i = Progstart ; i < Progend; i++){
			data[j++] = mem[i];
			//if want to test: printf("i = %X mem[i] = %04hX\n",i ,mem[i]);
	}
	name[strlen(name)-3] = '\0';
	strcat(name, "obj");
	FILE *fhobj = fopen(name, "wb");
	{
		int i = 0;
		union {
			int i;
			char buf[4];
		} u;
		u.i = 0x12345678;
		if (u.buf[0] == 0x78) { /* little endian */
			for (i = 0 ; i < Progend - Progstart; i++) {
				data[i] = data[i] << 8 | ((data[i] >> 8) & 0xff);
			}
		}
	}
	fwrite(data, Progend - Progstart, sizeof(short), fhobj);
	fclose(fhobj);
}
				
int SecondPass(FILE *fh, char *name){
	int i = 0, j = 0, k = 0;
	for(i = 0 ; i < 65536 ; i++){
		if(labelstruct[i].label[0] && labelstruct[i].address == 0){
			//undefined label
			while (labelstruct[i].linenumber[k])
				errormessage3(1, labelstruct[i].linenumber[k++], labelstruct[i].label);
		}
		else if(labelstruct[i].label[0] && labelstruct[i].address > 0){
			for (j = 0 ; labelstruct[i].mapping[j] ; j++){
				//count difference
				labelstruct[i].differ[j] = labelstruct[i].address - (labelstruct[i].mapping[j] + 1);
				//check if over the range
				//JSR -> PCoffset11
				if (labelstruct[i].PCoffset11[j] && (labelstruct[i].differ[j] > 1023 || labelstruct[i].differ[j] < -1024))
					errormessage(9, labelstruct[i].linenumber[j], labelstruct[i].label);
				//else -> PCoffset9
				else if (labelstruct[i].differ[j] > 255 || labelstruct[i].differ[j] < -256)
					errormessage(9, labelstruct[i].linenumber[j], labelstruct[i].label);
			}
		}
	}
	//initial labelcount
	for (i = 0 ; i < 65536 ; i++){
		labelstruct[i].labelcount = 0;
	}
	for (i = Progstart ; i < Progend ; i++){
		if(ptr[i] != NULL){
			for (j = 0 ; j < 65536 ; j++){
				if (!strcmp(labelstruct[j].label, ptr[i])){
					if(labelstruct[j].address){
						mem[i] = mem[i] | (labelstruct[j].differ[labelstruct[j].labelcount++] & 0x1FF);
					}
					else
						errormessage3(2, labelstruct[j].linenumber[labelstruct[j].labelcount++], NULL);
				}
			}
		}
	}
	//doesn't have any error
	if(!errornum2)
		WriteObjectFile(name);
	return errornum2;
}

int main(int argc, char **argv){
	int pass1rti, pass2rti;
	if (argc != 2){
		fprintf(stderr,"usage: %s file.asm\n", argv[0]);
		exit(1);
	}
	if (strcmp(argv[1] + strlen(argv[1]) - 4,".asm")){
		fprintf(stderr,"file name must end with .asm\n");
		exit(2);
	}
	FILE *fhasm = fopen(argv[1],"r");
	if (fhasm == NULL){
		fprintf(stderr,"file doesn't exist\n");
		exit(3);
	}
	printf("Assembling %s\n", argv[1]);
	//pass1
	puts("Starting Pass 1...");
	pass1rti = FirstPass(fhasm, argv[1]);
	if (pass1rti){
		fprintf(stderr,"Pass 1 - %d error(s)\n", pass1rti);
		exit(4);
	}
	else
		puts("Pass 1 - 0 error(s)");
	//pass2
	puts("Starting Pass 2...");
	pass2rti = SecondPass(fhasm, argv[1]);
	if (pass2rti){
		fprintf(stderr,"Pass 2 - %d error(s)\n", pass2rti);
		exit(5);
	}
	else
		puts("Pass 2 - 0 error(s)");
	fclose(fhasm);
	return 0;
}	
