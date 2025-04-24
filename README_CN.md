# NovaLang

[English](README.md) | [中文](README_CN.md)

## 简介

NovaLang 是一种现代的编译型编程语言，它将 Python 的优雅语法与编译语言的性能相结合。虽然它与 Python 有相似的语法，但 NovaLang 不是 Python 的编译器或解释器 - 它是一个完全独立的语言，专为高性能和类型安全而设计。

主要特性：
- 类 Python 语法，易于学习和采用
- 编译为本地代码，提供卓越性能
- 强类型系统，提高可靠性
- 现代语言特性和优化
- 基于 LLVM 的编译管道

## 开始使用

### 先决条件

- Ubuntu/Debian Linux
- CMake 3.10 或更高版本
- LLVM 14
- C++17 兼容的编译器
- 构建工具

### 安装

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

创建名为 `hello.nova` 的文件：
```python
def main():
    print("Hello, NovaLang!")
```

编译并运行：
```bash
./compiler hello.nova
```

## 测试

NovaLang 包含一个全面的测试套件，用于确保语言特性正常工作。测试套件分为三个主要类别：

1. **基础语言特性**
   - 变量声明和赋值
   - 运算符
   - 条件语句
   - 循环
   - 函数

2. **高级语言特性**
   - 变量作用域
   - 数据结构
   - 类型推断
   - 错误处理
   - 数学运算

3. **面向对象特性**
   - 类
   - 继承
   - 封装

### 运行测试

#### 运行所有测试

使用提供的脚本运行所有测试：
```bash
./run_tests.sh
```

#### 运行单个测试

运行特定测试：
```bash
./run_test.sh 01_variables  # 替换为要运行的测试名称
```

#### 捕获预期输出

使用提供的脚本捕获测试的当前输出作为预期输出：
```bash
./capture_expected.sh
```

这将在 `test/expected/` 目录中创建文件，这些文件将用于后续测试运行的验证。

有关测试套件的更详细信息，请参阅 [test/README.md](test/README.md) 文件。

## 贡献

我们欢迎社区的贡献！以下是您可以提供帮助的方式：

1. Fork 仓库
2. 创建特性分支 (`git checkout -b feature/AmazingFeature`)
3. 提交更改 (`git commit -m '添加一些 AmazingFeature'`)
4. 推送到分支 (`git push origin feature/AmazingFeature`)
5. 打开 Pull Request

请确保适当更新测试并遵循我们的编码风格指南。

## 许可证

本项目采用 MIT 许可证 - 详见 [LICENSE](LICENSE) 文件。

## 致谢

- LLVM 项目提供编译器基础设施
- Python 启发了语法设计
- 所有帮助改进 NovaLang 的贡献者 