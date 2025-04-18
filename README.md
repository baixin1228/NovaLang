# NovaLang

[English](README.md) | [中文](README_CN.md)

## Introduction

NovaLang is a modern, compiled programming language that combines the elegance of Python's syntax with the performance of compiled languages. While it shares a similar syntax with Python, NovaLang is not a Python compiler or interpreter - it's a completely independent language designed for high performance and type safety.

Key features:
- Python-like syntax for easy learning and adoption
- Compiled to native code for superior performance
- Strong type system for better reliability
- Modern language features and optimizations
- LLVM-based compilation pipeline

## Getting Started

### Prerequisites

- Ubuntu/Debian Linux
- CMake 3.10 or higher
- LLVM 14
- C++17 compatible compiler
- Build essentials

### Installation

1. Install dependencies:
```bash
sudo apt-get install -y build-essential cmake git llvm-14 llvm-14-dev clang-14 libclang-14-dev
```

2. Clone the repository:
```bash
git clone https://github.com/yourusername/NovaLang.git
cd NovaLang
```

3. Build the project:
```bash
mkdir build && cd build
cmake ..
make
```

### Hello World Example

Create a file named `hello.nova`:
```python
def main():
    print("Hello, NovaLang!")
```

Compile and run:
```bash
./compiler hello.nova
```

## Contributing

We welcome contributions from the community! Here's how you can help:

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/AmazingFeature`)
3. Commit your changes (`git commit -m 'Add some AmazingFeature'`)
4. Push to the branch (`git push origin feature/AmazingFeature`)
5. Open a Pull Request

Please make sure to update tests as appropriate and follow our coding style guidelines.

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Acknowledgments

- LLVM Project for providing the compiler infrastructure
- Python for inspiring the syntax design
- All contributors who help make NovaLang better 