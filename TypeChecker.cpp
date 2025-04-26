#include "TypeChecker.h"
#include "ASTNode.h"
#include "Function.h"

int TypeChecker::check()
{
  auto &stmts = ctx.get_ast();

  for (auto &stmt : stmts) {
    if (stmt) {
      int ret = stmt->visit_stmt();
      if (ret == -1) {
        return -1;
      }
    }
  }

  /* 直接删除未被引用的函数，否则参数类型无法推断 */
  for (auto it = stmts.begin(); it != stmts.end();)
  {
      if (auto *func = dynamic_cast<Function *>(&**it))
      {
          if (func->reference_count == 0)
          {
              it = stmts.erase(it);
              continue;
          }
      }
      it++;
  }
  return 0;
}