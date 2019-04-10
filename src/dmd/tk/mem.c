/*_ mem.c       */
/* Memory management package    */
/* Written by Walter Bright     */

#include        <stdio.h>
#if MSDOS || __OS2__ || __NT__ || _WIN32
#include        <io.h>
#else
#include        <sys/time.h>
#include        <sys/resource.h>
#include        <unistd.h>
#endif
#include        <stdarg.h>
#include        <stddef.h>

#ifndef malloc
#if __SC__ || __DMC__ ||  _MSC_VER
#include        <malloc.h>
#else
#include        <stdlib.h>
#endif
#endif

extern "C"
{

#include <stdio.h> // for size_t


#if __APPLE__ && __i386__
    /* size_t is 'unsigned long', which makes it mangle differently
     * than D's 'uint'
     */
    typedef unsigned d_size_t;
#else
    typedef size_t d_size_t;
#endif

/*
 * Memory management routines.
 *
 * Compiling:
 *
 *      #define MEM_DEBUG 1 when compiling to enable extended debugging
 *      features.
 *
 *      #define MEM_NONE 1 to compile out mem, i.e. have it all drop
 *      directly to calls to malloc, free, etc.
 *
 *      #define MEM_NOMEMCOUNT 1 to remove checks on the number of free's
 *      matching the number of alloc's.
 *
 * Features always enabled:
 *
 *      o mem_init() is called at startup, and mem_term() at
 *        close, which checks to see that the number of alloc's is
 *        the same as the number of free's.
 *      o Behavior on out-of-memory conditions can be controlled
 *        via mem_setexception().
 *
 * Extended debugging features:
 *
 *      o Enabled by #define MEM_DEBUG 1 when compiling.
 *      o Check values are inserted before and after the alloc'ed data
 *        to detect pointer underruns and overruns.
 *      o Free'd pointers are checked against alloc'ed pointers.
 *      o Free'd storage is cleared to smoke out references to free'd data.
 *      o Realloc'd pointers are always changed, and the previous storage
 *        is cleared, to detect erroneous dependencies on the previous
 *        pointer.
 *      o The routine mem_checkptr() is provided to check an alloc'ed
 *        pointer.
 */

/********************* GLOBAL VARIABLES *************************/

extern int mem_inited;          /* != 0 if mem package is initialized.  */
                                /* Test this if you have other packages */
                                /* that depend on mem being initialized */

/********************* PUBLIC FUNCTIONS *************************/

/***********************************
 * Set behavior when mem runs out of memory.
 * Input:
 *      flag =  MEM_ABORTMSG:   Abort the program with the message
 *                              'Fatal error: out of memory' sent
 *                              to stdout. This is the default behavior.
 *              MEM_ABORT:      Abort the program with no message.
 *              MEM_RETNULL:    Return NULL back to caller.
 *              MEM_CALLFP:     Call application-specified function.
 *                              fp must be supplied.
 *      fp                      Optional function pointer. Supplied if
 *                              (flag == MEM_CALLFP). This function returns
 *                              MEM_XXXXX, indicating what mem should do next.
 *                              The function could do things like swap
 *                              data out to disk to free up more memory.
 *      fp could also return:
 *              MEM_RETRY:      Try again to allocate the space. Be
 *                              careful not to go into an infinite loop.
 *      The type of fp is:
 *              int (*handler)(void)
 */

#if !MEM_NONE
#if __SC__ || __DMC__ || __GNUC__ || _MSC_VER
typedef int MEM_E;
enum { MEM_ABORTMSG, MEM_ABORT, MEM_RETNULL, MEM_CALLFP, MEM_RETRY };
void mem_setexception(MEM_E,...);
#else
#define MEM_ABORTMSG    0
#define MEM_ABORT       1
#define MEM_RETNULL     2
#define MEM_CALLFP      3
#define MEM_RETRY       4
void mem_setexception(int,...);
#endif
#endif

/****************************
 * Allocate space for string, copy string into it, and
 * return pointer to the new string.
 * This routine doesn't really belong here, but it is used so often
 * that I gave up and put it here.
 * Use:
 *      char *mem_strdup(const char *s);
 * Returns:
 *      pointer to copied string if succussful.
 *      else returns NULL (if MEM_RETNULL)
 */

char *mem_strdup(const char *);

/**************************
 * Function so we can have a pointer to function mem_free().
 * This is needed since mem_free is sometimes defined as a macro,
 * and then the preprocessor screws up.
 * The pointer to mem_free() is used frequently with the list package.
 * Use:
 *      void mem_freefp(void *p);
 */

/***************************
 * Check for errors. This routine does a consistency check on the
 * storage allocator, looking for corrupted data. It should be called
 * when the application has CPU cycles to burn.
 * Use:
 *      void mem_check(void);
 */

void mem_check(void);

/***************************
 * Check ptr to see if it is in the range of allocated data.
 * Cause assertion failure if it isn't.
 */

void mem_checkptr(void *ptr);

/***************************
 * Allocate and return a pointer to numbytes of storage.
 * Use:
 *      void *mem_malloc(size_t numbytes);
 *      void *mem_calloc(size_t numbytes); allocated memory is cleared
 * Input:
 *      numbytes        Number of bytes to allocate
 * Returns:
 *      if (numbytes > 0)
 *              pointer to allocated data, NULL if out of memory
 *      else
 *              return NULL
 */

void *mem_malloc(d_size_t);
void *mem_calloc(size_t);

/*****************************
 * Reallocate memory.
 * Use:
 *      void *mem_realloc(void *ptr,size_t numbytes);
 */

void *mem_realloc(void *,size_t);

/*****************************
 * Free memory allocated by mem_malloc(), mem_calloc() or mem_realloc().
 * Use:
 *      void mem_free(void *ptr);
 */

void mem_free(void *);

/***************************
 * Initialize memory handler.
 * Use:
 *      void mem_init(void);
 * Output:
 *      mem_inited = 1
 */

void mem_init(void);

/***************************
 * Terminate memory handler. Useful for checking for errors.
 * Use:
 *      void mem_term(void);
 * Output:
 *      mem_inited = 0
 */

void mem_term(void);

/*******************************
 * The mem_fxxx() functions are for allocating memory that will persist
 * until program termination. The trick is that if the memory is never
 * free'd, we can do a very fast allocation. If MEM_DEBUG is on, they
 * act just like the regular mem functions, so it can be debugged.
 */

#if MEM_NONE
#define mem_fmalloc(u)  malloc(u)
#define mem_fcalloc(u)  calloc((u),1)
#define mem_ffree(p)    ((void)0)
#define mem_fstrdup(p)  strdup(p)
#else
#if MEM_DEBUG
#define mem_fmalloc     mem_malloc
#define mem_fcalloc     mem_calloc
#define mem_ffree       mem_free
#define mem_fstrdup     mem_strdup
#else
void *mem_fmalloc(size_t);
void *mem_fcalloc(size_t);
#define mem_ffree(p)    ((void)0)
char *mem_fstrdup(const char *);
#endif
#endif

/* The following stuff forms the implementation rather than the
 * definition, so ignore it.
 */

#if MEM_NONE

#define mem_inited      1
#define mem_strdup(p)   strdup(p)
#define mem_malloc(u)   malloc(u)
#define mem_calloc(u)   calloc((u),1)
#define mem_realloc(p,u)        realloc((p),(u))
#define mem_free(p)     free(p)
#define mem_freefp      free
#define mem_check()     ((void)0)
#define mem_checkptr(p) ((void)(p))
#define mem_init()      ((void)0)
#define mem_term()      ((void)0)

#include <stdlib.h>

#else

#if MEM_DEBUG           /* if creating debug version    */
#define mem_strdup(p)   mem_strdup_debug((p),__FILE__,__LINE__)
#define mem_malloc(u)   mem_malloc_debug((u),__FILE__,__LINE__)
#define mem_calloc(u)   mem_calloc_debug((u),__FILE__,__LINE__)
#define mem_realloc(p,u)        mem_realloc_debug((p),(u),__FILE__,__LINE__)
#define mem_free(p)     mem_free_debug((p),__FILE__,__LINE__)

char *mem_strdup_debug  (const char *,const char *,int);
void *mem_calloc_debug  (size_t,const char *,int);
void *mem_malloc_debug  (size_t,const char *,int);
void *mem_realloc_debug (void *,size_t,const char *,int);
void  mem_free_debug    (void *,const char *,int);
void  mem_freefp        (void *);

void mem_setnewfileline (void *,const char *,int);

#else

#define mem_freefp      mem_free
#define mem_check()
#define mem_checkptr(p)

#endif /* MEM_DEBUG */
#endif /* MEM_NONE  */


#ifndef MEM_NOMEMCOUNT
#define MEM_NOMEMCOUNT  0
#endif

#if !MEM_NONE

#ifndef assert
#include        <assert.h>
#endif

#ifndef VAX11C
#ifdef BSDUNIX
#include <strings.h>
#else
#include <string.h>
#endif
#else
extern char *strcpy(),*memcpy();
extern int strlen();
#endif  /* VAX11C */

int mem_inited = 0;             /* != 0 if initialized                  */

static int mem_behavior = MEM_ABORTMSG;
static int (*oom_fp)(void) = NULL;  /* out-of-memory handler                */
static int mem_count;           /* # of allocs that haven't been free'd */
static int mem_scount;          /* # of sallocs that haven't been free'd */

/* Determine where to send error messages       */
#if _WINDLL
void err_message(const char *,...);
#define PRINT   err_message(
#elif MSDOS
#define PRINT   printf( /* stderr can't be redirected with MS-DOS       */
#else
#define ferr    stderr
#define PRINT   fprintf(ferr,
#endif

/*******************************/

void mem_setexception(MEM_E flag,...)
{   va_list ap;
    typedef int (*fp_t)(void);

    mem_behavior = flag;
    va_start(ap,flag);
    oom_fp = (mem_behavior == MEM_CALLFP) ? va_arg(ap,fp_t) : 0;
    va_end(ap);
#if MEM_DEBUG
    assert(0 <= flag && flag <= MEM_RETRY);
#endif
}

/*************************
 * This is called when we're out of memory.
 * Returns:
 *      1:      try again to allocate the memory
 *      0:      give up and return NULL
 */

int mem_exception()
{   int behavior;

    behavior = mem_behavior;
    while (1)
    {
        switch (behavior)
        {
            case MEM_ABORTMSG:
#if MSDOS || __OS2__ || __NT__ || _WIN32
                /* Avoid linking in buffered I/O */
            {   static char msg[] = "Fatal error: out of memory\r\n";

                write(1,msg,sizeof(msg) - 1);
            }
#else
                PRINT "Fatal error: out of memory\n");
#endif
                /* FALL-THROUGH */
            case MEM_ABORT:
                exit(EXIT_FAILURE);
                /* NOTREACHED */
            case MEM_CALLFP:
                assert(oom_fp);
                behavior = (*oom_fp)();
                break;
            case MEM_RETNULL:
                return 0;
            case MEM_RETRY:
                return 1;
            default:
                assert(0);
        }
    }
}

