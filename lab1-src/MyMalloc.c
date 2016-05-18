//
// CS252: MyMalloc Project
//
// The current implementation gets memory from the OS
// every time memory is requested and never frees memory.
//
// You will implement the allocator as indicated in the handout.
//
// Also you will need to add the necessary locking mechanisms to
// support multi-threaded programs.
//

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/mman.h>
#include <pthread.h>
#include "MyMalloc.h"

static pthread_mutex_t mutex;

const int ArenaSize = 2097152;
const int NumberOfFreeLists = 1;

// Header of an object. Used both when the object is allocated and freed
struct ObjectHeader {
    size_t _objectSize;         // Real size of the object.
    int _allocated;             // 1 = yes, 0 = no 2 = sentinel
    struct ObjectHeader * _next;       // Points to the next object in the freelist (if free).
    struct ObjectHeader * _prev;       // Points to the previous object.
};

struct ObjectFooter {
    size_t _objectSize;
    int _allocated;
};

//STATE of the allocator

// Size of the heap
static size_t _heapSize;

// initial memory pool
static void * _memStart;

// number of chunks request from OS
static int _numChunks;

// True if heap has been initialized
static int _initialized;

// Verbose mode
static int _verbose;

// # malloc calls
static int _mallocCalls;

// # free calls
static int _freeCalls;

// # realloc calls
static int _reallocCalls;

// # realloc calls
static int _callocCalls;

static int _smallestSize;

// Free list is a sentinel
static struct ObjectHeader _freeListSentinel; // Sentinel is used to simplify list operations
static struct ObjectHeader *_freeList;


//FUNCTIONS

//Initializes the heap
void initialize();

// Allocates an object
void * allocateObject( size_t size );

// Frees an object
void freeObject( void * ptr );

struct ObjectHeader * freeMemoryBlock( struct ObjectHeader * header, size_t size );

// Returns the size of an object
size_t objectSize( void * ptr );

// At exit handler
void atExitHandler();

//Prints the heap size and other information about the allocator
void print();
void print_list();

// Gets memory from the OS
void * getMemoryFromOS( size_t size );

void * seekNewMemoryFromOS();

void increaseMallocCalls() { _mallocCalls++; }

void increaseReallocCalls() { _reallocCalls++; }

void increaseCallocCalls() { _callocCalls++; }

void increaseFreeCalls() { _freeCalls++; }

extern void
atExitHandlerInC()
{
    atExitHandler();
}

void initialize()
{
    // Environment var VERBOSE prints stats at end and turns on debugging
    // Default is on
    
    _verbose = 1;
    const char * envverbose = getenv( "MALLOCVERBOSE" );
    if ( envverbose && !strcmp( envverbose, "NO") ) {
        _verbose = 0;
    }
    
    pthread_mutex_init(&mutex, NULL);
    void * _mem = getMemoryFromOS( ArenaSize + (2*sizeof(struct ObjectHeader)) + (2*sizeof(struct ObjectFooter)) );
    
    // In verbose mode register also printing statistics at exit
    atexit( atExitHandlerInC );
    
    //establish fence posts
    struct ObjectFooter * fencepost1 = (struct ObjectFooter *)_mem;
    fencepost1->_allocated = 1;
    fencepost1->_objectSize = 123456789;
    char * temp =
    (char *)_mem + (2*sizeof(struct ObjectFooter)) + sizeof(struct ObjectHeader) + ArenaSize;
    struct ObjectHeader * fencepost2 = (struct ObjectHeader *)temp;
    fencepost2->_allocated = 1;
    fencepost2->_objectSize = 123456789;
    fencepost2->_next = NULL;
    fencepost2->_prev = NULL;
    
    //initialize the list to point to the _mem
    temp = (char *) _mem + sizeof(struct ObjectFooter);
    struct ObjectHeader * currentHeader = (struct ObjectHeader *) temp;
    temp = (char *)_mem + sizeof(struct ObjectFooter) + sizeof(struct ObjectHeader) + ArenaSize;
    struct ObjectFooter * currentFooter = (struct ObjectFooter *) temp;
    _freeList = &_freeListSentinel;
    currentHeader->_objectSize = ArenaSize + sizeof(struct ObjectHeader) + sizeof(struct ObjectFooter); //2MB
    currentHeader->_allocated = 0;
    currentHeader->_next = _freeList;
    currentHeader->_prev = _freeList;
    currentFooter->_allocated = 0;
    currentFooter->_objectSize = currentHeader->_objectSize;
    _freeList->_prev = currentHeader;
    _freeList->_next = currentHeader;
    _freeList->_allocated = 2; // sentinel. no coalescing.
    _freeList->_objectSize = 0;
    _memStart = (char*) currentHeader;
}

