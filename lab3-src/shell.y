
/*
 * CS-252
 * shell.y: parser for shell
 *
 * This parser compiles the following grammar:
 *
 *	cmd [arg]* [> filename]
 *
 * you must extend it to understand the complete shell grammar
 *
 */

/* Input Library **/

%token	<string_val> WORD
%token  <string_val> SUBSHELL

%token 	NOTOKEN GREAT NEWLINE LESS GREATGREAT PIPE AMPERSAND APPENDREDIRECTANDERROR REDIRECTOUTANDERROR

%union	{
    char   *string_val;
}

%{
    //#define yylex yylex
    #include <stdio.h>
    #include <stdlib.h>
    #include <string.h>
    #include "command.h"
    #include <regex.h>
    #include <sys/types.h>
    #include <sys/stat.h>
    #include <dirent.h>
    #include <unistd.h>
    void yyerror(const char * s);
    int yylex();
    void expandWildcardsIfNecessary(char * arg);
    void processWildCardWithRecursion(char * prefix,char * suffix);
    void recursionHelper(char * prefix,char * newPrefix,char * suffix, char * component);
    int wildcardToRegularExpression(char * reg,char * component);
    void cleanupAndAddArgument(char * suffix);
    %}

/* Declare Shell syntax **/


%%

goal:
commands
;

commands:
complex_command
| commands complex_command
;

complex_command:
commands_with_pipe iomodifier_list background_task NEWLINE {
    Command::_currentCommand.execute();
}
| NEWLINE
| error NEWLINE { yyerrok; }
;

commands_with_pipe:
commands_with_pipe PIPE command_and_args
| command_and_args
;

command_and_args:
command_word argument_list {
    Command::_currentCommand.insertSimpleCommand( Command::_currentSimpleCommand );
};

argument_list:
argument_list argument
| /* can be empty */
;

argument: WORD {
    expandWildcardsIfNecessary($1);
}
| SUBSHELL {
    //printf("$1 is:%s\n",$1);
    // Save stdin/stdout
    int originalInput = dup(0);
    int originalOutput = dup(1);
    
    int originalPipe[2];
    int subshellPipe[2];
    
    if (pipe(originalPipe) || pipe(subshellPipe)) {
        perror("pipe");
        exit(1);
    }
    
    
    dup2(subshellPipe[1], 1);
    close(subshellPipe[1]);
    
    int pid;
    if ((pid = fork()) == 0) {
        dup2(subshellPipe[0], 0);
        close(subshellPipe[0]);
        dup2(originalPipe[1], 1);
        close(originalPipe[1]);
        execvp("/proc/self/exe", NULL);
        perror("execvp");
        _exit(1);
    } else if (pid< 0) {
        perror("fork");
        exit(1);
    } else {
        close(originalPipe[1]);
        close(subshellPipe[0]);
        
        
        dup2(originalPipe[0], 0);
        close(originalPipe[0]);
        
        //clean up user input
        char * buf = (char *)malloc(sizeof(char) * (strlen($1) + 2));
        char * bufferHelper = buf;
        
        while (*$1) {
            *bufferHelper++ = *$1++;
        }
        *bufferHelper++ = '\n';
        *bufferHelper++ = '\0';
        //printf("buf is:%s\n",buf);
        
        write(1, buf, strlen(buf));
        
        
        dup2(originalOutput, 1);
        close(originalOutput);
        
        
        
        char * outbuf = (char *)malloc(sizeof(char) * 1024);
        char * outbufferHelper = outbuf;
        
        int totalread = 0;
        int currentMaxLen = 1024;
        while(read(0, outbufferHelper, 1)) {
            // Reads output from child to outbuf
            if (totalread >= currentMaxLen) {
                outbuf = (char *)realloc(outbuf, currentMaxLen * 2);
                currentMaxLen *= 2;
            }
            if (*outbufferHelper == '\n') {
                *outbufferHelper = ' ';
            }
            outbufferHelper++;
            totalread++;
            
        }
        
        *outbufferHelper = '\0';
        char * pch;
        pch = strtok (outbuf," ");
        while (pch != NULL)
        {
            Command::_currentSimpleCommand->insertArgument( strdup(pch) );
            pch = strtok (NULL, " ");
        }
        
        dup2(originalInput, 0);
        close(originalInput);
    }
};