/****************************/

#if MEM_DEBUG

#undef mem_strdup

char *mem_strdup(const char *s)
{
        return mem_strdup_debug(s,__FILE__,__LINE__);
}

char *mem_strdup_debug(const char *s,const char *file,int line)
{
        char *p;

        p = s
            ? (char *) mem_malloc_debug((unsigned) strlen(s) + 1,file,line)
            : NULL;
        return p ? strcpy(p,s) : p;
}
#else
char *mem_strdup(const char *s)
{
        if (s)
        {   size_t len = strlen(s) + 1;
            char *p = (char *) mem_malloc(len);
            if (p)
                return (char *)memcpy(p,s,len);
        }
        return NULL;
}

#endif /* MEM_DEBUG */

#if __cplusplus && !MEM_NONE
extern "C++"
{

/* Cause initialization and termination functions to be called  */
#if 0
static struct cMemDebug
{
    cMemDebug() { mem_init(); }
   ~cMemDebug() { mem_term(); }
} dummy;
#endif

} // extern "C++"
#endif

#if MEM_DEBUG

static size_t mem_maxalloc;       /* max # of bytes allocated             */
static size_t mem_numalloc;       /* current # of bytes allocated         */

#define BEFOREVAL       0x4F464542      /* value to detect underrun     */
#define AFTERVAL        0x45544641      /* value to detect overrun      */

