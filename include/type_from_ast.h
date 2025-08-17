#pragma once
/* This bridging header includes both AST and Type definitions and
 * declares functions that convert AST type nodes to semantic Types.
 *
 * Implementations that need both AST and Type should include this header.
 * Needed due to circular imports
 */

#include "ast.h"
#include "type.h"

Type *asttype_to_type(AstType *ast_node_type); /* consumes AST node and returns newly allocated Type* */
Type *astfunction_to_type(AstFunctionDeclaration *ast_fn);
