/*
 * zerovm manifest parser
 *
 * the manifest file format: key = value. parser ignores white spaces
 * each line can only contain single key=value
 *
 * the api is very simple: just call GetValuByKey() after
 * manifest is constructed
 *
 * Copyright (c) 2012, LiteStack, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>
#include <ctype.h>
#include <assert.h>
#include <glib.h>

#include "src/main/tools.h"
#include "src/main/manifest_parser.h"

static int32_t mft_count; /* number of found keys */
static char *mft_data; /* area to hold manifest text */
static struct
{
  char *key;
  char *value;
} *mft_ptr; /* array of pointers to keys/values */

/* remove leading and ending spaces from the given string */
static char *cut_spaces(char *a)
{
  char *end;
  if (a == NULL) return a;

  while (isspace(*a)) a++;  /* Trim leading space */
  if (*a == 0) return NULL; /* if string is empty */
  end = a + strlen(a) - 1;
  while (end > a && isspace(*end))
    end--;                  /* Trim trailing space */
  *(end + 1) = 0;           /* Write new null terminator */

  return a;
}

/* return string until '=' */
/* todo(d'b): can be replaced with ParseValue() */
static char *get_key(char *a)
{
  char *end;
  if(a == NULL) return a;

  end = strrchr(a, (int)'=');
  if(end == NULL) return NULL;  /* string does not contain '='. invalid key. */
  if(a == end) return NULL;     /* empty key is not allowed. invalid key. */
  *end  = '\0';                 /* cut the string in position of the last '=' */

  return cut_spaces(a);
}

/* return string after '=' */
/* todo(d'b): can be replaced with ParseValue() */
static char* get_value(char *a)
{
  char *begin;
  if(a == NULL) return a;

  begin = strchr(a, '=');
  if(begin == NULL) return NULL;        /* string does not contain '='. invalid line. */
  if(strchr(++begin, '=')) return NULL; /* string contain more the one '='. invalid line. */
  if(*begin == '\0') return NULL;       /* empty value is not allowed. invalid line. */

  return cut_spaces(begin);
}

/* parse manifest from memory. return 0 if success */
static int ParseManifest()
{
  char *str = mft_data;
  char *p;

  assert(mft_count == 0);
  assert(mft_data != 0);
  assert(mft_ptr != 0);

  /* warning: the order of extracting pair key/value does matter */
  p = strtok(str, EOL);
  while(p)
  {
    mft_ptr[mft_count].value = get_value(p);
    mft_ptr[mft_count].key = get_key(p);
    if (mft_ptr[mft_count].key && mft_ptr[mft_count].value) ++mft_count;
    p = strtok(NULL, EOL);
  }

  /* return error if no recods were found */
  return mft_count ? 0 : -1;
}

char *GetValueByKey(const char *key)
{
  int i;

  /* check for a design error */
  assert(key != NULL);
  assert(mft_ptr != NULL);
  assert(mft_count > 0);

  for(i = 0; i < mft_count; ++i)
  {
    if(strcmp(key, mft_ptr[i].key) == 0)
      return mft_ptr[i].value;
  }
  return NULL;
}

int GetValuesByKey(const char *key, char *values[], int capacity)
{
  int count = 0;
  int i;

  /* check for a design error */
  assert(key != NULL);
  assert(values != NULL);
  assert(capacity > 0);
  assert(mft_ptr != NULL);
  assert(mft_count > 0);

  /* populate array with found "value" strings */
  for(i = 0; i < mft_count; ++i)
    if(strcmp(key, mft_ptr[i].key) == 0 && count < capacity)
      values[count++] = mft_ptr[i].value;

  return count;
}

int ParseValue(char *value, const char *delimiter, char *tokens[], int capacity)
{
  int count;
  int i;

  /* control design errors */
  assert(delimiter != NULL);
  assert(tokens != NULL);
  assert(capacity > 0);
  assert(mft_ptr != NULL);
  assert(mft_count > 0);

  /* by design. useful */
  if(value == NULL) return 0;

  /* 1st token */
  tokens[0] = cut_spaces(strtok(value, delimiter));
  count = tokens[0] != NULL ? 1 : 0;

  /* the rest of tokens */
  for(i = 1; i < capacity; ++i)
  {
    tokens[i] = cut_spaces(strtok(NULL, delimiter));
    if(tokens[i] != NULL) ++count;
  }

  return count;
}

int ManifestCtor(const char *name)
{
  int64_t mft_size;
  int retcode;
  FILE *mft;

  /* get file size and check it for sanity */
  mft = fopen(name, "r");
  ZLOGFAIL(mft == NULL, errno, "cannot open manifest file %s", name);
  mft_size = GetFileSize(name);
  ZLOGFAIL(mft_size > MANIFEST_MAX, EFBIG, "manifest file is too large: %d", mft_size);
  ZLOGFAIL(mft_size == 0, EPERM, "manifest file is empty");

  /* allocate memory to hold manifest data */
  mft_data = g_malloc(mft_size + 1);
  mft_ptr =  g_malloc0(mft_size * sizeof *mft_ptr);
  mft_count = 0;
  mft_data[mft_size] = '\0';

  /* read data from manifest into the memory */
  retcode = fread(mft_data, 1, mft_size, mft);
  ZLOGFAIL(retcode != mft_size, EIO, "cannot read manifest");

  /* parse manifest */
  retcode = ParseManifest();
  ZLOGFAIL(retcode, EFAULT, "invalid manifest");

  /* close file and return */
  fclose(mft);
  return 0;
}

void ManifestDtor()
{
  /* it is safe to g_free NULL, so no assertions */
  g_free(mft_data);
  g_free(mft_ptr);
  mft_data = NULL;
  mft_ptr = NULL;
  mft_count = 0;
}
