////////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Project:  Embedded Learning Library (ELL)
//  File:     IREmitter.cpp (emitters)
//  Authors:  Umesh Madan
//
////////////////////////////////////////////////////////////////////////////////////////////////////

#include "IREmitter.h"
#include "EmitterException.h"
#include "LLVMUtilities.h"

// utilities
#include "Logger.h"

// llvm
#include <llvm/IR/InstrTypes.h>
#include <llvm/Support/raw_os_ostream.h>

// stl
#include <algorithm>
#include <sstream>

namespace ell
{
namespace emitters
{
    using namespace logging;

    //
    // Helpful utility functions in anonymous namespace
    //
    namespace
    {
        void ValidateArguments(LLVMFunction f, const IRValueList& args)
        {
            llvm::FunctionType* fType = f->getFunctionType();
            auto numArgs = args.size();
            if (f->isVarArg()) // If the function is a varargs function, don't check the extra varargs parameters
            {
                numArgs = fType->getNumParams();
            }

            if (numArgs > args.size())
            {
                throw EmitterException(EmitterError::badFunctionArguments, "wrong number of arguments for function");
            }

            for (size_t index = 0; index < numArgs; ++index)
            {
                auto paramType = fType->getParamType(index);
                auto argType = args[index]->getType();
                if (paramType == nullptr)
                {
                    throw EmitterException(EmitterError::badFunctionDefinition, "function has null parameter type");
                }
                if (argType == nullptr)
                {
                    throw EmitterException(EmitterError::badFunctionArguments, "function being called with null parameter type");
                }
                if (paramType != argType)
                {
                    llvm::raw_os_ostream out(Log());
                    out << "Wanted ";
                    paramType->print(out);
                    out << ", got ";
                    argType->print(out);
                    out.flush();
                    throw EmitterException(EmitterError::badFunctionArguments, "mismatched types for function.");
                }
            }
        }
    };

    //
    // IREmitter implementation
    //
    IREmitter::IREmitter(llvm::LLVMContext& context)
        : _llvmContext(context), _irBuilder(context)
    {
    }

    LLVMType IREmitter::Type(VariableType type)
    {
        switch (type)
        {
        case VariableType::Void:
            return GetVariableType(type);
        case VariableType::VoidPointer:
            return GetVariableType(VariableType::Void)->getPointerTo();
        case VariableType::Byte:
            return GetVariableType(type);
        case VariableType::BytePointer:
            return GetVariableType(VariableType::Byte)->getPointerTo();
        case VariableType::Int16:
            return GetVariableType(type);
        case VariableType::Int16Pointer:
            return GetVariableType(VariableType::Int16)->getPointerTo();
        case VariableType::Int32:
            return GetVariableType(type);
        case VariableType::Int32Pointer:
            return GetVariableType(VariableType::Int32)->getPointerTo();
        case VariableType::Int64:
            return GetVariableType(type);
        case VariableType::Int64Pointer:
            return GetVariableType(VariableType::Int64)->getPointerTo();
        case VariableType::Float:
            return GetVariableType(type);
        case VariableType::FloatPointer:
            return GetVariableType(VariableType::Float)->getPointerTo();
        case VariableType::Double:
            return GetVariableType(type);
        case VariableType::DoublePointer:
            return GetVariableType(VariableType::Double)->getPointerTo();
        case VariableType::Char8:
            return GetVariableType(type);
        case VariableType::Char8Pointer:
            return GetVariableType(VariableType::Char8)->getPointerTo();
        default:
            throw EmitterException(EmitterError::valueTypeNotSupported);
        }
    }

    llvm::PointerType* IREmitter::PointerType(VariableType type)
    {
        return Type(type)->getPointerTo();
    }

    llvm::PointerType* IREmitter::PointerType(LLVMType type)
    {
        return type->getPointerTo();
    }

    llvm::ArrayType* IREmitter::ArrayType(VariableType type, size_t size)
    {
        return llvm::ArrayType::get(Type(type), size);
    }

    llvm::ArrayType* IREmitter::ArrayType(VariableType type, size_t rows, size_t columns)
    {
        auto rowType = llvm::ArrayType::get(Type(type), columns);
        return llvm::ArrayType::get(rowType, rows);
    }

    llvm::ArrayType* IREmitter::ArrayType(LLVMType type, size_t size)
    {
        return llvm::ArrayType::get(type, size);
    }

    llvm::ArrayType* IREmitter::ArrayType(LLVMType type, size_t rows, size_t columns)
    {
        auto rowType = llvm::ArrayType::get(type, columns);
        return llvm::ArrayType::get(rowType, rows);
    }

    llvm::VectorType* IREmitter::VectorType(VariableType type, size_t size)
    {
        return llvm::VectorType::get(Type(type), size);
    }

    llvm::VectorType* IREmitter::VectorType(LLVMType type, size_t size)
    {
        return llvm::VectorType::get(type, size);
    }

    llvm::Constant* IREmitter::Literal(const bool value)
    {
        return Integer(VariableType::Byte, value ? 1 : 0);
    }

    llvm::Constant* IREmitter::Literal(const uint8_t value)
    {
        return Integer(VariableType::Byte, value);
    }

    llvm::Constant* IREmitter::Literal(const short value)
    {
        return Integer(VariableType::Int16, value);
    }

    llvm::Constant* IREmitter::Literal(const int value)
    {
        return Integer(VariableType::Int32, value);
    }

    llvm::Constant* IREmitter::Literal(const int64_t value)
    {
        return Integer(VariableType::Int64, value);
    }

