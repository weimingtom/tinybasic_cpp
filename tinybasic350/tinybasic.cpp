// https://qiita.com/iigura/items/6980aa5612817653626a

// TinyBasic350 ( 350 lines tiny basic by Koji Iigura, 2022.)
// original : https://gist.github.com/pmachapman/661f0fff9814231fde48
// reference: https://qiita.com/thtitech/items/91e2456c989ca969850d
// to build : g++ -std=c++20 tinyBasic350.cpp
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <math.h>
#include <string>
#include <vector>
#include <map>

bool USE_DEBUG = false;

enum Type    { Invalid, Delimiter, Variable, Number, Command, String, Quote, };
enum Keyword { INVALID, PRINT, INPUT, IF, THEN, GOTO, FOR, NEXT, TO, GOSUB, RETURN,
               END, REM, EOL, FINISHED, };
enum Error   { SyntaxError, Parentheses, NoExp, NoEqualSign, NotAVariable, DupLabel,
               NoSuchALabel, NoThen, NoTo, MismatchNext, MismatchReturn, SystemError };
struct LoopInfo { int counterVarID; int targetValue; char *loopTopPos; };

const int ProgSize=10000;
char *gProgPos; // holds expression to be analyzed 
int gVariables[26]={ 0 };
char gToken[80]; Type gTokenType; Keyword gTokVal;

bool isDelim(char inChar) {
	return strchr(" ;,+-<>/*%^=()",inChar)!=0
		   || inChar=='\t' || inChar=='\r' || inChar=='\n' || inChar=='\0';
}

std::map<Error,const char*> gErrDict;

void init1() {
	gErrDict[MismatchReturn] = "RETURN without GOSUB";
	gErrDict[NoTo] = "TO expected";
	gErrDict[Parentheses] = "unbalanced parentheses";
	gErrDict[NoThen] = "THEN expected";
	gErrDict[NoEqualSign] = "equals sign expected";
	gErrDict[DupLabel] = "duplicate label";
	gErrDict[MismatchNext] = "NEXT without FOR";
	gErrDict[SyntaxError] = "syntax error";
	gErrDict[NoSuchALabel] = "undefined label";
	gErrDict[NoExp] = "no expression present";
	gErrDict[NotAVariable] = "not a variable";
	gErrDict[SystemError] = "system error";
}

void error(Error inErrorID) {
	if(gErrDict.find(inErrorID)==gErrDict.end()) {
		fprintf(stderr,"SYSTEM ERROR (no such error ID).\n"); exit(-2);
	}
  	printf("%s\n",gErrDict[inErrorID]); exit(-1);
}

std::vector<LoopInfo> gFStack; // loop info for FOR-NEXT
void fpush(LoopInfo inLoopInfo) { gFStack.push_back(inLoopInfo); }
LoopInfo fpop() {
	if(gFStack.size()==0) { error(MismatchNext); }
	LoopInfo loopInfo=gFStack.back(); gFStack.pop_back(); return loopInfo;
}

std::vector<char*> gGStack; // call stack for GOSUB-RETURN
void gpush(char *inProgPos) { gGStack.push_back(inProgPos); }
char *gpop() {
	if(gGStack.size()==0) { error(MismatchReturn); }
	char *progPos=gGStack.back(); gGStack.pop_back(); return progPos;
}

std::map<std::string,Keyword> gMap;

void init2() {
	gMap["print"] = PRINT;
	gMap["input"] = INPUT;
	gMap["if"] = IF;
	gMap["then"] = THEN;
	gMap["goto"] = GOTO;
	gMap["for"] = FOR; 
	gMap["next"] = NEXT;
	gMap["to"] = TO;
	gMap["gosub"] = GOSUB;
	gMap["return"] = RETURN;
	gMap["end"] = END;
	gMap["rem"] = REM;
}

Keyword lookUpCommand(char *inToken) {
	if (USE_DEBUG) {
		printf("<<< lookUpCommand: %s, %d\n", inToken, strlen(inToken));
	}
	char tempToken[80] = {0};
	for (int i = 0; i < strlen(inToken); ++i) {
		tempToken[i] = tolower(inToken[i]);
	}
	return gMap.find(tempToken)==gMap.end() ? INVALID : gMap[tempToken];
}

std::map<std::string,char*> gLabelTable;
bool isUniqueLabel(char *inLabel) {
	return gLabelTable.find(inLabel)==gLabelTable.end();
}
char *findLabel(char *inLabel) {
	return gLabelTable.find(inLabel)==gLabelTable.end() ? NULL : gLabelTable[inLabel];
}

char *loadProgram(char *inFileName) {
	FILE *fp=fopen(inFileName,"rb");
	if(fp==NULL) { return NULL; }
	char *bufTop=(char *)malloc(ProgSize);
	if(bufTop!=NULL) {
		char *p=bufTop;
		for(int i=0; feof(fp)==0 && i<ProgSize; p++,i++) { *p=getc(fp); }
		//*(p-2)='\0';	// null terminate the program
		*(p-1)='\0'; //FIXME: remove getc returned EOF '\xff'
	}
	fclose(fp);
	return bufTop;
}

