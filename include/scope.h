#pragma once

#include "symbol_table.h"
#include "type_from_ast.h"

typedef struct Scope {
    SymbolTable *variables;
    SymbolTable *functions;
    struct Scope *parent;
} Scope;

int symbol_table_construction(Scope *global_scope, AstProgram *program);
void scope_print(Scope *scope);
void scope_print_hierarchical(Scope *scope);
void free_scope_maps(Scope *s);
