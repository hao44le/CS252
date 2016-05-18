#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>


int QueueLength = 5;

//save for current http status
int _ERROR = 0;
int _typeisText = 1;

//use for concurrence
pthread_mutex_t mutexLock;

//        HTTP/1.1 404 Not Found<crlf>
//        Server: <serverType><crlf>
//        Content-Type: text/html<crlf>
//        <crlf>
//        <notFound>

const char * protocal = "HTTP/1.1 ";
const char * serverType = "CS 252 Lab 5";
const char * sp = "\040";
const char * crlf = "\015\012";
const char * notFound =
"<html>\n"
"    <body>\n"
"        <h1>File Not Found</h1>\n"
"    </body>\n"
"</html>\n";

// different server mode method
void defaultSingleThreadSever(int masterSocket);
void newThreadForEachRequest(int masterSocket);
void newProcessForEachRequest(int masterSocket);
void poolOfThreads(int masterSocket);
void loopThread(int socket);
void threadSlave(int socket);

//deal with every single request
void processTimeRequest( int socket );
char* getExpandPath(char*path,char*cwd);
extern "C" void killzombie(int sint);
char * urlToFileMapping(int fd);
char * findTypeOfContent(char * filePath);
int endsWith(const char *str, const char *suffix);
void checkAndChangeFilePath(char * filePath, char * cwd);
void write404(int socket);
void writeHTML(int socket,int file,char * contentType);

extern "C" void killzombie(int sint) {
    while(waitpid(-1,NULL,WNOHANG)>0);
}

int
main( int argc, char ** argv )
{
    
    //set up to kill zombie
    struct sigaction signalAction;
    signalAction.sa_handler = &killzombie;
    sigemptyset(&signalAction.sa_mask);
    signalAction.sa_flags=SA_RESTART;
    int err = sigaction(SIGCHLD, &signalAction, NULL);
    if(err) {
        perror("sigaction");
        exit(-1);
    }

    
    int port;
    int threadType = 0;
    int hasThread = 0;
    
    //check server mode
    if ( argc < 2 ) {
        port = 8888;
    } else if (argc == 2){
        if(strcmp(argv[1],"-f") == 0) {
            threadType = 1;
            hasThread = 1;
        }
        else if(strcmp(argv[1],"-t") == 0) {
            threadType = 2;
            hasThread = 1;
        }
        else if(strcmp(argv[1],"-p") == 0) {
            threadType = 3;
            hasThread = 1;
        }
        if(hasThread == 0) {
            port = atoi( argv[1] );
        }
    } else {
        if(strcmp(argv[1],"-f") == 0) {
            threadType = 1;
        }
        else if(strcmp(argv[1],"-t") == 0) {
            threadType = 2;
        }
        else if(strcmp(argv[1],"-p") == 0) {
            threadType = 3;
        }
        port = atoi( argv[2] );
    }
    
    
    // Set the IP address and port for this server
    struct sockaddr_in serverIPAddress;
    memset( &serverIPAddress, 0, sizeof(serverIPAddress) );
    serverIPAddress.sin_family = AF_INET;
    serverIPAddress.sin_addr.s_addr = INADDR_ANY;
    serverIPAddress.sin_port = htons((u_short) port);
    
    // Allocate a socket
    int masterSocket =  socket(PF_INET, SOCK_STREAM, 0);
    if ( masterSocket < 0) {
        perror("socket");
        exit( -1 );
    }
    
    // Set socket options to reuse port. Otherwise we will
    // have to wait about 2 minutes before reusing the sae port number
    int optval = 1;
    err = setsockopt(masterSocket, SOL_SOCKET, SO_REUSEADDR,
                     (char *) &optval, sizeof( int ) );
    
    // Bind the socket to the IP address and port
    int error = bind( masterSocket,
                     (struct sockaddr *)&serverIPAddress,
                     sizeof(serverIPAddress) );
    if ( error ) {
        perror("bind");
        exit( -1 );
    }
    
    // Put socket in listening mode and set the
    // size of the queue of unprocessed connections
    error = listen( masterSocket, QueueLength);
    if ( error ) {
        perror("listen");
        exit( -1 );
    }
    printf("threadType:%d\n",threadType);
    if (threadType!=3) {
        while ( 1 ) {
            
            // Accept incoming connections
            struct sockaddr_in clientIPAddress;
            int alen = sizeof( clientIPAddress );
            int slaveSocket = accept( masterSocket,
                                     (struct sockaddr *)&clientIPAddress,
                                     (socklen_t*)&alen);
            
            if ( slaveSocket < 0 ) {
                perror( "accept" );
                continue;
            }
            
            if (threadType==2) {
                // Create a new thread for each request
                newThreadForEachRequest(slaveSocket);
            } else if (threadType==1) {
                // Create a new process for each request
                newProcessForEachRequest(slaveSocket);
            } else if (threadType==0){
                // nothing specify ,default iterative single thread server.
                defaultSingleThreadSever(slaveSocket);
            }
        }
       
    } else {
        // pool of threads
        poolOfThreads(masterSocket);
    }
    
}

