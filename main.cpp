#include <csignal>
#include <cstdlib>
#include <cstring>
#include <execinfo.h>
#include <fstream>
#include <iostream>
#include <string>
#include <unistd.h>
#include <vector>

#include "ASTParser.h"
#include "CodeGen.h"
#include "Context.h"
#include "Error.h"
#include "Lexer.h"
#include "TypeChecker.h"

void signalHandler(int signal) {
  void *array[10];
  size_t size;

  // 获取调用栈信息
  size = backtrace(array, 10);

  std::string program_name = std::string(program_invocation_name);
  // 打印调用栈信息
  char **strings = backtrace_symbols(array, size);
  if (strings != nullptr) {
    bool skip = false;
    for (size_t i = 0; i < size; ++i) {
      if (strncmp(strings[i], program_name.c_str(), program_name.length()) != 0)
        continue;

      if (!skip) {
        skip = true;
        continue;
      }

      // 提取地址
      char *address_start = std::strchr(strings[i], '(');
      if (address_start != nullptr) {
        address_start++;
        char *address_end = std::strchr(address_start, ')');
        if (address_end != nullptr) {
          *address_end = '\0';
          std::string address = address_start;

          // 调用 addr2line 工具
          std::string command = "addr2line -e " + program_name + " " + address;
          std::system(command.c_str());
        }
      }
    }
    free(strings);
  }

  // 终止程序
  std::exit(EXIT_FAILURE);
}

void print_usage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [options] <input_file>\n"
              << "Options:\n"
              << "  -o <output_file>  Specify output file\n"
              << "  -c                Compile to object file\n"
              << "  -S                Compile to assembly file\n"
              << "  -h, --help        Show this help message\n";
}

int main(int argc, char* argv[]) {
#ifdef DEBUG
    std::signal(SIGSEGV, signalHandler);
#endif

    bool generate_debug_info = false;
    std::string input_file;
    std::string output_file;
    bool use_jit = true;
    bool generate_object = false;
    bool generate_assembly = false;

    // 解析命令行参数
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-g") == 0) {
            generate_debug_info = true;
        } else if (strcmp(argv[i], "-o") == 0) {
            if (i + 1 < argc) {
                output_file = argv[++i];
                use_jit = false;
            } else {
                std::cerr << "Error: -o option requires an output file\n";
                print_usage(argv[0]);
                return 1;
            }
        } else if (strcmp(argv[i], "-c") == 0) {
            generate_object = true;
            use_jit = false;
        } else if (strcmp(argv[i], "-S") == 0) {
            generate_assembly = true;
            use_jit = false;
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (argv[i][0] != '-') {
            input_file = argv[i];
        } else {
            std::cerr << "Error: Unknown option " << argv[i] << "\n";
            print_usage(argv[0]);
            return 1;
        }
    }

    if (input_file.empty()) {
        std::cerr << "Error: No input file specified\n";
        print_usage(argv[0]);
        return 1;
    }

    // 如果没有指定输出文件名，根据参数生成默认输出文件名
    if (output_file.empty() && !use_jit) {
        if (generate_object) {
            // 对于 -c，默认输出文件名是输入文件名替换后缀为 .o
            output_file = input_file.substr(0, input_file.find_last_of('.')) + ".o";
        } else if (generate_assembly) {
            // 对于 -S，默认输出文件名是输入文件名替换后缀为 .s
            output_file = input_file.substr(0, input_file.find_last_of('.')) + ".s";
        } else {
            // 对于可执行文件，默认输出文件名是 a.out
            output_file = "a.out";
        }
    }

    // 读取输入文件
    std::ifstream file(input_file);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open input file " << input_file << "\n";
        return 1;
    }

    std::string source((std::istreambuf_iterator<char>(file)),
                      std::istreambuf_iterator<char>());
    file.close();

    // 创建上下文
    Context ctx;
    ctx.set_source_filename(input_file);
    
    // 词法分析
    Lexer lexer(ctx, source);
    auto tokens = lexer.tokenize();
    lexer.print_tokens(tokens);

    // 语法分析
    ASTParser parser(ctx, std::move(tokens));
    int ret = parser.parse();
    if (ret != 0) {
        ctx.print_errors();
        return 1;
    }
    
    // 类型检查
    TypeChecker checker(ctx);
    checker.check();
    
    if (ctx.has_errors()) {
#ifdef DEBUG
        parser.print_ast();
#endif
        ctx.print_errors();
        return 1;
    }

    parser.print_ast();
    // 代码生成
    CodeGen codegen(ctx, generate_debug_info);
    if (codegen.generate() == -1) {
      if (ctx.has_errors()) {
        ctx.print_errors();
      }
      return -1;
    }

    // 根据选项决定是JIT执行还是保存到文件
    if (use_jit) {
#ifdef DEBUG
      codegen.save_to_file(input_file.replace(input_file.find_last_of('.'), input_file.length(), ".ll"));
#endif
      codegen.execute();
      if (ctx.has_errors()) {
        ctx.print_errors();
        return 1;
      }
    } else {
        codegen.compile_to_executable(output_file, generate_object, generate_assembly);
        if (ctx.has_errors()) {
#ifdef DEBUG
            codegen.print_ir();
#endif
            ctx.print_errors();
            return 1;
        }
        std::cout << "Compiled to " << output_file << "\n";
    }

    return 0;
}