    llvm::Constant* IREmitter::Literal(const float value)
    {
        return llvm::ConstantFP::get(_llvmContext, llvm::APFloat(value));
    }

    llvm::Constant* IREmitter::Literal(const double value)
    {
        return llvm::ConstantFP::get(_llvmContext, llvm::APFloat(value));
    }

    LLVMValue IREmitter::Literal(const char* pValue)
    {
        assert(pValue != nullptr);
        std::string str(pValue);
        return Literal(str);
    }

    LLVMValue IREmitter::Literal(const std::string& value)
    {
        LLVMValue literal = _stringLiterals.Get(value);
        if (literal == nullptr)
        {
            literal = _irBuilder.CreateGlobalStringPtr(value);
            _stringLiterals.Add(value, literal);
        }
        return literal;
    }

    llvm::Constant* IREmitter::Literal(const std::vector<uint8_t>& value)
    {
        return llvm::ConstantDataArray::get(_llvmContext, value);
    }

    llvm::Constant* IREmitter::Literal(const std::vector<float>& value)
    {
        return llvm::ConstantDataArray::get(_llvmContext, value);
    }

    llvm::Constant* IREmitter::Literal(const std::vector<double>& value)
    {
        return llvm::ConstantDataArray::get(_llvmContext, value);
    }

    llvm::Constant* IREmitter::Literal(const std::vector<int>& value)
    {
        return llvm::ConstantDataArray::get(_llvmContext, reinterpret_cast<const std::vector<uint32_t>&>(value));
    }

    llvm::Constant* IREmitter::Literal(const std::vector<int64_t>& value)
    {
        return llvm::ConstantDataArray::get(_llvmContext, reinterpret_cast<const std::vector<uint64_t>&>(value));
    }

    LLVMValue IREmitter::Literal(const std::string& name, const std::string& value)
    {
        return _irBuilder.CreateGlobalStringPtr(value, name);
    }

    llvm::Constant* IREmitter::Zero(VariableType type)
    {
        switch (type)
        {
        case VariableType::Byte:
        case VariableType::Int16:
        case VariableType::Int32:
        case VariableType::Int64:
            return Integer(type, 0);
        case VariableType::Float:
        case VariableType::Double:
            return Literal(0.0);
        default:
            break;
        }
        return nullptr;
    }

    llvm::Constant* IREmitter::Zero(LLVMType type)
    {
        return llvm::Constant::getNullValue(type);
    }

    llvm::Constant* IREmitter::True()
    {
        return Literal(true);
    }

    llvm::Constant* IREmitter::False()
    {
        return Literal(false);
    }

    llvm::Constant* IREmitter::TrueBit()
    {
        return llvm::ConstantInt::getTrue(_llvmContext);
    }

    llvm::Constant* IREmitter::FalseBit()
    {
        return llvm::ConstantInt::getFalse(_llvmContext);
    }

    llvm::ConstantPointerNull* IREmitter::NullPointer(llvm::PointerType* pointerType)
    {
        return llvm::ConstantPointerNull::get(pointerType);
    }

    //
    // Typecast
    //

    // bool -> ?
    template <>
    LLVMValue IREmitter::CastValue<bool, bool>(LLVMValue pValue)
    {
        return CastInt(pValue, VariableType::Byte, false);
    }

    template <>
    LLVMValue IREmitter::CastValue<bool, int>(LLVMValue pValue)
    {
        return CastInt(pValue, VariableType::Int32, false);
    }

    template <>
    LLVMValue IREmitter::CastValue<bool, int64_t>(LLVMValue pValue)
    {
        return CastInt(pValue, VariableType::Int64, false);
    }

    template <>
    LLVMValue IREmitter::CastValue<bool, float>(LLVMValue pValue)
    {
        return CastIntToFloat(pValue, VariableType::Float, false);
    }

    template <>
    LLVMValue IREmitter::CastValue<bool, double>(LLVMValue pValue)
    {
        return CastIntToFloat(pValue, VariableType::Double, false);
    }

    // int -> ?
    template <>
    LLVMValue IREmitter::CastValue<int, bool>(LLVMValue pValue)
    {
        return CastInt(pValue, VariableType::Byte, true);
    }

    template <>
    LLVMValue IREmitter::CastValue<int, int>(LLVMValue pValue)
    {
        return CastInt(pValue, VariableType::Int32, true);
    }

    template <>
    LLVMValue IREmitter::CastValue<int, int64_t>(LLVMValue pValue)
    {
        return CastInt(pValue, VariableType::Int64, true);
    }

    template <>
    LLVMValue IREmitter::CastValue<int, float>(LLVMValue pValue)
    {
        return CastIntToFloat(pValue, VariableType::Float, true);
    }

    template <>
    LLVMValue IREmitter::CastValue<int, double>(LLVMValue pValue)
    {
        return CastIntToFloat(pValue, VariableType::Double, true);
    }

    template <>
    LLVMValue IREmitter::CastValue<int64_t, bool>(LLVMValue pValue)
    {
        return CastInt(pValue, VariableType::Byte, true);
    }

    template <>
    LLVMValue IREmitter::CastValue<int64_t, int>(LLVMValue pValue)
    {
        return CastInt(pValue, VariableType::Int32, true);
    }

    template <>
    LLVMValue IREmitter::CastValue<int64_t, int64_t>(LLVMValue pValue)
    {
        return CastInt(pValue, VariableType::Int64, true);
    }