command_word: WORD {
    Command::_currentSimpleCommand = new SimpleCommand();
    Command::_currentSimpleCommand->insertArgument($1);
}
;

iomodifier_list: iomodifier_list iomodifier_opt
|
;

iomodifier_opt:
GREAT WORD {
    if (Command::_currentCommand._outFile == 0){
        Command::_currentCommand._outFile = strdup($2);
    } else {
        printf("Ambiguous output redirect");
        exit(1);
    }
    
}
| LESS WORD {
    if (Command::_currentCommand._inFile == 0){
        Command::_currentCommand._inFile = strdup($2);
    } else {
        printf("Ambiguous input redirect");
        exit(1);
    }
    
    
}
| GREATGREAT WORD {
    if (Command::_currentCommand._outFile == 0){
        Command::_currentCommand._outFile = strdup($2);
        Command::_currentCommand._append = 1;
    } else {
        printf("Ambiguous output redirect");
        exit(1);
    }
    
}
| REDIRECTOUTANDERROR WORD {
    if (Command::_currentCommand._outFile == 0){
        Command::_currentCommand._outFile = strdup($2);
        Command::_currentCommand._errFile = strdup($2);
    } else {
        printf("Ambiguous output redirect");
        exit(1);
    }
    
}
| APPENDREDIRECTANDERROR WORD {
    if (Command::_currentCommand._outFile == 0){
        Command::_currentCommand._outFile = strdup($2);
        Command::_currentCommand._errFile = strdup($2);
        Command::_currentCommand._append = 1;
    } else {
        printf("Ambiguous output redirect");
        exit(1);
    }
    
};
background_task: AMPERSAND {
    Command::_currentCommand._background = 1;
}
|
;

%%
/* Another Implementation in C **/



void
yyerror(const char * s)
{
    fprintf(stderr,"%s", s);
}


struct dirent * ent;
int nEntries = 0;
int maxEntries = 20;
char **array;

void expandWildcardsIfNecessary(char * arg){
    char * asterisk = strchr(arg,'*');
    char * questionMark = strchr(arg,'?');
    char * outputFile = strchr(arg,'>');
    char * specialOutChar = strstr(arg,"\">\"");
    char * backSlash = strstr(arg,"\\");
    if (backSlash != NULL) {
        char * buffer = (char*) malloc(1024);
        int a = 0; 
        int b = 0;
        int len = strlen(arg);
        
        while (a < len) 
        {
            if(arg[a] == 92)
            {
                if(arg[a+1] == 92){
                    buffer[b] = arg[a+1];
                    a++; 
                    b++;
                }
            }
            else {
                buffer[b] = arg[a];
                b++;
            }
            a++;
        }
        Command::_currentSimpleCommand->insertArgument(strdup(buffer)); 
        free(buffer);
    } else if (specialOutChar != NULL) {
        int numberOfArguemnt = Command::_currentSimpleCommand->_numOfArguments;
        char * originalStr = Command::_currentSimpleCommand->_arguments[numberOfArguemnt-1];
        int len = strlen(originalStr);
        char *new_str = (char*)malloc(len + 3);
        *new_str = '\0';
        strcat(new_str, originalStr);
        strcat(new_str, " >");
        Command::_currentSimpleCommand->_arguments[numberOfArguemnt-1] = strdup(new_str);
        free(new_str);
    } else if (asterisk == NULL && questionMark == NULL && outputFile == NULL) {
        Command::_currentSimpleCommand->insertArgument(arg);
        return;
    } else if (outputFile != NULL) {
        if (Command::_currentCommand._outFile == 0){
            char * temp = strdup(outputFile);
            temp++;
            Command::_currentCommand._outFile = strdup(temp);
            *outputFile = 0;
            Command::_currentSimpleCommand->insertArgument(arg);
            return;
        }

    
    }else {
        processWildCardWithRecursion(NULL,arg);
    }
}

