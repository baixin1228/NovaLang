#include "Return.h"
#include "TypeChecker.h"
#include "ASTParser.h"

int Return::visit_stmt(VarType &result) {
    return value->visit_expr(result);
}

int Return::visit_expr(VarType &result) {
    if (value) {
        return value->visit_expr(result);
    }
    return 0;
} 