void * allocateObject( size_t size )
{
    //Make sure that allocator is initialized
    if ( !_initialized ) {
        _initialized = 1;
        initialize();
    }
    
    // Add the ObjectHeader/Footer to the size and round the total size up to a multiple of
    // 8 bytes for alignment.
    size_t roundedSize = (size + sizeof(struct ObjectHeader) + sizeof(struct ObjectFooter) + 7) & ~7;
    
    // Naively get memory from the OS every time
    
    size = roundedSize;
    void * _mem;
    struct ObjectHeader * current = _freeList->_next;
    //Find the first block in the free list that is >= the size
    while (1) {
        if (current->_objectSize >= size) {
            //found it
            break;
        }
        current = current->_next;
        if (current->_allocated == 2) {
            //If the list is empty, request a new 2MB block (plus the size of a header and footer!), seek new copy of free list
            seekNewMemoryFromOS();
        }
    }
    
    
    struct ObjectFooter * footer = (struct ObjectFooter *)((char*)current + size - sizeof(struct ObjectFooter));
    footer->_allocated = 1;
    footer->_objectSize = size;
    
    
    _smallestSize = 8 + sizeof(struct ObjectHeader) + sizeof(struct ObjectFooter);
    
    if (current->_objectSize - size > _smallestSize) {
        // If the new mem is larger than smallestSize, split it into smaller chunk
        //The first block (lowest memory) should be removed from the free list and returned to satisfy the request. Set the _allocated flag to true in the header/footer. The remaining block should be re-inserted into the free list.
        size_t remainingFreeSize = current->_objectSize - size;
        current->_allocated = 1;
        current->_objectSize = size;
        
        struct ObjectHeader * header = (struct ObjectHeader *)((char *)current + size);
        header->_next = current->_next;
        header->_prev = current->_prev;
        header->_allocated = 0;
        header->_objectSize = remainingFreeSize;
        
        current->_prev->_next = header;
        current->_next->_prev = header;
        
        struct ObjectFooter * footer2 = (struct ObjectFooter *)((char *)header + header->_objectSize - sizeof(struct ObjectFooter));
        footer2->_allocated = 0;
        footer2->_objectSize = remainingFreeSize;
        
    } else {
        
        //else, configure the current memory block , and then remove this block from free list
        
        //configure the current memory block
        current->_allocated = 1;
        
        //remove this block from free list
        struct ObjectHeader * previous = current->_prev;
        struct ObjectHeader * next = current->_next;
        
        previous->_next = next;
        next->_prev = previous;
        
        current->_next = NULL;
        current->_prev = NULL;
        
        
    }
    _mem = current;
    // Store the size in the header
    struct ObjectHeader * o = (struct ObjectHeader *) _mem;
    
    o->_objectSize = roundedSize;
    
    pthread_mutex_unlock(&mutex);
    
    // Return a pointer to usable memory
    return (void *) (o + 1);
    
}


void * seekNewMemoryFromOS()
{
    void * _mem = getMemoryFromOS( ArenaSize + (2*sizeof(struct ObjectHeader)) + (2*sizeof(struct ObjectFooter)) );
    
    //establish fence posts
    struct ObjectFooter * fencepost1 = (struct ObjectFooter *)_mem;
    fencepost1->_allocated = 1;
    fencepost1->_objectSize = 123456789;
    char * temp =
    (char *)_mem + (2*sizeof(struct ObjectFooter)) + sizeof(struct ObjectHeader) + ArenaSize;
    struct ObjectHeader * fencepost2 = (struct ObjectHeader *)temp;
    fencepost2->_allocated = 1;
    fencepost2->_objectSize = 123456789;
    fencepost2->_next = NULL;
    fencepost2->_prev = NULL;
    
    //initialize the list to point to the _mem
    temp = (char *) _mem + sizeof(struct ObjectFooter);
    struct ObjectHeader * currentHeader = (struct ObjectHeader *) temp;
    temp = (char *)_mem + sizeof(struct ObjectFooter) + sizeof(struct ObjectHeader) + ArenaSize;
    struct ObjectFooter * currentFooter = (struct ObjectFooter *) temp;
    currentHeader->_objectSize = ArenaSize + sizeof(struct ObjectHeader) + sizeof(struct ObjectFooter); //2MB
    currentHeader->_allocated = 0;
    currentHeader->_prev = _freeList->_prev;
    currentHeader->_next = _freeList;
    currentFooter->_objectSize = currentHeader->_objectSize;
    currentFooter->_allocated = 0;
    _freeList->_prev->_next = currentHeader;
    _freeList->_prev = currentHeader;
    
}

