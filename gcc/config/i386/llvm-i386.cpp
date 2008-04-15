/* LLVM LOCAL begin (ENTIRE FILE!)  */
/* High-level LLVM backend interface 
Copyright (C) 2005 Free Software Foundation, Inc.
Contributed by Evan Cheng (evan.cheng@apple.com)

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
// This is a C++ source file that implements specific llvm IA-32 ABI.
//===----------------------------------------------------------------------===//

#include "llvm-abi.h"
#include "llvm-internal.h"
#include "llvm/DerivedTypes.h"
#include "llvm/Instructions.h"
#include "llvm/Intrinsics.h"
#include "llvm/Module.h"
#include "llvm-i386-target.h"

extern "C" {
#include "toplev.h"
}

/* TargetIntrinsicLower - For builtins that we want to expand to normal LLVM
 * code, emit the code now.  If we can handle the code, this macro should emit
 * the code, return true.
 */
bool TreeToLLVM::TargetIntrinsicLower(tree exp,
                                      unsigned FnCode,
                                      const MemRef *DestLoc,
                                      Value *&Result,
                                      const Type *ResultType,
                                      std::vector<Value*> &Ops) {
  switch (FnCode) {
  default: break;
  case IX86_BUILTIN_ADDPS:
  case IX86_BUILTIN_ADDPD:
  case IX86_BUILTIN_PADDB:
  case IX86_BUILTIN_PADDW:
  case IX86_BUILTIN_PADDD:
  case IX86_BUILTIN_PADDQ:
  case IX86_BUILTIN_PADDB128:
  case IX86_BUILTIN_PADDW128:
  case IX86_BUILTIN_PADDD128:
  case IX86_BUILTIN_PADDQ128:
    Result = Builder.CreateAdd(Ops[0], Ops[1], "tmp");
    return true;
  case IX86_BUILTIN_SUBPS:
  case IX86_BUILTIN_SUBPD:
  case IX86_BUILTIN_PSUBB:
  case IX86_BUILTIN_PSUBW:
  case IX86_BUILTIN_PSUBD:
  case IX86_BUILTIN_PSUBQ:
  case IX86_BUILTIN_PSUBB128:
  case IX86_BUILTIN_PSUBW128:
  case IX86_BUILTIN_PSUBD128:
  case IX86_BUILTIN_PSUBQ128:
    Result = Builder.CreateSub(Ops[0], Ops[1], "tmp");
    return true;
  case IX86_BUILTIN_MULPS:
  case IX86_BUILTIN_MULPD:
  case IX86_BUILTIN_PMULLW:
  case IX86_BUILTIN_PMULLW128:
    Result = Builder.CreateMul(Ops[0], Ops[1], "tmp");
    return true;
  case IX86_BUILTIN_PSLLWI: {
    Function *psllw =
      Intrinsic::getDeclaration(TheModule, Intrinsic::x86_mmx_psll_w);
    Ops[1] = Builder.CreateZExt(Ops[1], Type::Int64Ty, "zext");
    Ops[1] = BuildVector(Ops[1], NULL);
    Result = Builder.CreateCall(psllw, Ops.begin(), Ops.begin()+2, "tmp");
    Result = Builder.CreateBitCast(Result, ResultType, "tmp");
    return true;
  }
  case IX86_BUILTIN_PSLLWI128: {
    Function *psllw =
      Intrinsic::getDeclaration(TheModule, Intrinsic::x86_sse2_psll_w);
    Value *Undef = UndefValue::get(Type::Int32Ty);
    Ops[1] = BuildVector(Ops[1], Undef, Undef, Undef, NULL);
    Ops[1] = Builder.CreateBitCast(Ops[1], Ops[0]->getType(), "tmp");
    Result = Builder.CreateCall(psllw, Ops.begin(), Ops.begin()+2, "tmp");
    Result = Builder.CreateBitCast(Result, ResultType, "tmp");
    return true;
  }
  case IX86_BUILTIN_PSLLDI: {
    Function *pslld =
      Intrinsic::getDeclaration(TheModule, Intrinsic::x86_mmx_psll_d);
    Ops[1] = Builder.CreateZExt(Ops[1], Type::Int64Ty, "zext");
    Ops[1] = BuildVector(Ops[1], NULL);
    Result = Builder.CreateCall(pslld, Ops.begin(), Ops.begin()+2, "tmp");
    Result = Builder.CreateBitCast(Result, ResultType, "tmp");
    return true;
  }
  case IX86_BUILTIN_PSLLDI128: {
    Function *pslld
      = Intrinsic::getDeclaration(TheModule, Intrinsic::x86_sse2_psll_d);
    Value *Undef = UndefValue::get(Type::Int32Ty);
    Ops[1] = BuildVector(Ops[1], Undef, Undef, Undef, NULL);
    Result = Builder.CreateCall(pslld, Ops.begin(), Ops.begin()+2, "tmp");
    Result = Builder.CreateBitCast(Result, ResultType, "tmp");
    return true;
  }
  case IX86_BUILTIN_PSLLQI: {
    Function *psllq =
      Intrinsic::getDeclaration(TheModule, Intrinsic::x86_mmx_psll_q);
    Ops[1] = Builder.CreateZExt(Ops[1], Type::Int64Ty, "zext");
    Ops[1] = BuildVector(Ops[1], NULL);
    Result = Builder.CreateCall(psllq, Ops.begin(), Ops.begin()+2, "tmp");
    Result = Builder.CreateBitCast(Result, ResultType, "tmp");
    return true;
  }
  case IX86_BUILTIN_PSLLQI128: {
    Function *psllq =
      Intrinsic::getDeclaration(TheModule, Intrinsic::x86_sse2_psll_q);
    Value *Undef = UndefValue::get(Type::Int32Ty);
    Ops[1] = BuildVector(Ops[1], Undef, Undef, Undef, NULL);
    Ops[1] = Builder.CreateBitCast(Ops[1], Ops[0]->getType(), "tmp");
    Result = Builder.CreateCall(psllq, Ops.begin(), Ops.begin()+2, "tmp");
    Result = Builder.CreateBitCast(Result, ResultType, "tmp");
    return true;
  }
  case IX86_BUILTIN_PSRLWI: {
    Function *psrlw =
      Intrinsic::getDeclaration(TheModule, Intrinsic::x86_mmx_psrl_w);
    Ops[1] = Builder.CreateZExt(Ops[1], Type::Int64Ty, "zext");
    Ops[1] = BuildVector(Ops[1], NULL);
    Result = Builder.CreateCall(psrlw, Ops.begin(), Ops.begin()+2, "tmp");
    Result = Builder.CreateBitCast(Result, ResultType, "tmp");
    return true;
  }
  case IX86_BUILTIN_PSRLWI128: {
    Function *psrlw =
      Intrinsic::getDeclaration(TheModule, Intrinsic::x86_sse2_psrl_w);
    Value *Undef = UndefValue::get(Type::Int32Ty);
    Ops[1] = BuildVector(Ops[1], Undef, Undef, Undef, NULL);
    Ops[1] = Builder.CreateBitCast(Ops[1], Ops[0]->getType(), "tmp");
    Result = Builder.CreateCall(psrlw, Ops.begin(), Ops.begin()+2, "tmp");
    Result = Builder.CreateBitCast(Result, ResultType, "tmp");
    return true;
  }
  case IX86_BUILTIN_PSRLDI: {
    Function *psrld =
      Intrinsic::getDeclaration(TheModule, Intrinsic::x86_mmx_psrl_d);
    Ops[1] = Builder.CreateZExt(Ops[1], Type::Int64Ty, "zext");
    Ops[1] = BuildVector(Ops[1], NULL);
    Result = Builder.CreateCall(psrld, Ops.begin(), Ops.begin()+2, "tmp");
    Result = Builder.CreateBitCast(Result, ResultType, "tmp");
    return true;
  }
  case IX86_BUILTIN_PSRLDI128: {
    Function *psrld =
      Intrinsic::getDeclaration(TheModule, Intrinsic::x86_sse2_psrl_d);
    Value *Undef = UndefValue::get(Type::Int32Ty);
    Ops[1] = BuildVector(Ops[1], Undef, Undef, Undef, NULL);
    Result = Builder.CreateCall(psrld, Ops.begin(), Ops.begin()+2, "tmp");
    Result = Builder.CreateBitCast(Result, ResultType, "tmp");
    return true;
  }
  case IX86_BUILTIN_PSRLQI: {
    Function *psrlq =
      Intrinsic::getDeclaration(TheModule, Intrinsic::x86_mmx_psrl_q);
    Ops[1] = Builder.CreateZExt(Ops[1], Type::Int64Ty, "zext");
    Ops[1] = BuildVector(Ops[1], NULL);
    Result = Builder.CreateCall(psrlq, Ops.begin(), Ops.begin()+2, "tmp");
    Result = Builder.CreateBitCast(Result, ResultType, "tmp");
    return true;
  }
  case IX86_BUILTIN_PSRLQI128: {
    Function *psrlq =
      Intrinsic::getDeclaration(TheModule, Intrinsic::x86_sse2_psrl_q);
    Value *Undef = UndefValue::get(Type::Int32Ty);
    Ops[1] = BuildVector(Ops[1], Undef, Undef, Undef, NULL);
    Ops[1] = Builder.CreateBitCast(Ops[1], Ops[0]->getType(), "tmp");
    Result = Builder.CreateCall(psrlq, Ops.begin(), Ops.begin()+2, "tmp");
    Result = Builder.CreateBitCast(Result, ResultType, "tmp");
    return true;
  }
  case IX86_BUILTIN_PSRAWI: {
    Function *psraw =
      Intrinsic::getDeclaration(TheModule, Intrinsic::x86_mmx_psra_w);
    Ops[1] = Builder.CreateZExt(Ops[1], Type::Int64Ty, "zext");
    Ops[1] = BuildVector(Ops[1], NULL);
    Result = Builder.CreateCall(psraw, Ops.begin(), Ops.begin()+2, "tmp");
    Result = Builder.CreateBitCast(Result, ResultType, "tmp");
    return true;
  }
  case IX86_BUILTIN_PSRAWI128: {
    Function *psraw =
      Intrinsic::getDeclaration(TheModule, Intrinsic::x86_sse2_psra_w);
    Value *Undef = UndefValue::get(Type::Int32Ty);
    Ops[1] = BuildVector(Ops[1], Undef, Undef, Undef, NULL);
    Ops[1] = Builder.CreateBitCast(Ops[1], Ops[0]->getType(), "tmp");
    Result = Builder.CreateCall(psraw, Ops.begin(), Ops.begin()+2, "tmp");
    Result = Builder.CreateBitCast(Result, ResultType, "tmp");
    return true;
  }
  case IX86_BUILTIN_PSRADI: {
    Function *psrad =
      Intrinsic::getDeclaration(TheModule, Intrinsic::x86_mmx_psra_d);
    Ops[1] = Builder.CreateZExt(Ops[1], Type::Int64Ty, "zext");
    Ops[1] = BuildVector(Ops[1], NULL);
    Result = Builder.CreateCall(psrad, Ops.begin(), Ops.begin()+2, "tmp");
    Result = Builder.CreateBitCast(Result, ResultType, "tmp");
    return true;
  }
  case IX86_BUILTIN_PSRADI128: {
    Function *psrad =
      Intrinsic::getDeclaration(TheModule, Intrinsic::x86_sse2_psra_d);
    Value *Undef = UndefValue::get(Type::Int32Ty);
    Ops[1] = BuildVector(Ops[1], Undef, Undef, Undef, NULL);
    Result = Builder.CreateCall(psrad, Ops.begin(), Ops.begin()+2, "tmp");
    Result = Builder.CreateBitCast(Result, ResultType, "tmp");
    return true;
  }
  case IX86_BUILTIN_DIVPS:
  case IX86_BUILTIN_DIVPD:
    Result = Builder.CreateFDiv(Ops[0], Ops[1], "tmp");
    return true;
  case IX86_BUILTIN_PAND:
  case IX86_BUILTIN_PAND128:
    Result = Builder.CreateAnd(Ops[0], Ops[1], "tmp");
    return true;
  case IX86_BUILTIN_PANDN:
  case IX86_BUILTIN_PANDN128:
    Ops[0] = Builder.CreateNot(Ops[0], "tmp");
    Result = Builder.CreateAnd(Ops[0], Ops[1], "tmp");
    return true;
  case IX86_BUILTIN_POR:
  case IX86_BUILTIN_POR128:
    Result = Builder.CreateOr(Ops[0], Ops[1], "tmp");
    return true;
  case IX86_BUILTIN_PXOR:
  case IX86_BUILTIN_PXOR128:
    Result = Builder.CreateXor(Ops[0], Ops[1], "tmp");
    return true;
  case IX86_BUILTIN_ANDPS:
  case IX86_BUILTIN_ORPS:
  case IX86_BUILTIN_XORPS:
  case IX86_BUILTIN_ANDNPS:
  case IX86_BUILTIN_ANDPD:
  case IX86_BUILTIN_ORPD:
  case IX86_BUILTIN_XORPD:
  case IX86_BUILTIN_ANDNPD:
    if (cast<VectorType>(ResultType)->getNumElements() == 4)  // v4f32
      Ops[0] = Builder.CreateBitCast(Ops[0], VectorType::get(Type::Int32Ty, 4),
                                     "tmp");
    else                                                      // v2f64
      Ops[0] = Builder.CreateBitCast(Ops[0], VectorType::get(Type::Int64Ty, 2),
                                     "tmp");
    
    Ops[1] = Builder.CreateBitCast(Ops[1], Ops[0]->getType(), "tmp");
    switch (FnCode) {
      case IX86_BUILTIN_ANDPS:
      case IX86_BUILTIN_ANDPD:
        Result = Builder.CreateAnd(Ops[0], Ops[1], "tmp");
        break;
      case IX86_BUILTIN_ORPS:
      case IX86_BUILTIN_ORPD:
        Result = Builder.CreateOr (Ops[0], Ops[1], "tmp");
         break;
      case IX86_BUILTIN_XORPS:
      case IX86_BUILTIN_XORPD:
        Result = Builder.CreateXor(Ops[0], Ops[1], "tmp");
        break;
      case IX86_BUILTIN_ANDNPS:
      case IX86_BUILTIN_ANDNPD:
        Ops[0] = Builder.CreateNot(Ops[0], "tmp");
        Result = Builder.CreateAnd(Ops[0], Ops[1], "tmp");
        break;
    }
    Result = Builder.CreateBitCast(Result, ResultType, "tmp");
    return true;
  case IX86_BUILTIN_SHUFPS:
    if (ConstantInt *Elt = dyn_cast<ConstantInt>(Ops[2])) {
      int EV = Elt->getZExtValue();
      Result = BuildVectorShuffle(Ops[0], Ops[1],
                                  ((EV & 0x03) >> 0),   ((EV & 0x0c) >> 2),
                                  ((EV & 0x30) >> 4)+4, ((EV & 0xc0) >> 6)+4);
    } else {
      error("%Hmask must be an immediate", &EXPR_LOCATION(exp));
      Result = Ops[0];
    }
    return true;
  case IX86_BUILTIN_PSHUFW:
  case IX86_BUILTIN_PSHUFD:
    if (ConstantInt *Elt = dyn_cast<ConstantInt>(Ops[1])) {
      int EV = Elt->getZExtValue();
      Result = BuildVectorShuffle(Ops[0], Ops[0],
                                  ((EV & 0x03) >> 0),   ((EV & 0x0c) >> 2),
                                  ((EV & 0x30) >> 4),   ((EV & 0xc0) >> 6));
    } else {
      error("%Hmask must be an immediate", &EXPR_LOCATION(exp));
      Result = Ops[0];
    }
    return true;
  case IX86_BUILTIN_PSHUFHW:
    if (ConstantInt *Elt = dyn_cast<ConstantInt>(Ops[1])) {
      int EV = Elt->getZExtValue();
      Result = BuildVectorShuffle(Ops[0], Ops[0],
                                  0, 1, 2, 3,
                                  ((EV & 0x03) >> 0)+4, ((EV & 0x0c) >> 2)+4,
                                  ((EV & 0x30) >> 4)+4, ((EV & 0xc0) >> 6)+4);
      return true;
    }
    return false;
  case IX86_BUILTIN_PSHUFLW:
    if (ConstantInt *Elt = dyn_cast<ConstantInt>(Ops[1])) {
      int EV = Elt->getZExtValue();
      Result = BuildVectorShuffle(Ops[0], Ops[0],
                                  ((EV & 0x03) >> 0),   ((EV & 0x0c) >> 2),
                                  ((EV & 0x30) >> 4),   ((EV & 0xc0) >> 6),
                                  4, 5, 6, 7);
    } else {
      error("%Hmask must be an immediate", &EXPR_LOCATION(exp));
      Result = Ops[0];
    }
    
    return true;
  case IX86_BUILTIN_PUNPCKHBW:
    Result = BuildVectorShuffle(Ops[0], Ops[1], 4, 12, 5, 13,
                                                6, 14, 7, 15);
    return true;
  case IX86_BUILTIN_PUNPCKHWD:
    Result = BuildVectorShuffle(Ops[0], Ops[1], 2, 6, 3, 7);
    return true;
  case IX86_BUILTIN_PUNPCKHDQ:
    Result = BuildVectorShuffle(Ops[0], Ops[1], 1, 3);
    return true;
  case IX86_BUILTIN_PUNPCKLBW:
    Result = BuildVectorShuffle(Ops[0], Ops[1], 0,  8, 1,  9,
                                                2, 10, 3, 11);
    return true;
  case IX86_BUILTIN_PUNPCKLWD:
    Result = BuildVectorShuffle(Ops[0], Ops[1], 0, 4, 1, 5);
    return true;
  case IX86_BUILTIN_PUNPCKLDQ:
    Result = BuildVectorShuffle(Ops[0], Ops[1], 0, 2);
    return true;
  case IX86_BUILTIN_PUNPCKHBW128:
    Result = BuildVectorShuffle(Ops[0], Ops[1],  8, 24,  9, 25,
                                                10, 26, 11, 27,
                                                12, 28, 13, 29,
                                                14, 30, 15, 31);
    return true;
  case IX86_BUILTIN_PUNPCKHWD128:
    Result = BuildVectorShuffle(Ops[0], Ops[1], 4, 12, 5, 13, 6, 14, 7, 15);
    return true;
  case IX86_BUILTIN_PUNPCKHDQ128:
    Result = BuildVectorShuffle(Ops[0], Ops[1], 2, 6, 3, 7);
    return true;
  case IX86_BUILTIN_PUNPCKLBW128:
    Result = BuildVectorShuffle(Ops[0], Ops[1],  0, 16,  1, 17,
                                                 2, 18,  3, 19,
                                                 4, 20,  5, 21,
                                                 6, 22,  7, 23);
    return true;
  case IX86_BUILTIN_PUNPCKLWD128:
    Result = BuildVectorShuffle(Ops[0], Ops[1], 0, 8, 1, 9, 2, 10, 3, 11);
    return true;
  case IX86_BUILTIN_PUNPCKLDQ128:
    Result = BuildVectorShuffle(Ops[0], Ops[1], 0, 4, 1, 5);
    return true;
  case IX86_BUILTIN_UNPCKHPS:
    Result = BuildVectorShuffle(Ops[0], Ops[1], 2, 6, 3, 7);
    return true;
  case IX86_BUILTIN_UNPCKLPS:
    Result = BuildVectorShuffle(Ops[0], Ops[1], 0, 4, 1, 5);
    return true;
  case IX86_BUILTIN_MOVHLPS:
    Result = BuildVectorShuffle(Ops[0], Ops[1], 6, 7, 2, 3);
    return true;
  case IX86_BUILTIN_MOVLHPS:
    Result = BuildVectorShuffle(Ops[0], Ops[1], 0, 1, 4, 5);
    return true;
  case IX86_BUILTIN_MOVSS:
    Result = BuildVectorShuffle(Ops[0], Ops[1], 4, 1, 2, 3);
    return true;
  case IX86_BUILTIN_MOVQ: {
    Value *Zero = ConstantInt::get(Type::Int32Ty, 0);
    Result = BuildVector(Zero, Zero, Zero, Zero, NULL);
    Result = BuildVectorShuffle(Result, Ops[0], 4, 5, 2, 3);
    return true;
  }
  case IX86_BUILTIN_LOADQ: {
    PointerType *f64Ptr = PointerType::getUnqual(Type::DoubleTy);
    Value *Zero = ConstantFP::get(Type::DoubleTy, APFloat(0.0));
    Ops[0] = Builder.CreateBitCast(Ops[0], f64Ptr, "tmp");
    Ops[0] = Builder.CreateLoad(Ops[0], "tmp");
    Result = BuildVector(Ops[0], Zero, NULL);
    Result = Builder.CreateBitCast(Result, ResultType, "tmp");
    return true;
  }
  case IX86_BUILTIN_LOADHPS: {
    PointerType *f64Ptr = PointerType::getUnqual(Type::DoubleTy);
    Ops[1] = Builder.CreateBitCast(Ops[1], f64Ptr, "tmp");
    Value *Load = Builder.CreateLoad(Ops[1], "tmp");
    Ops[1] = BuildVector(Load, UndefValue::get(Type::DoubleTy), NULL);
    Ops[1] = Builder.CreateBitCast(Ops[1], ResultType, "tmp");
    Result = BuildVectorShuffle(Ops[0], Ops[1], 0, 1, 4, 5);
    Result = Builder.CreateBitCast(Result, ResultType, "tmp");
    return true;
  }
  case IX86_BUILTIN_LOADLPS: {
    PointerType *f64Ptr = PointerType::getUnqual(Type::DoubleTy);
    Ops[1] = Builder.CreateBitCast(Ops[1], f64Ptr, "tmp");
    Value *Load = Builder.CreateLoad(Ops[1], "tmp");
    Ops[1] = BuildVector(Load, UndefValue::get(Type::DoubleTy), NULL);
    Ops[1] = Builder.CreateBitCast(Ops[1], ResultType, "tmp");
    Result = BuildVectorShuffle(Ops[0], Ops[1], 4, 5, 2, 3);
    Result = Builder.CreateBitCast(Result, ResultType, "tmp");
    return true;
  }
  case IX86_BUILTIN_STOREHPS: {
    VectorType *v2f64 = VectorType::get(Type::DoubleTy, 2);
    PointerType *f64Ptr = PointerType::getUnqual(Type::DoubleTy);
    Ops[0] = Builder.CreateBitCast(Ops[0], f64Ptr, "tmp");
    Value *Idx = ConstantInt::get(Type::Int32Ty, 1);
    Ops[1] = Builder.CreateBitCast(Ops[1], v2f64, "tmp");
    Ops[1] = Builder.CreateExtractElement(Ops[1], Idx, "tmp");
    Result = Builder.CreateStore(Ops[1], Ops[0]);
    return true;
  }
  case IX86_BUILTIN_STORELPS: {
    VectorType *v2f64 = VectorType::get(Type::DoubleTy, 2);
    PointerType *f64Ptr = PointerType::getUnqual(Type::DoubleTy);
    Ops[0] = Builder.CreateBitCast(Ops[0], f64Ptr, "tmp");
    Value *Idx = ConstantInt::get(Type::Int32Ty, 0);
    Ops[1] = Builder.CreateBitCast(Ops[1], v2f64, "tmp");
    Ops[1] = Builder.CreateExtractElement(Ops[1], Idx, "tmp");
    Result = Builder.CreateStore(Ops[1], Ops[0]);
    return true;
  }
  case IX86_BUILTIN_MOVSHDUP:
    Result = BuildVectorShuffle(Ops[0], Ops[0], 1, 1, 3, 3);
    return true;
  case IX86_BUILTIN_MOVSLDUP:
    Result = BuildVectorShuffle(Ops[0], Ops[0], 0, 0, 2, 2);
    return true;
  case IX86_BUILTIN_VEC_INIT_V2SI:
    Result = BuildVector(Ops[0], Ops[1], NULL);
    return true;
  case IX86_BUILTIN_VEC_INIT_V4HI:
    // Sometimes G++ promotes arguments to int.
    for (unsigned i = 0; i != 4; ++i)
      Ops[i] = Builder.CreateIntCast(Ops[i], Type::Int16Ty, false, "tmp");
    Result = BuildVector(Ops[0], Ops[1], Ops[2], Ops[3], NULL);
    return true;
  case IX86_BUILTIN_VEC_INIT_V8QI:
    // Sometimes G++ promotes arguments to int.
    for (unsigned i = 0; i != 8; ++i)
      Ops[i] = Builder.CreateIntCast(Ops[i], Type::Int8Ty, false, "tmp");
    Result = BuildVector(Ops[0], Ops[1], Ops[2], Ops[3],
                         Ops[4], Ops[5], Ops[6], Ops[7], NULL);
    return true;
  case IX86_BUILTIN_VEC_EXT_V2SI:
  case IX86_BUILTIN_VEC_EXT_V4HI:
  case IX86_BUILTIN_VEC_EXT_V2DF:
  case IX86_BUILTIN_VEC_EXT_V2DI:
  case IX86_BUILTIN_VEC_EXT_V4SI:
  case IX86_BUILTIN_VEC_EXT_V4SF:
  case IX86_BUILTIN_VEC_EXT_V8HI:
    Result = Builder.CreateExtractElement(Ops[0], Ops[1], "tmp");
    return true;
  case IX86_BUILTIN_VEC_SET_V4HI:
  case IX86_BUILTIN_VEC_SET_V8HI:
    // GCC sometimes doesn't produce the right element type.
    Ops[1] = Builder.CreateIntCast(Ops[1], Type::Int16Ty, false, "tmp");
    Result = Builder.CreateInsertElement(Ops[0], Ops[1], Ops[2], "tmp");
    return true;
  case IX86_BUILTIN_CMPEQPS:
  case IX86_BUILTIN_CMPLTPS:
  case IX86_BUILTIN_CMPLEPS:
  case IX86_BUILTIN_CMPGTPS:
  case IX86_BUILTIN_CMPGEPS:
  case IX86_BUILTIN_CMPNEQPS:
  case IX86_BUILTIN_CMPNLTPS:
  case IX86_BUILTIN_CMPNLEPS:
  case IX86_BUILTIN_CMPNGTPS:
  case IX86_BUILTIN_CMPNGEPS:
  case IX86_BUILTIN_CMPORDPS:
  case IX86_BUILTIN_CMPUNORDPS: {
    Function *cmpps =
      Intrinsic::getDeclaration(TheModule, Intrinsic::x86_sse_cmp_ps);
    bool flip = false;
    unsigned PredCode;
    switch (FnCode) {
    default: assert(0 && "Unknown fncode!");
    case IX86_BUILTIN_CMPEQPS: PredCode = 0; break;
    case IX86_BUILTIN_CMPLTPS: PredCode = 1; break;
    case IX86_BUILTIN_CMPGTPS: PredCode = 1; flip = true; break;
    case IX86_BUILTIN_CMPLEPS: PredCode = 2; break;
    case IX86_BUILTIN_CMPGEPS: PredCode = 2; flip = true; break;
    case IX86_BUILTIN_CMPUNORDPS: PredCode = 3; break;
    case IX86_BUILTIN_CMPNEQPS: PredCode = 4; break;
    case IX86_BUILTIN_CMPNLTPS: PredCode = 5; break;
    case IX86_BUILTIN_CMPNGTPS: PredCode = 5; flip = true; break;
    case IX86_BUILTIN_CMPNLEPS: PredCode = 6; break;
    case IX86_BUILTIN_CMPNGEPS: PredCode = 6; flip = true; break;
    case IX86_BUILTIN_CMPORDPS: PredCode = 7; break;
    }
    Value *Pred = ConstantInt::get(Type::Int8Ty, PredCode);
    Value *Arg0 = Ops[0];
    Value *Arg1 = Ops[1];
    if (flip) std::swap(Arg0, Arg1);
    Value *CallOps[3] = { Arg0, Arg1, Pred };
    Result = Builder.CreateCall(cmpps, CallOps, CallOps+3, "tmp");
    Result = Builder.CreateBitCast(Result, ResultType, "tmp");
    return true;
  }
  case IX86_BUILTIN_CMPEQSS:
  case IX86_BUILTIN_CMPLTSS:
  case IX86_BUILTIN_CMPLESS:
  case IX86_BUILTIN_CMPNEQSS:
  case IX86_BUILTIN_CMPNLTSS:
  case IX86_BUILTIN_CMPNLESS:
  case IX86_BUILTIN_CMPNGTSS:
  case IX86_BUILTIN_CMPNGESS:
  case IX86_BUILTIN_CMPORDSS:
  case IX86_BUILTIN_CMPUNORDSS: {
    Function *cmpss =
      Intrinsic::getDeclaration(TheModule, Intrinsic::x86_sse_cmp_ss);
    unsigned PredCode;
    switch (FnCode) {
    default: assert(0 && "Unknown fncode");
    case IX86_BUILTIN_CMPEQSS:    PredCode = 0; break;
    case IX86_BUILTIN_CMPLTSS:    PredCode = 1; break;
    case IX86_BUILTIN_CMPLESS:    PredCode = 2; break;
    case IX86_BUILTIN_CMPUNORDSS: PredCode = 3; break;
    case IX86_BUILTIN_CMPNEQSS:   PredCode = 4; break;
    case IX86_BUILTIN_CMPNLTSS:   PredCode = 5; break;
    case IX86_BUILTIN_CMPNLESS:   PredCode = 6; break;
    case IX86_BUILTIN_CMPORDSS:   PredCode = 7; break;
    }
    Value *Pred = ConstantInt::get(Type::Int8Ty, PredCode);
    Value *CallOps[3] = { Ops[0], Ops[1], Pred };
    Result = Builder.CreateCall(cmpss, CallOps, CallOps+3, "tmp");
    Result = Builder.CreateBitCast(Result, ResultType, "tmp");
    return true;
  }
  case IX86_BUILTIN_CMPEQPD:
  case IX86_BUILTIN_CMPLTPD:
  case IX86_BUILTIN_CMPLEPD:
  case IX86_BUILTIN_CMPGTPD:
  case IX86_BUILTIN_CMPGEPD:
  case IX86_BUILTIN_CMPNEQPD:
  case IX86_BUILTIN_CMPNLTPD:
  case IX86_BUILTIN_CMPNLEPD:
  case IX86_BUILTIN_CMPNGTPD:
  case IX86_BUILTIN_CMPNGEPD:
  case IX86_BUILTIN_CMPORDPD:
  case IX86_BUILTIN_CMPUNORDPD: {
    Function *cmppd =
      Intrinsic::getDeclaration(TheModule, Intrinsic::x86_sse2_cmp_pd);
    bool flip = false;
    unsigned PredCode;
    switch (FnCode) {
    default: assert(0 && "Unknown fncode!");
    case IX86_BUILTIN_CMPEQPD:    PredCode = 0; break;
    case IX86_BUILTIN_CMPLTPD:    PredCode = 1; break;
    case IX86_BUILTIN_CMPGTPD:    PredCode = 1; flip = true; break;
    case IX86_BUILTIN_CMPLEPD:    PredCode = 2; break;
    case IX86_BUILTIN_CMPGEPD:    PredCode = 2; flip = true; break;
    case IX86_BUILTIN_CMPUNORDPD: PredCode = 3; break;
    case IX86_BUILTIN_CMPNEQPD:   PredCode = 4; break;
    case IX86_BUILTIN_CMPNLTPD:   PredCode = 5; break;
    case IX86_BUILTIN_CMPNGTPD:   PredCode = 5; flip = true; break;
    case IX86_BUILTIN_CMPNLEPD:   PredCode = 6; break;
    case IX86_BUILTIN_CMPNGEPD:   PredCode = 6; flip = true; break;
    case IX86_BUILTIN_CMPORDPD:   PredCode = 7; break;
    }
    Value *Pred = ConstantInt::get(Type::Int8Ty, PredCode);
    Value *Arg0 = Ops[0];
    Value *Arg1 = Ops[1];
    if (flip) std::swap(Arg0, Arg1);

    Value *CallOps[3] = { Arg0, Arg1, Pred };
    Result = Builder.CreateCall(cmppd, CallOps, CallOps+3, "tmp");
    Result = Builder.CreateBitCast(Result, ResultType, "tmp");
    return true;
  }
  case IX86_BUILTIN_CMPEQSD:
  case IX86_BUILTIN_CMPLTSD:
  case IX86_BUILTIN_CMPLESD:
  case IX86_BUILTIN_CMPNEQSD:
  case IX86_BUILTIN_CMPNLTSD:
  case IX86_BUILTIN_CMPNLESD:
  case IX86_BUILTIN_CMPORDSD:
  case IX86_BUILTIN_CMPUNORDSD: {
    Function *cmpsd =
      Intrinsic::getDeclaration(TheModule, Intrinsic::x86_sse2_cmp_sd);
    unsigned PredCode;
    switch (FnCode) {
      default: assert(0 && "Unknown fncode");
    case IX86_BUILTIN_CMPEQSD:    PredCode = 0; break;
    case IX86_BUILTIN_CMPLTSD:    PredCode = 1; break;
    case IX86_BUILTIN_CMPLESD:    PredCode = 2; break;
    case IX86_BUILTIN_CMPUNORDSD: PredCode = 3; break;
    case IX86_BUILTIN_CMPNEQSD:   PredCode = 4; break;
    case IX86_BUILTIN_CMPNLTSD:   PredCode = 5; break;
    case IX86_BUILTIN_CMPNLESD:   PredCode = 6; break;
    case IX86_BUILTIN_CMPORDSD:   PredCode = 7; break;
    }
    Value *Pred = ConstantInt::get(Type::Int8Ty, PredCode);
    Value *CallOps[3] = { Ops[0], Ops[1], Pred };
    Result = Builder.CreateCall(cmpsd, CallOps, CallOps+3, "tmp");
    Result = Builder.CreateBitCast(Result, ResultType, "tmp");
    return true;
  }
  case IX86_BUILTIN_LDMXCSR: {
    Function *ldmxcsr =
      Intrinsic::getDeclaration(TheModule, Intrinsic::x86_sse_ldmxcsr);
    Value *Ptr = CreateTemporary(Type::Int32Ty);
    Builder.CreateStore(Ops[0], Ptr);
    Ptr = Builder.CreateBitCast(Ptr, PointerType::getUnqual(Type::Int8Ty), "tmp");
    Result = Builder.CreateCall(ldmxcsr, Ptr);
    return true;
  }
  case IX86_BUILTIN_STMXCSR: {
    Function *stmxcsr =
      Intrinsic::getDeclaration(TheModule, Intrinsic::x86_sse_stmxcsr);
    Value *Ptr  = CreateTemporary(Type::Int32Ty);
    Value *BPtr = Builder.CreateBitCast(Ptr, PointerType::getUnqual(Type::Int8Ty),
                                        "tmp");
    Builder.CreateCall(stmxcsr, BPtr);
    
    Result = Builder.CreateLoad(Ptr, "tmp");
    return true;
  }
  }

  return false;
}

