# NovaLang 测试套件

本目录包含 NovaLang 编程语言的测试用例。

## 测试文件

测试套件包含以下类别的测试：

1. **基础语言特性**
   - `01_variables.nova` - 测试变量声明和赋值
   - `02_operators.nova` - 测试算术、比较和逻辑运算符
   - `03_conditionals.nova` - 测试条件语句
   - `04_loops.nova` - 测试循环结构
   - `05_functions.nova` - 测试函数定义和调用

2. **高级语言特性**
   - `06_scope.nova` - 测试变量作用域（全局、局部）
   - `07_data_structures.nova` - 测试结构体、列表和其他数据结构
   - `08_type_inference.nova` - 测试类型推断系统
   - `09_error_handling.nova` - 测试错误处理机制
   - `10_math_operations.nova` - 测试复杂数学运算

3. **面向对象特性**
   - `11_classes.nova` - 测试类定义和使用
   - `12_inheritance.nova` - 测试继承和多态
   - `13_encapsulation.nova` - 测试封装和访问修饰符

## 如何运行测试

### 运行所有测试

运行所有测试：
```bash
cd build
cmake ..
make run_all_tests
```

### 运行单个测试

运行特定测试：
```bash
cd build
cmake ..
make run_01_variables  # 替换为要运行的测试名称
```

### 捕获预期输出

捕获测试的当前输出作为预期输出：
```bash
cd build
cmake ..
make capture_expected_output
```

这将在 `expected/` 目录中创建文件，这些文件将用于后续测试运行的验证。

## 添加新测试

添加新测试用例：

1. 在测试目录中创建新的 `.nova` 文件
2. 遵循命名约定：`XX_description.nova`
3. 运行 `make capture_expected_output` 生成预期输出

## 验证

测试系统将每个测试的输出与存储在 `expected/` 目录中的预期输出进行比较。如果匹配，则测试通过；否则，测试失败。 