void freeObject( void * ptr )
{
    // Add your code here
    struct ObjectHeader * header = (struct ObjectHeader *)((char *)ptr - sizeof(struct ObjectHeader));
    
    struct ObjectFooter * leftFooter = (struct ObjectFooter *)((char *)header - sizeof(struct ObjectFooter));
    
    if (leftFooter->_allocated == 0) {
        // if left footer is not allocated, remove it from the free list. And then check the right object.
        struct ObjectHeader * leftHeader = (struct ObjectHeader *)((char *)header - leftFooter->_objectSize);
        leftHeader = freeMemoryBlock(leftHeader,leftHeader->_objectSize + header->_objectSize);
        
        struct ObjectHeader * rightHeader = (struct ObjectHeader *)((char *)header + header->_objectSize);
        if(rightHeader->_allocated == 0){
            //if it is free, coalesce the two blocks and insert the new block into the free list and update the header/footer.
            leftHeader = freeMemoryBlock(leftHeader,leftHeader->_objectSize + rightHeader->_objectSize);
            //merge the leftheader into the freelist.
            rightHeader->_next->_prev = leftHeader;
            leftHeader->_next = rightHeader->_next;
            rightHeader->_next = NULL;
            rightHeader->_prev = NULL;
            return;
        }
        
    } else {
        //else,check the right object.
        struct ObjectHeader * rightHeader = (struct ObjectHeader *)((char *)header + header->_objectSize);
        if (rightHeader->_allocated == 0) {
            // if right header is not allocated,free the header along with right object.
            header = freeMemoryBlock(header,header->_objectSize + rightHeader->_objectSize);
            //merge the rightheader into the freelist.
            rightHeader->_prev->_next = header;
            rightHeader->_next->_prev = header;
            header->_next = rightHeader->_next;
            rightHeader->_next = NULL;
            rightHeader->_prev = NULL;
            
            return;
            
        } else {
            header = freeMemoryBlock(header,header->_objectSize);
            //insert header into free list without any coalescing.
            struct ObjectHeader * previous = _freeList;
            struct ObjectHeader * current = previous->_next;
            while (1) {
                if(previous->_allocated == 2 && current->_allocated == 2) {
                    break;
                }
                long h = (long)header;
                long c = (long)current;
                long p = (long)previous;
                if (h < c && p < h) {
                    break;
                }
                previous = current;
                current = current->_next;
            }
            
            header->_next = current;
            header->_prev = previous;
            previous->_next = header;
            current->_prev = header;
            return;
        }
    }
    
    return;
    
}

struct ObjectHeader * freeMemoryBlock( struct ObjectHeader * header, size_t size ) {
    header->_objectSize = size;
    header->_allocated = 0;
    
    struct ObjectFooter * footer = (struct ObjectFooter *)((char *)header + size - sizeof(struct ObjectFooter));
    footer->_objectSize = size;
    footer->_allocated = 0;
    return header;
}

size_t objectSize( void * ptr )
{
    // Return the size of the object pointed by ptr. We assume that ptr is a valid obejct.
    struct ObjectHeader * o =
    (struct ObjectHeader *) ( (char *) ptr - sizeof(struct ObjectHeader) );
    
    // Substract the size of the header
    return o->_objectSize;
}

void print()
{
    printf("\n-------------------\n");
    
    printf("HeapSize:\t%zd bytes\n", _heapSize );
    printf("# mallocs:\t%d\n", _mallocCalls );
    printf("# reallocs:\t%d\n", _reallocCalls );
    printf("# callocs:\t%d\n", _callocCalls );
    printf("# frees:\t%d\n", _freeCalls );
    
    printf("\n-------------------\n");
}

void print_list()
{
    printf("FreeList: ");
    if ( !_initialized ) {
        _initialized = 1;
        initialize();
    }
    struct ObjectHeader * ptr = _freeList->_next;
    while(ptr != _freeList){
        long offset = (long)ptr - (long)_memStart;
        printf("[offset:%ld,size:%zd]",offset,ptr->_objectSize);
        ptr = ptr->_next;
        if(ptr != NULL){
            printf("->");
        }
    }
    printf("\n");
}

void * getMemoryFromOS( size_t size )
{
    // Use sbrk() to get memory from OS
    _heapSize += size;
    
    void * _mem = sbrk( size );
    
    if(!_initialized){
        _memStart = _mem;
    }
    
    _numChunks++;
    
    return _mem;
}

void atExitHandler()
{
    // Print statistics when exit
    if ( _verbose ) {
        print();
    }
}

//
// C interface
//

extern void *
malloc(size_t size)
{
    pthread_mutex_lock(&mutex);
    increaseMallocCalls();
    
    return allocateObject( size );
}

extern void
free(void *ptr)
{
    pthread_mutex_lock(&mutex);
    increaseFreeCalls();
    
    if ( ptr == 0 ) {
        // No object to free
        pthread_mutex_unlock(&mutex);
        return;
    }
    
    freeObject( ptr );
}

extern void *
realloc(void *ptr, size_t size)
{
    pthread_mutex_lock(&mutex);
    increaseReallocCalls();
    
    // Allocate new object
    void * newptr = allocateObject( size );
    
    // Copy old object only if ptr != 0
    if ( ptr != 0 ) {
        
        // copy only the minimum number of bytes
        size_t sizeToCopy =  objectSize( ptr );
        if ( sizeToCopy > size ) {
            sizeToCopy = size;
        }
        
        memcpy( newptr, ptr, sizeToCopy );
        
        //Free old object
        freeObject( ptr );
    }
    
    return newptr;
}

extern void *
calloc(size_t nelem, size_t elsize)
{
    pthread_mutex_lock(&mutex);
    increaseCallocCalls();
    
    // calloc allocates and initializes
    size_t size = nelem * elsize;
    
    void * ptr = allocateObject( size );
    
    if ( ptr ) {
        // No error
        // Initialize chunk with 0s
        memset( ptr, 0, size );
    }
    
    return ptr;
}

