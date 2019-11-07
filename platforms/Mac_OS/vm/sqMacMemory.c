/****************************************************************************
*   PROJECT: Mac allocate memory from somewhere for the image
*   FILE:    sqMacMemory.c
*   CONTENT: 
*
*   AUTHOR:  John Maloney, John McIntosh, and others.
*   ADDRESS: 
*   EMAIL:   johnmci@smalltalkconsulting.com
*   RCSID:   $Id: sqMacMemory.c 1708 2007-06-10 00:40:04Z johnmci $
*
*   NOTES: 
*  Feb 22nd, 2002, JMM moved code into 10 other files, see sqMacMain.c for comments
*  Mar  8th, 2002, JMM Must unmap view first then free.
*  3.2.8b1 July 24th, 2002 JMM support for os-x plugin under IE 5.x
*  3.5.1b5 June 25th, 2003 JMM get memory upper limit from os-x user preferences
 3.8.11b1 Mar 4th, 2006 JMM refactor, cleanup and add headless support
*	3.8.14b1 Oct	,2006 JMM browser rewrite

*****************************************************************************/

#include "sq.h" 
#include "sqMacMemory.h"
#include "sqMacMain.h"

#include <sys/mman.h>
#include <unistd.h>

#if ! SPURVM /* SPURVM uses unix source code */
extern usqLong  gMaxHeapSize;
static usqLong	gHeapSize;
void *mmapWasAt;

/* compute the desired memory allocation */

static usqInt	memoryAllocationBase;
static int	    pageSize = 0;
static unsigned int pageMask = 0;

#define roundDownToPage(v) ((v)&pageMask)
#define roundUpToPage(v) (((v)+pageSize-1)&pageMask)

usqInt
sqGetAvailableMemory()
{
	/******
	  Note:  (Except Spur)
	    For os-x this doesn't matter we just mmap 512MB for the image, and 
	    the application allocates more out of the 4GB address for VM logic. 
	******/

	return gMaxHeapSize >= 0xFFFFFFFFULL ? 0xFFFFFFFF : gMaxHeapSize;
}

usqInt
sqAllocateMemoryMac(usqInt desiredHeapSize, usqInt minHeapSize)
{
    void * debug, *actually;
# pragma unused(minHeapSize,desiredHeapSize)

	pageSize= getpagesize();
	pageMask= ~(pageSize - 1);
    gHeapSize = gMaxHeapSize;
    debug = mmap( NULL, gMaxHeapSize+pageSize, PROT_READ | PROT_WRITE, MAP_ANON | MAP_SHARED,-1,0);

    if (debug == MAP_FAILED)
        return 0;
	mmapWasAt = debug;
	actually = debug+pageSize-1;
	actually = (void*) (((usqInt) actually) & pageMask);

	return memoryAllocationBase = (usqInt) actually;
}

sqInt
sqGrowMemoryBy(sqInt memoryLimit, sqInt delta)
{
    if ((usqInt) memoryLimit + (usqInt) delta - memoryAllocationBase > gMaxHeapSize)
        return memoryLimit;

    gHeapSize += delta;
    return memoryLimit + delta;
}

sqInt
sqShrinkMemoryBy(sqInt memoryLimit, sqInt delta)
{
    return sqGrowMemoryBy(memoryLimit,0-delta);
}

sqInt
sqMemoryExtraBytesLeft(int flag)
{
    return flag ? gMaxHeapSize - gHeapSize : 0;
}

void
sqMacMemoryFree()
{
	if (!memoryAllocationBase) 
		return;
	if (munmap((void *)memoryAllocationBase,gMaxHeapSize+pageSize))
		perror("munmap");
	memoryAllocationBase = 0;
}

#if COGVM
#   if DUAL_MAPPED_CODE_ZONE
/* We are indebted to Chris Wellons who designed this elegant API for dual
 * mapping which we depend on for fine-grained code modification (classical
 * Deutsch/Schiffman style inline cacheing and derivatives).  Chris's code is:
	https://nullprogram.com/blog/2016/04/10/
 *
 * To cope with modern OSs that disallow executing code in writable memory we
 * dual-map the code zone, one mapping with read/write permissions and the other
 * with read/execute permissions. In such a configuration the code zone has
 * already been alloated and is not included in (what is no longer) the initial
 * alloc.
 */
static void
memory_alias_map(size_t size, size_t naddr, void **addrs)
{
extern char  *exeName;
	char path[128];
	snprintf(path, sizeof(path), "/%s(%lu,%p)",
			 exeName ? exeName : __FUNCTION__, (long)getpid(), addrs);
	int fd = shm_open(path, O_RDWR | O_CREAT | O_EXCL, 0600);
	if (fd == -1) {
		perror("memory_alias_map: shm_open");
		exit(0666);
	}
	shm_unlink(path);
	ftruncate(fd, size);
	for (size_t i = 0; i < naddr; i++) {
		addrs[i] = mmap(addrs[i], size, PROT_READ | PROT_WRITE,
								addrs[i] ? MAP_FIXED | MAP_SHARED : MAP_SHARED,
								fd, 0);
		if (addrs[i] == MAP_FAILED) {
			perror("memory_alias_map: mmap(addrs[i]...");
			exit(0667);
		}
	}
	close(fd);
	return;
}
#   endif /* DUAL_MAPPED_CODE_ZONE */
void
sqMakeMemoryExecutableFromToCodeToDataDelta(usqInt startAddr,
											usqInt endAddr,
											sqInt *codeToDataDelta)
{
	usqInt firstPage = roundDownToPage(startAddr);
	usqInt size = endAddr - firstPage;

#  if DUAL_MAPPED_CODE_ZONE
	usqInt mappings[2];

	mappings[0] = firstPage;
	mappings[1] = 0;

	memory_alias_map(size, 2, (void **)mappings);
	assert(mappings[0] == firstPage);
	*codeToDataDelta = mappings[1] - startAddr;

	if (mprotect((void *)firstPage,
				 size,
				 PROT_READ | PROT_EXEC) < 0)
		perror("mprotect(x,y,PROT_READ | PROT_EXEC)");

#  else /* DUAL_MAPPED_CODE_ZONE */

	if (mprotect((void *)firstPage,
				 size,
				 PROT_READ | PROT_WRITE | PROT_EXEC) < 0)
		perror("mprotect(x,y,PROT_READ | PROT_WRITE | PROT_EXEC)");

	*codeToDataDelta = 0;
#  endif
}

void
sqMakeMemoryNotExecutableFromTo(usqInt startAddr, usqInt endAddr)
{
# if 0
	/* We get EACCESS on 10.6.3 when trying to disable exec perm; Why? */
	/* Arguably this is pointless since allocated memory always does include
	 * write permission.  Annoyingly the mprotect call fails on both linux &
	 * mac os x.  So make the whole thing a nop.
	 */
	usqInt firstPage = roundDownToPage(startAddr);
	if (mprotect((void *)firstPage,
				 endAddr - firstPage + 1,
				 PROT_READ | PROT_WRITE) < 0
	 && errno != EACCES)
		perror("mprotect(x,y,PROT_READ | PROT_WRITE)");
# endif
}
#endif /* COGVM */

#endif /* SPURVM */
