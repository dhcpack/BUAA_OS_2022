/* See COPYRIGHT for copyright information. */

#ifndef _ERROR_H_
#define _ERROR_H_

// Kernel error codes -- keep in sync with list in kern/printf.c.
#define E_UNSPECIFIED	1	// Unspecified or unknown problem
#define E_BAD_ENV       2       // Environment doesn't exist or otherwise
				// cannot be used in requested action
#define E_INVAL		3	// Invalid parameter
#define E_NO_MEM	4	// Request failed due to memory shortage
#define E_NO_FREE_ENV   5       // Attempt to create a new environment beyond
				// the maximum allowed
#define E_IPC_NOT_RECV  6	// Attempt to send to env that is not recving.

// File system error codes -- only seen in user-level
#define	E_NO_DISK	7	// No free space left on disk
#define E_MAX_OPEN	8	// Too many files are open
#define E_NOT_FOUND	9 	// File or block not found
#define E_BAD_PATH	10	// Bad path
#define E_FILE_EXISTS	11	// File already exists
#define E_NOT_EXEC	12	// File not a valid executable

// define for lab4-challenge
// pthread
#define E_THREAD_MAX     13  // current env has max threads or current sem has max threads
#define E_THREAD_NOT_FOUND  14  // thread not found
#define E_THREAD_JOIN_FAIL  15  // detached thread cannot join
#define E_THREAD_DETACHED_FAIL  16  // thread already joined and waiting

// sem
#define E_SEM_NOT_FOUND  17  // sem not found in current env scope
#define E_SEM_AGAIN     18  // semaphore value <= 0
#define E_SEM_FREE      19  // action on a free semaphore
#define E_SEM_STILL_USED 20
#define MAXERROR 19 // TODO

#endif // _ERROR_H_