#if SUN || SUN386
static long afterval = AFTERVAL;        /* so we can do &afterval       */
#endif

/* The following should be selected to give maximum probability that    */
/* pointers loaded with these values will cause an obvious crash. On    */
/* Unix machines, a large value will cause a segment fault.             */
/* MALLOCVAL is the value to set malloc'd data to.                      */

#if MSDOS || __OS2__ || __NT__ || _WIN32
#define BADVAL          0xFF
#define MALLOCVAL       0xEE
#else
#define BADVAL          0x7A
#define MALLOCVAL       0xEE
#endif

/* Disable mapping macros       */
#undef  mem_malloc
#undef  mem_calloc
#undef  mem_realloc
#undef  mem_free

/* Create a list of all alloc'ed pointers, retaining info about where   */
/* each alloc came from. This is a real memory and speed hog, but who   */
/* cares when you've got obscure pointer bugs.                          */

static struct mem_debug
{
    struct mem_debug *Mnext;    /* next in list                         */
    struct mem_debug *Mprev;    /* previous value in list               */
    const char *Mfile;          /* filename of where allocated          */
    int Mline;                  /* line number of where allocated       */
    size_t Mnbytes;             /* size of the allocation               */
    unsigned long Mbeforeval;   /* detect underrun of data              */
    char data[1];               /* the data actually allocated          */
} mem_alloclist =
{
        (struct mem_debug *) NULL,
        (struct mem_debug *) NULL,
        NULL,
        11111,
        0,
        BEFOREVAL,
#if !(__linux__ || __APPLE__ || __FreeBSD__ || __OpenBSD__ || __DragonFly__ || __sun)
        AFTERVAL
#endif
};

