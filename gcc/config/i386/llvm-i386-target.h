/* LLVM LOCAL begin (ENTIRE FILE!)  */
/* Some target-specific hooks for gcc->llvm conversion
Copyright (C) 2007 Free Software Foundation, Inc.
Contributed by Anton Korobeynikov (asl@math.spbu.ru)

This file is part of GCC.

GCC is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free
Software Foundation; either version 2, or (at your option) any later
version.

GCC is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License
along with GCC; see the file COPYING.  If not, write to the Free
Software Foundation, 59 Temple Place - Suite 330, Boston, MA
02111-1307, USA.  */

/* LLVM specific stuff for supporting calling convention output */
#define TARGET_ADJUST_LLVM_CC(CC, type)                         \
  {                                                             \
    tree type_attributes = TYPE_ATTRIBUTES (type);              \
    if (lookup_attribute ("stdcall", type_attributes)) {        \
      CC = CallingConv::X86_StdCall;                            \
    } else if (lookup_attribute("fastcall", type_attributes)) { \
      CC = CallingConv::X86_FastCall;                           \
    } else if (!TARGET_64BIT &&                                 \
               lookup_attribute("sseregparm", type_attributes)){\
      CC = CallingConv::X86_SSECall;                            \
    }                                                           \
  }

/* LLVM specific stuff for converting gcc's `regparm` attribute to LLVM's
   `inreg` parameter attribute */
#define LLVM_TARGET_ENABLE_REGPARM

extern int ix86_regparm;

#define LLVM_TARGET_INIT_REGPARM(local_regparm, local_fp_regparm, type) \
  {                                                             \
    tree attr;                                                  \
    local_regparm = ix86_regparm;                               \
    local_fp_regparm = TARGET_SSEREGPARM ? 3 : 0;               \
    attr = lookup_attribute ("regparm",                         \
                              TYPE_ATTRIBUTES (type));          \
    if (attr) {                                                 \
      local_regparm = TREE_INT_CST_LOW (TREE_VALUE              \
                                        (TREE_VALUE (attr)));   \
    }                                                           \
    attr = lookup_attribute("sseregparm",                       \
                              TYPE_ATTRIBUTES (type));          \
    if (attr)                                                   \
      local_fp_regparm = 3;                                     \
  }

#define LLVM_ADJUST_REGPARM_ATTRIBUTE(Attribute, Type, Size,    \
                                      local_regparm,            \
                                      local_fp_regparm)         \
  {                                                             \
    if (!TARGET_64BIT) {                                        \
      if (TREE_CODE(Type) == REAL_TYPE &&                       \
          (TYPE_PRECISION(Type)==32 ||                          \
           TYPE_PRECISION(Type)==64)) {                         \
          local_fp_regparm -= 1;                                \
          if (local_fp_regparm >= 0)                            \
            Attribute |= ParamAttr::InReg;                      \
          else                                                  \
            local_fp_regparm = 0;                               \
      } else if (TREE_CODE(Type) == INTEGER_TYPE ||             \
                 TREE_CODE(Type) == POINTER_TYPE) {             \
          int words =                                           \
                  (Size + BITS_PER_WORD - 1) / BITS_PER_WORD;   \
          local_regparm -= words;                               \
          if (local_regparm>=0)                                 \
            Attribute |= ParamAttr::InReg;                      \
          else                                                  \
            local_regparm = 0;                                  \
      }                                                         \
    }                                                           \
  }

#ifdef LLVM_ABI_H

/* Objects containing SSE vectors are 16 byte aligned, everything else 4. */
extern "C" bool contains_128bit_aligned_vector_p(tree);
#define LLVM_BYVAL_ALIGNMENT(T) \
  (TARGET_64BIT ? 0 : \
   TARGET_SSE && contains_128bit_aligned_vector_p(T) ? 16 : 4)

/* Structs containing a single data field plus zero-length fields are
   considered as if they were the type of the data field. */
#ifndef LLVM_SHOULD_RETURN_SELT_STRUCT_AS_SCALAR
#define LLVM_SHOULD_RETURN_SELT_STRUCT_AS_SCALAR(X) \
  isSingleElementStructOrArray(X, true, false)
#endif

extern bool llvm_x86_should_pass_vector_in_integer_regs(tree);

/* Vectors which are not MMX nor SSE should be passed as integers. */
#define LLVM_SHOULD_PASS_VECTOR_IN_INTEGER_REGS(X)      \
  llvm_x86_should_pass_vector_in_integer_regs((X))

extern tree llvm_x86_should_return_vector_as_scalar(tree, bool);

/* The MMX vector v1i64 is returned in EAX and EDX on Darwin.  Communicate
    this by returning i64 here.  Likewise, (generic) vectors such as v2i16
    are returned in EAX.  */
#define LLVM_SHOULD_RETURN_VECTOR_AS_SCALAR(X,isBuiltin)\
  llvm_x86_should_return_vector_as_scalar((X), (isBuiltin))

extern bool llvm_x86_should_return_vector_as_shadow(tree, bool);

/* MMX vectors v2i32, v4i16, v8i8, v2f32 are returned using sret on Darwin
   32-bit.  Vectors bigger than 128 are returned using sret.  */
#define LLVM_SHOULD_RETURN_VECTOR_AS_SHADOW(X,isBuiltin)\
  llvm_x86_should_return_vector_as_shadow((X),(isBuiltin))

extern bool llvm_x86_should_pass_aggregate_in_memory(tree, const Type *);

#define LLVM_SHOULD_PASS_AGGREGATE_USING_BYVAL_ATTR(X, TY)      \
  llvm_x86_should_pass_aggregate_in_memory(X, TY)


extern bool
llvm_x86_64_should_pass_aggregate_in_mixed_regs(tree, const Type *Ty,
                                                std::vector<const Type*>&);
extern bool
llvm_x86_32_should_pass_aggregate_in_mixed_regs(tree, const Type *Ty,
                                                std::vector<const Type*>&);

#define LLVM_SHOULD_PASS_AGGREGATE_IN_MIXED_REGS(T, TY, E)            \
  (TARGET_64BIT ?                                                     \
   llvm_x86_64_should_pass_aggregate_in_mixed_regs((T), (TY), (E)) :  \
   llvm_x86_32_should_pass_aggregate_in_mixed_regs((T), (TY), (E)))
#endif /* LLVM_ABI_H */

/* LLVM LOCAL end (ENTIRE FILE!)  */