/* These are defined in i386.c */
#define MAX_CLASSES 4
extern "C" enum machine_mode ix86_getNaturalModeForType(tree);
extern "C" int ix86_HowToPassArgument(enum machine_mode, tree, int, int*, int*);
extern "C" int ix86_ClassifyArgument(enum machine_mode, tree,
                               enum x86_64_reg_class classes[MAX_CLASSES], int);

/* Target hook for llvm-abi.h. It returns true if an aggregate of the
   specified type should be passed in memory. This is only called for
   x86-64. */
static bool llvm_x86_64_should_pass_aggregate_in_memory(tree TreeType,
                                                        enum machine_mode Mode){
  int IntRegs, SSERegs;
  /* If ix86_HowToPassArgument return 0, then it's passed byval in memory.*/
  return !ix86_HowToPassArgument(Mode, TreeType, 0, &IntRegs, &SSERegs);
}

/* Returns true if all elements of the type are integer types. */
static bool llvm_x86_is_all_integer_types(const Type *Ty) {
  for (Type::subtype_iterator I = Ty->subtype_begin(), E = Ty->subtype_end();
       I != E; ++I) {
    const Type *STy = I->get();
    if (!STy->isIntOrIntVector() && !isa<PointerType>(STy))
      return false;
  }
  return true;
}