/* Determine allocation size of a mem_debug     */
#define mem_debug_size(n)       (sizeof(struct mem_debug) - 1 + (n) + sizeof(AFTERVAL))

/* Convert from a void *to a mem_debug struct.  */
#define mem_ptrtodl(p)  ((struct mem_debug *) ((char *)p - offsetof(struct mem_debug,data[0])))

/* Convert from a mem_debug struct to a mem_ptr.        */
#define mem_dltoptr(dl) ((void *) &((dl)->data[0]))

/*****************************
 * Set new value of file,line
 */

void mem_setnewfileline( void *ptr, const char *fil, int lin)
{
    struct mem_debug *dl;

    dl = mem_ptrtodl(ptr);
    dl->Mfile = fil;
    dl->Mline = lin;
}

/****************************
 * Print out struct mem_debug.
 */

static void mem_printdl(struct mem_debug *dl)
{
        PRINT "alloc'd from file '%s' line %d nbytes %d ptr %p\n",
                dl->Mfile,dl->Mline,dl->Mnbytes,(long)mem_dltoptr(dl));
}

/****************************
 * Print out file and line number.
 */

static void mem_fillin(const char *fil, int lin)
{
        PRINT "File '%s' line %d\n",fil,lin);
#ifdef ferr
        fflush(ferr);
#endif
}

/****************************
 * If MEM_DEBUG is not on for some modules, these routines will get
 * called.
 */

void *mem_calloc(size_t u)
{
        return mem_calloc_debug(u,__FILE__,__LINE__);
}

void *mem_malloc(d_size_t u)
{
        return mem_malloc_debug(u,__FILE__,__LINE__);
}

void *mem_realloc(void *p, size_t u)
{
        return mem_realloc_debug(p,u,__FILE__,__LINE__);
}

void mem_free(void *p)
{
        mem_free_debug(p,__FILE__,__LINE__);
}


/**************************/

void mem_freefp(void *p)
{
        mem_free(p);
}

/***********************
 * Debug versions of mem_calloc(), mem_free() and mem_realloc().
 */

void *mem_malloc_debug(size_t n, const char *fil, int lin)
{   void *p;

    p = mem_calloc_debug(n,fil,lin);
    if (p)
        memset(p,MALLOCVAL,n);
    return p;
}