    template <>
    LLVMValue IREmitter::CastValue<int64_t, float>(LLVMValue pValue)
    {
        return CastIntToFloat(pValue, VariableType::Float, true);
    }

    template <>
    LLVMValue IREmitter::CastValue<int64_t, double>(LLVMValue pValue)
    {
        return CastIntToFloat(pValue, VariableType::Double, true);
    }

    // float -> ?
    template <>
    LLVMValue IREmitter::CastValue<float, bool>(LLVMValue pValue)
    {
        return CastFloatToInt(pValue, VariableType::Byte);
    }

    template <>
    LLVMValue IREmitter::CastValue<float, int>(LLVMValue pValue)
    {
        return CastFloatToInt(pValue, VariableType::Int32);
    }

    template <>
    LLVMValue IREmitter::CastValue<float, int64_t>(LLVMValue pValue)
    {
        return CastFloatToInt(pValue, VariableType::Int64);
    }

    template <>
    LLVMValue IREmitter::CastValue<float, float>(LLVMValue pValue)
    {
        return _irBuilder.CreateFPCast(pValue, Type(VariableType::Float));
    }

    template <>
    LLVMValue IREmitter::CastValue<float, double>(LLVMValue pValue)
    {
        return _irBuilder.CreateFPCast(pValue, Type(VariableType::Double));
    }

    // double -> ?
    template <>
    LLVMValue IREmitter::CastValue<double, bool>(LLVMValue pValue)
    {
        return CastFloatToInt(pValue, VariableType::Byte);
    }

    template <>
    LLVMValue IREmitter::CastValue<double, int>(LLVMValue pValue)
    {
        return CastFloatToInt(pValue, VariableType::Int32);
    }

    template <>
    LLVMValue IREmitter::CastValue<double, int64_t>(LLVMValue pValue)
    {
        return CastFloatToInt(pValue, VariableType::Int64);
    }

    template <>
    LLVMValue IREmitter::CastValue<double, float>(LLVMValue pValue)
    {
        return _irBuilder.CreateFPCast(pValue, Type(VariableType::Float));
    }

    template <>
    LLVMValue IREmitter::CastValue<double, double>(LLVMValue pValue)
    {
        return _irBuilder.CreateFPCast(pValue, Type(VariableType::Double));
    }

    LLVMValue IREmitter::BitCast(LLVMValue pValue, VariableType destinationType)
    {
        assert(pValue != nullptr);
        return BitCast(pValue, Type(destinationType));
    }

    LLVMValue IREmitter::BitCast(LLVMValue pValue, LLVMType destinationType)
    {
        assert(pValue != nullptr);
        auto valueType = pValue->getType();

        // We can't do a bit cast if the types don't have the same size
        if (!llvm::CastInst::isBitCastable(valueType, destinationType))
        {
            auto currentBlock = _irBuilder.GetInsertBlock();
            assert(currentBlock);
            auto dataLayout = currentBlock->getModule()->getDataLayout();
            auto size1 = dataLayout.getTypeStoreSizeInBits(valueType);
            auto size2 = dataLayout.getTypeStoreSizeInBits(destinationType);
            if (size1 != 0 && size2 != 0)
            {
                // unequal sizes: need to bitcast to an int, truncate, then bitcast to destination type
                auto intType1 = llvm::Type::getIntNTy(_llvmContext, size1);
                auto intType2 = llvm::Type::getIntNTy(_llvmContext, size2);

                auto intValue1 = _irBuilder.CreateBitOrPointerCast(pValue, intType1);
                auto resizedIntValue = _irBuilder.CreateZExtOrTrunc(intValue1, intType2);
                return _irBuilder.CreateBitOrPointerCast(resizedIntValue, destinationType);
            }
            else
            {
                throw EmitterException(EmitterError::castNotSupported, "Bad cast");
            }
        }
        else
        {
            return _irBuilder.CreateBitOrPointerCast(pValue, destinationType);
        }
    }

    LLVMValue IREmitter::CastPointer(LLVMValue pValue, LLVMType destinationType)
    {
        assert(pValue != nullptr);
        return _irBuilder.CreatePointerCast(pValue, destinationType);
    }

    LLVMValue IREmitter::CastPointer(LLVMValue pValue, VariableType destinationType)
    {
        return CastPointer(pValue, Type(destinationType));
    }

    LLVMValue IREmitter::CastIntToPointer(LLVMValue pValue, LLVMType destinationType)
    {
        assert(pValue != nullptr);
        return _irBuilder.CreateIntToPtr(pValue, destinationType);
    }

    LLVMValue IREmitter::CastPointerToInt(LLVMValue pValue, LLVMType destinationType)
    {
        assert(pValue != nullptr);
        return _irBuilder.CreatePtrToInt(pValue, destinationType);
    }

    LLVMValue IREmitter::CastIntToFloat(LLVMValue pValue, VariableType destinationType, bool isSigned)
    {
        assert(pValue != nullptr);
        if (isSigned)
        {
            return _irBuilder.CreateSIToFP(pValue, Type(destinationType));
        }
        else
        {
            return _irBuilder.CreateUIToFP(pValue, Type(destinationType));
        }
    }

    LLVMValue IREmitter::CastFloatToInt(LLVMValue pValue, VariableType destinationType, bool isSigned)
    {
        assert(pValue != nullptr);

        auto type = Type(destinationType);
        if (!type->isIntegerTy())
        {
            throw EmitterException(EmitterError::notSupported);
        }

        if (isSigned)
        {
            return _irBuilder.CreateFPToSI(pValue, type);
        }
        else
        {
            return _irBuilder.CreateFPToUI(pValue, type);
        }
    }