/* Target hook for llvm-abi.h. It returns true if an aggregate of the
   specified type should be passed in a number of registers of mixed types.
   It also returns a vector of types that correspond to the registers used
   for parameter passing. This is only called for x86-32. */
bool
llvm_x86_32_should_pass_aggregate_in_mixed_regs(tree TreeType, const Type *Ty,
                                                std::vector<const Type*> &Elts){
  // If this is a small fixed size type, investigate it.
  HOST_WIDE_INT SrcSize = int_size_in_bytes(TreeType);
  if (SrcSize <= 0 || SrcSize > 16)
    return false;

  // X86-32 passes aggregates on the stack.  If this is an extremely simple
  // aggregate whose elements would be passed the same if passed as scalars,
  // pass them that way in order to promote SROA on the caller and callee side.
  // Note that we can't support passing all structs this way.  For example,
  // {i16, i16} should be passed in on 32-bit unit, which is not how "i16, i16"
  // would be passed as stand-alone arguments.
  const StructType *STy = dyn_cast<StructType>(Ty);
  if (!STy || STy->isPacked()) return false;

  for (unsigned i = 0, e = STy->getNumElements(); i != e; ++i) {
    const Type *EltTy = STy->getElementType(i);
    // 32 and 64-bit integers are fine, as are float and double.  Long double
    // (which can be picked as the type for a union of 16 bytes) is not fine, 
    // as loads and stores of it get only 10 bytes.
    if (EltTy == Type::Int32Ty ||
        EltTy == Type::Int64Ty || 
        EltTy == Type::FloatTy ||
        EltTy == Type::DoubleTy ||
        isa<PointerType>(EltTy)) {
      Elts.push_back(EltTy);
      continue;
    }
    
    // TODO: Vectors are also ok to pass if they don't require extra alignment.
    // TODO: We can also pass structs like {i8, i32}.
    
    Elts.clear();
    return false;
  }
  
  return true;
}  

