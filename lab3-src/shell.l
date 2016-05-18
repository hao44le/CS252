
/*
 *
 * CS-252 Fall 2013
 * shell.l: lexical analyzer for shell
 * You have to extend it.
 *
 */

%{

#include <string.h>
#include <unistd.h>
#include "y.tab.h"


extern "C" char * read_line();
////////////  Start added code ///////////
int mygetc(FILE * f) {
static char *p;
char ch;
if (!isatty(0)) {
  // stdin is not a tty. Call real getc
  return getc(f);
}
// stdin is a tty. Call our read_line.
if (p==NULL || *p == 0) {
  char * s = read_line();
  p = s;
}
ch = *p;
p++;
return ch;
}
#undef getc
#define getc(f) mygetc(f)
/////////// End added code ///////////
static  void yyunput (int c,char *buf_ptr  );

void myunputc(int c) {
    unput(c);
}
%}

%%

\n 		{
			return NEWLINE;
		}

[ \t] 	{
		/* Discard spaces and tabs */
     	}	

"<" 	{
     		return LESS;
    	}

">" 	{
			return GREAT;
		}
">&"    {
			return REDIRECTOUTANDERROR;
		}
">>&"   {
    		return APPENDREDIRECTANDERROR;
		}
">>" 	{ 
			return GREATGREAT; 
		} 

"|"  	{ 
			return PIPE;
		}

"&" 	{ 
			return AMPERSAND;
		}

\`[^\n]*\`    {
            yylval.string_val = strndup(yytext + 1, strlen(yytext) - 2);
            return SUBSHELL;
}  

[^ \t\n][^ \t\n]*  {
        /* Assume that file names have only alpha chars */
        yylval.string_val = strdup(yytext);
        return WORD;
}
\"[^\n\"]*\" {
    /* quoted string */
            yylval.string_val = strdup(yytext+1);
            if (yylval.string_val[yyleng-2] == '"')
            yylval.string_val[yyleng-2] = '\0';
            return WORD;
}

%%