//used by newThreadForEachRequest
void threadSlave(int socket){
    processTimeRequest(socket);
    close(socket);
}

// Create a new thread for each request
void newThreadForEachRequest(int slaveSocket){

        
        pthread_t tid;
        pthread_attr_t attr;
        
        pthread_attr_init(&attr);
        pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
        pthread_create(&tid, &attr, (void *(*)(void* ))threadSlave, (void *)slaveSocket);
    
}
// Create a new process for each request
void newProcessForEachRequest(int slaveSocket){
        int pid = fork();
        if(pid == 0) {
            // Process request.
            processTimeRequest( slaveSocket );
            // Close socket
            close( slaveSocket );
            _ERROR = 0;
            exit(1);
        }
        close(slaveSocket);
        
    
}
//used by poolOfThreads
void loopThread(int masterSocket) {
    while(1) {
        struct sockaddr_in clientIPAddress;
        int alen = sizeof( clientIPAddress );
        
        pthread_mutex_lock(&mutexLock);
        int slaveSocket = accept( masterSocket,
                                 (struct sockaddr *)&clientIPAddress,
                                 (socklen_t*)&alen);
        pthread_mutex_unlock(&mutexLock);
        processTimeRequest(slaveSocket);
        close(slaveSocket);
        _ERROR = 0;
    }
}

// pool of threads
void poolOfThreads(int masterSocket){
    pthread_t tid[10];
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
    
    pthread_mutex_init(&mutexLock, NULL);
    
    for(int i = 0; i< 10; i++) {
        pthread_create(&tid[i], &attr, (void *(*)(void *)) loopThread, (void *) masterSocket);
    }
    
    
    pthread_join(tid[0], NULL);
}

// nothing specify ,default iterative single thread server.
void defaultSingleThreadSever(int slaveSocket){

        if ( slaveSocket < 0 ) {
            perror( "accept" );
            exit( -1 );
        }
        
        // Process request.
        processTimeRequest( slaveSocket );
        
        // Close socket
        close( slaveSocket );
        _ERROR = 0;
    
}

//used by processTimeRequest to check url path
void checkAndChangeFilePath(char * filePath, char * cwd) {
    const char * extension = "/http-root-dir";
    int length = strlen(cwd)+strlen(extension)+1;
    if( strlen(filePath) < length ) {
        _ERROR = 1;
        printf("The URL is invalid.");
    } else if(strstr(filePath, "..")) {
        char * buffer = (char *)malloc(sizeof(char)*1024);
        filePath = realpath(filePath, buffer);
        free(buffer);
    }
}

// process one request at a time
void processTimeRequest( int socket ) {
    
    printf("inside processTimeRequest\n");
    
    //get file path from url
    char* path = urlToFileMapping(socket);
    printf("path:%s\n",path);
    
    //get user current working directory
    char* cwd = (char*)malloc(sizeof(char)*256);
    getcwd(cwd,256);
    
    // get expanded path from both cwd & path
    char* actualPath = getExpandPath(path,cwd);
    checkAndChangeFilePath(actualPath,cwd);
    printf("\nactualPath:--%s--\n",actualPath);
    
    //get contentType of file specified by actualPath
    char* contentType;
    contentType = findTypeOfContent(actualPath);
    printf("contentType:%s\n",contentType);
    
    
    FILE * doc;
    if( _typeisText) {
        // if it is text, then use fopen with r
        doc = fopen(actualPath, "r");
    }
    else {
        // else use fopen with rb
        doc = fopen(actualPath, "rb");
    }
    if(doc == NULL) {
        perror("open");
        _ERROR = 1;
    }
    
    if ( _ERROR) {
        // if there is an error, call write404
        write404(socket);
    }
    else {
        //else write HTML
        int file = fileno(doc);
        
        if (file > 0) {
            writeHTML(socket,file,contentType);
            fclose(doc);
        }
    }
}