/* Target hook for llvm-abi.h. It returns true if an aggregate of the
   specified type should be passed in memory. */
bool llvm_x86_should_pass_aggregate_in_memory(tree TreeType, const Type *Ty) {
  enum machine_mode Mode = ix86_getNaturalModeForType(TreeType);
  HOST_WIDE_INT Bytes =
    (Mode == BLKmode) ? int_size_in_bytes(TreeType) : (int) GET_MODE_SIZE(Mode);

  // Zero sized array, struct, or class, not passed in memory.
  if (Bytes == 0)
    return false;

  if (TARGET_64BIT &&
      (Bytes == GET_MODE_SIZE(SImode) || Bytes == GET_MODE_SIZE(DImode))) {
    // 32-bit or 64-bit and all elements are integers, not passed in memory.
    const Type *Ty = ConvertType(TreeType);
    if (llvm_x86_is_all_integer_types(Ty))
      return false;
  }
  if (!TARGET_64BIT) {
    std::vector<const Type*> Elts;
    return !llvm_x86_32_should_pass_aggregate_in_mixed_regs(TreeType, Ty, Elts);
  }
  return llvm_x86_64_should_pass_aggregate_in_memory(TreeType, Mode);
}

/* llvm_x86_64_type_needs_multiple_regs - Return number of register classes
   need by TREETYPE. Return 0 if TREETYPE does not need multiple registers. 
   This helper routine is used to determine LLVM type of the function 
   arguments as well as return values.  */