int wildcardToRegularExpression(char * reg,char * component){
    int first = 0;
    int isHidden = 0;
    // 1. Convert wildcard to regular expression
    // Convert “*” -> “.*”
    // "?" -> "."
    // "." -> "\." and others you need
    // Also add ^ at the beginning and $ at the end to match
    // the beginning ant the end of the word.
    // Allocate enough space for regular expression
    
    char * a = component;
    char * r = reg;
    *r = '^';r++; // match beginning of line
    while(*a){
        if (*a == '*') { *r = '.'; r++; *r = '*';}
        
        else if (*a == '.') {
            *r = '\\';
            r++;
            *r = '.';
            if (first == 0) isHidden = 1;
        }
        
        else if (*a == '?')
        *r = '.';
        else
        *r = *a;
        a++;
        r++;
        first++;
    }
    *r='$';r++;*r=0; //match end of line and add null char to terminate
    return isHidden;
}

void recursionHelper(char * prefix,char * newPrefix,char * suffix, char * component){
    if(prefix == NULL){
        sprintf(newPrefix,"%s",component);
    }else if (!strcmp(prefix,"/")){
        sprintf(newPrefix,"%s%s",prefix,component);
    } else {
        sprintf(newPrefix,"%s/%s",prefix,component);
    }
    processWildCardWithRecursion(newPrefix,suffix);
}

void processWildCardWithRecursion(char * prefix,char *suffix){
    
    //do base check for suffix and prefix
    if (suffix[0] == 0) {
        if(nEntries == 0){
            array = (char **)malloc(sizeof(char*) * maxEntries);
        }
        if(nEntries == maxEntries){
            maxEntries *= 2;
            array = (char**)realloc(array,sizeof(char*)*maxEntries);
        }
        array[nEntries] = strdup(prefix);
        nEntries++;
        return;
    }
    
    char *s = strchr(suffix,'/');
    char component[1024] = "";
    
    if(s!= NULL){
        int length = s - suffix;
        if (length != 0){strncpy(component,suffix,length);}
        else {strcpy(component,"/");}
        suffix = s + 1;
    } else {
        strcpy(component,suffix);
        suffix += strlen(suffix);
    }
    
    char newPrefix[1024] = "";
    
    if(strchr(component,'*') == NULL && strchr(component,'?') == NULL ){
        recursionHelper(prefix,newPrefix,suffix,component);
    } else {
        char * reg = (char *)malloc(2*strlen(component)+10);
        int isHidden = wildcardToRegularExpression(reg,component);
        
        // 2. compile regular expression. See lab3-src/regular.cc
        regex_t re;
        int result = regcomp( &re, reg,  REG_EXTENDED|REG_NOSUB);
        if (result != 0){
            perror("regcomp");
            return;
        } else {
            // 3. List directory and add as arguments the entries
            // that match the regular expression
            
            char const * dr;
            if (prefix == NULL)
            dr = ".";
            else
            dr = prefix;
            
            DIR *dir = opendir(dr);
            
            if (dir == NULL) {
                return;
            }
            struct dirent * ent;
            while ( (ent = readdir(dir))!= NULL) {
                regmatch_t match;
                
                // Check if name matches
                if (regexec(&re,ent->d_name,1,&match,0) == 0 ) {
                    if (ent->d_name[0] == '.') {
                        if(isHidden){
                            recursionHelper(prefix,newPrefix,suffix,ent->d_name);
                        }
                    } else {
                        recursionHelper(prefix,newPrefix,suffix,ent->d_name);
                    }
                }
            }
        }
        
        
        free(reg);
    }
    cleanupAndAddArgument(suffix);
}

void cleanupAndAddArgument(char * suffix){
    if (suffix[0] == 0) {
        char *tmp;
        int numberOfItem = nEntries;
        int finished;
        int i;
        
        while (!finished){
            finished = 1;
            for (i = 0; i < numberOfItem - 1; i++) {
                if (strcmp(array[i], array[i + 1]) > 0) {
                    tmp = array[i];
                    array[i] = array[i + 1];
                    array[i + 1] = tmp;
                    finished = 0;
                }
            }
            numberOfItem--;
        }
        
        for (i = 0; i < nEntries; i++)
        Command::_currentSimpleCommand->insertArgument(array[i]);
        
        free(array);
        nEntries = 0;
        maxEntries = 20;
    }
}

#if 0
main()
{
    yyparse();
}
#endif