Type getToken() {
	if (USE_DEBUG) {
		printf(">>>gToken: %s%<<<\n", gToken);
	}
	gTokenType=Invalid; gTokVal=INVALID;
	char *dest=gToken;
	if(*gProgPos=='\0') { // end of file?
		*gToken='\0'; gTokVal=FINISHED; gTokenType=Delimiter; goto leave;
	}
	while(*gProgPos==' ' || *gProgPos=='\t') { ++gProgPos; } 
	if(gProgPos[0]=='\r' && gProgPos[1]=='\n') {	// crlf?
		gProgPos+=2; gToken[0]='\r'; gToken[1]='\n'; gToken[2]='\0'; gTokVal=EOL;
		gTokenType=Delimiter; goto leave;
	}
	if(*gProgPos=='\n') { // lf?
		gProgPos++; gToken[0]='\n'; gToken[1]='\0'; gTokVal=EOL;
		gTokenType=Delimiter; goto leave;
	}
	if( strchr("+-*^/%=;(),><",*gProgPos) ) { // delimiter?
		*dest=*gProgPos; gProgPos++; *(++dest)='\0'; gTokenType=Delimiter; goto leave;
	}
	if(*gProgPos=='"') { // quoted string?
		gProgPos++; while(*gProgPos!='"' && *gProgPos!='\r') { *dest++=*gProgPos++; }
		if(*gProgPos=='\r') { error(Parentheses); }
		gProgPos++; *dest='\0'; gTokenType=Quote; goto leave;
	}
	if( isdigit(*gProgPos) ) { // number?
		while(isDelim(*gProgPos)==false) { *dest++=*gProgPos++; }
		*dest='\0'; gTokenType=Number; goto leave;
	}
	if( isalpha(*gProgPos) ) { // var or command?
		while(isDelim(*gProgPos)==false) { *dest++=*gProgPos++; }
		gTokenType=String;
	}
	*dest='\0';
	if(gTokenType==String) { // see if a string is a command or a variable
		gTokVal=lookUpCommand(gToken);	// convert to internal rep
		if (USE_DEBUG) {
			printf("<<< getToken String\n");
		}
		gTokenType = gTokVal==INVALID ? Variable : Command;
	}
leave: return gTokenType;
}

// return a token to input stream.
void putBack() { for(char *t=gToken; *t!='\0'; t++) { gProgPos--; } }

int findVar(char *inToken) {
	if(isalpha(*inToken)==0) { error(NotAVariable); }
	return gVariables[toupper(*gToken)-'A'];
}

bool isSameLine() { return gTokVal!=EOL && *gProgPos!='\0'; }

int getExp();
int primitive() {
	int ret=-1;
	switch(gTokenType) {
		case Variable: ret=findVar(gToken); getToken(); break;
		case Number:   ret=atoi(gToken);    getToken(); break;
		default: error(SyntaxError);
	}
	if(*gProgPos=='^') { gProgPos++; ret=(int)pow(ret,getExp()); }
	return ret;
}

int factor() {
	getToken();
	switch(*gToken) {
		case '(': { int ret=getExp(); getToken();
			if(*gToken!=')') { error(Parentheses); }
			getToken(); return ret; }
		case '+': return factor();
		case '-': return -factor();
	}
	return primitive();
}

int term() {
	int prod=factor();
	while(isSameLine() && (*gToken=='*' || *gToken=='/' || *gToken=='%')) {
		char op=*gToken; 
		prod = op=='*' ? prod*factor() : op=='/' ? prod/factor() : prod%factor();
	}
	return prod;
}

int getExp() {
	if(*gProgPos=='\0') { error(NoExp); }
	int sum=term();
	while(isSameLine() && (*gToken=='+' || *gToken=='-')) {
		sum += *gToken=='+' ? term() : -term(); // otherwise '-'.
	}
	putBack();
	return sum;
}

void assignment() {
	getToken();	// get the variable name
	if(isalpha(*gToken)==0) { error(NotAVariable); }
	int var=toupper(*gToken)-'A';
	getToken(); // get equal sign
	if(*gToken!='=') { error(NoEqualSign); }
	gVariables[var]=getExp();
}

// find the start of the next line.
void findEol() {
	while(*gProgPos!='\n' && *gProgPos!='\0') { ++gProgPos; }
	if( *gProgPos ) { gProgPos++; }
}

// find all labels
void scanLabels() {
	char *progPosBackup=gProgPos; // save pointer to top of program 
	getToken();
	if(gTokenType==Number) { gLabelTable[gToken]=gProgPos; }
	findEol();
	do {
		getToken();
		if(gTokenType==Number) {
			if(isUniqueLabel(gToken)==false) { error(DupLabel); }
			gLabelTable[gToken]=gProgPos;
		}
		if(gTokVal!=EOL) { findEol(); } // if not on a blank line, find next line 
	} while(gTokVal!=FINISHED);
	gProgPos=progPosBackup;
}

