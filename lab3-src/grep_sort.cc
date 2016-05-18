
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

const char * usage = ""
"Usage:\n"
"    grepsort word inputfile outfile\n"
"\n"
"    It does something similar to the shell command:\n"
"        csh> grep word | grep < inputfile >> outfile\n"
"\n"
"Example:\n"
"    grepsort hello infile outfile\n"
"    cat outfile\n\n";

const char *grep = "grep";
const char *sort = "sort";

main(int argc, char **argv, char **envp)
{
	if (argc < 4) {
		fprintf(stderr, "%s", usage );
		exit(1);
	}

	// Save default input, output, and error because we will
	// change them during redirection and we will need to restore them
	// at the end.

	int defaultin = dup( 0 );
	int defaultout = dup( 1 );

	//////////////////  grep //////////////////////////

	// Input:    inputFile
	// Output:   pipe
	// Error:    defaulterr

	// Create new pipe 

	int fdpipe[2];
	if ( pipe(fdpipe) == -1) {
		perror( "grep_sort: pipe");
		exit( 2 );
	}

	// Redirect input
    int redirectedInput = open(argv[2],O_RDONLY);
    if (redirectedInput < 0) {
        perror("Failed to open Input File");
        exit(1);
    }
	dup2( redirectedInput, 0 );
	
	// Redirect output to pipe
	dup2( fdpipe[ 1 ], 1 );


	// Create new process for "grep"
	int pid = fork();
	if ( pid == -1 ) {
		perror( "grep_sort: fork\n");
		exit( 2 );
	}

	if (pid == 0) {
		//Child
		
		// close file descriptors that are not needed
		close(fdpipe[0]);
		close(fdpipe[1]);
		close( defaultin );
		close( defaultout );

		// You can use execvp() instead if the arguments are stored in an array
		execlp(grep, grep, argv[1], (char *) NULL);

		// exec() is not suppose to return, something went wrong
		perror( "grep_sort: exec grep");
		exit( 2 );
	}

	//////////////////  sort //////////////////////////

	// Input:    pipe
	// Output:   outfile with append mode
	// Error:    defaulterr

	// Redirect input.
	dup2( fdpipe[0], 0);

	// Redirect output to utfile
	int redirectedOutput = open(argv[3],O_CREAT|O_WRONLY|O_APPEND,0700);

	if ( redirectedOutput < 0 ) {
		perror( "grep_sort: creat outfile" );
		exit( 2 );
	}
	
	dup2( redirectedOutput, 1 );
	close( redirectedOutput );


	pid = fork();
	if (pid == -1 ) {
		perror( "grep_sort: fork");
		exit( 2 );
	}
	
	if (pid == 0) {
		//Child

		// close file descriptors that are not needed
		close(fdpipe[0]);
		close(fdpipe[1]);
		close( defaultin );
		close( defaultout );
		
		// You can use execvp() instead if the arguments are stored in an array
		execlp(sort, sort, (char *) 0);

		// exec() is not suppose to return, something went wrong
		perror( "grep_sort: exec sort");
		exit( 2 );

	}

	// Restore input, output, and error

	dup2( defaultin, 0 );
	dup2( defaultout, 1 );

	// Close file descriptors that are not needed
	close(fdpipe[0]);
	close(fdpipe[1]);
	close( defaultin );
	close( defaultout );

	// Wait for last process in the pipe line
	waitpid( pid, 0, 0 );

	exit( 2 );
}