    LLVMValue IREmitter::CastInt(LLVMValue pValue, VariableType destinationType, bool isSigned)
    {
        assert(pValue != nullptr);
        auto type = Type(destinationType);
        return _irBuilder.CreateIntCast(pValue, type, isSigned);
    }

    LLVMValue IREmitter::CastBool(LLVMValue pValue)
    {
        return CastInt(pValue, VariableType::Byte, false);
    }

    llvm::ReturnInst* IREmitter::ReturnVoid()
    {
        return _irBuilder.CreateRetVoid();
    }

    //
    // Return
    //
    llvm::ReturnInst* IREmitter::Return(LLVMValue pValue)
    {
        assert(pValue != nullptr);
        return _irBuilder.CreateRet(pValue);
    }

    //
    // Native LLVM operations / comparisons
    //
    LLVMValue IREmitter::UnaryOperation(const UnaryOperationType type, LLVMValue value, const std::string& variableName)
    {
        assert(value != nullptr);

        switch (type)
        {
        case UnaryOperationType::logicalNot:
            return _irBuilder.CreateNot(value, variableName);
        default:
            throw EmitterException(EmitterError::operatorTypeNotSupported, "Unsupported unary operator");
        }
    }

    LLVMValue IREmitter::BinaryOperation(const TypedOperator type, LLVMValue pLeftValue, LLVMValue pRightValue, const std::string& variableName)
    {
        assert(pLeftValue != nullptr);
        assert(pRightValue != nullptr);

        switch (type)
        {
        case TypedOperator::add:
            return _irBuilder.CreateAdd(pLeftValue, pRightValue, variableName);
        case TypedOperator::subtract:
            return _irBuilder.CreateSub(pLeftValue, pRightValue, variableName);
        case TypedOperator::multiply:
            return _irBuilder.CreateMul(pLeftValue, pRightValue, variableName);
        case TypedOperator::divideSigned:
            return _irBuilder.CreateSDiv(pLeftValue, pRightValue, variableName);
        case TypedOperator::moduloSigned:
            return _irBuilder.CreateSRem(pLeftValue, pRightValue, variableName);
        case TypedOperator::addFloat:
            return _irBuilder.CreateFAdd(pLeftValue, pRightValue, variableName);
        case TypedOperator::subtractFloat:
            return _irBuilder.CreateFSub(pLeftValue, pRightValue, variableName);
        case TypedOperator::multiplyFloat:
            return _irBuilder.CreateFMul(pLeftValue, pRightValue, variableName);
        case TypedOperator::divideFloat:
            return _irBuilder.CreateFDiv(pLeftValue, pRightValue, variableName);
        case TypedOperator::logicalAnd:
            return _irBuilder.CreateAnd(pLeftValue, pRightValue, variableName);
        case TypedOperator::logicalOr:
            return _irBuilder.CreateOr(pLeftValue, pRightValue, variableName);
        case TypedOperator::logicalXor:
            return _irBuilder.CreateXor(pLeftValue, pRightValue, variableName);
        case TypedOperator::shiftLeft:
            return _irBuilder.CreateShl(pLeftValue, pRightValue, variableName);
        case TypedOperator::logicalShiftRight:
            return _irBuilder.CreateLShr(pLeftValue, pRightValue, variableName);
        case TypedOperator::arithmeticShiftRight:
            return _irBuilder.CreateAShr(pLeftValue, pRightValue, variableName);
        default:
            throw EmitterException(EmitterError::operatorTypeNotSupported);
        }
    }

    LLVMValue IREmitter::Comparison(const TypedComparison type, LLVMValue pLeftValue, LLVMValue pRightValue)
    {
        assert(pLeftValue != nullptr);
        assert(pRightValue != nullptr);

        switch (type)
        {
        case TypedComparison::equals:
            return _irBuilder.CreateICmpEQ(pLeftValue, pRightValue);
        case TypedComparison::lessThan:
            return _irBuilder.CreateICmpSLT(pLeftValue, pRightValue);
        case TypedComparison::lessThanOrEquals:
            return _irBuilder.CreateICmpSLE(pLeftValue, pRightValue);
        case TypedComparison::greaterThan:
            return _irBuilder.CreateICmpSGT(pLeftValue, pRightValue);
        case TypedComparison::greaterThanOrEquals:
            return _irBuilder.CreateICmpSGE(pLeftValue, pRightValue);
        case TypedComparison::notEquals:
            return _irBuilder.CreateICmpNE(pLeftValue, pRightValue);
        case TypedComparison::equalsFloat:
            return _irBuilder.CreateFCmpOEQ(pLeftValue, pRightValue);
        case TypedComparison::lessThanFloat:
            return _irBuilder.CreateFCmpOLT(pLeftValue, pRightValue);
        case TypedComparison::lessThanOrEqualsFloat:
            return _irBuilder.CreateFCmpOLE(pLeftValue, pRightValue);
        case TypedComparison::greaterThanFloat:
            return _irBuilder.CreateFCmpOGT(pLeftValue, pRightValue);
        case TypedComparison::greaterThanOrEqualsFloat:
            return _irBuilder.CreateFCmpOGE(pLeftValue, pRightValue);
        case TypedComparison::notEqualsFloat:
            return _irBuilder.CreateFCmpONE(pLeftValue, pRightValue);
        default:
            throw EmitterException(EmitterError::comparisonTypeNotSupported);
        }
    }

