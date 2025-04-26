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
#include "CodeGen/CodeGen.h"
#include "Context.h"
#include "Error.h"
#include "Lexer.h"
#include "TypeChecker.h"

void signalHandler(int signal) {
  print_backtrace();
  // 终止程序
  std::exit(EXIT_FAILURE);
}

void print_usage(const char* program_name) {
  std::cout << "Usage: " << program_name << " [options] <input_file>\n"
            << "Options:\n"
            << "  -o <output_file>  Specify output file\n"
            << "  -c                Compile to object file\n"
            << "  -S                Compile to assembly file\n"
            << "  -j                JIT execute the program\n"
            << "  -g                Generate debug information\n"
            << "  -D                Enable debug mode\n"
            << "  -h, --help        Show this help message\n";
}

int main(int argc, char* argv[]) {
#ifdef DEBUG
    std::signal(SIGSEGV, signalHandler);
#endif

    bool generate_debug_info = false;
    bool enable_debug_mode = false;
    std::string input_file;
    std::string output_file;
    bool use_jit = false;
    bool generate_object = false;
    bool generate_assembly = false;

    // 解析命令行参数
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-g") == 0) {
            generate_debug_info = true;
        } else if (strcmp(argv[i], "-D") == 0) {
            enable_debug_mode = true;
        } else if (strcmp(argv[i], "-o") == 0) {
            if (i + 1 < argc) {
                output_file = argv[++i];
            } else {
                std::cerr << "Error: -o option requires an output file\n";
                print_usage(argv[0]);
                return 1;
            }
        } else if (strcmp(argv[i], "-c") == 0) {
            generate_object = true;
        } else if (strcmp(argv[i], "-S") == 0) {
            generate_assembly = true;
        } else if (strcmp(argv[i], "-j") == 0) {
            use_jit = true;
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

    if (output_file.empty() && !use_jit) {
        if (generate_object) {
            output_file = input_file.substr(0, input_file.find_last_of('.')) + ".o";
        } else if (generate_assembly) {
            output_file = input_file.substr(0, input_file.find_last_of('.')) + ".s";
        } else {
            output_file = "a.out";
        }
    }

    std::ifstream file(input_file);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open input file " << input_file << "\n";
        return 1;
    }

    std::string source((std::istreambuf_iterator<char>(file)),
                      std::istreambuf_iterator<char>());
    file.close();

    Context ctx;
    ctx.set_source_filename(input_file);
    
    Lexer lexer(ctx, source);
    std::vector<Token> tokens;
    int ret = lexer.tokenize(tokens);
    if (ret != 0) {
      ctx.print_errors();
      return -1;
    }

    if (enable_debug_mode) {
      lexer.print_tokens(tokens);
    }
    ASTParser parser(ctx, std::move(tokens));
    ret = parser.parse();
    if (ret != 0) {
      ctx.print_errors();
      return -1;
    }

    TypeChecker checker(ctx);
    ret = checker.check();
    if (ret != 0) {
      if (enable_debug_mode) {
        parser.print_ast();
      }
      ctx.print_errors();
      return -1;
    }
    if (enable_debug_mode) {
      parser.print_ast();
    }

    CodeGen codegen(ctx, generate_debug_info);
    if (codegen.generate() == -1) {
      if (ctx.has_errors()) {
        ctx.print_errors();
      }
      return -1;
    }

    // if (enable_debug_mode) {
    //   codegen.print_ir();
    // }

    if (use_jit) {
      if (enable_debug_mode) {
        codegen.save_to_file(input_file.replace(input_file.find_last_of('.'), input_file.length(), ".ll"));
      }
      codegen.execute();
      if (ctx.has_errors()) {
        ctx.print_errors();
        return -1;
      }
    } else {
        codegen.compile_to_executable(output_file, generate_object, generate_assembly);
        if (ctx.has_errors()) {
            if (enable_debug_mode) {
              parser.print_ast();
              codegen.print_ir();
            }
            ctx.print_errors();
            return -1;
        }
        std::cout << "Compiled to " << output_file << "\n";
    }

    return 0;
}