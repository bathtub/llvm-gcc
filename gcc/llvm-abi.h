/* LLVM LOCAL begin (ENTIRE FILE!)  */
/* Processor ABI customization hooks
Copyright (C) 2005, 2006, 2007 Free Software Foundation, Inc.
Contributed by Chris Lattner (sabre@nondot.org)

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

//===----------------------------------------------------------------------===//
// This is a C++ header file that specifies how argument values are passed and
// returned from function calls.  This allows the target to specialize handling
// of things like how structures are passed by-value.
//===----------------------------------------------------------------------===//

#ifndef LLVM_ABI_H
#define LLVM_ABI_H

#include "llvm-internal.h"

#include "llvm/Attributes.h"
#include "llvm/CallingConv.h"
#include "llvm/Constants.h"
#include "llvm/DerivedTypes.h"
#include "llvm/LLVMContext.h"
#include "llvm/Target/TargetData.h"
#include "llvm/Support/Compiler.h"

namespace llvm {
  class BasicBlock;
}

#undef VISIBILITY_HIDDEN

extern "C" {
#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "tm.h"
#include "tree.h"
}  

/// DefaultABIClient - This is a simple implementation of the ABI client
/// interface that can be subclassed.
struct DefaultABIClient {
  bool isShadowReturn() const { return false; }

  /// HandleScalarResult - This callback is invoked if the function returns a
  /// simple scalar result value, which is of type RetTy.
  void HandleScalarResult(const Type *RetTy) {}

  /// HandleAggregateResultAsScalar - This callback is invoked if the function
  /// returns an aggregate value by bit converting it to the specified scalar
  /// type and returning that.  The bit conversion should start at byte Offset
  /// within the struct, and ScalarTy is not necessarily big enough to cover
  /// the entire struct.
  void HandleAggregateResultAsScalar(const Type *ScalarTy, unsigned Offset=0) {}

  /// HandleAggregateResultAsAggregate - This callback is invoked if the function
  /// returns an aggregate value using multiple return values.
  void HandleAggregateResultAsAggregate(const Type *AggrTy) {}

  /// HandleAggregateShadowResult - This callback is invoked if the function
  /// returns an aggregate value by using a "shadow" first parameter, which is
  /// a pointer to the aggregate, of type PtrArgTy.  If RetPtr is set to true,
  /// the pointer argument itself is returned from the function.
  void HandleAggregateShadowResult(const PointerType *PtrArgTy, bool RetPtr){}

  /// HandleScalarShadowResult - This callback is invoked if the function
  /// returns a scalar value by using a "shadow" first parameter, which is a
  /// pointer to the scalar, of type PtrArgTy.  If RetPtr is set to true,
  /// the pointer argument itself is returned from the function.
  void HandleScalarShadowResult(const PointerType *PtrArgTy, bool RetPtr) {}


  /// HandleScalarArgument - This is the primary callback that specifies an
  /// LLVM argument to pass.  It is only used for first class types.
  /// If RealSize is non Zero then it specifies number of bytes to access
  /// from LLVMTy. 
  void HandleScalarArgument(const llvm::Type *LLVMTy, tree type,
                            unsigned RealSize = 0) {}

  /// HandleByInvisibleReferenceArgument - This callback is invoked if a pointer
  /// (of type PtrTy) to the argument is passed rather than the argument itself.
  void HandleByInvisibleReferenceArgument(const llvm::Type *PtrTy, tree type) {}

  /// HandleByValArgument - This callback is invoked if the aggregate function
  /// argument is passed by value.
  void HandleByValArgument(const llvm::Type *LLVMTy, tree type) {}

  /// HandleFCAArgument - This callback is invoked if the aggregate function
  /// argument is passed by value as a first class aggregate.
  void HandleFCAArgument(const llvm::Type *LLVMTy,
                         tree type ATTRIBUTE_UNUSED) {}

  /// EnterField - Called when we're about the enter the field of a struct
  /// or union.  FieldNo is the number of the element we are entering in the
  /// LLVM Struct, StructTy is the LLVM type of the struct we are entering.
  void EnterField(unsigned FieldNo, const llvm::Type *StructTy) {}
  void ExitField() {}
};

/// isAggregateTreeType - Return true if the specified GCC type is an aggregate
/// that cannot live in an LLVM register.
static inline bool isAggregateTreeType(tree type) {
  return TREE_CODE(type) == RECORD_TYPE || TREE_CODE(type) == ARRAY_TYPE ||
         TREE_CODE(type) == UNION_TYPE  || TREE_CODE(type) == QUAL_UNION_TYPE ||
         TREE_CODE(type) == COMPLEX_TYPE;
}

// LLVM_SHOULD_NOT_RETURN_COMPLEX_IN_MEMORY - A hook to allow
// special _Complex handling. Return true if X should be returned using
// multiple value return instruction.
#ifndef LLVM_SHOULD_NOT_RETURN_COMPLEX_IN_MEMORY
#define LLVM_SHOULD_NOT_RETURN_COMPLEX_IN_MEMORY(X) \
 false
#endif

// LLVM_SHOULD_NOT_USE_SHADOW_RETURN - A hook to allow aggregates to be
// returned in registers.
#ifndef LLVM_SHOULD_NOT_USE_SHADOW_RETURN
#define LLVM_SHOULD_NOT_USE_SHADOW_RETURN(X, CC) \
 false
#endif

// doNotUseShadowReturn - Return true if the specified GCC type 
// should not be returned using a pointer to struct parameter. 
static inline bool doNotUseShadowReturn(tree type, tree fndecl,
                                        CallingConv::ID CC) {
  if (!TYPE_SIZE(type))
    return false;
  if (TREE_CODE(TYPE_SIZE(type)) != INTEGER_CST)
    return false;
  // LLVM says do not use shadow argument.
  if (LLVM_SHOULD_NOT_RETURN_COMPLEX_IN_MEMORY(type) ||
     LLVM_SHOULD_NOT_USE_SHADOW_RETURN(type, CC))
    return true;
  // GCC says use shadow argument.
  if (aggregate_value_p(type, fndecl))
    return false;
  return true;
}

/// isSingleElementStructOrArray - If this is (recursively) a structure with one
/// field or an array with one element, return the field type, otherwise return
/// null.  If ignoreZeroLength, the struct (recursively) may include zero-length
/// fields in addition to the single element that has data.  If 
/// rejectFatBitField, and the single element is a bitfield of a type that's
/// bigger than the struct, return null anyway.
static inline
tree isSingleElementStructOrArray(tree type, bool ignoreZeroLength,
                                  bool rejectFatBitfield) {
  // Scalars are good.
  if (!isAggregateTreeType(type)) return type;
  
  tree FoundField = 0;
  switch (TREE_CODE(type)) {
  case QUAL_UNION_TYPE:
  case UNION_TYPE:     // Single element unions don't count.
  case COMPLEX_TYPE:   // Complex values are like 2-element records.
  default:
    return 0;
  case RECORD_TYPE:
    // If this record has variable length, reject it.
    if (TREE_CODE(TYPE_SIZE(type)) != INTEGER_CST)
      return 0;

    for (tree Field = TYPE_FIELDS(type); Field; Field = TREE_CHAIN(Field))
      if (TREE_CODE(Field) == FIELD_DECL) {
        if (ignoreZeroLength) {
          if (DECL_SIZE(Field) && 
              TREE_CODE(DECL_SIZE(Field)) == INTEGER_CST &&
              TREE_INT_CST_LOW(DECL_SIZE(Field)) == 0)
            continue;
        }
        if (!FoundField) {
          if (rejectFatBitfield &&
              TREE_CODE(TYPE_SIZE(type)) == INTEGER_CST &&
              TREE_INT_CST_LOW(TYPE_SIZE(getDeclaredType(Field))) > 
              TREE_INT_CST_LOW(TYPE_SIZE(type)))
            return 0;
          FoundField = getDeclaredType(Field);
        } else {
          return 0;   // More than one field.
        }
      }
    return FoundField ? isSingleElementStructOrArray(FoundField, 
                                                     ignoreZeroLength, false)
                      : 0;
  case ARRAY_TYPE:
    const ArrayType *Ty = dyn_cast<ArrayType>(ConvertType(type));
    if (!Ty || Ty->getNumElements() != 1)
      return 0;
    return isSingleElementStructOrArray(TREE_TYPE(type), false, false);
  }
}

/// isZeroSizedStructOrUnion - Returns true if this is a struct or union 
/// which is zero bits wide.
static inline bool isZeroSizedStructOrUnion(tree type) {
  if (TREE_CODE(type) != RECORD_TYPE &&
      TREE_CODE(type) != UNION_TYPE &&
      TREE_CODE(type) != QUAL_UNION_TYPE)
    return false;
  return int_size_in_bytes(type) == 0;
}

// getLLVMScalarTypeForStructReturn - Return LLVM Type if TY can be 
// returned as a scalar, otherwise return NULL. This is the default
// target independent implementation.
static inline
const Type* getLLVMScalarTypeForStructReturn(tree type, unsigned *Offset) {
  const Type *Ty = ConvertType(type);
  unsigned Size = getTargetData().getTypeAllocSize(Ty);
  *Offset = 0;
  if (Size == 0)
    return Type::getVoidTy(getGlobalContext());
  else if (Size == 1)
    return Type::getInt8Ty(getGlobalContext());
  else if (Size == 2)
    return Type::getInt16Ty(getGlobalContext());
  else if (Size <= 4)
    return Type::getInt32Ty(getGlobalContext());
  else if (Size <= 8)
    return Type::getInt64Ty(getGlobalContext());
  else if (Size <= 16)
    return IntegerType::get(getGlobalContext(), 128);
  else if (Size <= 32)
    return IntegerType::get(getGlobalContext(), 256);

  return NULL;
}

// getLLVMAggregateTypeForStructReturn - Return LLVM type if TY can be
// returns as multiple values, otherwise return NULL. This is the default
// target independent implementation.
static inline const Type* getLLVMAggregateTypeForStructReturn(tree type) {
  return NULL;
}

// LLVM_SHOULD_PASS_VECTOR_IN_INTEGER_REGS - Return true if this vector
// type should be passed as integer registers.  Generally vectors which are
// not part of the target architecture should do this.
#ifndef LLVM_SHOULD_PASS_VECTOR_IN_INTEGER_REGS
#define LLVM_SHOULD_PASS_VECTOR_IN_INTEGER_REGS(TY) \
  false
#endif

// LLVM_SHOULD_PASS_VECTOR_USING_BYVAL_ATTR - Return true if this vector
// type should be passed byval.  Used for generic vectors on x86-64.
#ifndef LLVM_SHOULD_PASS_VECTOR_USING_BYVAL_ATTR
#define LLVM_SHOULD_PASS_VECTOR_USING_BYVAL_ATTR(X) \
  false
#endif

// LLVM_SHOULD_PASS_AGGREGATE_USING_BYVAL_ATTR - Return true if this aggregate
// value should be passed by value, i.e. passing its address with the byval
// attribute bit set. The default is false.
#ifndef LLVM_SHOULD_PASS_AGGREGATE_USING_BYVAL_ATTR
#define LLVM_SHOULD_PASS_AGGREGATE_USING_BYVAL_ATTR(X, TY) \
    false
#endif

// LLVM_SHOULD_PASS_AGGREGATE_AS_FCA - Return true if this aggregate value
// should be passed by value as a first class aggregate. The default is false.
#ifndef LLVM_SHOULD_PASS_AGGREGATE_AS_FCA
#define LLVM_SHOULD_PASS_AGGREGATE_AS_FCA(X, TY) \
    false
#endif

// LLVM_SHOULD_PASS_AGGREGATE_IN_MIXED_REGS - Return true if this aggregate
// value should be passed in a mixture of integer, floating point, and vector
// registers. The routine should also return by reference a vector of the
// types of the registers being used. The default is false.
#ifndef LLVM_SHOULD_PASS_AGGREGATE_IN_MIXED_REGS
#define LLVM_SHOULD_PASS_AGGREGATE_IN_MIXED_REGS(T, TY, CC, E) \
    false
#endif

// LLVM_AGGREGATE_PARTIALLY_PASSED_IN_REGS - Only called if
// LLVM_SHOULD_PASS_AGGREGATE_IN_MIXED_REGS returns true. This returns true if
// there are only enough unused argument passing registers to pass a part of
// the aggregate. Note, this routine should return false if none of the needed
// registers are available.
#ifndef LLVM_AGGREGATE_PARTIALLY_PASSED_IN_REGS
#define LLVM_AGGREGATE_PARTIALLY_PASSED_IN_REGS(E, SE, ISR, CC) \
    false
#endif

// LLVM_BYVAL_ALIGNMENT - Returns the alignment of the type in bytes, if known,
// in the getGlobalContext() of its use as a function parameter.
// Note that the alignment in the TYPE node is usually the alignment appropriate
// when the type is used within a struct, which may or may not be appropriate
// here.
#ifndef LLVM_BYVAL_ALIGNMENT
#define LLVM_BYVAL_ALIGNMENT(T)  0
#endif

// LLVM_SHOULD_PASS_AGGREGATE_IN_INTEGER_REGS - Return true if this aggregate
// value should be passed in integer registers.  By default, we do this for all
// values that are not single-element structs.  This ensures that things like
// {short,short} are passed in one 32-bit chunk, not as two arguments (which
// would often be 64-bits).  We also do it for single-element structs when the
// single element is a bitfield of a type bigger than the struct; the code
// for field-by-field struct passing does not handle this one right.
#ifndef LLVM_SHOULD_PASS_AGGREGATE_IN_INTEGER_REGS
#define LLVM_SHOULD_PASS_AGGREGATE_IN_INTEGER_REGS(X, Y, Z) \
   !isSingleElementStructOrArray((X), false, true)
#endif

// LLVM_SHOULD_RETURN_SELT_STRUCT_AS_SCALAR - Return a TYPE tree if this single
// element struct should be returned using the convention for that scalar TYPE, 
// 0 otherwise.
// The returned TYPE must be the same size as X for this to work; that is
// checked elsewhere.  (Structs where this is not the case can be constructed
// by abusing the __aligned__ attribute.)
#ifndef LLVM_SHOULD_RETURN_SELT_STRUCT_AS_SCALAR
#define LLVM_SHOULD_RETURN_SELT_STRUCT_AS_SCALAR(X) \
  isSingleElementStructOrArray(X, false, false)
#endif

// LLVM_SHOULD_RETURN_VECTOR_AS_SCALAR - Return a TYPE tree if this vector type
// should be returned using the convention for that scalar TYPE, 0 otherwise.
// X may be evaluated more than once.
#ifndef LLVM_SHOULD_RETURN_VECTOR_AS_SCALAR
#define LLVM_SHOULD_RETURN_VECTOR_AS_SCALAR(X,Y) 0
#endif

// LLVM_SHOULD_RETURN_VECTOR_AS_SHADOW - Return true if this vector type
// should be returned using the aggregate shadow (sret) convention, 0 otherwise.
// X may be evaluated more than once.
#ifndef LLVM_SHOULD_RETURN_VECTOR_AS_SHADOW
#define LLVM_SHOULD_RETURN_VECTOR_AS_SHADOW(X,Y) 0
#endif

// LLVM_SCALAR_TYPE_FOR_STRUCT_RETURN - Return LLVM Type if X can be 
// returned as a scalar, otherwise return NULL.
#ifndef LLVM_SCALAR_TYPE_FOR_STRUCT_RETURN
#define LLVM_SCALAR_TYPE_FOR_STRUCT_RETURN(X, Y) \
  getLLVMScalarTypeForStructReturn((X), (Y))
#endif

// LLVM_AGGR_TYPE_FOR_STRUCT_RETURN - Return LLVM Type if X can be 
// returned as an aggregate, otherwise return NULL.
#ifndef LLVM_AGGR_TYPE_FOR_STRUCT_RETURN
#define LLVM_AGGR_TYPE_FOR_STRUCT_RETURN(X, CC)    \
  getLLVMAggregateTypeForStructReturn(X)
#endif

// LLVM_EXTRACT_MULTIPLE_RETURN_VALUE - Extract multiple return value from
// SRC and assign it to DEST. Each target that supports multiple return
// value must implement this hook.
#ifndef LLVM_EXTRACT_MULTIPLE_RETURN_VALUE
#define LLVM_EXTRACT_MULTIPLE_RETURN_VALUE(Src,Dest,V,B)     \
  llvm_default_extract_multiple_return_value((Src),(Dest),(V),(B))
#endif
static inline
void llvm_default_extract_multiple_return_value(Value *Src, Value *Dest,
                                                bool isVolatile,
                                                LLVMBuilder &Builder) {
  assert (0 && "LLVM_EXTRACT_MULTIPLE_RETURN_VALUE is not implemented!");
}

/// DefaultABI - This class implements the default LLVM ABI where structures are
/// passed by decimating them into individual components and unions are passed
/// by passing the largest member of the union.
///
template<typename Client>
class DefaultABI {
protected:
  Client &C;
public:
  DefaultABI(Client &c) : C(c) {}

  bool isShadowReturn() const { return C.isShadowReturn(); }
  
  /// HandleReturnType - This is invoked by the target-independent code for the
  /// return type. It potentially breaks down the argument and invokes methods
  /// on the client that indicate how its pieces should be handled.  This
  /// handles things like returning structures via hidden parameters.
  void HandleReturnType(tree type, tree fn, bool isBuiltin) {
    unsigned Offset = 0;
    const Type *Ty = ConvertType(type);
    if (isa<VectorType>(Ty)) {
      // Vector handling is weird on x86.  In particular builtin and
      // non-builtin function of the same return types can use different
      // calling conventions.
      tree ScalarType = LLVM_SHOULD_RETURN_VECTOR_AS_SCALAR(type, isBuiltin);
      if (ScalarType)
        C.HandleAggregateResultAsScalar(ConvertType(ScalarType));
      else if (LLVM_SHOULD_RETURN_VECTOR_AS_SHADOW(type, isBuiltin))
        C.HandleScalarShadowResult(Ty->getPointerTo(), false);
      else
        C.HandleScalarResult(Ty);
    } else if (Ty->isSingleValueType() || Ty->isVoidTy()) {
      // Return scalar values normally.
      C.HandleScalarResult(Ty);
    } else if (doNotUseShadowReturn(type, fn, C.getCallingConv())) {
      tree SingleElt = LLVM_SHOULD_RETURN_SELT_STRUCT_AS_SCALAR(type);
      if (SingleElt && TYPE_SIZE(SingleElt) && 
          TREE_CODE(TYPE_SIZE(SingleElt)) == INTEGER_CST &&
          TREE_INT_CST_LOW(TYPE_SIZE_UNIT(type)) == 
            TREE_INT_CST_LOW(TYPE_SIZE_UNIT(SingleElt))) {
        C.HandleAggregateResultAsScalar(ConvertType(SingleElt));
      } else {
        // Otherwise return as an integer value large enough to hold the entire
        // aggregate.
        if (const Type *AggrTy = LLVM_AGGR_TYPE_FOR_STRUCT_RETURN(type,
                                    C.getCallingConv()))
          C.HandleAggregateResultAsAggregate(AggrTy);
        else if (const Type* ScalarTy = 
                    LLVM_SCALAR_TYPE_FOR_STRUCT_RETURN(type, &Offset))
          C.HandleAggregateResultAsScalar(ScalarTy, Offset);
        else {
          assert(0 && "Unable to determine how to return this aggregate!");
          abort();
        }
      }
    } else {
      // If the function is returning a struct or union, we pass the pointer to
      // the struct as the first argument to the function.

      // FIXME: should return the hidden first argument for some targets
      // (e.g. ELF i386).
      C.HandleAggregateShadowResult(Ty->getPointerTo(), false);
    }
  }
  
  /// HandleArgument - This is invoked by the target-independent code for each
  /// argument type passed into the function.  It potentially breaks down the
  /// argument and invokes methods on the client that indicate how its pieces
  /// should be handled.  This handles things like decimating structures into
  /// their fields.
  void HandleArgument(tree type, std::vector<const Type*> &ScalarElts,
                      Attributes *Attributes = NULL) {
    unsigned Size = 0;
    bool DontCheckAlignment = false;
    const Type *Ty = ConvertType(type);
    // Figure out if this field is zero bits wide, e.g. {} or [0 x int].  Do
    // not include variable sized fields here.
    std::vector<const Type*> Elts;
    if (Ty->isVoidTy()) {
      // Handle void explicitly as an opaque type.
      const Type *OpTy = OpaqueType::get(getGlobalContext());
      C.HandleScalarArgument(OpTy, type);
      ScalarElts.push_back(OpTy);
    } else if (isPassedByInvisibleReference(type)) { // variable size -> by-ref.
      const Type *PtrTy = Ty->getPointerTo();
      C.HandleByInvisibleReferenceArgument(PtrTy, type);
      ScalarElts.push_back(PtrTy);
    } else if (isa<VectorType>(Ty)) {
      if (LLVM_SHOULD_PASS_VECTOR_IN_INTEGER_REGS(type)) {
        PassInIntegerRegisters(type, ScalarElts, 0, false);
      } else if (LLVM_SHOULD_PASS_VECTOR_USING_BYVAL_ATTR(type)) {
        C.HandleByValArgument(Ty, type);
        if (Attributes) {
          *Attributes |= Attribute::ByVal;
          *Attributes |= 
            Attribute::constructAlignmentFromInt(LLVM_BYVAL_ALIGNMENT(type));
        }
      } else {
        C.HandleScalarArgument(Ty, type);
        ScalarElts.push_back(Ty);
      }
    } else if (Ty->isSingleValueType()) {
      C.HandleScalarArgument(Ty, type);
      ScalarElts.push_back(Ty);
    } else if (LLVM_SHOULD_PASS_AGGREGATE_AS_FCA(type, Ty)) {
      C.HandleFCAArgument(Ty, type);
    } else if (LLVM_SHOULD_PASS_AGGREGATE_IN_MIXED_REGS(type, Ty,
                                                        C.getCallingConv(),
                                                        Elts)) {
      if (!LLVM_AGGREGATE_PARTIALLY_PASSED_IN_REGS(Elts, ScalarElts,
                                                   C.isShadowReturn(),
                                                   C.getCallingConv()))
        PassInMixedRegisters(Ty, Elts, ScalarElts);
      else {
        C.HandleByValArgument(Ty, type);
        if (Attributes) {
          *Attributes |= Attribute::ByVal;
          *Attributes |= 
            Attribute::constructAlignmentFromInt(LLVM_BYVAL_ALIGNMENT(type));
        }
      }
    } else if (LLVM_SHOULD_PASS_AGGREGATE_USING_BYVAL_ATTR(type, Ty)) {
      C.HandleByValArgument(Ty, type);
      if (Attributes) {
        *Attributes |= Attribute::ByVal;
        *Attributes |= 
          Attribute::constructAlignmentFromInt(LLVM_BYVAL_ALIGNMENT(type));
      }
    } else if (LLVM_SHOULD_PASS_AGGREGATE_IN_INTEGER_REGS(type, &Size,
                                                     &DontCheckAlignment)) {
      PassInIntegerRegisters(type, ScalarElts, Size, DontCheckAlignment);
    } else if (isZeroSizedStructOrUnion(type)) {
      // Zero sized struct or union, just drop it!
      ;
    } else if (TREE_CODE(type) == RECORD_TYPE) {
      for (tree Field = TYPE_FIELDS(type); Field; Field = TREE_CHAIN(Field))
        if (TREE_CODE(Field) == FIELD_DECL) {
          const tree Ftype = getDeclaredType(Field);
          const Type *FTy = ConvertType(Ftype);
          unsigned FNo = GET_LLVM_FIELD_INDEX(Field);
          assert(FNo != ~0U && "Case not handled yet!");

          // Currently, a bvyal type inside a non-byval struct is a zero-length
          // object inside a bigger object on x86-64.  This type should be
          // skipped (but only when it is inside a bigger object).
          // (We know there currently are no other such cases active because
          // they would hit the assert in FunctionPrologArgumentConversion::
          // HandleByValArgument.)
          if (!LLVM_SHOULD_PASS_AGGREGATE_USING_BYVAL_ATTR(Ftype, FTy)) {
            C.EnterField(FNo, Ty);
            HandleArgument(getDeclaredType(Field), ScalarElts);
            C.ExitField();
          }
        }
    } else if (TREE_CODE(type) == COMPLEX_TYPE) {
      C.EnterField(0, Ty);
      HandleArgument(TREE_TYPE(type), ScalarElts);
      C.ExitField();
      C.EnterField(1, Ty);
      HandleArgument(TREE_TYPE(type), ScalarElts);
      C.ExitField();
    } else if ((TREE_CODE(type) == UNION_TYPE) ||
               (TREE_CODE(type) == QUAL_UNION_TYPE)) {
      HandleUnion(type, ScalarElts);
    } else if (TREE_CODE(type) == ARRAY_TYPE) {
      const ArrayType *ATy = cast<ArrayType>(Ty);
      for (unsigned i = 0, e = ATy->getNumElements(); i != e; ++i) {
        C.EnterField(i, Ty);
        HandleArgument(TREE_TYPE(type), ScalarElts);
        C.ExitField();
      }
    } else {
      assert(0 && "unknown aggregate type!");
      abort();
    }
  }

  /// HandleUnion - Handle a UNION_TYPE or QUAL_UNION_TYPE tree.
  ///
  void HandleUnion(tree type, std::vector<const Type*> &ScalarElts) {
    if (TYPE_TRANSPARENT_UNION(type)) {
      tree Field = TYPE_FIELDS(type);
      assert(Field && "Transparent union must have some elements!");
      while (TREE_CODE(Field) != FIELD_DECL) {
        Field = TREE_CHAIN(Field);
        assert(Field && "Transparent union must have some elements!");
      }
      
      HandleArgument(TREE_TYPE(Field), ScalarElts);
    } else {
      // Unions pass the largest element.
      unsigned MaxSize = 0;
      tree MaxElt = 0;
      for (tree Field = TYPE_FIELDS(type); Field; Field = TREE_CHAIN(Field)) {
        if (TREE_CODE(Field) == FIELD_DECL) {
          // Skip fields that are known not to be present.
          if (TREE_CODE(type) == QUAL_UNION_TYPE &&
              integer_zerop(DECL_QUALIFIER(Field)))
              continue;

          tree SizeTree = TYPE_SIZE(TREE_TYPE(Field));
          unsigned Size = ((unsigned)TREE_INT_CST_LOW(SizeTree)+7)/8;
          if (Size > MaxSize) {
            MaxSize = Size;
            MaxElt = Field;
          }

          // Skip remaining fields if this one is known to be present.
          if (TREE_CODE(type) == QUAL_UNION_TYPE &&
              integer_onep(DECL_QUALIFIER(Field)))
              break;
        }
      }
      
      if (MaxElt)
        HandleArgument(TREE_TYPE(MaxElt), ScalarElts);
    }
  }
    
  /// PassInIntegerRegisters - Given an aggregate value that should be passed in
  /// integer registers, convert it to a structure containing ints and pass all
  /// of the struct elements in.  If Size is set we pass only that many bytes.
  void PassInIntegerRegisters(tree type, std::vector<const Type*> &ScalarElts,
                              unsigned origSize, bool DontCheckAlignment) {
    unsigned Size;
    if (origSize)
      Size = origSize;
    else
      Size = TREE_INT_CST_LOW(TYPE_SIZE(type))/8;

    // FIXME: We should preserve all aggregate value alignment information.
    // Work around to preserve some aggregate value alignment information:
    // don't bitcast aggregate value to Int64 if its alignment is different
    // from Int64 alignment. ARM backend needs this.
    unsigned Align = TYPE_ALIGN(type)/8;
    unsigned Int64Align =
      getTargetData().getABITypeAlignment(Type::getInt64Ty(getGlobalContext()));
    bool UseInt64 = (getTargetData().isLegalInteger(64) &&
                     (DontCheckAlignment || Align >= Int64Align));

    unsigned ElementSize = UseInt64 ? 8:4;
    unsigned ArraySize = Size / ElementSize;

    // Put as much of the aggregate as possible into an array.
    const Type *ATy = NULL;
    const Type *ArrayElementType = NULL;
    if (ArraySize) {
      Size = Size % ElementSize;
      ArrayElementType = (UseInt64 ?
                          Type::getInt64Ty(getGlobalContext()) :
                          Type::getInt32Ty(getGlobalContext()));
      ATy = ArrayType::get(ArrayElementType, ArraySize);
    }

    // Pass any leftover bytes as a separate element following the array.
    unsigned LastEltRealSize = 0;
    const llvm::Type *LastEltTy = 0;
    if (Size > 4) {
      LastEltTy = Type::getInt64Ty(getGlobalContext());
    } else if (Size > 2) {
      LastEltTy = Type::getInt32Ty(getGlobalContext());
    } else if (Size > 1) {
      LastEltTy = Type::getInt16Ty(getGlobalContext());
    } else if (Size > 0) {
      LastEltTy = Type::getInt8Ty(getGlobalContext());
    }
    if (LastEltTy) {
      if (Size != getTargetData().getTypeAllocSize(LastEltTy))
        LastEltRealSize = Size;
    }

    std::vector<const Type*> Elts;
    if (ATy)
      Elts.push_back(ATy);
    if (LastEltTy)
      Elts.push_back(LastEltTy);
    const StructType *STy = StructType::get(getGlobalContext(), Elts, false);

    unsigned i = 0;
    if (ArraySize) {
      C.EnterField(0, STy);
      for (unsigned j = 0; j < ArraySize; ++j) {
        C.EnterField(j, ATy);
        C.HandleScalarArgument(ArrayElementType, 0);
        ScalarElts.push_back(ArrayElementType);
        C.ExitField();
      }
      C.ExitField();
      ++i;
    }
    if (LastEltTy) {
      C.EnterField(i, STy);
      C.HandleScalarArgument(LastEltTy, 0, LastEltRealSize);
      ScalarElts.push_back(LastEltTy);
      C.ExitField();
    }
  }

  /// PassInMixedRegisters - Given an aggregate value that should be passed in
  /// mixed integer, floating point, and vector registers, convert it to a
  /// structure containing the specified struct elements in.
  void PassInMixedRegisters(const Type *Ty, std::vector<const Type*> &OrigElts,
                            std::vector<const Type*> &ScalarElts) {
    // We use VoidTy in OrigElts to mean "this is a word in the aggregate
    // that occupies storage but has no useful information, and is not passed
    // anywhere".  Happens on x86-64.
    std::vector<const Type*> Elts(OrigElts);
    const Type* wordType = getTargetData().getPointerSize() == 4 ?
        Type::getInt32Ty(getGlobalContext()) : Type::getInt64Ty(getGlobalContext());
    for (unsigned i=0, e=Elts.size(); i!=e; ++i)
      if (OrigElts[i]->isVoidTy())
        Elts[i] = wordType;

    const StructType *STy = StructType::get(getGlobalContext(), Elts, false);

    unsigned Size = getTargetData().getTypeAllocSize(STy);
    const StructType *InSTy = dyn_cast<StructType>(Ty);
    unsigned InSize = 0;
    // If Ty and STy size does not match then last element is accessing
    // extra bits.
    unsigned LastEltSizeDiff = 0;
    if (InSTy) {
      InSize = getTargetData().getTypeAllocSize(InSTy);
      if (InSize < Size) {
        unsigned N = STy->getNumElements();
        const llvm::Type *LastEltTy = STy->getElementType(N-1);
        if (LastEltTy->isInteger())
          LastEltSizeDiff = 
            getTargetData().getTypeAllocSize(LastEltTy) - (Size - InSize);
      }
    }
    for (unsigned i = 0, e = Elts.size(); i != e; ++i) {
      if (!OrigElts[i]->isVoidTy()) {
        C.EnterField(i, STy);
        unsigned RealSize = 0;
        if (LastEltSizeDiff && i == (e - 1))
          RealSize = LastEltSizeDiff;
        C.HandleScalarArgument(Elts[i], 0, RealSize);
        ScalarElts.push_back(Elts[i]);
        C.ExitField();
      }
    }
  }
};

// Make sure the SVR4 ABI is used on 32-bit PowerPC Linux.
#if defined(POWERPC_LINUX) && (TARGET_64BIT == 0)
#define TheLLVMABI SVR4ABI
#endif

/// TheLLVMABI - This can be defined by targets if they want total control over
/// ABI decisions.
///
#ifndef TheLLVMABI
#define TheLLVMABI DefaultABI
#endif

/// SVR4ABI - This class implements the System V Release 4 ABI for PowerPC. The
/// SVR4 ABI is the ABI used on 32-bit PowerPC Linux.
///
template<typename Client>
class SVR4ABI {
  // Number of general purpose argument registers which have already been
  // assigned.
  unsigned NumGPR;
protected:
  Client &C;
public:
  SVR4ABI(Client &c) : NumGPR(0), C(c) {}

  bool isShadowReturn() const { return C.isShadowReturn(); }
  
  /// HandleReturnType - This is invoked by the target-independent code for the
  /// return type. It potentially breaks down the argument and invokes methods
  /// on the client that indicate how its pieces should be handled.  This
  /// handles things like returning structures via hidden parameters.
  ///
  /// This is the default implementation which was copied from DefaultABI.
  void HandleReturnType(tree type, tree fn, bool isBuiltin) {
    unsigned Offset = 0;
    const Type *Ty = ConvertType(type);
    if (isa<VectorType>(Ty)) {
      // Vector handling is weird on x86.  In particular builtin and
      // non-builtin function of the same return types can use different
      // calling conventions.
      tree ScalarType = LLVM_SHOULD_RETURN_VECTOR_AS_SCALAR(type, isBuiltin);
      if (ScalarType)
        C.HandleAggregateResultAsScalar(ConvertType(ScalarType));
      else if (LLVM_SHOULD_RETURN_VECTOR_AS_SHADOW(type, isBuiltin))
        C.HandleScalarShadowResult(Ty->getPointerTo(), false);
      else
        C.HandleScalarResult(Ty);
    } else if (Ty->isSingleValueType() || Ty->isVoidTy()) {
      // Return scalar values normally.
      C.HandleScalarResult(Ty);
    } else if (doNotUseShadowReturn(type, fn, C.getCallingConv())) {
      tree SingleElt = LLVM_SHOULD_RETURN_SELT_STRUCT_AS_SCALAR(type);
      if (SingleElt && TYPE_SIZE(SingleElt) && 
          TREE_CODE(TYPE_SIZE(SingleElt)) == INTEGER_CST &&
          TREE_INT_CST_LOW(TYPE_SIZE_UNIT(type)) == 
            TREE_INT_CST_LOW(TYPE_SIZE_UNIT(SingleElt))) {
        C.HandleAggregateResultAsScalar(ConvertType(SingleElt));
      } else {
        // Otherwise return as an integer value large enough to hold the entire
        // aggregate.
        if (const Type *AggrTy = LLVM_AGGR_TYPE_FOR_STRUCT_RETURN(type,
                                    C.getCallingConv()))
          C.HandleAggregateResultAsAggregate(AggrTy);
        else if (const Type* ScalarTy = 
                    LLVM_SCALAR_TYPE_FOR_STRUCT_RETURN(type, &Offset))
          C.HandleAggregateResultAsScalar(ScalarTy, Offset);
        else {
          assert(0 && "Unable to determine how to return this aggregate!");
          abort();
        }
      }
    } else {
      // If the function is returning a struct or union, we pass the pointer to
      // the struct as the first argument to the function.

      // FIXME: should return the hidden first argument for some targets
      // (e.g. ELF i386).
      C.HandleAggregateShadowResult(Ty->getPointerTo(), false);
    }
  }
  
  /// HandleArgument - This is invoked by the target-independent code for each
  /// argument type passed into the function.  It potentially breaks down the
  /// argument and invokes methods on the client that indicate how its pieces
  /// should be handled.  This handles things like decimating structures into
  /// their fields.
  ///
  /// _Complex arguments are never split, thus their two scalars are either
  /// passed both in argument registers or both on the stack. Also _Complex
  /// arguments are always passed in general purpose registers, never in
  /// Floating-point registers or vector registers. Arguments which should go
  /// on the stack are marked with the inreg parameter attribute.
  /// Giving inreg this target-dependent (and counter-intuitive) meaning
  /// simplifies things, because functions calls are not always coming from the
  /// frontend but are also created implicitly e.g. for libcalls. If inreg would
  /// actually mean that the argument is passed in a register, then all places
  /// which create function calls/function definitions implicitly would need to
  /// be aware of this fact and would need to mark arguments accordingly. With
  /// inreg meaning that the argument is passed on the stack, this is not an
  /// issue, except for calls which involve _Complex types.
  void HandleArgument(tree type, std::vector<const Type*> &ScalarElts,
                      Attributes *Attributes = NULL) {
    // Eight GPR's are availabe for parameter passing.
    const unsigned NumArgRegs = 8;
    const Type *Ty = ConvertType(type);
    // Figure out if this field is zero bits wide, e.g. {} or [0 x int].  Do
    // not include variable sized fields here.
    std::vector<const Type*> Elts;
    if (isPassedByInvisibleReference(type)) { // variable size -> by-ref.
      const Type *PtrTy = Ty->getPointerTo();
      C.HandleByInvisibleReferenceArgument(PtrTy, type);
      ScalarElts.push_back(PtrTy);

      unsigned Attr = Attribute::None;
      
      if (NumGPR < NumArgRegs) {
        NumGPR++;
      } else {
        Attr |= Attribute::InReg;
      }

      if (Attributes) {
        *Attributes |= Attr;
      }
    } else if (isa<VectorType>(Ty)) {
      if (LLVM_SHOULD_PASS_VECTOR_IN_INTEGER_REGS(type)) {
        PassInIntegerRegisters(type, ScalarElts, 0, false);
      } else if (LLVM_SHOULD_PASS_VECTOR_USING_BYVAL_ATTR(type)) {
        C.HandleByValArgument(Ty, type);
        if (Attributes) {
          *Attributes |= Attribute::ByVal;
          *Attributes |= 
            Attribute::constructAlignmentFromInt(LLVM_BYVAL_ALIGNMENT(type));
        }
      } else {
        C.HandleScalarArgument(Ty, type);
        ScalarElts.push_back(Ty);
      }
    } else if (Ty->isSingleValueType()) {
      C.HandleScalarArgument(Ty, type);
      ScalarElts.push_back(Ty);
      
      unsigned Attr = Attribute::None;
      
      if (Ty->isInteger()) {
        unsigned TypeSize = Ty->getPrimitiveSizeInBits();

        // Determine how many general purpose registers are needed for the
        // argument.
        unsigned NumRegs = (TypeSize + 31) / 32;

        // Make sure argument registers are aligned. 64-bit arguments are put in
        // a register pair which starts with an odd register number.
        if (TypeSize == 64 && (NumGPR % 2) == 1) {
          NumGPR++;
        }
        
        if (NumGPR <= (NumArgRegs - NumRegs)) {
          NumGPR += NumRegs;
        } else {
          Attr |= Attribute::InReg;
          NumGPR = NumArgRegs;
        }
      } else if (isa<PointerType>(Ty)) {
        if (NumGPR < NumArgRegs) {
          NumGPR++;
        } else {
          Attr |= Attribute::InReg;
        }
      // We don't care about arguments passed in Floating-point or vector
      // registers.
      } else if (!(Ty->isFloatingPoint() || isa<VectorType>(Ty))) {
        abort();
      }

      if (Attributes) {
        *Attributes |= Attr;
      }
    } else if (LLVM_SHOULD_PASS_AGGREGATE_IN_MIXED_REGS(type, Ty,
                                                        C.getCallingConv(),
                                                        Elts)) {
      HOST_WIDE_INT SrcSize = int_size_in_bytes(type);
      
      // With the SVR4 ABI, the only aggregates which are passed in registers
      // are _Complex aggregates.
      assert(TREE_CODE(type) == COMPLEX_TYPE && "Not a _Complex type!");

      unsigned Attr = Attribute::None;
      
      switch (SrcSize) {
      default:
        abort();
        break;
      case 32:
        // _Complex long double
        if (NumGPR == 0) {
          NumGPR += NumArgRegs;
        } else {
          Attr |= Attribute::InReg;
          NumGPR = NumArgRegs;
        }
        break;
      case 16:
        // _Complex long long
        // _Complex double
        if (NumGPR <= (NumArgRegs - 4)) {
          NumGPR += 4;
        } else {
          Attr |= Attribute::InReg;
          NumGPR = NumArgRegs;
        }
        break;
      case 8:
        // _Complex int
        // _Complex long
        // _Complex float
        
        // Make sure argument registers are aligned. 64-bit arguments are put in
        // a register pair which starts with an odd register number.
        if (NumGPR % 2 == 1) {
          NumGPR++;
        }

        if (NumGPR <= (NumArgRegs - 2)) {
          NumGPR += 2;
        } else {
          Attr |= Attribute::InReg;
          NumGPR = NumArgRegs;
        }
        break;
      case 4:
      case 2:
        // _Complex short
        // _Complex char
        if (NumGPR < NumArgRegs) {
          NumGPR++;
        } else {
          Attr |= Attribute::InReg;
        }
        break;
      }

      if (Attributes) {
        *Attributes |= Attr;
      }
      
      PassInMixedRegisters(Ty, Elts, ScalarElts);
    } else if (LLVM_SHOULD_PASS_AGGREGATE_USING_BYVAL_ATTR(type, Ty)) {
      C.HandleByValArgument(Ty, type);
      if (Attributes) {
        *Attributes |= Attribute::ByVal;
        *Attributes |= 
          Attribute::constructAlignmentFromInt(LLVM_BYVAL_ALIGNMENT(type));
      }
      
      unsigned Attr = Attribute::None;
      
      if (NumGPR < NumArgRegs) {
        NumGPR++;
      } else {
        Attr |= Attribute::InReg;
      }

      if (Attributes) {
        *Attributes |= Attr;
      }
    } else if (isZeroSizedStructOrUnion(type)) {
      // Zero sized struct or union, just drop it!
      ;
    } else {
      assert(0 && "unknown aggregate type!");
      abort();
    }
  }

  /// HandleUnion - Handle a UNION_TYPE or QUAL_UNION_TYPE tree.
  ///
  /// This is the default implementation which was copied from DefaultABI.
  void HandleUnion(tree type, std::vector<const Type*> &ScalarElts) {
    if (TYPE_TRANSPARENT_UNION(type)) {
      tree Field = TYPE_FIELDS(type);
      assert(Field && "Transparent union must have some elements!");
      while (TREE_CODE(Field) != FIELD_DECL) {
        Field = TREE_CHAIN(Field);
        assert(Field && "Transparent union must have some elements!");
      }
      
      HandleArgument(TREE_TYPE(Field), ScalarElts);
    } else {
      // Unions pass the largest element.
      unsigned MaxSize = 0;
      tree MaxElt = 0;
      for (tree Field = TYPE_FIELDS(type); Field; Field = TREE_CHAIN(Field)) {
        if (TREE_CODE(Field) == FIELD_DECL) {
          // Skip fields that are known not to be present.
          if (TREE_CODE(type) == QUAL_UNION_TYPE &&
              integer_zerop(DECL_QUALIFIER(Field)))
              continue;

          tree SizeTree = TYPE_SIZE(TREE_TYPE(Field));
          unsigned Size = ((unsigned)TREE_INT_CST_LOW(SizeTree)+7)/8;
          if (Size > MaxSize) {
            MaxSize = Size;
            MaxElt = Field;
          }

          // Skip remaining fields if this one is known to be present.
          if (TREE_CODE(type) == QUAL_UNION_TYPE &&
              integer_onep(DECL_QUALIFIER(Field)))
              break;
        }
      }
      
      if (MaxElt)
        HandleArgument(TREE_TYPE(MaxElt), ScalarElts);
    }
  }
    
  /// PassInIntegerRegisters - Given an aggregate value that should be passed in
  /// integer registers, convert it to a structure containing ints and pass all
  /// of the struct elements in.  If Size is set we pass only that many bytes.
  ///
  /// This is the default implementation which was copied from DefaultABI.
  void PassInIntegerRegisters(tree type, std::vector<const Type*> &ScalarElts,
                              unsigned origSize, bool DontCheckAlignment) {
    unsigned Size;
    if (origSize)
      Size = origSize;
    else
      Size = TREE_INT_CST_LOW(TYPE_SIZE(type))/8;

    // FIXME: We should preserve all aggregate value alignment information.
    // Work around to preserve some aggregate value alignment information:
    // don't bitcast aggregate value to Int64 if its alignment is different
    // from Int64 alignment. ARM backend needs this.
    unsigned Align = TYPE_ALIGN(type)/8;
    unsigned Int64Align =
      getTargetData().getABITypeAlignment(Type::getInt64Ty(getGlobalContext()));
    bool UseInt64 = (getTargetData().isLegalInteger(64) &&
                     (DontCheckAlignment || Align >= Int64Align));

    // FIXME: In cases where we can, we should use the original struct.
    // Consider cases like { int, int } and {int, short} for example!  This will
    // produce far better LLVM code!
    std::vector<const Type*> Elts;

    unsigned ElementSize = UseInt64 ? 8:4;
    unsigned ArraySize = Size / ElementSize;

    // Put as much of the aggregate as possible into an array.
    const Type *ATy = NULL;
    const Type *ArrayElementType = NULL;
    if (ArraySize) {
      Size = Size % ElementSize;
      ArrayElementType = (UseInt64 ?
                          Type::getInt64Ty(getGlobalContext()) :
                          Type::getInt32Ty(getGlobalContext()));
      ATy = ArrayType::get(ArrayElementType, ArraySize);
      Elts.push_back(ATy);
    }

    // Pass any leftover bytes as a separate element following the array.
    unsigned LastEltRealSize = 0;
    const llvm::Type *LastEltTy = 0;
    if (Size > 4) {
      LastEltTy = Type::getInt64Ty(getGlobalContext());
    } else if (Size > 2) {
      LastEltTy = Type::getInt32Ty(getGlobalContext());
    } else if (Size > 1) {
      LastEltTy = Type::getInt16Ty(getGlobalContext());
    } else if (Size > 0) {
      LastEltTy = Type::getInt8Ty(getGlobalContext());
    }
    if (LastEltTy) {
      Elts.push_back(LastEltTy);
      if (Size != getTargetData().getTypeAllocSize(LastEltTy))
        LastEltRealSize = Size;
    }

    const StructType *STy = StructType::get(getGlobalContext(), Elts, false);

    unsigned i = 0;
    if (ArraySize) {
      C.EnterField(0, STy);
      for (unsigned j = 0; j < ArraySize; ++j) {
        C.EnterField(j, ATy);
        C.HandleScalarArgument(ArrayElementType, 0);
        ScalarElts.push_back(ArrayElementType);
        C.ExitField();
      }
      C.ExitField();
      ++i;
    }
    if (LastEltTy) {
      C.EnterField(i, STy);
      C.HandleScalarArgument(LastEltTy, 0, LastEltRealSize);
      ScalarElts.push_back(LastEltTy);
      C.ExitField();
    }
  }

  /// PassInMixedRegisters - Given an aggregate value that should be passed in
  /// mixed integer, floating point, and vector registers, convert it to a
  /// structure containing the specified struct elements in.
  ///
  /// This is the default implementation which was copied from DefaultABI.
  void PassInMixedRegisters(const Type *Ty, std::vector<const Type*> &OrigElts,
                            std::vector<const Type*> &ScalarElts) {
    // We use VoidTy in OrigElts to mean "this is a word in the aggregate
    // that occupies storage but has no useful information, and is not passed
    // anywhere".  Happens on x86-64.
    std::vector<const Type*> Elts(OrigElts);
    const Type* wordType = getTargetData().getPointerSize() == 4
      ? Type::getInt32Ty(getGlobalContext()) : Type::getInt64Ty(getGlobalContext());
    for (unsigned i=0, e=Elts.size(); i!=e; ++i)
      if (OrigElts[i]->isVoidTy())
        Elts[i] = wordType;

    const StructType *STy = StructType::get(getGlobalContext(), Elts, false);

    unsigned Size = getTargetData().getTypeAllocSize(STy);
    const StructType *InSTy = dyn_cast<StructType>(Ty);
    unsigned InSize = 0;
    // If Ty and STy size does not match then last element is accessing
    // extra bits.
    unsigned LastEltSizeDiff = 0;
    if (InSTy) {
      InSize = getTargetData().getTypeAllocSize(InSTy);
      if (InSize < Size) {
        unsigned N = STy->getNumElements();
        const llvm::Type *LastEltTy = STy->getElementType(N-1);
        if (LastEltTy->isInteger())
          LastEltSizeDiff = 
            getTargetData().getTypeAllocSize(LastEltTy) - (Size - InSize);
      }
    }
    for (unsigned i = 0, e = Elts.size(); i != e; ++i) {
      if (!OrigElts[i]->isVoidTy()) {
        C.EnterField(i, STy);
        unsigned RealSize = 0;
        if (LastEltSizeDiff && i == (e - 1))
          RealSize = LastEltSizeDiff;
        C.HandleScalarArgument(Elts[i], 0, RealSize);
        ScalarElts.push_back(Elts[i]);
        C.ExitField();
      }
    }
  }
};

#endif /* LLVM_ABI_H */
/* LLVM LOCAL end (ENTIRE FILE!)  */