//write 404 not found
void write404(int socket){
    //        HTTP/1.1 404 Not Found<crlf>
    //        Content-Type: text/html<crlf>
    //        Server: <serverType><crlf>
    //        <crlf>
    //        <notFound>
    write(socket, protocal, strlen(protocal));
    write(socket, "404 File Not Found", strlen("404 File Not Found"));
    write(socket, crlf, strlen(crlf));
    write(socket, "Content-type: ", 14);
    write(socket, "text/html", 9);
    write(socket, crlf, strlen(crlf));
    write(socket, "Server: ", strlen("Server"));
    write(socket, serverType, strlen(serverType));
    write(socket, crlf, strlen(crlf));
    write(socket, crlf, strlen(crlf));
    write(socket, notFound, strlen(notFound));
}

//write the real file
void writeHTML(int socket,int file,char * contentType){
    //        HTTP/1.1 200 OK<crlf>
    //        Server: <Server-Type><crlf>
    //        Content-type: <contentType><crlf>
    //        <crlf>
    //        <Document Data>
    write(socket, protocal, strlen(protocal));
    write(socket, "200 ", 4);
    write(socket, "OK ", 3);
    write(socket, crlf, strlen(crlf));
    write(socket, "Server: ", 8);
    write(socket, serverType, strlen(serverType));
    write(socket, crlf, strlen(crlf));
    write(socket, "Content-type: ", 14);
    write(socket, contentType, strlen(contentType));
    write(socket, crlf, strlen(crlf));
    write(socket, crlf, strlen(crlf));
    
    char c;
    int count = 0;
    
    count = read(file, &c, sizeof(c));
    //write the document data
    while(count != 0) {
        if(write(socket, &c, sizeof(c)) != count) {
            perror("write");
            break;
        }
        count = read(file, &c, sizeof(c));
    }
}

// helper method used by findTypeOfContent
int endsWith(const char *str, const char *suffix) {
    if (!str || !suffix)
        return 0;
    size_t lenstr = strlen(str);
    size_t lensuffix = strlen(suffix);
    if (lensuffix >  lenstr)
        return 0;
    return strncmp(str + lenstr - lensuffix, suffix, lensuffix) == 0;
}

//check which type of file it is
char * findTypeOfContent(char * filePath) {
    char * contentType;
    if( endsWith(filePath, (char *)".gif") || endsWith(filePath, (char *)".gif/")) {
        _typeisText = 0;
        contentType = (char*)malloc(sizeof(char) * strlen("image/gif"));
        strcpy(contentType,"image/gif");
    } else if (endsWith(filePath, (char *)".html") || endsWith(filePath, (char *)".html/")) {
        contentType = (char*)malloc(sizeof(char) * strlen("text/html"));
        strcpy(contentType,"text/html");
    }
    else {
        contentType = (char*)malloc(sizeof(char) * strlen("text/plain"));
        strcpy(contentType,"text/plain");
    }
    return contentType;
}

//get url from path
char * urlToFileMapping(int fd){
    char*result;
    printf("urlToFileMapping:\n");
    char tmpString[1024];
    char afterOldChar=' ';
    char oldChar=' ';
    char firstCharacter=' ';
    char beforefirstCharacter= ' ';
    
    int c = read(fd, &beforefirstCharacter, sizeof(firstCharacter));
    for(int i = 0, j = 0;c != 0;c = read(fd, &beforefirstCharacter, sizeof(firstCharacter))) {
        if( afterOldChar == '\015' && oldChar == '\n' &&
           firstCharacter == '\015' && beforefirstCharacter == '\n') {
            break;
        }
        
        if( beforefirstCharacter != ' ') {
            afterOldChar = oldChar;
            oldChar = firstCharacter;
            firstCharacter = beforefirstCharacter;
            tmpString[i] = beforefirstCharacter;
            i++;
        } else {
            tmpString[i] = '\0';
            i = 0;
            j++;
            if(j == 2){
                result = strdup(tmpString);
            }
            
        }
    }
    printf("end of urltoFileMapping\n");
    return result;
}

//get expaned path from both path & cwd
char * getExpandPath(char*path,char*cwd){
    char* result = (char*)malloc(sizeof(char)*500);
    if(strlen(path) == 1) {
        strcpy(result,cwd);
        strcat(result,"/http-root-dir/htdocs/index.html");
    } else if(strstr(path, "/icons") != NULL || strstr(path,"/htdocs") != NULL) {
        strcpy(result,cwd);
        strcat(result,"/http-root-dir");
        strcat(result,path);
    } else {
        strcpy(result,cwd);
        strcat(result,"/http-root-dir/htdocs");
        strcat(result,path);
    }
    return result;
}