void *mem_calloc_debug(size_t n, const char *fil, int lin)
{
    struct mem_debug *dl;

    do
        dl = (struct mem_debug *) calloc(mem_debug_size(n),1);
    while (dl == NULL && mem_exception());
    if (dl == NULL)
        return NULL;
    dl->Mfile = fil;
    dl->Mline = lin;
    dl->Mnbytes = n;
    dl->Mbeforeval = BEFOREVAL;
#if SUN || SUN386 /* bus error if we store a long at an odd address */
    memcpy(&(dl->data[n]),&afterval,sizeof(AFTERVAL));
#else
    *(long *) &(dl->data[n]) = AFTERVAL;
#endif

    /* Add dl to start of allocation list       */
    dl->Mnext = mem_alloclist.Mnext;
    dl->Mprev = &mem_alloclist;
    mem_alloclist.Mnext = dl;
    if (dl->Mnext != NULL)
        dl->Mnext->Mprev = dl;

    mem_count++;
    mem_numalloc += n;
    if (mem_numalloc > mem_maxalloc)
        mem_maxalloc = mem_numalloc;
    return mem_dltoptr(dl);
}

void mem_free_debug(void *ptr, const char *fil, int lin)
{
        struct mem_debug *dl;
        int error;

        if (ptr == NULL)
                return;
        if (mem_count <= 0)
        {       PRINT "More frees than allocs at ");
                goto err;
        }
        dl = mem_ptrtodl(ptr);
        if (dl->Mbeforeval != BEFOREVAL)
        {
                PRINT "Pointer x%lx underrun\n",(long)ptr);
                PRINT "'%s'(%d)\n",fil,lin);
                goto err2;
        }
#if SUN || SUN386 /* Bus error if we read a long from an odd address    */
        error = (memcmp(&dl->data[dl->Mnbytes],&afterval,sizeof(AFTERVAL)) != 0);
#else
        error = (*(long *) &dl->data[dl->Mnbytes] != AFTERVAL);
#endif
        if (error)
        {
                PRINT "Pointer x%lx overrun\n",(long)ptr);
                goto err2;
        }
        mem_numalloc -= dl->Mnbytes;
        if (mem_numalloc < 0)
        {       PRINT "error: mem_numalloc = %ld, dl->Mnbytes = %d\n",
                        mem_numalloc,dl->Mnbytes);
                goto err2;
        }

        /* Remove dl from linked list   */
        if (dl->Mprev)
                dl->Mprev->Mnext = dl->Mnext;
        if (dl->Mnext)
                dl->Mnext->Mprev = dl->Mprev;

        /* Stomp on the freed storage to help detect references */
        /* after the storage was freed.                         */
        memset((void *) dl,BADVAL,sizeof(*dl) + dl->Mnbytes);
        mem_count--;

        free((void *) dl);
        return;

err2:
        mem_printdl(dl);
err:
        PRINT "free'd from ");
        mem_fillin(fil,lin);
        assert(0);
        /* NOTREACHED */
}

/*******************
 * Debug version of mem_realloc().
 */

void *mem_realloc_debug(void *oldp, size_t n, const char *fil, int lin)
{   void *p;
    struct mem_debug *dl;

    if (n == 0)
    {   mem_free_debug(oldp,fil,lin);
        p = NULL;
    }
    else if (oldp == NULL)
        p = mem_malloc_debug(n,fil,lin);
    else
    {
        p = mem_malloc_debug(n,fil,lin);
        if (p != NULL)
        {
            dl = mem_ptrtodl(oldp);
            if (dl->Mnbytes < n)
                n = dl->Mnbytes;
            memcpy(p,oldp,n);
            mem_free_debug(oldp,fil,lin);
        }
    }
    return p;
}

/***************************/

static void mem_checkdl(struct mem_debug *dl)
{   void *p;
#if (__SC__ || __DMC__) && !_WIN32
    unsigned u;
    int error;

    /* Take advantage of fact that SC's allocator stores the size of the
     * alloc in the unsigned immediately preceding the allocation.
     */
    u = ((unsigned *)dl)[-1] - sizeof(unsigned);
    assert((u & (sizeof(unsigned) - 1)) == 0 && u >= mem_debug_size(dl->Mnbytes));
#endif
    p = mem_dltoptr(dl);
    if (dl->Mbeforeval != BEFOREVAL)
    {
            PRINT "Pointer x%lx underrun\n",(long)p);
            goto err2;
    }
