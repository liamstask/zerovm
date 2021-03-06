/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/*
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

/*
 * NaCl Simple/secure ELF loader (NaCl SEL).
 *
 * notice: "NaCl" is here by historical reasons. in fact zvm only can
 * use binaries built with ZeroVM toolchain (former NaCl toolchain)
 *
 * This loader can only process ZeroVM object files as produced using
 * the ZeroVM toolchain.  Other ELF files will be rejected.
 *
 * The primary function, NaClAppLoadFile, parses an ELF file,
 * allocates memory, loads the relocatable image from the ELF file
 * into memory, and performs relocation.  NaClAppRun runs the
 * resultant program.
 */

#ifndef SEL_LDR_H_
#define SEL_LDR_H_ 1

#include <stdint.h>

/* Keys for auxiliary vector (auxv). */
#define AT_NULL         0   /* Terminating item in auxv array */
#define AT_ENTRY        9   /* Entry point of the executable */
#define AT_SYSINFO      32  /* System call entry point */

#include "src/main/zlog.h"
#include "src/platform/gio.h"
#include "src/main/nacl_config.h"
#include "src/loader/sel_rt.h"
#include "src/main/tools.h"
#include "src/main/etag.h"

EXTERN_C_BEGIN

#define NACL_DEFAULT_STACK_MAX  (16 << 20)  /* main thread stack */

/*
 * helper macro. _element should be nap->mem_map[index], _addr - block
 * address (bytes), _size - block size (bytes), _prot - block protection
 */
#define SET_MEM_MAP_IDX(_element, _name, _addr, _size, _prot) \
  do {\
    _element.name = _name;\
    _element.start = (_addr);\
    _element.end = (_addr) + (_size);\
    _element.size = (_size);\
    _element.prot = _prot;\
  } while(0)

enum MemMapIndices {
  LeftBumperIdx, /* includes NULL protector */
  TextIdx, /* includes trampoline */
  RODataIdx,
  HeapIdx, /* includes r/w data */
  HoleIdx,
  SysDataIdx,
  StackIdx,
  RightBumperIdx,
  MemMapSize
};

struct MemBlock {
  char      *name; /* block name */
  uintptr_t start; /* block start. system virtual address */
  uintptr_t end;   /* block end (for ease). system virtual address */
  size_t    size;  /* number of bytes */
  int       prot;  /* mprotect attribute */
};

struct NaClApp {
  /*
   * public, user settable prior to app start.
   */
  uint8_t                   addr_bits;
  uintptr_t                 stack_size;
  /*
   * stack_size is the maximum size of the (main) stack.  The stack
   * memory is eager allocated (mapped in w/o MAP_NORESERVE) so
   * there must be enough swap space; page table entries are not
   * populated (no MAP_POPULATE), so actual accesses will likely
   * incur page faults.
   */

  /*
   * Determined at load time; OS-determined.
   * Read-only after load, so accesses do not require locking.
   */
  uintptr_t                 mem_start;

  uintptr_t                 dispatch_thunk;

  /* only used for ET_EXEC:  for CS restriction */
  uintptr_t                 static_text_end;  /* relative to mem_start */
  /* ro after app starts. memsz from phdr */

  /*
   * The dynamic code area follows the static code area.  These fields
   * are both set to static_text_end if the dynamic code area has zero
   * size.
   */
  uintptr_t                 dynamic_text_start;
  uintptr_t                 dynamic_text_end;

  /*
   * rodata_start and data_start may be 0 if these segments are not
   * present in the executable.
   */
  uintptr_t                 rodata_start;  /* initialized data, ro */
  uintptr_t                 data_start;    /* initialized data/bss, rw */
  /*
   * Various region sizes must be a multiple of NACL_MAP_PAGESIZE
   * before the NaCl app can run.  The sizes from the ELF file
   * (p_filesz field) might not be -- that would waste space for
   * padding -- and while we could use p_memsz to specify padding, but
   * we will record the virtual addresses of the start of the segments
   * and figure out the gap between the p_vaddr + p_filesz of one
   * segment and p_vaddr of the next to determine padding.
   */

  uintptr_t                 data_end;
  /* see break_addr below */

  /*
   * initial_entry_pt is the first address in untrusted code to jump
   * to.  When using the IRT (integrated runtime), this is provided by
   * the IRT library, and user_entry_pt is the entry point in the user
   * executable.  Otherwise, initial_entry_pt is in the user
   * executable and user_entry_pt is zero.
   */
  uintptr_t                 initial_entry_pt;
  uintptr_t                 user_entry_pt;

