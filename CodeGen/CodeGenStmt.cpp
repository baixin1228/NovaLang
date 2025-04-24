int CodeGen::visit_stmt(ASTNode& node) {
    // Set debug location for each statement only if debug info is enabled
    // if (generate_debug_info) {
    //     auto debug_loc = llvm::DILocation::get(context, node.line, 0, current_function->getSubprogram());
    //     builder.SetCurrentDebugLocation(debug_loc);
    // }
    return 0;
} 