/*
 * Copyright (c) 2021 Raymond Cardillo
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

#ifndef CBFUSE_UTIL_HEADER_SEEN
#define CBFUSE_UTIL_HEADER_SEEN

#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

// If the LCB result code (rc) indicates a failure then
// set the function result (fr) and jump to the Error block.
// Goto statements can be very bad when used incorrectly.
// This should only be used to jump forward to the end of a block for clean up.
// In this case, a goto macro helps to avoid a lot of boilerplate code or
// crazy nested indentations when multiple conditions can fail.
#define IfLCBFailGotoDone(rc, fr) \
if ((rc) != LCB_SUCCESS) { \
  fprintf(stderr, "  %s:%s:%d LCB_FAIL %s\n", __FILENAME__, __func__, __LINE__, lcb_strerror_short(rc)); \
  fresult = fr; \
  goto done; \
}

#define IfLCBFailGotoDoneWithRef(rc, fr, ref) \
if ((rc) != LCB_SUCCESS) { \
  fprintf(stderr, "  %s:%s:%d LCB_FAIL %s %s\n", __FILENAME__, __func__, __LINE__, ref, lcb_strerror_short(rc)); \
  fresult = fr; \
  goto done; \
}

#define IfNULLGotoDoneWithRef(val, fr, ref) \
if ((val) == NULL) { \
  fprintf(stderr, "  %s:%s:%d TEST_NULL %s\n", __FILENAME__, __func__, __LINE__, ref); \
  fresult = fr; \
  goto done; \
}

#define IfTrueGotoDoneWithRef(val, fr, ref) \
if (val) { \
  fprintf(stderr, "  %s:%s:%d TEST_BOOL %s\n", __FILENAME__, __func__, __LINE__, ref); \
  fresult = fr; \
  goto done; \
}

#define IfFalseGotoDoneWithRef(val, fr, ref) IfTrueGotoDoneWithRef((!(val)), fr, ref)

#define IfFRFailGotoDoneWithRef(ref) \
if (fresult != 0) { \
  fprintf(stderr, "  %s:%s:%d FR_FAIL (%d)(%s) %s\n", __FILENAME__, __func__, __LINE__, fresult, strerror(-fresult), ref); \
  goto done; \
}

static inline void *memdupm(const void *src, size_t n, size_t m)
{
    void *dest = malloc(m);
    if (dest == NULL) {
        return NULL;
    }
    return memcpy(dest, src, n);
}

static inline void *memdup(const void *src, size_t n)
{
  return memdupm(src, n, n);
}

#endif /* !CBFUSE_UTIL_HEADER_SEEN */
