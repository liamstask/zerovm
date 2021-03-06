/*
 * ztest.h
 *
 *  Created on: Feb 26, 2013
 *      Author: bortoq
 */

#ifndef ZTEST_H_
#define ZTEST_H_

static int errcount = 0;
#ifndef ERRCOUNT
#define ERRCOUNT errcount
#endif

#define PAGESIZE 0x10000
#define ROUNDDOWN_64K(a) ((a) & ~(PAGESIZE - 1LLU))
#define ROUNDUP_64K(a) ROUNDDOWN_64K((a) + PAGESIZE - 1LLU)

#define UNREFERENCED_VAR(a) do { (void)a; } while(0)
#define UNREFERENCED_FUNCTION(f) do {if(0) f();} while(0)

#define ZTEST(cond) \
  do {\
    if(cond)\
      FPRINTF(STDERR, "succeed on %3d: %s\n", __LINE__, #cond);\
    else\
    {\
      FPRINTF(STDERR, "failed on %4d: %s\n", __LINE__, #cond);\
      ++ERRCOUNT;\
    }\
  } while(0)

/* count errors and exit with it */
#define ZREPORT \
  do { \
    if(ERRCOUNT > 0) \
      FPRINTF(STDERR, "TEST FAILED with %d errors\n", ERRCOUNT); \
    else \
      FPRINTF(STDERR, "TEST SUCCEED\n\n"); \
    exit(ERRCOUNT); \
  } while(0)

/* works like ZTEST but ends the test execution after error */
#define ZFAIL(cond) \
  do { \
    ZTEST(cond); \
    if(!(cond)) ZREPORT; \
  } while(0)

#endif /* ZTEST_H_ */