static int
llvm_x86_64_type_needs_multiple_regs(enum machine_mode Mode, tree TreeType,
                                     enum x86_64_reg_class Class[MAX_CLASSES]) {
  int NumClasses = ix86_ClassifyArgument(Mode, TreeType, Class, 0);
  if (!NumClasses)
    return 0;

  for (int i = 0; i < NumClasses; ++i) {
    switch (Class[i]) {
    case X86_64_X87_CLASS:
    case X86_64_X87UP_CLASS:
    case X86_64_COMPLEX_X87_CLASS:
      return 0;
    case X86_64_NO_CLASS:
      return 0;
    default:
      break;
    }
  }
  return NumClasses;
}

/* Target hook for llvm-abi.h. It returns true if an aggregate of the
   specified type should be passed in a number of registers of mixed types.
   It also returns a vector of types that correspond to the registers used
   for parameter passing. This is only called for x86-64. */
bool
llvm_x86_64_should_pass_aggregate_in_mixed_regs(tree TreeType, const Type *Ty,
                                                std::vector<const Type*> &Elts){
  enum x86_64_reg_class Class[MAX_CLASSES];
  enum machine_mode Mode = ix86_getNaturalModeForType(TreeType);
  HOST_WIDE_INT Bytes =
    (Mode == BLKmode) ? int_size_in_bytes(TreeType) : (int) GET_MODE_SIZE(Mode);
  int NumClasses = llvm_x86_64_type_needs_multiple_regs(Mode, TreeType, Class);
  if (!NumClasses)
    return false;
 
  for (int i = 0; i < NumClasses; ++i) {
    switch (Class[i]) {
    case X86_64_INTEGER_CLASS:
    case X86_64_INTEGERSI_CLASS:
      Elts.push_back(Type::Int64Ty);
      Bytes -= 8;
      break;
    case X86_64_SSE_CLASS:
      // If it's a SSE class argument, then one of the followings are possible:
      // 1. 1 x SSE, size is 8: 1 x Double.
      // 2. 1 x SSE + 1 x SSEUP, size is 16: 1 x <4 x i32>, <4 x f32>,
      //                                         <2 x i64>, or <2 x f64>.
      // 3. 1 x SSE + 1 x SSESF, size is 12: 1 x Double, 1 x Float.
      // 4. 2 x SSE, size is 16: 2 x Double.
      if ((NumClasses-i) == 1) {
        if (Bytes == 8) {
          Elts.push_back(Type::DoubleTy);
          Bytes -= 8;
        } else
          assert(0 && "Not yet handled!");
      } else if ((NumClasses-i) == 2) {
        if (Class[i+1] == X86_64_SSEUP_CLASS) {
          const Type *Ty = ConvertType(TreeType);
          if (const StructType *STy = dyn_cast<StructType>(Ty))
            // Look pass the struct wrapper.
            if (STy->getNumElements() == 1)
              Ty = STy->getElementType(0);
          if (const VectorType *VTy = dyn_cast<VectorType>(Ty)) {
            if (VTy->getNumElements() == 2) {
              if (VTy->getElementType()->isInteger())
                Elts.push_back(VectorType::get(Type::Int64Ty, 2));
              else
                Elts.push_back(VectorType::get(Type::DoubleTy, 2));
              Bytes -= 8;
            } else {
              assert(VTy->getNumElements() == 4);
              if (VTy->getElementType()->isInteger())
                Elts.push_back(VectorType::get(Type::Int32Ty, 4));
              else
                Elts.push_back(VectorType::get(Type::FloatTy, 4));
              Bytes -= 4;
            }
          } else if (llvm_x86_is_all_integer_types(Ty)) {
            Elts.push_back(VectorType::get(Type::Int32Ty, 4));
            Bytes -= 4;
          } else {
            Elts.push_back(VectorType::get(Type::FloatTy, 4));
            Bytes -= 4;
          }
        } else if (Class[i+1] == X86_64_SSESF_CLASS) {
          assert(Bytes == 12 && "Not yet handled!");
          Elts.push_back(Type::DoubleTy);
          Elts.push_back(Type::FloatTy);
          Bytes -= 12;
        } else if (Class[i+1] == X86_64_SSE_CLASS) {
          Elts.push_back(Type::DoubleTy);
          Elts.push_back(Type::DoubleTy);
          Bytes -= 16;
        } else
          assert(0 && "Not yet handled!");
        ++i; // Already handled the next one.
      } else
        assert(0 && "Not yet handled!");
      break;
    case X86_64_SSESF_CLASS:
      Elts.push_back(Type::FloatTy);
      Bytes -= 4;
      break;
    case X86_64_SSEDF_CLASS:
      Elts.push_back(Type::DoubleTy);
      Bytes -= 8;
      break;
    case X86_64_X87_CLASS:
    case X86_64_X87UP_CLASS:
    case X86_64_COMPLEX_X87_CLASS:
      return false;
    case X86_64_NO_CLASS:
      return false;
    default: assert(0 && "Unexpected register class!");
    }
  }
  return true;
}

