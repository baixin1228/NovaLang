cmake_minimum_required(VERSION 3.10)
project(Compiler)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置构建类型
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

# 为不同构建类型设置不同的编译选项
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -DNDEBUG")

# 查找 ICU 库
find_package(ICU 70 COMPONENTS uc i18n REQUIRED)
include_directories(${ICU_INCLUDE_DIRS})

# 添加头文件搜索路径
include_directories(${CMAKE_SOURCE_DIR})
include_directories(${CMAKE_SOURCE_DIR}/AST)
include_directories(${CMAKE_SOURCE_DIR}/CodeGen)
include_directories(${CMAKE_SOURCE_DIR}/Runtime)

# 查找 LLVM
find_package(LLVM 14 REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

# 包含 LLVM 头文件和定义
include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

# 获取 LLVM 库
llvm_map_components_to_libnames(llvm_libs
    Core
    ExecutionEngine
    OrcJIT
    Support
    Target
    X86
    X86CodeGen
    X86AsmParser
    X86Desc
    X86Info
    MC
    MCJIT
    Object
    Passes
    Analysis
    BitWriter
    BitReader
    TransformUtils
    InstCombine
    ScalarOpts
    Vectorize
    IPO
    Linker
    DebugInfoDWARF
    DebugInfoMSF
    DebugInfoCodeView
    DebugInfoPDB
    Symbolize
    Demangle
    AArch64
    AMDGPU
    ARM
    AVR
    BPF
    Hexagon
    Lanai
    Mips
    MSP430
    NVPTX
    PowerPC
    RISCV
    Sparc
    SystemZ
    VE
    WebAssembly
    XCore
    M68k
)

# 设置 AST 源文件
set(AST_SOURCES
    AST/Common.cpp
    AST/ASTNode.cpp
    AST/Assign.cpp
    AST/CompoundAssign.cpp
    AST/BinOp.cpp
    AST/BoolLiteral.cpp
    AST/Call.cpp
    AST/FloatLiteral.cpp
    AST/For.cpp
    AST/Function.cpp
    AST/Global.cpp
    AST/If.cpp
    AST/Increment.cpp
    AST/IntLiteral.cpp
    AST/StringLiteral.cpp
    AST/Print.cpp
    AST/Return.cpp
    AST/UnaryOp.cpp
    AST/Variable.cpp
    AST/While.cpp
    AST/StructLiteral.cpp
    AST/DictLiteral.cpp
    AST/ListLiteral.cpp
    AST/StructFieldAccess.cpp
    AST/StructFieldAssign.cpp
    AST/IndexAccess.cpp
    AST/IndexAssign.cpp
    AST/Annotation.cpp
)

# 设置 CodeGen 源文件
set(CODEGEN_SOURCES
    CodeGen/CodeGen.cpp
    CodeGen/RuntimeManager.cpp
)

# 查找ICU库
find_package(ICU REQUIRED COMPONENTS uc i18n)

# 编译运行时库
add_library(novalang_runtime SHARED
    Runtime/nova_list.c
    Runtime/nova_dict.c
    Runtime/nova_string.c
    Runtime/nova_memory_manager.c
)
set_target_properties(novalang_runtime PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
    POSITION_INDEPENDENT_CODE ON
)
target_link_libraries(novalang_runtime PRIVATE
    ICU::uc
    ICU::i18n
)

# 添加可执行文件
add_executable(nova 
    main.cpp 
    Error.cpp 
    Context.cpp 
    ASTParser.cpp
    Lexer.cpp 
    TypeChecker.cpp 
    ${CODEGEN_SOURCES}
    ${AST_SOURCES}
)

# 为编译器目标设置特定的编译定义
target_compile_definitions(nova PRIVATE
    $<$<CONFIG:Debug>:DEBUG>
)

# 链接 LLVM 库和 ICU 库
target_link_libraries(nova PRIVATE 
    ${llvm_libs} 
    ICU::uc 
    ICU::i18n
    novalang_runtime
)

# Enable testing
enable_testing()

# Add the test directory
add_subdirectory(test)