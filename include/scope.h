#ifndef SCOPE_H
#define SCOPE_H

#include "symbol_table.h"
#include "type.h"
#include "ast.h"

typedef struct Scope {
    SymbolTable *variables;
    SymbolTable *functions;
    struct Scope *parent;
} Scope;

int symbol_table_construction(Scope *global_scope, AstProgram *program);
void scope_print(Scope *scope);
void free_scope_maps(Scope *s);

#endif /* SCOPE_H */