/* On Darwin, vectors which are not MMX nor SSE should be passed as integers. */
bool llvm_x86_should_pass_vector_in_integer_regs(tree type) {
  if (!TARGET_MACHO)
    return false;
  if (TREE_CODE(type) == VECTOR_TYPE &&
      TYPE_SIZE(type) &&
      TREE_CODE(TYPE_SIZE(type))==INTEGER_CST) {
    if (TREE_INT_CST_LOW(TYPE_SIZE(type))==64 && TARGET_MMX)
      return false;
    if (TREE_INT_CST_LOW(TYPE_SIZE(type))==128 && TARGET_SSE)
      return false;
  }
  return true;
}

/* The MMX vector v1i64 is returned in EAX and EDX on Darwin.  Communicate
    this by returning i64 here.  Likewise, (generic) vectors such as v2i16
    are returned in EAX.  */
tree llvm_x86_should_return_vector_as_scalar(tree type, bool isBuiltin) {
  if (TARGET_MACHO &&
      !isBuiltin &&
      !TARGET_64BIT &&
      TREE_CODE(type) == VECTOR_TYPE &&
      TYPE_SIZE(type) &&
      TREE_CODE(TYPE_SIZE(type))==INTEGER_CST) {
    if (TREE_INT_CST_LOW(TYPE_SIZE(type))==64 &&
        TYPE_VECTOR_SUBPARTS(type)==1)
      return uint64_type_node;
    if (TREE_INT_CST_LOW(TYPE_SIZE(type))==32)
      return uint32_type_node;
  }
  return 0;
}