    LLVMValue IREmitter::Comparison(LLVMValue pValue, bool testValue)
    {
        assert(pValue != nullptr);

        LLVMValue pTestValue = pValue;
        auto pValueType = pValue->getType();
        if (pValueType->isIntegerTy())
        {
            // We use bytes as booleans
            pTestValue = CastInt(pValue, VariableType::Byte, false);
        }
        return Comparison(TypedComparison::equals, pTestValue, testValue ? True() : False());
    }

    //
    // Select
    //
    LLVMValue IREmitter::Select(LLVMValue pCmp, LLVMValue pTrueValue, LLVMValue pFalseValue)
    {
        assert(pCmp != nullptr);
        assert(pTrueValue != nullptr);
        assert(pFalseValue != nullptr);

        return _irBuilder.CreateSelect(pCmp, pTrueValue, pFalseValue);
    }

    //
    // CreateModule
    //
    std::unique_ptr<llvm::Module> IREmitter::CreateModule(const std::string& name)
    {
        return std::make_unique<llvm::Module>(name, _llvmContext);
    }

    //
    // Functions
    //
    LLVMFunction IREmitter::DeclareFunction(llvm::Module* pModule, const std::string& name)
    {
        assert(pModule != nullptr);
        auto functionType = llvm::FunctionType::get(_irBuilder.getVoidTy(), false);
        return DeclareFunction(pModule, name, functionType);
    }

    LLVMFunction IREmitter::DeclareFunction(llvm::Module* pModule, const std::string& name, VariableType returnType)
    {
        auto functionType = llvm::FunctionType::get(Type(returnType), false);
        return DeclareFunction(pModule, name, functionType);
    }

    LLVMFunction IREmitter::DeclareFunction(llvm::Module* pModule, const std::string& name, VariableType returnType, const VariableTypeList& arguments)
    {
        auto types = GetLLVMTypes(arguments);
        auto functionType = llvm::FunctionType::get(Type(returnType), types, false);
        return DeclareFunction(pModule, name, functionType);
    }

    LLVMFunction IREmitter::DeclareFunction(llvm::Module* pModule, const std::string& name, VariableType returnType, const NamedVariableTypeList& arguments)
    {
        auto types = BindArgumentTypes(arguments);
        auto functionType = llvm::FunctionType::get(Type(returnType), types, false);
        return DeclareFunction(pModule, name, functionType);
    }

    LLVMFunction IREmitter::DeclareFunction(llvm::Module* pModule, const std::string& name, llvm::FunctionType* type)
    {
        assert(pModule != nullptr);
        return static_cast<LLVMFunction>(pModule->getOrInsertFunction(name, type));
    }

    LLVMFunction IREmitter::Function(llvm::Module* pModule, const std::string& name, VariableType returnType, llvm::Function::LinkageTypes linkage, const VariableTypeList* pArguments)
    {
        assert(pModule != nullptr);

        llvm::FunctionType* pFunctionType = nullptr;
        if (pArguments != nullptr)
        {
            auto types = GetLLVMTypes(*pArguments);
            pFunctionType = llvm::FunctionType::get(Type(returnType), types, false);
        }
        else
        {
            pFunctionType = llvm::FunctionType::get(Type(returnType), false);
        }
        return CreateFunction(pModule, name, linkage, pFunctionType);
    }

    LLVMFunction IREmitter::Function(llvm::Module* pModule, const std::string& name, VariableType returnType, llvm::Function::LinkageTypes linkage, const NamedVariableTypeList& arguments)
    {
        assert(pModule != nullptr);

        auto types = BindArgumentTypes(arguments);
        llvm::FunctionType* pFunctionType = llvm::FunctionType::get(Type(returnType), types, false);
        LLVMFunction pFunction = CreateFunction(pModule, name, linkage, pFunctionType);
        BindArgumentNames(pFunction, arguments);
        return pFunction;
    }

    LLVMFunction IREmitter::Function(llvm::Module* pModule, const std::string& name, LLVMType returnType, llvm::Function::LinkageTypes linkage, const NamedVariableTypeList& arguments)
    {
        assert(pModule != nullptr);

        auto types = BindArgumentTypes(arguments);
        LLVMFunction pFunction = Function(pModule, name, returnType, linkage, types);
        BindArgumentNames(pFunction, arguments);
        return pFunction;
    }

    LLVMFunction IREmitter::Function(llvm::Module* pModule, const std::string& name, LLVMType returnType, llvm::Function::LinkageTypes linkage, const std::vector<LLVMType>& argTypes)
    {
        assert(pModule != nullptr);

        auto functionType = llvm::FunctionType::get(returnType, argTypes, false);
        return CreateFunction(pModule, name, linkage, functionType);
    }

    LLVMFunction IREmitter::Function(llvm::Module* pModule, const std::string& name, LLVMType returnType, llvm::Function::LinkageTypes linkage, const NamedLLVMTypeList& arguments)
    {
        assert(pModule != nullptr);

        auto types = BindArgumentTypes(arguments);
        LLVMFunction pFunction = Function(pModule, name, returnType, linkage, types);
        BindArgumentNames(pFunction, arguments);
        return pFunction;
    }

    //
    // Blocks
    //
    llvm::BasicBlock* IREmitter::Block(LLVMFunction pFunction, const std::string& label)
    {
        assert(pFunction != nullptr);
        return llvm::BasicBlock::Create(_llvmContext, label, pFunction);
    }

