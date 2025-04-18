# NovaLang

[English](README.md) | [中文](README_CN.md)

## 简介

NovaLang 是一门现代化的编译型编程语言，它结合了 Python 语法的优雅和编译型语言的性能。虽然它的语法与 Python 相似，但 NovaLang 既不是 Python 的编译器也不是解释器 - 它是一个完全独立的语言，专为高性能和类型安全而设计。

主要特点：
- 类 Python 语法，易于学习和使用
- 编译为本地代码，提供卓越的性能
- 强类型系统，提供更好的可靠性
- 现代化的语言特性和优化
- 基于 LLVM 的编译管道

## 开始使用

### 环境要求

- Ubuntu/Debian Linux
- CMake 3.10 或更高版本
- LLVM 14
- 支持 C++17 的编译器
- 基础构建工具

### 安装步骤

1. 安装依赖：
```bash
sudo apt-get install -y build-essential cmake git llvm-14 llvm-14-dev clang-14 libclang-14-dev
```

2. 克隆仓库：
```bash
git clone https://github.com/yourusername/NovaLang.git
cd NovaLang
```

3. 构建项目：
```bash
mkdir build && cd build
cmake ..
make
```

### Hello World 示例

创建一个名为 `hello.nova` 的文件：
```python
def main():
    print("Hello, NovaLang!")
```

编译并运行：
```bash
./compiler hello.nova
```

## 参与贡献

我们欢迎社区的贡献！以下是参与方式：

1. Fork 本仓库
2. 创建特性分支 (`git checkout -b feature/AmazingFeature`)
3. 提交更改 (`git commit -m '添加一些很棒的特性'`)
4. 推送到分支 (`git push origin feature/AmazingFeature`)
5. 提交 Pull Request

请确保更新相应的测试并遵循我们的代码风格指南。

## 许可证

本项目采用 MIT 许可证 - 详见 [LICENSE](LICENSE) 文件。

## 致谢

- LLVM 项目提供编译器基础设施
- Python 启发语法设计
- 所有帮助 NovaLang 变得更好的贡献者 