  /*
   * bundle_size is the bundle alignment boundary for validation (16
   * or 32), so int is okay.  This value must be a power of 2.
   * todo(d'b): can be replaced with define. only need for NaClSandboxCodeAddr
   */
  int                       bundle_size;

  /* user memory map */
  struct MemBlock           mem_map[MemMapSize];

  uintptr_t                 break_addr;   /* user addr */
  /* data_end <= break_addr is an invariant */

  struct SystemManifest     *system_manifest;
  uintptr_t                 heap_end; /* end of user heap */
  int                       validation_state; /* needs for the report */
  GString                   *channels_tag; /* all etag digests for report */
  void                      *mem_tag; /* tag context for memory */

  /* former natp field */
  uint32_t                  sysret; /* syscall return code */
};

/*
 * Initializes a NaCl application with the default parameters.
 * nap is a pointer to the NaCl object that is being filled in.
 * assumes all fields already set to zeroes
 */
void NaClAppCtor(struct NaClApp *nap);

/* free app memory and globals */
void NaClAppDtor(struct NaClApp *nap);

/*
 * Loads a NaCl ELF file into memory in preparation for running it.
 *
 * gp is a pointer to a generic I/O object and should be a GioMem with
 * a memory buffer containing the file read entirely into memory if
 * the file system might be subject to race conditions (e.g., another
 * thread / process might modify a downloaded NaCl ELF file while we
 * are loading it here).
 *
 * nap is a pointer to the NaCl object that is being filled in.  it
 * should be properly constructed via NaClAppCtor.
 *
 * note: it may be necessary to flush the icache if the memory
 * allocated for use had already made it into the icache from another
 * NaCl application instance, and the icache does not detect
 * self-modifying code / data writes and automatically invalidate the
 * cache lines.
 */
void NaClAppLoadFile(struct Gio *gp, struct NaClApp *nap);

/* todo(d'b): replace spaces with format options and use macro */
void  NaClAppPrintDetails(struct NaClApp  *nap,
                          struct Gio      *gp, int verbosity);

/*
 * set an empty user stack and other context, then pass
 * control to user code
 */
void CreateSession(struct NaClApp *nap);

/*
 * Install syscall trampolines at all possible well-formed entry points
 * within the trampoline pages.  Many of these syscalls will correspond
 * to unimplemented system calls and will just abort the program.
 */
void NaClLoadTrampoline(struct NaClApp *nap);

static const uintptr_t kNaClBadAddress = (uintptr_t) -1;

#include "src/loader/sel_ldr-inl.h"

void NaClFillMemoryRegionWithHalt(void *start, size_t size);

void NaClFillTrampolineRegion(struct NaClApp *nap);

void NaClMakeDispatchThunk(struct NaClApp *nap);

void NaClFreeDispatchThunk(struct NaClApp *nap);

/* Install a syscall trampoline at target_addr.  NB: Thread-safe. */
void NaClPatchOneTrampoline(struct NaClApp *nap,
                            uintptr_t target_addr);
/*
 * target is an absolute address in the source region.  the patch code
 * will figure out the corresponding address in the destination region
 * and modify as appropriate.  this makes it easier to specify, since
 * the target is typically the address of some symbol from the source
 * template.
 */
struct NaClPatch {
  uintptr_t           target;
  uint64_t            value;
};

struct NaClPatchInfo {
  uintptr_t           dst;
  uintptr_t           src;
  size_t              nbytes;

  struct NaClPatch    *abs16;
  size_t              num_abs16;

  struct NaClPatch    *abs32;
  size_t              num_abs32;

  struct NaClPatch    *abs64;
  size_t              num_abs64;
};

struct NaClPatchInfo *NaClPatchInfoCtor(struct NaClPatchInfo *self);

/*
 * This function is called by NaClLoadTrampoline and NaClLoadSpringboard to
 * patch a single memory location specified in NaClPatchInfo structure.
 */
void NaClApplyPatchToMemory(struct NaClPatchInfo *patch);

int NaClThreadContextCtor(struct NaClThreadContext  *ntcp,
                          struct NaClApp            *nap,
                          nacl_reg_t                prog_ctr,
                          nacl_reg_t                stack_ptr,
                          uint32_t                  tls_info);

EXTERN_C_END

#endif  /* SEL_LDR_H_ */