    llvm::BasicBlock* IREmitter::BlockBefore(LLVMFunction pFunction, llvm::BasicBlock* pBlock, const std::string& label)
    {
        assert(pFunction != nullptr);
        assert(pBlock != nullptr);

        llvm::BasicBlock* pNewBlock = Block(label);
        BlockBefore(pFunction, pBlock, pNewBlock);
        return pNewBlock;
    }

    llvm::BasicBlock* IREmitter::BlockBefore(LLVMFunction pFunction, llvm::BasicBlock* pBlock, llvm::BasicBlock* pNewBlock)
    {
        assert(pFunction != nullptr);
        assert(pBlock != nullptr);
        assert(pNewBlock != nullptr);
        pFunction->getBasicBlockList().insert(pBlock->getIterator(), pNewBlock);
        return pNewBlock;
    }

    llvm::BasicBlock* IREmitter::BlockAfter(LLVMFunction pFunction, llvm::BasicBlock* pBlock, const std::string& label)
    {
        assert(pFunction != nullptr);
        assert(pBlock != nullptr);

        llvm::BasicBlock* pNewBlock = Block(label);
        BlockAfter(pFunction, pBlock, pNewBlock);
        return pNewBlock;
    }

    llvm::BasicBlock* IREmitter::BlockAfter(LLVMFunction pFunction, llvm::BasicBlock* pBlock, llvm::BasicBlock* pNewBlock)
    {
        assert(pFunction != nullptr);
        assert(pBlock != nullptr);
        assert(pNewBlock != nullptr);
        pFunction->getBasicBlockList().insertAfter(pBlock->getIterator(), pNewBlock);
        return pNewBlock;
    }

    llvm::BasicBlock* IREmitter::Block(const std::string& label)
    {
        return llvm::BasicBlock::Create(_llvmContext, label);
    }

    void IREmitter::SetCurrentBlock(llvm::BasicBlock* pBlock)
    {
        // assert(pBlock != nullptr);
        if (pBlock != nullptr)
        {
            _irBuilder.SetInsertPoint(pBlock);
        }
    }

    void IREmitter::SetCurrentInsertPoint(llvm::IRBuilder<>::InsertPoint pos)
    {
        _irBuilder.restoreIP(pos);
    }

    void IREmitter::SetCurrentInsertPoint(llvm::Instruction* pos)
    {
        _irBuilder.SetInsertPoint(pos);
    }

    //
    // Calling functions
    //
    llvm::CallInst* IREmitter::Call(LLVMFunction pFunction)
    {
        assert(pFunction != nullptr);
        return _irBuilder.CreateCall(pFunction, llvm::None);
    }

    llvm::CallInst* IREmitter::Call(LLVMFunction pFunction, LLVMValue pArgument)
    {
        assert(pFunction != nullptr);
        return _irBuilder.CreateCall(pFunction, pArgument);
    }

    llvm::CallInst* IREmitter::Call(LLVMFunction pFunction, const IRValueList& arguments)
    {
        assert(pFunction != nullptr);
        ValidateArguments(pFunction, arguments);
        return _irBuilder.CreateCall(pFunction, arguments);
    }

    // Intrinsics / library functions
    llvm::CallInst* IREmitter::MemoryMove(LLVMValue pSource, LLVMValue pDestination, LLVMValue pCountBytes)
    {
        assert(pSource != nullptr);
        assert(pDestination != nullptr);
        assert(pCountBytes != nullptr);
        return _irBuilder.CreateMemMove(pDestination, pSource, pCountBytes, 0, true);
    }

    llvm::CallInst* IREmitter::MemoryCopy(LLVMValue pSource, LLVMValue pDestination, LLVMValue pCountBytes)
    {
        assert(pSource != nullptr);
        assert(pDestination != nullptr);
        assert(pCountBytes != nullptr);
        return _irBuilder.CreateMemCpy(pDestination, pSource, pCountBytes, 0, true);
    }

    llvm::CallInst* IREmitter::MemorySet(LLVMValue pDestination, LLVMValue value, LLVMValue size)
    {
        assert(pDestination != nullptr);
        assert(value != nullptr);
        return _irBuilder.CreateMemSet(pDestination, value, size, 0, true);
    }

    LLVMFunction IREmitter::GetIntrinsic(llvm::Module* pModule, llvm::Intrinsic::ID id, const VariableTypeList& arguments)
    {
        assert(pModule != nullptr);
        auto types = GetLLVMTypes(arguments);
        return llvm::Intrinsic::getDeclaration(pModule, id, types);
    }

    LLVMFunction IREmitter::GetIntrinsic(llvm::Module* pModule, llvm::Intrinsic::ID id, const LLVMTypeList& arguments)
    {
        assert(pModule != nullptr);
        return llvm::Intrinsic::getDeclaration(pModule, id, arguments);
    }

    llvm::PHINode* IREmitter::Phi(VariableType type, LLVMValue pLeftValue, llvm::BasicBlock* pLeftBlock, LLVMValue pRightValue, llvm::BasicBlock* pRightBlock)
    {
        assert(pLeftBlock != nullptr);
        assert(pRightBlock != nullptr);
        assert(pRightValue != nullptr);
        assert(pRightBlock != nullptr);

        llvm::PHINode* phi = _irBuilder.CreatePHI(Type(type), 2);
        phi->addIncoming(pLeftValue, pLeftBlock);
        phi->addIncoming(pRightValue, pRightBlock);
        return phi;
    }

    LLVMValue IREmitter::PointerOffset(LLVMValue pArray, LLVMValue pOffset, const std::string& name)
    {
        assert(pArray != nullptr);
        assert(pOffset != nullptr);
        return _irBuilder.CreateGEP(pArray, pOffset, name);
    }