#if SUN || SUN386 /* Bus error if we read a long from an odd address    */
    error = memcmp(&dl->data[dl->Mnbytes],&afterval,sizeof(AFTERVAL)) != 0;
#else
    error = *(long *) &dl->data[dl->Mnbytes] != AFTERVAL;
#endif
    if (error)
    {
            PRINT "Pointer x%lx overrun\n",(long)p);
            goto err2;
    }
    return;

err2:
    mem_printdl(dl);
    assert(0);
}

/***************************/

void mem_check()
{   register struct mem_debug *dl;

#if (__SC__ || _MSC_VER) && !defined(malloc)
    int i;

    i = _heapset(0xF4);
    assert(i == _HEAPOK);
#endif
    for (dl = mem_alloclist.Mnext; dl != NULL; dl = dl->Mnext)
        mem_checkdl(dl);
}

/***************************/

void mem_checkptr(void *p)
{   register struct mem_debug *dl;

    for (dl = mem_alloclist.Mnext; dl != NULL; dl = dl->Mnext)
    {
        if (p >= (void *) &(dl->data[0]) &&
            p < (void *)((char *)dl + sizeof(struct mem_debug)-1 + dl->Mnbytes))
            goto L1;
    }
    assert(0);

L1:
    mem_checkdl(dl);
}

#else

/***************************/

void *mem_malloc(d_size_t numbytes)
{       void *p;

        if (numbytes == 0)
                return NULL;
        while (1)
        {
                p = malloc(numbytes);
                if (p == NULL)
                {       if (mem_exception())
                                continue;
                }
#if !MEM_NOMEMCOUNT
                else
                        mem_count++;
#endif
                break;
        }
        /*printf("malloc(%d) = x%lx, mem_count = %d\n",numbytes,p,mem_count);*/
        return p;
}

/***************************/

void *mem_calloc(size_t numbytes)
{       void *p;

        if (numbytes == 0)
            return NULL;
        while (1)
        {
                p = calloc(numbytes,1);
                if (p == NULL)
                {       if (mem_exception())
                                continue;
                }
#if !MEM_NOMEMCOUNT
                else
                        mem_count++;
#endif
                break;
        }
        /*printf("calloc(%d) = x%lx, mem_count = %d\n",numbytes,p,mem_count);*/
        return p;
}

/***************************/

void *mem_realloc(void *oldmem_ptr,size_t newnumbytes)
{   void *p;

    if (oldmem_ptr == NULL)
        p = mem_malloc(newnumbytes);
    else if (newnumbytes == 0)
    {   mem_free(oldmem_ptr);
        p = NULL;
    }
    else
    {
        do
            p = realloc(oldmem_ptr,newnumbytes);
        while (p == NULL && mem_exception());
    }
    /*printf("realloc(x%lx,%d) = x%lx, mem_count = %d\n",oldmem_ptr,newnumbytes,p,mem_count);*/
    return p;
}

/***************************/

void mem_free(void *ptr)
{
    /*printf("free(x%lx) mem_count=%d\n",ptr,mem_count);*/
    if (ptr != NULL)
    {
#if !MEM_NOMEMCOUNT
        assert(mem_count != 0);
        mem_count--;
#endif
        free(ptr);
    }
}

/***************************/
/* This is our low-rent fast storage allocator  */

static char *heap;
static size_t heapleft;

/***************************/

#if 0 && __SC__ && __INTSIZE == 4 && __I86__ && !_DEBUG_TRACE && _WIN32 && (SCC || SCPP || JAVA)

__declspec(naked) void *mem_fmalloc(size_t numbytes)
{
    __asm
    {
        mov     EDX,4[ESP]
        mov     EAX,heap
        add     EDX,3
        mov     ECX,heapleft
        and     EDX,~3
        je      L5A
        cmp     EDX,ECX
        ja      L2D
        sub     ECX,EDX
        add     EDX,EAX
        mov     heapleft,ECX
        mov     heap,EDX
        ret     4

L2D:    push    EBX
        mov     EBX,EDX
//      add     EDX,03FFFh
//      and     EDX,~03FFFh
        add     EDX,03C00h
        mov     heapleft,EDX
L3D:    push    heapleft
        call    mem_malloc
        test    EAX,EAX
        mov     heap,EAX
        jne     L18
        call    mem_exception
        test    EAX,EAX
        jne     L3D
        pop     EBX
L5A:    xor     EAX,EAX
        ret     4

L18:    add     heap,EBX
        sub     heapleft,EBX
        pop     EBX
        ret     4
    }
}

