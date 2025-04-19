#include "CodeGen.h"
#include "Assign.h"

int CodeGen::generate_global_variable(Assign &assign) {
  VarType type = assign.lookup_var_type(assign.var);
  if (type == VarType::NONE) {
    throw std::runtime_error("未定义的变量: " + assign.var + " code:" + std::to_string(assign.line) + " line:" + std::to_string(__LINE__));
    return -1;
  }

  if (assign.need_create) {
    llvm::Type *ty;
    switch (type) {
    case VarType::INT:
      ty = builder.getInt64Ty();
      break;
    case VarType::FLOAT:
      ty = builder.getDoubleTy();
      break;
    case VarType::BOOL:
      ty = builder.getInt1Ty();
      break;
    case VarType::STRING:
      ty = llvm::PointerType::get(runtime_manager->getUnicodeStringType(), 0);
      break;
    default:
      ty = builder.getVoidTy();
      break;
    }

    std::cout << "==== Generating global variable: " << assign.var
              << "====" << std::endl;
    // 创建全局变量
    auto global_var = new llvm::GlobalVariable(
        *module,
        ty,
        false, // 是否为常量
        llvm::GlobalValue::ExternalLinkage,
        llvm::Constant::getNullValue(ty), // 初始值
        assign.var
    );
    global_var->setAlignment(llvm::Align(get_type_align(type)));

    assign.add_llvm_symbol(assign.var, global_var);
  }
  return 0;
}

void CodeGen::GenLocalVar(Assign& assign) {
    if (!assign.need_create || assign.get_scope_depth() <= 0) {
        return;
    }
    
    VarType type = assign.lookup_var_type(assign.var);
    llvm::Type *ty;
    switch (type) {
        case VarType::INT:
            ty = builder.getInt64Ty();
            break;
        case VarType::FLOAT:
            ty = builder.getDoubleTy();
            break;
        case VarType::BOOL:
            ty = builder.getInt1Ty();
            break;
        case VarType::STRING:
            ty = llvm::PointerType::get(runtime_manager->getUnicodeStringType(), 0);
            break;
        default:
            ty = builder.getVoidTy();
            break;
    }
    auto ptr = builder.CreateAlloca(ty, nullptr, assign.var);
    ptr->setAlignment(llvm::Align(get_type_align(type)));
    assign.add_llvm_symbol(assign.var, ptr);
} 