    LLVMValue IREmitter::DereferenceGlobalPointer(LLVMValue pArray)
    {
        assert(pArray != nullptr);
        LLVMValue derefArguments[1]{
            Zero()
        };

        return _irBuilder.CreateGEP(pArray, derefArguments);
    }

    LLVMValue IREmitter::PointerOffset(llvm::GlobalVariable* pArray, LLVMValue pOffset)
    {
        assert(pArray != nullptr);
        assert(pOffset != nullptr);

        LLVMValue derefArguments[2]{
            Zero(),
            pOffset
        };

        return _irBuilder.CreateGEP(pArray, derefArguments);
    }

    // TODO: rename this to avoid clashes with other PointerOffset()
    LLVMValue IREmitter::PointerOffset(llvm::GlobalVariable* pArray, LLVMValue pOffset, LLVMValue pFieldOffset)
    {
        assert(pArray != nullptr);
        assert(pOffset != nullptr);
        assert(pFieldOffset != nullptr);

        LLVMValue derefArguments[3]{
            Zero(),
            pOffset,
            pFieldOffset
        };
        return _irBuilder.CreateInBoundsGEP(pArray->getValueType(), pArray, derefArguments);
    }

    LLVMValue IREmitter::PointerOffset(llvm::AllocaInst* pArray, LLVMValue pOffset, LLVMValue pFieldOffset)
    {
        assert(pArray != nullptr);
        assert(pOffset != nullptr);
        assert(pFieldOffset != nullptr);

        LLVMValue derefArguments[2]{
            pOffset,
            pFieldOffset
        };
        return _irBuilder.CreateInBoundsGEP(pArray, derefArguments);
    }

    LLVMValue IREmitter::GetStructFieldPointer(LLVMValue structPtr, size_t fieldIndex)
    {
        auto structPtrType = llvm::dyn_cast<llvm::PointerType>(structPtr->getType());
        assert(structPtrType && "Error: must pass pointer to GetStructFieldPointer");
        assert(structPtrType->getElementType()->isStructTy() && "Error: must pass pointer to a struct type to GetStructFieldPointer");
        return _irBuilder.CreateStructGEP(structPtrType->getElementType(), structPtr, fieldIndex);
    }

    LLVMValue IREmitter::ExtractStructField(LLVMValue structValue, size_t fieldIndex)
    {
        assert(structValue->getType()->isStructTy() && "Error: must pass a struct type to ExtractStructField");
        return _irBuilder.CreateExtractValue(structValue, {static_cast<unsigned int>(fieldIndex)});
    }

    llvm::LoadInst* IREmitter::Load(LLVMValue pPointer)
    {
        assert(pPointer != nullptr);
        return _irBuilder.CreateLoad(pPointer);
    }

    llvm::LoadInst* IREmitter::Load(LLVMValue pPointer, const std::string& name)
    {
        assert(pPointer != nullptr);
        return _irBuilder.CreateLoad(pPointer, name);
    }

    llvm::StoreInst* IREmitter::Store(LLVMValue pPointer, LLVMValue pValue)
    {
        assert(pPointer != nullptr);
        assert(pValue != nullptr);
        return _irBuilder.CreateStore(pValue, pPointer);
    }

    llvm::AllocaInst* IREmitter::StackAllocate(VariableType type)
    {
        return _irBuilder.CreateAlloca(Type(type), nullptr);
    }

    llvm::AllocaInst* IREmitter::StackAllocate(LLVMType type)
    {
        return _irBuilder.CreateAlloca(type, nullptr);
    }

    llvm::AllocaInst* IREmitter::StackAllocate(VariableType type, const std::string& name)
    {
        return _irBuilder.CreateAlloca(Type(type), nullptr, name);
    }

    llvm::AllocaInst* IREmitter::StackAllocate(LLVMType pType, const std::string& name)
    {
        assert(pType != nullptr);

        return _irBuilder.CreateAlloca(pType, nullptr, name);
    }

    llvm::AllocaInst* IREmitter::StackAllocate(VariableType type, size_t size)
    {
        return _irBuilder.CreateAlloca(Type(type), Literal(static_cast<int>(size)));
    }

    llvm::AllocaInst* IREmitter::StackAllocate(VariableType type, size_t rows, size_t columns)
    {
        auto rowType = ArrayType(Type(type), columns);
        return _irBuilder.CreateAlloca(rowType, Literal(static_cast<int>(rows)));
    }

    llvm::AllocaInst* IREmitter::StackAllocate(LLVMType type, size_t size)
    {
        return _irBuilder.CreateAlloca(type, Literal(static_cast<int>(size)));
    }

    llvm::AllocaInst* IREmitter::StackAllocate(LLVMType type, size_t rows, size_t columns)
    {
        auto rowType = ArrayType(type, columns);
        return _irBuilder.CreateAlloca(rowType, Literal(static_cast<int>(rows)));
    }

    llvm::BranchInst* IREmitter::Branch(LLVMValue pConditionValue, llvm::BasicBlock* pThenBlock, llvm::BasicBlock* pElseBlock)
    {
        assert(pConditionValue != nullptr);
        assert(pThenBlock != nullptr);
        assert(pElseBlock != nullptr);

        return _irBuilder.CreateCondBr(pConditionValue, pThenBlock, pElseBlock);
    }