/* MMX vectors v2i32, v4i16, v8i8, v2f32 are returned using sret on Darwin
   32-bit.  Vectors bigger than 128 are returned using sret.  */
bool llvm_x86_should_return_vector_as_shadow(tree type, bool isBuiltin) {
  if (TARGET_MACHO &&
    !isBuiltin &&
    !TARGET_64BIT &&
    TREE_CODE(type) == VECTOR_TYPE &&
    TYPE_SIZE(type) &&
    TREE_CODE(TYPE_SIZE(type))==INTEGER_CST) {
    if (TREE_INT_CST_LOW(TYPE_SIZE(type))==64 &&
       TYPE_VECTOR_SUBPARTS(type)>1)
      return true;
    if (TREE_INT_CST_LOW(TYPE_SIZE(type))>128)
      return true;
  }
  return false;
}

// llvm_suitable_multiple_ret_value_type - Return TRUE if return value 
// of type TY should be returned using multiple value return instruction.
static bool llvm_suitable_multiple_ret_value_type(const Type *Ty) {

  //NOTE: Work in progress. Do not open the flood gate yet.
  return false; 

  if (!TARGET_64BIT)
    return false;

  const StructType *STy = dyn_cast<StructType>(Ty);
  if (!STy)
    return false;
  
  unsigned NumElements = STy->getNumElements();

  bool useMultipleReturnVals = true;
  for (unsigned i = 0; i < NumElements; ++i) {
    const Type *T = STy->getElementType(i);
    
    if (T->isFirstClassType())
      continue;
    
    if (const ArrayType *ATy = dyn_cast<ArrayType>(T)) {
      // Allow { float f[4]; } but block { float f[10]; } or { char c[4]; }
      // FIXME :Double check '5'.
      if (ATy->getElementType()->isFloatingPoint()
          && ATy->getNumElements() < 5)
        continue;
    }
    return false;
  }
  
  return true;
}

// llvm_x86_scalar_type_for_struct_return - Return LLVM type if TY 
// can be returned as a scalar, otherwise return NULL.
const Type *llvm_x86_scalar_type_for_struct_return(const Type *Ty) {

  unsigned Size = getTargetData().getABITypeSize(Ty);
  if (Size == 0)
    return Type::VoidTy;
  else if (Size == 1)
    return Type::Int8Ty;
  else if (Size == 2)
    return Type::Int16Ty;
  else if (Size <= 4)
    return Type::Int32Ty;

  // Check if Ty should be returned using multiple value return instruction.
  if (llvm_suitable_multiple_ret_value_type(Ty))
    return NULL;

  if (Size <= 8)
    return Type::Int64Ty;
  else if (Size <= 16)
    return IntegerType::get(128);
  else if (Size <= 32)
    return IntegerType::get(256);

  return NULL;
}

// Return LLVM Type if TY can be returned as an aggregate, otherwise return NULL.
const Type *llvm_x86_aggr_type_for_struct_return(const Type *Ty) {

  if (!llvm_suitable_multiple_ret_value_type(Ty))
    return NULL;

  const StructType *STy = cast<StructType>(Ty);
  unsigned NumElements = STy->getNumElements();

  std::vector<const Type *> ElementTypes;
  for (unsigned i = 0; i < NumElements; ++i) {
    const Type *T = STy->getElementType(i);
    
    if (T->isFirstClassType()) {
      ElementTypes.push_back(T);
      continue;
    }
    
    const ArrayType *ATy = dyn_cast<ArrayType>(T);
    assert (ATy && "Unexpected struct element type!");
    assert (ATy->getElementType()->isFirstClassType() 
            && "Unexpected ArrayType element type!");
    
    unsigned size = ATy->getNumElements();
    if (ATy->getElementType()->isFloatingPoint()) {
      switch (size) {
      case 2:
        // use { <2 x float> } for struct { float[2]; }
        ElementTypes.push_back(VectorType::get(ATy->getElementType(), 2));
        break;
      case 3:
        // use { <2 x float>, float } for struct { float[3]; }
        ElementTypes.push_back(VectorType::get(ATy->getElementType(), 2));
        ElementTypes.push_back(ATy->getElementType());
        break;
      case 4:
        // use { <4  x float> } for struct { float[4]; }
        ElementTypes.push_back(VectorType::get(ATy->getElementType(), 4));
        break;
      default:
        assert (0 && "Unexpected floating point array size!");
      }
    } else {
      for (unsigned j = 0; j < size; ++j)
        ElementTypes.push_back(ATy->getElementType());
    }
  }
  
  return StructType::get(ElementTypes, STy->isPacked());
}


