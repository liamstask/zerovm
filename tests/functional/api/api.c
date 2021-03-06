/*
 * the zerovm api test. compares the zerovm manifest to the user manifest
 */
#include "include/zvmlib.h"
#include "include/ztest.h"

#define LOT_ENOUGH 0x100

/* prepare everything to cheat "manifest_parser.c" */
/* skip "extra load" headers */
#define TOOLS_H_ 1
#define __G_LIB_H__ 1
#define _ERRNO_H 1
#define _CTYPE_H 1

/* disable falling */
#undef ZLOGFAIL
#define ZLOGFAIL(...)
#define ZLogTag(...)
#define ZLog(...)
#define FailIf(...)
#define fopen(...) NULL
#define fread(...) -1
#define fclose(...)
#define GetFileSize(...) -1

/* redefine functions */
#define g_malloc MALLOC
#define g_malloc0(size) CALLOC(size, 1)
#define g_free FREE

/* a cheat to have an access to static members */
#define NDEBUG /* to disable assert() */
#include "src/main/manifest_parser.c"

#define CONTROL "/dev/control"
#define CHANNEL_ATTRIBUTES_NUMBER 8
enum ChannelFields {
  CFieldURI,
  CFieldAlias,
  CFieldType,
  CFieldTag,
  CFieldGetsLimit,
  CFieldGetSizeLimit,
  CFieldPutsLimit,
  CFieldPutSizeLimit
};

#define FOURGIG 0x100000000ULL

/*
 * get control data and parse it to make part of manifest parser
 * functions usable: GetValueByKey, GetValuesByKey, ParseValue
 * return 0 if success, otherwise -1
 */
static void manifest_ctor()
{
  static char buf_mft[BIG_ENOUGH];
  static char buf_ptr[BIG_ENOUGH];

  /* get the data and initialize manifest */
  mft_count = 0;
  mft_data = buf_mft;
  mft_ptr = (void*)buf_ptr;

  ZTEST(READ(CONTROL, mft_data, MANIFEST->channels[OPEN(CONTROL)].size) > 0);

  ParseManifest();
  ZTEST(mft_count > 0);
}

void check_memory()
{
  int64_t umft_size;
  int64_t heap_end;

  /* fail if memory is not set in manifest */
  ZTEST(GetValueByKey("Memory") != NULL);

  /* user manifest area size */
  umft_size = (uintptr_t)&MANIFEST->channels[MANIFEST->channels_count - 1].name;
  umft_size = (FOURGIG - MANIFEST->stack_size) - ROUNDDOWN_64K(umft_size);

  /* get memory size specified in manifest */
  heap_end = ATOI(GetValueByKey("Memory"));
  ZTEST(heap_end > 0);

  /* calculate approximate heap_ptr */
  heap_end -= MANIFEST->stack_size;
  ZTEST(heap_end - (uintptr_t)MANIFEST->heap_ptr - MANIFEST->heap_size < umft_size);
}

/* parse the channel record, find the channel and test its info */
static void test_channel(char *token)
{
  char *tokens[LOT_ENOUGH];
  int i;

  /* show channel control info */
  FPRINTF(STDERR, "the testing channel control info: ");
  FPRINTF(STDERR, token);
  FPRINTF(STDERR, "\n");

  /* parse token */
  i = ParseValue(token, ", ", tokens, LOT_ENOUGH);
  ZTEST(i == CHANNEL_ATTRIBUTES_NUMBER);

  /* find appropriate channel */
  i = OPEN(tokens[CFieldAlias]);

  /* compare channel attributes with the control ones */
  ZTEST(STRCMP(MANIFEST->channels[i].name, tokens[CFieldAlias]) == 0);
  ZTEST(MANIFEST->channels[i].limits[GetsLimit] == ATOI(tokens[CFieldGetsLimit]));
  ZTEST(MANIFEST->channels[i].limits[GetSizeLimit] == ATOI(tokens[CFieldGetSizeLimit]));
  ZTEST(MANIFEST->channels[i].limits[PutsLimit] == ATOI(tokens[CFieldPutsLimit]));
  ZTEST(MANIFEST->channels[i].limits[PutSizeLimit] == ATOI(tokens[CFieldPutSizeLimit]));
  ZTEST(MANIFEST->channels[i].type == ATOI(tokens[CFieldType]));
}

int main(int argc, char **argv)
{
  char *tokens[LOT_ENOUGH];
  int number;

  /* get the control data */
  manifest_ctor();

  /* check general information provided by zerovm */
  /* memory */
  check_memory();

  /* check channels number */
  number = GetValuesByKey("Channel", tokens, LOT_ENOUGH);
  ZTEST(MANIFEST->channels_count == number);

  /* test channels one by one */
  for(number = 0; number < MANIFEST->channels_count; ++number)
    test_channel(tokens[number]);

  /* count errors and exit with it */
  ZREPORT;
  return 0;
}