    llvm::BranchInst* IREmitter::Branch(llvm::BasicBlock* pDestination)
    {
        assert(pDestination != nullptr);

        return _irBuilder.CreateBr(pDestination);
    }

    llvm::StructType* IREmitter::DeclareStruct(const std::string& name, const VariableTypeList& fields)
    {
        llvm::StructType* type = GetStruct(name);
        if (type != nullptr)
        {
            throw EmitterException(EmitterError::duplicateSymbol);
        }
        LLVMTypeList llvmFields;
        for (const auto& field : fields)
        {
            llvmFields.push_back(Type(field));
        }
        return DeclareStruct(name, llvmFields);
    }

    llvm::StructType* IREmitter::DeclareStruct(const std::string& name, const LLVMTypeList& fields)
    {
        llvm::StructType* type = GetStruct(name);
        if (type != nullptr)
        {
            throw EmitterException(EmitterError::duplicateSymbol);
        }
        auto structType = llvm::StructType::create(_llvmContext, fields, name);
        _structs[name] = structType;
        return structType;
    }

    llvm::StructType* IREmitter::DeclareStruct(const std::string& name, const NamedVariableTypeList& fields)
    {
        llvm::StructType* type = GetStruct(name);
        if (type != nullptr)
        {
            throw EmitterException(EmitterError::duplicateSymbol);
        }
        auto types = BindArgumentTypes(fields);
        auto structType = llvm::StructType::create(_llvmContext, types, name);
        _structs[name] = structType;
        return structType;
    }

    llvm::StructType* IREmitter::GetAnonymousStructType(const LLVMTypeList& fields, bool packed)
    {
        return llvm::StructType::get(_llvmContext, fields, packed);
    }

    llvm::StructType* IREmitter::GetStruct(const std::string& name)
    {
        return _structs[name];
    }

    LLVMType IREmitter::GetVariableType(VariableType type)
    {
        switch (type)
        {
        case VariableType::Void:
            return _irBuilder.getVoidTy();
        case VariableType::Byte:
            return _irBuilder.getInt8Ty();
        case VariableType::Int16:
            return _irBuilder.getInt16Ty();
        case VariableType::Int32:
            return _irBuilder.getInt32Ty();
        case VariableType::Int64:
            return _irBuilder.getInt64Ty();
        case VariableType::Float:
            return _irBuilder.getFloatTy();
        case VariableType::Double:
            return _irBuilder.getDoubleTy();
        case VariableType::Char8:
            return _irBuilder.getInt8Ty();
        default:
            throw EmitterException(EmitterError::valueTypeNotSupported);
        }
    }

    int IREmitter::SizeOf(VariableType type)
    {
        switch (type)
        {
        // Are these correct for the int types??
        case VariableType::Byte:
            return 8;
        case VariableType::Int16:
            return 16;
        case VariableType::Int32:
            return 32;
        case VariableType::Int64:
            return 64;
        case VariableType::Float:
            return 4;
        case VariableType::Double:
            return 8;
        case VariableType::Char8:
            return 8;
        default:
            throw EmitterException(EmitterError::valueTypeNotSupported);
        }
    }

    llvm::Constant* IREmitter::Integer(VariableType type, const size_t value)
    {
        return llvm::ConstantInt::get(_llvmContext, llvm::APInt(SizeOf(type), value, true));
    }

    LLVMTypeList IREmitter::GetLLVMTypes(const VariableTypeList& types)
    {
        LLVMTypeList llvmTypes;
        for (auto t : types)
        {
            llvmTypes.push_back(Type(t));
        }

        return llvmTypes;
    }

    std::vector<LLVMType> IREmitter::BindArgumentTypes(const NamedVariableTypeList& arguments)
    {
        std::vector<LLVMType> types(arguments.size());
        std::transform(arguments.begin(), arguments.end(), types.begin(), [this](NamedVariableType argument) { return Type(argument.second); });
        return types;
    }

    std::vector<LLVMType> IREmitter::BindArgumentTypes(const NamedLLVMTypeList& arguments)
    {
        std::vector<LLVMType> types(arguments.size());
        std::transform(arguments.begin(), arguments.end(), types.begin(), [](auto argument) { return argument.second; });
        return types;
    }

    LLVMFunction IREmitter::CreateFunction(llvm::Module* pModule, const std::string& name, llvm::Function::LinkageTypes linkage, llvm::FunctionType* pFunctionType)
    {
        LLVMFunction pFunction = llvm::Function::Create(pFunctionType, linkage, name, pModule);
        if (pFunction == nullptr)
        {
            throw EmitterException(EmitterError::functionNotFound);
        }
        return pFunction;
    }

    LLVMValue IREmitter::Zero()
    {
        if (_pZeroLiteral == nullptr)
        {
            _pZeroLiteral = Literal(0);
        }
        return _pZeroLiteral;
    }

    void DebugDump(llvm::Module* module)
    {
        module->print(llvm::errs(), nullptr, /*ShouldPreserveUseListOrder*/ false, /*IsForDebug*/ true);
        llvm::errs() << '\n';
    }

    void DebugDump(llvm::Type* type)
    {
        type->print(llvm::errs(), /*IsForDebug*/ true);
        llvm::errs() << '\n';
    }

    void DebugDump(llvm::Value* value)
    {
        value->print(llvm::errs(), /*IsForDebug*/ true);
        llvm::errs() << '\n';
    }

    void DebugDump(llvm::Function* function)
    {
        function->print(llvm::errs(), nullptr, false, /*IsForDebug*/ true);
        llvm::errs() << '\n';
    }
}
}