// llvm_x86_build_mrv_array_element - This is a helper function used by
// llvm_x6_build_multiple_return_value. This function builds a vector
// from the array fields of RetVal.
static Value *llvm_x86_build_mrv_array_element(const Type *STyFieldTy,
                                               Value *RetVal,
                                               unsigned FieldNo,
                                               unsigned ArraySize,
                                               IRBuilder &Builder) {
  llvm::Value *Idxs[3];
  Idxs[0] = ConstantInt::get(llvm::Type::Int32Ty, 0);
  Idxs[1] = ConstantInt::get(llvm::Type::Int32Ty, FieldNo);

  Value *R1 = UndefValue::get(STyFieldTy);

  for (unsigned i = 0; i < ArraySize; ++i) {
    Idxs[2] = ConstantInt::get(llvm::Type::Int32Ty, i);
    Value *GEP = Builder.CreateGEP(RetVal, Idxs, Idxs+3, "mrv_gep");
    Value *ElemVal = Builder.CreateLoad(GEP, "mrv");
    R1 = Builder.CreateInsertElement(R1, ElemVal, 
                                     ConstantInt::get(llvm::Type::Int32Ty, i), 
                                     "mrv");
  }
  return R1;
}

// llvm_x86_build_multiple_return_value - Function FN returns multiple value
// where RETVAL points to the aggregate being returned. Build a RETVALS vector
// of individual values from RETVAL aggregate. RETVALS will be used by 
// the client to build multiple value return instruction. 
void llvm_x86_build_multiple_return_value(Function *Fn, Value *RetVal,
                                          SmallVectorImpl <Value *> &RetVals,
                                          IRBuilder &Builder) {
  const StructType *STy = cast<StructType>(Fn->getReturnType());
  const PointerType *PTy = cast<PointerType>(RetVal->getType());
  const StructType *RetSTy = cast<StructType>(PTy->getElementType());

  // Walk RetSTy elements and populate RetVals vector. Note, STy and RetSTy
  // may not match. For example, when STy is { <2 x float> } the RetSTy is
  // { float[2]; }
  unsigned NumElements = RetSTy->getNumElements();
  for (unsigned RNO = 0, SNO = 0; RNO < NumElements; ++RNO, ++SNO) {
    const Type *ElemType = RetSTy->getElementType(RNO);
    if (ElemType->isFirstClassType()) {
      Value *GEP = Builder.CreateStructGEP(RetVal, RNO, "mrv_idx");
      Value *ElemVal = Builder.CreateLoad(GEP, "mrv");
      RetVals.push_back(ElemVal);
    } else {
      const ArrayType *ATy = cast<ArrayType>(ElemType);
      unsigned ArraySize = ATy->getNumElements();
      const VectorType *SElemTy = cast<VectorType>(STy->getElementType(SNO));
      unsigned Size = SElemTy->getNumElements();
      assert (ArraySize >= Size && "Invalid multiple return value type!");
      Value *R = llvm_x86_build_mrv_array_element(SElemTy, RetVal, RNO,
                                                  Size, Builder);
      RetVals.push_back(R);
      if (ArraySize > Size) {
        assert (ArraySize == Size + 1 && "Unable to build multiple return value!");
        // Build remaining values.
        const Type *NextTy = STy->getElementType(SNO + 1);
        if (NextTy->getTypeID() == Type::FloatTyID) {
          Value *Idxs[3];
          Idxs[0] = ConstantInt::get(llvm::Type::Int32Ty, 0);
          Idxs[1] = ConstantInt::get(llvm::Type::Int32Ty, RNO);
          Idxs[2] = ConstantInt::get(llvm::Type::Int32Ty, Size + 1);
          Value *GEP3 = Builder.CreateGEP(RetVal, Idxs, Idxs+3, "mrv_gep");
          Value *ElemVal3 = Builder.CreateLoad(GEP3, "mrv");
          RetVals.push_back(ElemVal3);
          SNO++;
        } else 
          assert ( 0 && "Unable to build multiple return value!");
      }
    }
  }
}

// llvm_x86_extract_mrv_array_element - Helper function that help extract 
// an array element from multiple return value.
//
// Here, SRC is returning multiple values. DEST's DESTFIELNO field is an array.
// Extract SRCFIELDNO's ELEMENO value and store it in DEST's FIELDNO field's 
// ELEMENTNO.
//
static void llvm_x86_extract_mrv_array_element(Value *Src, Value *Dest,
                                               unsigned SrcFieldNo, 
                                               unsigned SrcElemNo,
                                               unsigned DestFieldNo, 
                                               unsigned DestElemNo,
                                               IRBuilder &Builder,
                                               bool isVolatile) {
  GetResultInst *GR = Builder.CreateGetResult(Src, SrcFieldNo, "mrv_gr");
  const StructType *STy = cast<StructType>(Src->getType());
  llvm::Value *Idxs[3];
  Idxs[0] = ConstantInt::get(llvm::Type::Int32Ty, 0);
  Idxs[1] = ConstantInt::get(llvm::Type::Int32Ty, DestFieldNo);
  Idxs[2] = ConstantInt::get(llvm::Type::Int32Ty, DestElemNo);
  Value *GEP = Builder.CreateGEP(Dest, Idxs, Idxs+3, "mrv_gep");
  if (isa<VectorType>(STy->getElementType(SrcFieldNo))) {
    Value *ElemIndex = ConstantInt::get(Type::Int32Ty, SrcElemNo);
    Value *GRElem = Builder.CreateExtractElement(GR, ElemIndex, "mrv");
    Builder.CreateStore(GRElem, GEP, isVolatile);
  } else {
    Builder.CreateStore(GR, GEP, isVolatile);
  }
}

// llvm_x86_extract_multiple_return_value - Extract multiple values returned
// by SRC and store them in DEST. It is expected thaty SRC and
// DEST types are StructType, but they may not match.
void llvm_x86_extract_multiple_return_value(Value *Src, Value *Dest,
                                            bool isVolatile,
                                            IRBuilder &Builder) {
  
  const StructType *STy = cast<StructType>(Src->getType());
  unsigned NumElements = STy->getNumElements();

  const PointerType *PTy = cast<PointerType>(Dest->getType());
  const StructType *DestTy = cast<StructType>(PTy->getElementType());
  unsigned SNO = 0;
  unsigned DNO = 0;
  while (SNO < NumElements) {

    const Type *DestElemType = DestTy->getElementType(DNO);

    // Directly access first class values using getresult.
    if (DestElemType->isFirstClassType()) {
      Value *GEP = Builder.CreateStructGEP(Dest, DNO, "mrv_gep");
      GetResultInst *GR = Builder.CreateGetResult(Src, SNO, "mrv_gr");
      Builder.CreateStore(GR, GEP, isVolatile);
      ++DNO; ++SNO;
      continue;
    } 
    
    // Access array elements individually. Note, Src and Dest type may
    // not match. For example { <2 x float>, float } and { float[3]; }
    const ArrayType *ATy = cast<ArrayType>(DestElemType);
    unsigned ArraySize = ATy->getNumElements();
    for (unsigned di = 0, si = 0; di < ArraySize; ++di, ++si) {
      llvm_x86_extract_mrv_array_element(Src, Dest, SNO, si, DNO, di, 
                                         Builder, isVolatile);
      if (const VectorType *SElemTy = 
          dyn_cast<VectorType>(STy->getElementType(SNO))) {
        unsigned NumVElem = SElemTy->getNumElements();
        if (NumVElem == si + 1) {
          // If extracted all elments from current Src field then 
          // move to next field.
          si = 0;
          ++SNO;
        }
      } else {
        // Copied content from current source field, so move to next field.
        ++SNO;
      }
    }
    // Process next DNO element.
    ++DNO;
  }
}
/* LLVM LOCAL end (ENTIRE FILE!)  */