#else

void *mem_fmalloc(size_t numbytes)
{   void *p;

    //printf("fmalloc(%d)\n",numbytes);
#if defined(__GNUC__) || defined(__clang__)
    // GCC and Clang assume some types, notably elem (see DMD issue 6215),
    // to be 16-byte aligned. Because we do not have any type information
    // available here, we have to 16 byte-align everything.
    numbytes = (numbytes + 0xF) & ~0xF;
#else
    if (sizeof(size_t) == 2)
        numbytes = (numbytes + 1) & ~1;         /* word align   */
    else
        numbytes = (numbytes + 3) & ~3;         /* dword align  */
#endif

    /* This ugly flow-of-control is so that the most common case
       drops straight through.
     */

    if (!numbytes)
        return NULL;

    if (numbytes <= heapleft)
    {
     L2:
        p = (void *)heap;
        heap += numbytes;
        heapleft -= numbytes;
        return p;
    }

#if 1
    heapleft = numbytes + 0x3C00;
    if (heapleft >= 16372)
        heapleft = numbytes;
#elif _WIN32
    heapleft = (numbytes + 0x3FFF) & ~0x3FFF;   /* round to next boundary */
#else
    heapleft = 0x3F00;
    assert(numbytes <= heapleft);
#endif
L1:
    heap = (char *)malloc(heapleft);
    if (!heap)
    {   if (mem_exception())
            goto L1;
        return NULL;
    }
    goto L2;
}

#endif

/***************************/

void *mem_fcalloc(size_t numbytes)
{   void *p;

    p = mem_fmalloc(numbytes);
    return p ? memset(p,0,numbytes) : p;
}

/***************************/

char *mem_fstrdup(const char *s)
{
        if (s)
        {   size_t len = strlen(s) + 1;
            char *p = (char *) mem_fmalloc(len);
            if (p)
                return (char *)memcpy(p,s,len);
        }
        return NULL;
}

#endif

/***************************/

void mem_init()
{
        if (mem_inited == 0)
        {       mem_count = 0;
                mem_scount = 0;
                oom_fp = NULL;
                mem_behavior = MEM_ABORTMSG;
#if MEM_DEBUG
                mem_numalloc = 0;
                mem_maxalloc = 0;
                mem_alloclist.Mnext = NULL;
#if __linux__ || __APPLE__ || __FreeBSD__ || __OpenBSD__ || __DragonFly__ || __sun
                *(long *) &(mem_alloclist.data[0]) = AFTERVAL;
#endif
#endif
#if (__ZTC__ || __SC__ || __DMC__) && !defined(malloc)
                free(malloc(1));        /* initialize storage allocator */
#endif
#if MEM_DEBUG && (__SC__ || _MSC_VER) && !defined(malloc)
                {   int i;

                    i = _heapset(0xF4);
                    assert(i == _HEAPOK);
                }
#endif
        }
        mem_inited++;
}

/***************************/

void mem_term()
{
        if (mem_inited)
        {
#if MEM_DEBUG
                struct mem_debug *dl;

                for (dl = mem_alloclist.Mnext; dl; dl = dl->Mnext)
                {       PRINT "Unfreed pointer: ");
                        mem_printdl(dl);
                }
#if 0
                PRINT "Max amount ever allocated == %ld bytes\n",
                        mem_maxalloc);
#endif
#if (__SC__ || _MSC_VER) && !defined(malloc)
                {   int i;

                    i = _heapset(0xF4);
                    assert(i == _HEAPOK);
                }
#endif
#else
                if (mem_count)
                        PRINT "%d unfreed items\n",mem_count);
                if (mem_scount)
                        PRINT "%d unfreed s items\n",mem_scount);
#endif /* MEM_DEBUG */
                assert(mem_count == 0 && mem_scount == 0);
        }
        mem_inited = 0;
}

#endif /* !MEM_NONE */

}
