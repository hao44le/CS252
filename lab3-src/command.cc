
/*
 * CS252: Shell project
 *
 * Template file.
 * You will need to add more code here to execute the command table.
 *
 * NOTE: You are responsible for fixing any bugs this code may have!
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#include <sys/wait.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>    /* For O_RDWR */

#include "command.h"

SimpleCommand::SimpleCommand()
{
    // Create available space for 5 arguments
    _numOfAvailableArguments = 5;
    _numOfArguments = 0;
    _arguments = (char **) malloc( _numOfAvailableArguments * sizeof( char * ) );
}

void
SimpleCommand::insertArgument( char * argument )
{
    if ( _numOfAvailableArguments == _numOfArguments  + 1 ) {
        // Double the available space
        _numOfAvailableArguments *= 2;
        _arguments = (char **) realloc( _arguments,
                                       _numOfAvailableArguments * sizeof( char * ) );
    }
    
    
    int countVariable = 0;
    char* convertedArgument  = (char*)malloc(100);

    for (int i = 0; i < strlen(argument); i++) {
        if(argument[i] == '$' && argument[i+1] == '{'){
            
            //get user input
            char* enviromentString  = (char*)malloc(100);
            int first = i+2;
            int second = 0;
            while(argument[first] != '}'){
                enviromentString[second] = argument[first];
                second++;
                first++;
            }
            enviromentString[second] = '\0';
            
            //get real enviroment variable
            char* tempString = (char*)malloc(100);
            tempString = getenv(enviromentString);
            if (tempString!=NULL) {
                for (int j = 0; j < strlen(tempString); j++)
                {
                    convertedArgument[countVariable] = tempString[j];
                    countVariable++;
                }
               
            }
             i = first;
            
        } else {
            convertedArgument[countVariable] = argument[i];
            countVariable++;
        }
    }
    convertedArgument[countVariable] = '\0';
    
    if (convertedArgument[0] == '~') {
        if(strlen(convertedArgument) == 1) {
            //get home directory
            convertedArgument = strdup(getenv("HOME"));
            //printf(convertedArgument);
        }
        else {
            //append home directory directly
            char * path = (char *)malloc((6 + strlen(convertedArgument)) * sizeof(char));
            const char * prefix = "/homes/";
            char * pathptr = path;
            char * argptr = convertedArgument;
            
            while (*prefix) {
                *path++ = *prefix++;
            }
            
            argptr++;
            
            while (*argptr) {
                *path++ = *argptr++;
            }
            *path = '\0';
            
            strcpy(convertedArgument, pathptr);
            //printf(convertedArgument);
        }
        
    }
    
    _arguments[ _numOfArguments ] = strdup(convertedArgument);
    
    // Add NULL argument at the end
    _arguments[ _numOfArguments + 1] = NULL;
    
    _numOfArguments++;
}

Command::Command()
{
    // Create available space for one simple command
    _numOfAvailableSimpleCommands = 1;
    _simpleCommands = (SimpleCommand **)
    malloc( _numOfSimpleCommands * sizeof( SimpleCommand * ) );
    
    _numOfSimpleCommands = 0;
    _outFile = 0;
    _inFile = 0;
    _errFile = 0;
    _background = 0;
}

void
Command::insertSimpleCommand( SimpleCommand * simpleCommand )
{
    if ( _numOfAvailableSimpleCommands == _numOfSimpleCommands ) {
        _numOfAvailableSimpleCommands *= 2;
        _simpleCommands = (SimpleCommand **) realloc( _simpleCommands,
                                                     _numOfAvailableSimpleCommands * sizeof( SimpleCommand * ) );
    }
    if (strchr(simpleCommand->_arguments[0],'|') != NULL) {
        char * originalArgument = simpleCommand->_arguments[1];
        char * pch = strtok (simpleCommand->_arguments[0],"|");
        simpleCommand->_arguments[0] = strdup(pch);
        simpleCommand->_arguments[1] = NULL;
        simpleCommand->_numOfArguments--;
        _simpleCommands[ _numOfSimpleCommands ] = simpleCommand;
        _numOfSimpleCommands++;
        if (pch != NULL)
        {
            pch = strtok (NULL, "|");
            Command::_currentSimpleCommand = new SimpleCommand();
            Command::_currentSimpleCommand->insertArgument(pch);
            Command::_currentSimpleCommand->insertArgument(originalArgument);
            Command::_currentCommand.insertSimpleCommand( Command::_currentSimpleCommand );
        }
        
    } else {
        _simpleCommands[ _numOfSimpleCommands ] = simpleCommand;
        _numOfSimpleCommands++;
 
    }
    
}

void
Command:: clear()
{
    for ( int i = 0; i < _numOfSimpleCommands; i++ ) {
        for ( int j = 0; j < _simpleCommands[ i ]->_numOfArguments; j ++ ) {
            free ( _simpleCommands[ i ]->_arguments[ j ] );
        }
        
        free ( _simpleCommands[ i ]->_arguments );
        free ( _simpleCommands[ i ] );
    }
    
    if ( _outFile ) {
        free( _outFile );
    }
    
    if ( _inFile ) {
        free( _inFile );
    }
    
    if ( _errFile ) {
        free( _errFile );
    }
    
    _numOfSimpleCommands = 0;
    _outFile = 0;
    _inFile = 0;
    _errFile = 0;
    _background = 0;
}