void execPrint() {
	char lastDelim;
	int len=0;
	do {
		getToken();	// get next list item
		if(gTokVal==EOL || gTokVal==FINISHED) { break; }
		if(gTokenType==Quote) { // is string
			printf("%s",gToken);
			len+=strlen(gToken);
			getToken();
		} else { // the case of expression
			putBack();
			int value=getExp();
			getToken();
			len+=printf("%d",value);
		}
		lastDelim=*gToken;
		if(*gToken==';') { // compute number of spaces to move to next tab
			for(int spaces=8-(len%8); spaces>0; spaces--) { printf(" "); }
		} else if(*gToken==',') { /* do nothing  */
		} else if(gTokVal!=EOL && gTokVal!=FINISHED) { error(SyntaxError); }
	} while(*gToken==';' || *gToken==',');
	if(gTokVal==EOL || gTokVal==FINISHED) {
		if(lastDelim!=';' && lastDelim!=',') { printf("\n"); }
	} else {
		error(SyntaxError);
	}
}

// execute a simple form of the BASIC INPUT command
void execInput() {
	getToken();	// see if prompt string is present
	if(gTokenType==Quote) {
		printf("%s",gToken);	// if so, print it and check for comma
		getToken();
		if(*gToken!=',') { error(Parentheses); }
		getToken();
	} else {
		printf("? ");	// therwise, prompt with 
	}
	char var=toupper(*gToken)-'A'; // get the input var
	int i; scanf("%d",&i);	// read input
	gVariables[var]=i;
}

void execGoto() {
	getToken();
	char *pos=findLabel(gToken);
	if(pos==NULL) { error(NoSuchALabel); }
	gProgPos=pos;
}

void execGosub() {
	getToken();
	char *pos=findLabel(gToken);
	if(pos==NULL) { error(NoSuchALabel); }
	gpush(gProgPos);
	gProgPos=pos;
}

void execReturn() { gProgPos=gpop(); }

void execIf() {
	int x=getExp();	// get left expression
	getToken();	    // get the operator
	if(strchr("=<>",*gToken)==0) { error(SyntaxError); }
	char op=*gToken;
	int y=getExp();
	// determine the outcome
	bool cond=false;
	switch(op) {
		case '<': cond = x<y;  break;
		case '>': cond = x>y;  break;
		case '=': cond = x==y; break;
	}
	if( cond ) { // is true so process target of IF
		getToken();
		if(gTokVal!=THEN) { error(NoThen); }
		// 'else' program execution starts on next line 
	} else {
		findEol();
	}
}

void execFor() {
	getToken();	// read the control variable
	if(isalpha(*gToken)==0) { error(NotAVariable); }
	LoopInfo loopInfo; loopInfo.counterVarID=toupper(*gToken)-'A';
	getToken();	// read the equals sign
	if(*gToken!='=') { error(NoEqualSign); }
	int value=getExp();
	gVariables[loopInfo.counterVarID]=value; // get initial value
	getToken();
	if(gTokVal!=TO) { error(NoTo); }
	loopInfo.targetValue=getExp();	// get target value
	// if loop can execute at least once, push info on stack
	if(value>=gVariables[loopInfo.counterVarID]) {
		loopInfo.loopTopPos=gProgPos;
		fpush(loopInfo);
	} else { // otherwise, skip loop code altogether
		while(gTokVal!=NEXT) { getToken(); }
	}
}

void execNext() {
	LoopInfo loopInfo=fpop(); // read the loop info
	gVariables[loopInfo.counterVarID]++; // increment control variable
	if(gVariables[loopInfo.counterVarID]>loopInfo.targetValue) { return; } // all done
	fpush(loopInfo);
	gProgPos=loopInfo.loopTopPos;	// loop!
}

void execRem() { for(getToken(); gTokVal!=EOL; getToken()) { /* empty */ } }

std::map<Keyword,void (*)()> gCmdMap;
void init3() {
	gCmdMap[PRINT] = execPrint;
	gCmdMap[INPUT] = execInput;
	gCmdMap[GOTO] = execGoto;
	gCmdMap[IF] = execIf;
	gCmdMap[GOSUB] = execGosub;
	gCmdMap[RETURN] = execReturn;
	gCmdMap[FOR] = execFor;
	gCmdMap[NEXT] = execNext;
	gCmdMap[REM] = execRem;
};

int main(int argc,char *argv[]) {
	init1();
	init2();
	init3();
	if(argc!=2) { fprintf(stderr,"usage: tinybasic <filename>\n"); exit(-1); }
	gProgPos=loadProgram(argv[1]);
	if(gProgPos==NULL) { fprintf(stderr,"can not load program.\n"); exit(-1); }
	scanLabels();	// find the labels in the program 
	if (USE_DEBUG) {
		printf("<<<<< start loop\n");
	}
	do {
		gTokenType=getToken();
		if(gTokenType==Variable) { // check for assignment statement
			if (USE_DEBUG) {
				printf("<<<<< start loop Variable\n");
			}
			putBack();		// eturn the var to the input stream 
			assignment();	// must be assignment statement
		} else {
			if(gTokVal==END) { break; }
			if(gCmdMap.find(gTokVal)!=gCmdMap.end()) { gCmdMap[gTokVal](); }
		}
	} while(gTokVal!=FINISHED);
	return 0;
}