void
Command::print()
{
    printf("\n\n");
    printf("              COMMAND TABLE                \n");
    printf("\n");
    printf("  #   Simple Commands\n");
    printf("  --- ----------------------------------------------------------\n");
    
    for ( int i = 0; i < _numOfSimpleCommands; i++ ) {
        printf("  %-3d ", i );
        for ( int j = 0; j < _simpleCommands[i]->_numOfArguments; j++ ) {
            printf("\"%s\" \t", _simpleCommands[i]->_arguments[ j ] );
        }
        printf("\n");
    }
    
    printf( "\n\n" );
    printf( "  Output       Input        Error        Background\n" );
    printf( "  ------------ ------------ ------------ ------------\n" );
    printf( "  %-12s %-12s %-12s %-12s\n", _outFile?_outFile:"default",
           _inFile?_inFile:"default", _errFile?_errFile:"default",
           _background?"YES":"NO");
    printf( "\n\n" );
    
}

void
Command::execute()
{
    // Don't do anything if there are no simple commands
    if ( _numOfSimpleCommands == 0 ) {
        prompt();
        return;
    }
    
    // Print contents of Command data structure
//        	print();
    
    //exit
    if (strcmp(_simpleCommands[0]->_arguments[0],"exit")==0) {
        printf("Good bye!!\n");
        exit(0);
    }
    // Setup i/o redirection
    int originalIn = dup(0);
    int originalOut = dup(1);
    int originalError = dup(2);
    
    int redirectedInput;
    int redirectedOutput;
    int redirectedError;
    
    if (_inFile) {
        redirectedInput = open(_inFile,O_RDONLY);
        if (redirectedInput < 0) {
            perror("Failed to open Input File");
            exit(1);
        }
    } else {
        redirectedInput = dup(originalIn);
    }
    
    if (_errFile) {
        if (_append) {
            redirectedError = open(_errFile,O_CREAT|O_WRONLY|O_APPEND,0664);
            if (redirectedError < 0) {
                perror("Failed to Append Error File");
            }
        } else {
            redirectedError = open(_outFile,O_CREAT|O_WRONLY|O_TRUNC,0664);
        }
        
    } else {
        redirectedError = dup(originalError);
    }
    
    int ret;
    
    for ( int i = 0; i < _numOfSimpleCommands; i++ ) {
        //redirect in -> redirectedInput
        dup2(redirectedInput,0);
        close(redirectedInput);
        //redirect error -> redirectedError
        dup2(redirectedError,2);
        close(redirectedError);
        
        
        
        
        //check for this particular command
        if (i == _numOfSimpleCommands - 1) {
            if (_outFile) {
                if (_append) {
                    redirectedOutput = open(_outFile,O_CREAT|O_WRONLY|O_APPEND,0700);
                } else {
                    redirectedOutput = open(_outFile,O_CREAT|O_WRONLY|O_TRUNC,0700);
                }
                
            } else {
                redirectedOutput = dup(originalOut);
            }
        } else {
            // not last command & use pipe to communicate
            int fdpipe[2];
            pipe(fdpipe);
            redirectedOutput = fdpipe[1];
            redirectedInput = fdpipe[0];
        }
        
        dup2(redirectedOutput,1);
        close(redirectedOutput);
        
        //cd
        if (strcmp(_simpleCommands[i]->_arguments[0], "cd") == 0){
            
            int path;
            if (_simpleCommands[i]->_numOfArguments > 1) {
                //more than one arguments need to add path
                path = chdir(_simpleCommands[i]->_arguments[1]);
            }else {
                // only "cd",need to append home
                path = chdir(getenv("HOME"));
            }
            
            if (path != 0) {
                perror("chdir");
                exit(2);
            }
        } else if(strcmp(_simpleCommands[i]->_arguments[0],"setenv") == 0){
            
            char*a = strdup(_simpleCommands[i]->_arguments[1]);
            char*b = strdup(_simpleCommands[i]->_arguments[2]);
            setenv(a,b,1);
        } else if (strcmp(_simpleCommands[i]->_arguments[0], "unsetenv") == 0){
            unsetenv(_simpleCommands[i]->_arguments[1]);
        } else {
            // For every simple command fork a new process
            ret = fork();
            if (ret == 0){
                //Child procress execute command
                execvp(_simpleCommands[i]->_arguments[0], _simpleCommands[i]->_arguments);
                perror("execvp");
                _exit(1);
            } else if (ret < 0) {
                // There was an error in fork.
                perror("fork");
                exit(2);
            }
            
        }
        
        
    }
    
    //restore the original in-out-error
    dup2(originalIn,0);
    dup2(originalOut,1);
    dup2(originalError,2);
    
    //close unused file descriptor
    close(originalIn);
    close(originalOut);
    close(originalError);
    if (!_background){
        //This is the parent process.
        //ret is the pid of the child.
        //wait until the child exists
        waitpid(ret,NULL,0);
    }
    cleanup();
}

void Command::cleanup(){
    clear();
    prompt();
}


// Shell implementation

void handel(int pro){
    while (waitpid((pid_t)(-1), 0, WNOHANG) > 0) {}
}
void ctrlc(int sig){
    Command::_currentCommand.clear();
    fprintf(stdout, "\n");
    Command::_currentCommand.prompt();
}

void
Command::prompt()
{
    if (isatty(0)) {
        printf("myshell>");
    }
    fflush(stdout);
}

Command Command::_currentCommand;
SimpleCommand * Command::_currentSimpleCommand;

int yyparse(void);

main()
{
    signal(SIGINT,ctrlc);
    signal(SIGCHLD,handel);
    Command::_currentCommand.prompt();
    yyparse();
}

