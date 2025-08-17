#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "ast_parse_statements.h"

/*
 * This file contains implementations of the lang.bnf specification
 */

int fail_with(Parser *p, AstNode *partial, ParseError *err, const char *msg) {
    if (partial) ast_node_free(partial);
    if (err) create_parse_error(err, msg, p);
    return 0;
}

// <Program> ::= { <Declaration> }
AstNode *parse_program(Parser *p, ParseError *err) {
    if (!p) return NULL;

    AstNode *program = ast_create_node(AST_PROGRAM);
    if (!program) {
        if (err) create_parse_error(err, "out of memory creating program node", p);
        return NULL;
    }
    astnode_array_init(&program->data.program.decls);

    /* declarations */
    AstNode *declaration = parse_declaration(p, err);
    while (declaration) {
        if (astnode_array_push(&program->data.program.decls, declaration) != 0) {
            ast_node_free(program);
            return NULL;
        }
        declaration = parse_declaration(p, err);
    }

    if (err && err->message) {
        ast_node_free(program);
        return NULL;
    }

    if (p->current < p->end) {
        fail_with(p, program, err, "unexpected tokens after program end");
        return NULL;
    }

    return program;
}

// <Declaration> ::= <VariableDeclaration> | <FunctionDeclaration>
AstNode *parse_declaration(Parser *p, ParseError *err) {
    if (!p) {
        if (err) create_parse_error(err, "internal parser error: null parser", p);
        return NULL;
    }

    Token *current = current_token(p);

    /* If we've hit EOF consume and return */
    if (current->type == TOK_EOF) {
        consume(p, TOK_EOF);
        return NULL;
    }

    /* Should never happen */
    if (!current) {
        if (err) create_parse_error(err, "Unexpected end of input", p);
        return NULL;
    }

    if (current->type == TOK_FN) {
        return parse_function_declaration(p, err);
    }

    if (current->type == TOK_IDENTIFIER) {
        return parse_declaration_stmt(p, err);
    }

    if (err) create_parse_error(err, "Expected function or variable declaration", p);
    return NULL; // no match
}

// <FunctionDeclaration> ::= FN IDENTIFIER LPAREN [ <ParamList> ] RPAREN [ ARROW <Type> ] <Block>
AstNode *parse_function_declaration(Parser *p, ParseError *err) {
    AstNode *func_decl = ast_create_node(AST_FUNCTION_DECLARATION);
    if (!func_decl) {
        if (err) create_parse_error(err, "out of memory creating function declaration node", p);
        return NULL;
    }
    astnode_array_init(&func_decl->data.function_declaration.params);

    /* fn */
    Token *fn_tok = consume(p, TOK_FN);
    if (!fn_tok) { fail_with(p, func_decl, err, "expected 'fn' keyword"); return NULL; }

    /* name */
    Token *name_tok = consume(p, TOK_IDENTIFIER);
    if (!name_tok) { fail_with(p, func_decl, err, "expected function name"); return NULL; }
    func_decl->data.function_declaration.name = name_tok->lexeme ? strdup(name_tok->lexeme) : NULL;

    /* parameters */
    if (!consume(p, TOK_LPAREN)) { fail_with(p, func_decl, err, "expected '(' after function name"); return NULL; }
    
    // parse parameters
    if (!parse_parameter_list(p, func_decl, err)) {
        return NULL; // parse_parameter_list already freed func_decl via fail_with
    }

    if (!consume(p, TOK_RPAREN)) { fail_with(p, func_decl, err, "expected ')' after function parameters"); return NULL; }

    /* optional return type */
    Token *arrow_tok = current_token(p);
    if (arrow_tok && arrow_tok->type == TOK_ARROW) {
        consume(p, TOK_ARROW);
        AstNode *return_type = parse_type(p, err);
        if (!return_type) { ast_node_free(func_decl); return NULL; }
        func_decl->data.function_declaration.return_type = return_type;
    } else {
        func_decl->data.function_declaration.return_type = NULL; // no return type
    }

    /* body */
    func_decl->data.function_declaration.body = parse_block(p, err);
    if (!func_decl->data.function_declaration.body) {
        ast_node_free(func_decl);
        return NULL;
    }
    return func_decl;
}
// <Block> ::= '{' { <Statement> } '}'
AstNode *parse_block(Parser *p, ParseError *err) {
    AstNode *block = ast_create_node(AST_BLOCK);

    if (!block) {
        if (err) create_parse_error(err, "out of memory creating block node", p);
        return NULL;
    }

    astnode_array_init(&block->data.block.statements);

    /* consume '{' */
    Token *lbrace = consume(p, TOK_L_BRACE);
    if (!lbrace) { fail_with(p, block, err, "expected '{' to start block"); return NULL; }

    /* parse statements until '}' */
    while (1) {
        Token *current = current_token(p);
        if (current->type == TOK_EOF) {
            fail_with(p, block, err, "unexpected end of input in block");
            return NULL;
        }

        if (current->type == TOK_R_BRACE) {
            consume(p, TOK_R_BRACE);
            break; // end of block
        }

        AstNode *stmt = parse_statement(p, err);
        if (!stmt) {
            ast_node_free(block);
            return NULL; // error already set in parse_declaration_stmt
        }

        if (astnode_array_push(&block->data.block.statements, stmt) != 0) {
            ast_node_free(block);
            return NULL; // out of memory
        }
    }

    return block;
}



AstNode *parse_declaration_stmt(Parser *p, ParseError *err) {
    /* parse the variable declaration (this consumes the identifier, type, initializer) */
    AstNode *declaration = parse_variable_declaration(p, err);
    if (!declaration) return NULL;

    /* now require the semicolon terminator */
    Token *semi = consume(p, TOK_SEMICOLON);
    if (!semi) {
        err->underline_previous_token_line = 1; // underline previous line
        fail_with(p, declaration, err, "expected a semicolon at declaration end");
        return NULL;
    }

    return declaration;
}

/* <VariableDeclaration> ::= IDENTIFIER COLON [ CONST ] <Type> [ ASSIGN ( <Expression> | <InitializerList> ) ] */
AstNode *parse_variable_declaration(Parser *p, ParseError *err)
{
    AstNode *var_decl = ast_create_node(AST_VARIABLE_DECLARATION);
    if (!var_decl) {
        if (err) create_parse_error(err, "out of memory creating variable-declaration node", p);
        return NULL;
    }

    /* name */
    Token *name_tok = consume(p, TOK_IDENTIFIER);
    if (!name_tok) { fail_with(p, var_decl, err, "expected identifier in variable declaration"); return NULL; }
    var_decl->data.variable_declaration.name = name_tok->lexeme ? strdup(name_tok->lexeme) : NULL;

    /* colon */
    if (!consume(p, TOK_COLON)) { fail_with(p, var_decl, err, "expected ':' after variable name"); return NULL; }

    /* optional 'const' */
    int is_const = 0;
    Token *token = current_token(p);
    if (token && token->type == TOK_CONST) { consume(p, TOK_CONST); is_const = 1; }

    /* type */
    AstNode *type_node = parse_type(p, err);
    if (!type_node) { ast_node_free(var_decl); return NULL; }
    type_node->data.ast_type.base_is_const = is_const;
    var_decl->data.variable_declaration.type = type_node;

    /* optional initializer */
    var_decl->data.variable_declaration.initializer = NULL;
    token = current_token(p);
    if (token && token->type == TOK_ASSIGN) {
        consume(p, TOK_ASSIGN);

        AstNode *initializer = NULL;
        token = current_token(p);
        if (token && token->type == TOK_L_BRACE) {
            initializer = parse_initializer_list(p, type_node, err);
        } else {
            initializer = parse_expression(p, err);
        }

        /* err already filled by parse_* on failure */
        if (!initializer) { ast_node_free(var_decl); return NULL; }
        var_decl->data.variable_declaration.initializer = initializer;
    }

    return var_decl;
}

// <Type> ::= <BaseType> [<PostfixType>]
AstNode *parse_type(Parser *p, ParseError *err) {
    AstNode *type_node = ast_create_node(AST_TYPE);
    if (!type_node) {
        if (err) create_parse_error(err, "out of memory creating type node", p);
        return NULL;
    }
    astnode_array_init(&type_node->data.ast_type.sizes);

    /* base type (doesn't support custom types yet) */
    Token *token = current_token(p);
    if (!token || token->type < TOK_I32 || token->type > TOK_F64) {
        fail_with(p, type_node, err, "expected base type (i32, f64, etc.)");
        return NULL;
    }

    char *base_type_str = token->lexeme ? strdup(token->lexeme) : NULL;
    if (token->lexeme && !base_type_str) {
        fail_with(p, type_node, err, "out of memory copying base type string");
        return NULL;
    }
    consume(p, token->type);
    type_node->data.ast_type.base_type = base_type_str;


    /* optional postfix type */
    token = current_token(p);
    if (token && (token->type == TOK_STAR || token->type == TOK_L_SQB)) {
        // parse postfix takes ownership of type_node and frees it on error
        if (parse_postfix_type(p, type_node, err) != 0) {
            return NULL;
        }
    }

    return type_node;
}

// parse_postfix_type: handles pre/post stars and array-sizes
int parse_postfix_type(Parser *p, AstNode *type_node, ParseError* err) {
    Token *token = current_token(p);

    /* pre_stars */
    while (token && token->type == TOK_STAR) {
        type_node->data.ast_type.pre_stars++;
        consume(p, token->type);
        token = current_token(p);
    }

    ///* indexes */
    token = current_token(p);
    while (token && token->type == TOK_L_SQB) {
        consume(p, TOK_L_SQB);

        token = current_token(p);
        if (!token) { if (err) create_parse_error(err, "unexpected end in array index", p); return 1; }

        if (token->type != TOK_R_SQB) {
            AstNode *const_expr = parse_expression(p, err);
            if (!const_expr) { return 1; }
            // type_size_push(type_node, const_expr);
            if(astnode_array_push(&type_node->data.ast_type.sizes, const_expr) != 0){
                if (err) create_parse_error(err, "out of memory pushing declaration", p);
                ast_node_free(const_expr);
                return 1;
            }

            token = current_token(p);
            if (!token || token->type != TOK_R_SQB) {
                if (err) fail_with(p, type_node, err, "expected ']' after array size");
                return 1;
            }
            consume(p, TOK_R_SQB);
        } else {
            //printf(type_node->data.type.ndim_count);
            if(astnode_array_push(&type_node->data.ast_type.sizes, NULL) != 0){
                if (err) create_parse_error(err, "out of memory pushing declaration", p);
                return 1;
            }
            consume(p, TOK_R_SQB);
        }
        token = current_token(p);
    }

    /* post_stars */
    token = current_token(p);
    while (token && token->type == TOK_STAR) {
        type_node->data.ast_type.post_stars++;
        consume(p, token->type);
        token = current_token(p);
    }

    return 0;
}


AstNode *parse_const_expr(Parser *p, ParseError *err){
    /* TODO: implement constant-expression parsing */
    if (err) create_parse_error(err, "constant-expression parsing not implemented", p);
    return NULL;
}

/* parse_assignment takes ownership of `lhs`. On success it returns a
 * new AST_ASSIGNMENT_EXPR node; on failure it frees `lhs` and returns NULL.
 */
AstNode *parse_assignment(Parser *p, AstNode *lhs, ParseError *err) {
    if (!lhs) return NULL;

    /* must be an lvalue */
    if (!is_lvalue_node(lhs)) {
        if (err) create_parse_error(err, "lvalue required on left side of assignment", p);
        ast_node_free(lhs);
        return NULL;
    }

    /* ensure there is an assignment operator token */
    Token *tok = current_token(p);
    if (!tok || !is_assignment_op(tok->type)) {
        /* caller should not call this unless there is an assignment op,
           but handle gracefully. */
        ast_node_free(lhs);
        return NULL;
    }

    /* consume the assignment operator and determine OpKind */
    TokenType op_type = tok->type;
    consume(p, op_type);

    /* parse RHS as a full expression (right-associative assignments) */
    AstNode *rhs = parse_expression(p, err);
    if (!rhs) {
        /* parse_expression should set err; free lhs and return */
        ast_node_free(lhs);
        return NULL;
    }

    /* build the assignment node and fill fields */
    AstNode *assignment = ast_create_node(AST_ASSIGNMENT_EXPR);
    if (!assignment) {
        if (err) create_parse_error(err, "out of memory creating assignment node", p);
        ast_node_free(lhs);
        ast_node_free(rhs);
        return NULL;
    }

    /* map token to OpKind */
    OpKind op = OP_ASSIGN; /* default */
    switch (op_type) {
        case TOK_EQ_EQ:      op = OP_EQ;    break;
        case TOK_PLUS_EQ:    op = OP_PLUS_EQ;   break;
        case TOK_MINUS_EQ:   op = OP_MINUS_EQ;  break;
        case TOK_STAR_EQ:    op = OP_MUL;       break;
        case TOK_SLASH_EQ:   op = OP_DIV;       break;
        case TOK_PERCENT_EQ: op = OP_MOD;       break;
        case TOK_ASSIGN:     op = OP_ASSIGN;    break; // just '='
        default:
            if (err) create_parse_error(err, "unexpected assignment operator", p);
            ast_node_free(lhs);
            ast_node_free(rhs);
            ast_node_free(assignment);
            return NULL; // unexpected operator, should not happen since we check in parse_expression
    }

    assignment->data.assignment_expr.lvalue = lhs;
    assignment->data.assignment_expr.rvalue = rhs;
    assignment->data.assignment_expr.op     = op;

    return assignment;
}

/* <Expression> ::= <Assignment> | <LogicalOr>
 *
 * Parse logical-or first, then, if an assignment-op follows, hand over
 * the parsed LHS to parse_assignment (which takes ownership).
 */
AstNode *parse_expression(Parser *p, ParseError *err) {
    AstNode *lhs = parse_logical_or(p, err);
    if (!lhs) return NULL;

    Token *token = current_token(p);
    if (token && is_assignment_op(token->type)) {
        AstNode *assignment = parse_assignment(p, lhs, err);
        /* parse_assignment takes ownership of lhs and frees it on error */
        if (!assignment) {
            return NULL; /* err already set by parse_assignment */
        }
        return assignment;
    }

    return lhs;
}

AstNode *parse_expression_statement(Parser *p, ParseError *err) {
    AstNode *expr = parse_expression(p, err);

    if (!expr) return NULL;

    /* consume the semicolon */
    Token *semi = consume(p, TOK_SEMICOLON);
    if (!semi) {
        err->underline_previous_token_line = 1; // underline previous token line
        fail_with(p, expr, err, "expected ';' at end of expression statement");
        return NULL;
    }
    return expr;
}


AstNode *parse_initializer_list(Parser *p, AstNode *type_node, ParseError *err) {
    (void)type_node; /* currently unused, kept for future type-directed checks */

    /* consume the opening '{' */
    if (!consume(p, TOK_L_BRACE)) {
        if (err) create_parse_error(err, "expected '{' to start initializer list", p);
        return NULL;
    }

    AstNode *init = ast_create_node(AST_INITIALIZER_LIST);
    if (!init) {
        if (err) create_parse_error(err, "out of memory creating initializer node", p);
        return NULL;
    }
    astnode_array_init(&init->data.initializer_list.elements);

    Token *tok = current_token(p);
    if (!tok) {
        /* unexpected EOF */
        fail_with(p, init, err, "unexpected end of input in initializer list");
        return NULL;
    }

    /* empty list: {} */
    if (tok->type == TOK_R_BRACE) {
        consume(p, TOK_R_BRACE);
        return init;
    }

    /* parse elements: either nested initializer or expression, separated by commas.
       Trailing comma is NOT allowed: a ',' must be followed by another element. */
    while (1) {
        tok = current_token(p);
        if (!tok) {
            fail_with(p, init, err, "unexpected end of input in initializer list");
            return NULL;
        }

        AstNode *element = NULL;
        if (tok->type == TOK_L_BRACE) {
            /* nested initializer — recursive call consumes the '{' */
            element = parse_initializer_list(p, NULL, err);
            if (!element) {
                /* err already set by recursive call; clean up */
                astnode_array_free(&init->data.initializer_list.elements);
                ast_node_free(init);
                return NULL;
            }
        } else {
            /* normal expression */
            element = parse_expression(p, err);
            if (!element) {
                astnode_array_free(&init->data.initializer_list.elements);
                ast_node_free(init);
                return NULL; /* parse_expression set err */
            }
        }

        /* push element into the initializer array */
        if (astnode_array_push(&init->data.initializer_list.elements, element) != 0) {
            if (err) create_parse_error(err, "out of memory pushing initializer element", p);
            ast_node_free(element);
            astnode_array_free(&init->data.initializer_list.elements);
            ast_node_free(init);
            return NULL;
        }

        /* after element, expect comma or '}' */
        tok = current_token(p);
        if (!tok) {
            fail_with(p, init, err, "unexpected end of input in initializer list");
            return NULL;
        }

        if (tok->type == TOK_COMMA) {
            /* consume comma */
            consume(p, TOK_COMMA);

            /* now the NEXT token must be another element start (not '}' ). Trailing comma is disallowed. */
            tok = current_token(p);
            if (!tok) {
                fail_with(p, init, err, "unexpected end of input after ',' in initializer list");
                return NULL;
            }
            if (tok->type == TOK_R_BRACE) {
                /* trailing comma — treat as error */
                fail_with(p, init, err, "trailing comma not allowed in initializer list");
                return NULL;
            }

            /* otherwise continue to parse the next element */
            continue;
        } else if (tok->type == TOK_R_BRACE) {
            consume(p, TOK_R_BRACE);
            return init;
        } else {
            /* invalid token */
            fail_with(p, init, err, "expected ',' or '}' in initializer list");
            return NULL;
        }
    }

    /* unreachable */
    return init;
}



 /* operand_parser: parse one operand/sublevel and return AST node (or NULL on error). */
typedef AstNode *(*operand_parser_fn)(Parser *p, ParseError *err);

/* map_token_to_op: return OP_* for this token, or -1 if token not handled. */
typedef OpKind (*map_token_to_op_fn)(Token *tok);

/* Generic left-associative binary parser:
 * - parse_operand parses the left/right subexpressions
 * - map_op returns the operator enum for the current token or OP_NULL if not an operator
 */
static AstNode *parse_left_assoc_binary(Parser *p, ParseError *err,
                        operand_parser_fn parse_operand,
                        map_token_to_op_fn map_op,
                        const char *oom_msg)
{
    AstNode *lhs = parse_operand(p, err);
    if (!lhs) return NULL;

    Token *token = current_token(p);
    int op;
    while (token && (op = map_op(token)) != OP_NULL) {
        /* consume the exact token type we saw */
        consume(p, token->type);

        AstNode *rhs = parse_operand(p, err);
        if (!rhs) {
            ast_node_free(lhs);
            return NULL; /* err set by parse_operand */
        }

        AstNode *bin = ast_create_node(AST_BINARY_EXPR);
        if (!bin) {
            if (err) create_parse_error(err, oom_msg ? oom_msg : "out of memory creating binary node", p);
            ast_node_free(lhs);
            ast_node_free(rhs);
            return NULL;
        }

        bin->data.binary_expr.left  = lhs;
        bin->data.binary_expr.right = rhs;
        bin->data.binary_expr.op    = op;

        lhs = bin;                  /* new accumulated LHS */
        token = current_token(p);   /* next token */
    }

    return lhs;
}

OpKind map_logical_or_op(Token *tok) {
    if (!tok) return OP_NULL;
    switch (tok->type) {
        case TOK_OR_OR: return OP_OR;
        default:     return OP_NULL; // not a logical-or operator
    }
}

// <LogicalOr> ::= <LogicalAnd> { OR <LogicalAnd> }
AstNode *parse_logical_or(Parser *p, ParseError *err) {
    return parse_left_assoc_binary(p, err, parse_logical_and,
                                      map_logical_or_op, "out of memory creating logical-or node");
}

OpKind map_logical_and_op(Token *tok) {
    if (!tok) return OP_NULL;
    switch (tok->type) {
        case TOK_AND_AND: return OP_AND;
        default:     return OP_NULL; // not a logical-and operator
    }
}

// <LogicalAnd> ::= <Equality> { AND <Equality> }
AstNode *parse_logical_and(Parser *p, ParseError *err) {
    return parse_left_assoc_binary(p, err, parse_equality,
                                      map_logical_and_op, "out of memory creating logical-and node");
}

OpKind map_equality_op(Token *tok) {
    if (!tok) return OP_NULL;
    switch (tok->type) {
        case TOK_EQ_EQ: return OP_EQ;
        case TOK_BANG_EQ:   return OP_NEQ;
        default:        return OP_NULL; // not an equality operator
    }
}

// <Equality> ::= <Relational> { ( EQ_EQ | BANG_EQ ) <Relational> }
AstNode *parse_equality(Parser *p, ParseError *err) {
    return parse_left_assoc_binary(p, err, parse_relational,
                                      map_equality_op, "out of memory creating equality node");
}   

OpKind map_relational_op(Token *tok) {
    if (!tok) return OP_NULL;
    switch (tok->type) {
        case TOK_LT: return OP_LT;
        case TOK_GT: return OP_GT;
        case TOK_LT_EQ: return OP_LE;
        case TOK_GT_EQ: return OP_GE;
        default:     return OP_NULL; // not a relational operator
    }
}

// <Relational> ::= <Additive> { ( LT | GT | LT_EQ | GT_EQ ) <Additive> }
AstNode *parse_relational(Parser *p, ParseError *err) {
    return parse_left_assoc_binary(p, err, parse_additive,
                                      map_relational_op, "out of memory creating relational node");
}

OpKind map_additive_op(Token *tok) {
    if (!tok) return OP_NULL;
    switch (tok->type) {
        case TOK_PLUS: return OP_ADD;
        case TOK_MINUS: return OP_SUB;
        default:     return OP_NULL; // not an additive operator
    }
}
// <Additive> ::= <Multiplicative> { ( PLUS | MINUS ) <Multiplicative> }
AstNode *parse_additive(Parser *p, ParseError *err) {
    return parse_left_assoc_binary(p, err, parse_multiplicative,
                                      map_additive_op, "out of memory creating additive node");
}

OpKind map_multiplicative_op(Token *tok) {
    if (!tok) return OP_NULL;
    switch (tok->type) {
        case TOK_STAR: return OP_MUL;
        case TOK_SLASH: return OP_DIV;
        case TOK_PERCENT: return OP_MOD;
        default:     return OP_NULL; // not a multiplicative operator
    }
}

// <Multiplicative> ::= <Unary> { ( STAR | SLASH | PERCENT ) <Unary> }
AstNode *parse_multiplicative(Parser *p, ParseError *err) {
    return parse_left_assoc_binary(p, err, parse_unary, 
                                      map_multiplicative_op, "out of memory creating multiplicative node");
}   

OpKind map_unary_op(Token *tok) { if (!tok) return OP_NULL; 
    switch (tok->type) { 
        case TOK_PLUS: return OP_ADD; 
        case TOK_MINUS: return OP_SUB; 
        case TOK_BANG: return OP_NOT; 
        case TOK_STAR: return OP_DEREF; 
        case TOK_AMP: return OP_ADRESS; 
        case TOK_PLUSPLUS: return OP_PRE_INC; 
        case TOK_MINUSMINUS: return OP_PRE_DEC; 
        default: return OP_NULL; 
    } 
}

/* <Unary> ::= <PrefixOp> <Unary> | <Postfix> */
AstNode *parse_unary(Parser *p, ParseError *err) {
    Token *token = current_token(p);

    /* If current token is a prefix operator, consume it and parse the operand recursively. */
    if (token &&
        (token->type == TOK_PLUS     || token->type == TOK_MINUS    ||
         token->type == TOK_BANG     || token->type == TOK_STAR     ||
         token->type == TOK_AMP      || token->type == TOK_PLUSPLUS ||
         token->type == TOK_MINUSMINUS)) {

        /* remember the operator token before consuming */
        Token *op_token = token;
        consume(p, op_token->type);

        /* parse the operand as another Unary (to allow chains of prefixes) */
        AstNode *operand = parse_unary(p, err);
        if (!operand) {
            /* parse_unary sets err on failure */
            return NULL;
        }

        /* create the unary AST node */
        AstNode *unary = ast_create_node(AST_UNARY_EXPR);
        if (!unary) {
            if (err) create_parse_error(err, "out of memory creating unary node", p);
            ast_node_free(operand);
            return NULL;
        }

        /* attach operand and operator */
        unary->data.unary_expr.expr = operand;
        unary->data.unary_expr.op      = map_unary_op(op_token);

        return unary;
    }

    /* otherwise it's a postfix (or primary) expression */
    return parse_postfix(p, err);
}

// <Postfix> ::= <Primary> { <PostfixOp> }
AstNode *parse_postfix(Parser *p, ParseError *err) {
    /* First, parse the primary expression */
    AstNode *primary = parse_primary(p, err);
    if (!primary) return NULL;

    /* Then, parse any postfix operators */
    Token *token = current_token(p);
    while (token && (token->type == TOK_PLUSPLUS || token->type == TOK_MINUSMINUS || 
                     token->type == TOK_L_SQB || token->type == TOK_LPAREN)) {
        
        if (token->type == TOK_PLUSPLUS || token->type == TOK_MINUSMINUS) {
            /* Postfix increment/decrement */
            AstNode *postfix = ast_create_node(AST_UNARY_EXPR);
            if (!postfix) {
                if (err) create_parse_error(err, "out of memory creating postfix node", p);
                ast_node_free(primary);
                return NULL;
            }
            
            postfix->data.unary_expr.expr = primary;
            postfix->data.unary_expr.op = (token->type == TOK_PLUSPLUS) ? OP_POST_INC : OP_POST_DEC;
            consume(p, token->type);
            primary = postfix;
            
        } else if (token->type == TOK_L_SQB) {
            /* Array subscript: primary[index] 
             * Any expression can be subscripted, not just identifiers.
             * This allows for nested array access like matrix[i][j] and
             * function calls returning arrays like func()[i]. */
            
            /* consume '[' and parse the index expression. */
            consume(p, TOK_L_SQB);

            AstNode *index = parse_expression(p, err);
            if (!index) {
                /* parse_expression sets err on failure. primary still needs freeing. */
                ast_node_free(primary);
                return NULL;
            }

            if (!consume(p, TOK_R_SQB)) {
                if (err) create_parse_error(err, "expected ']' after array index", p);
                ast_node_free(primary);
                ast_node_free(index);
                return NULL;
            }

            /* Create subscript node and use primary as the target */
            AstNode *array_access = ast_create_node(AST_SUBSCRIPT_EXPR);
            if (!array_access) {
                if (err) create_parse_error(err, "out of memory creating array access node", p);
                ast_node_free(primary);
                ast_node_free(index);
                return NULL;
            }

            /* Set target and index directly */
            array_access->data.subscript_expr.target = primary;
            array_access->data.subscript_expr.index = index;
            primary = array_access;
        } else if (token->type == TOK_LPAREN) {
            /* Function call: primary(args) */
            consume(p, TOK_LPAREN);

            /* Create function call node */
            AstNode *func_call = ast_create_node(AST_CALL_EXPR);
            if (!func_call) {
                if (err) create_parse_error(err, "out of memory creating function call node", p);
                ast_node_free(primary);
                return NULL;
            }

            astnode_array_init(&func_call->data.call_expr.args);

        
            /* Parse arguments */
            if(!parse_argument_list(p, func_call, err)){
                ast_node_free(primary);
                return NULL;
            }

            if(!consume(p, TOK_RPAREN)){
                if (err) create_parse_error(err, "expected ')' after function arguments", p);
                ast_node_free(primary);
                ast_node_free(func_call);
                return NULL;
            }
            

            func_call->data.call_expr.callee = primary;
            primary = func_call;
        }
        
        token = current_token(p);
    }

    return primary;
}

LiteralType get_literal_type(TokenType type) {
    switch (type) {
        case TOK_INTEGER: return INT_LITERAL;
        case TOK_FLOAT:   return FLOAT_LITERAL;
        case TOK_TRUE:
        case TOK_FALSE:  return BOOL_LITERAL;
        default:         return LIT_UNKNOWN; // not a literal type
    }
}

/*  <Primary> ::= INTEGER | FLOAT | BOOLEAN
             | CHAR_LITERAL | IDENTIFIER
             | LPAREN <Expression> RPAREN*/
AstNode *parse_primary(Parser *p, ParseError *err) {
    Token *token = current_token(p);
    if (!token) {
        if (err) create_parse_error(err, "unexpected end of input in primary expression", p);
        return NULL;
    }

    switch (token->type) {
        case TOK_INTEGER:
        case TOK_FLOAT:
        case TOK_TRUE:
        case TOK_FALSE:{
            /* Parse literals */
            AstNode *literal = ast_create_node(AST_LITERAL);
            if (!literal) {
                if (err) create_parse_error(err, "out of memory creating literal node", p);
                return NULL;
            }

            literal->data.literal.type = get_literal_type(token->type);
            if (literal->data.literal.type == LIT_UNKNOWN) {
                if (err) create_parse_error(err, "unexpected token type for literal", p);
                ast_node_free(literal);
                return NULL;
            }
            literal->data.literal.value = token->lexeme ? strdup(token->lexeme) : NULL;
            consume(p, token->type);
            return literal;
        }
        
        case TOK_IDENTIFIER: {
            /* Parse identifiers */
            AstNode *identifier = ast_create_node(AST_IDENTIFIER);
            if (!identifier) {
                if (err) create_parse_error(err, "out of memory creating identifier node", p);
                return NULL;
            }
            identifier->data.identifier.identifier = token->lexeme ? strdup(token->lexeme) : NULL;
            consume(p, TOK_IDENTIFIER);
            return identifier;
        }
        
        case TOK_LPAREN: {
            /* Parse parenthesized expressions: ( <Expression> ) */
            consume(p, TOK_LPAREN);
            AstNode *expr = parse_expression(p, err);
            if (!expr) return NULL; /* err already set */
            
            if (!consume(p, TOK_RPAREN)) {
                if (err) create_parse_error(err, "expected ')' after parenthesized expression", p);
                ast_node_free(expr);
                return NULL;
            }
            return expr;
        }
        
        default:
            if (err) create_parse_error(err, "expected primary expression (literal, identifier, or parenthesized expression)", p);
            return NULL;
    }
}


int parse_argument_list(Parser *p, AstNode* call, ParseError *err) {

    Token *tok = current_token(p);
    /* empty arglist: ')' immediately */
    if (!tok) {
        if (err) create_parse_error(err, "unexpected end of input in argument list", p);
        return 0;
    }

    if (tok->type == TOK_RPAREN) {
        return 1;
    }

    /* parse first argument (and subsequent ones separated by commas) */
    while (1) {
        tok = current_token(p);
        if (!tok) {
            if (err) create_parse_error(err, "unexpected end of input in argument list", p);
            return 0;
        }

        AstNode *argument = NULL;
        /* allow initializer-list as an argument */
        if (tok->type == TOK_L_BRACE) {
            argument = parse_initializer_list(p, NULL, err);
            if (!argument) {
                /* parse_initializer_list sets err and frees partial nodes */
                return 0;
            }
        } else {
            argument = parse_expression(p, err);
            if (!argument) {
                return 0; /* parse_expression sets err */
            }
        }

        /* push the pointer *address* into the dynarray (store AstNode*) */
        if (astnode_array_push(&call->data.call_expr.args, argument) != 0) {
            if (err) create_parse_error(err, "out of memory pushing argument", p);
            ast_node_free(argument);
            return 0;
        }

        tok = current_token(p);
        if (!tok) {
            if (err) fail_with(p, call, err, "unexpected end of input in argument list");
            return 0;
        }

        if (tok->type == TOK_RPAREN) {
            /* done, caller will consume ')' */
            break;
        }

        /* expect comma and continue */
        if (!consume(p, TOK_COMMA)) {
            if (err) fail_with(p, call, err, "expected a ',' or ')'");
            return 0;
        }

        /* loop to parse next argument */
    }

    return 1;
}

//<ParamList> ::= <Param> { COMMA <Param> }
//<Param>     ::= IDENTIFIER COLON <Type>
int parse_parameter_list(Parser *p, AstNode *func_decl, ParseError *err) {
    if (!func_decl || func_decl->node_type != AST_FUNCTION_DECLARATION) {
        if (err) create_parse_error(err, "invalid function declaration node for parameter list", p);
        return 0;
    }

    Token *tok = current_token(p);
    if (!tok) {
        if (err) create_parse_error(err, "unexpected end of input in parameter list", p);
        return 0;
    }

    /* empty parameter list: caller consumes the ')' */
    if (tok->type == TOK_RPAREN) {
        return 1; // nothing to parse
    }

    /* parse parameters */
    while (1) {
        /* refresh token at loop start */
        tok = current_token(p);
        if (!tok) {
            if (err) create_parse_error(err, "unexpected end of input in parameter list", p);
            return 0;
        }

        if (tok->type != TOK_IDENTIFIER) {
            if (err) create_parse_error(err, "expected identifier for parameter name", p);
            return 0;
        }

        /* create and fill a new parameter node */
        AstNode *param = ast_create_node(AST_PARAM);
        if (!param) {
            if (err) create_parse_error(err, "out of memory creating parameter node", p);
            return 0;
        }

        param->data.param.name = tok->lexeme ? strdup(tok->lexeme) : NULL;
        if (tok->lexeme && !param->data.param.name) {
            ast_node_free(param);
            if (err) create_parse_error(err, "out of memory copying parameter name", p);
            return 0;
        }

        consume(p, TOK_IDENTIFIER);

        if (!consume(p, TOK_COLON)) {
            if (err) create_parse_error(err, "expected ':' after parameter name", p);
            ast_node_free(param);
            return 0;
        }

        param->data.param.type = parse_type(p, err);
        if (!param->data.param.type) {
            ast_node_free(param);
            return 0; /* parse_type set err */
        }

        /* push the parameter into the function declaration */
        if (astnode_array_push(&func_decl->data.function_declaration.params, param) != 0) {
            if (err) create_parse_error(err, "out of memory pushing parameter", p);
            ast_node_free(param);
            return 0;
        }

        /* look ahead: either ')' (done) or ',' (another param) */
        tok = current_token(p);
        if (!tok) {
            if (err) create_parse_error(err, "unexpected end of input in parameter list", p);
            return 0;
        }

        if (tok->type == TOK_RPAREN) {
            /* done, caller will consume ')' */
            break;
        }

        if (!consume(p, TOK_COMMA)) {
            if (err) fail_with(p, func_decl, err, "expected a ',' or ')'");
            return 0;
        }

        /* loop to parse next parameter (tok will be refreshed at top) */
    }

    return 1; // successfully parsed parameter list
}


AstNode *parse_statement(Parser *p, ParseError *err) {
    Token *tok = current_token(p);
    if (!tok) {
        if (err) create_parse_error(err, "unexpected end of input in statement", p);
        return NULL;
    }

    AstNode *stmt = NULL;
    switch (tok->type) {
        case TOK_IF:
            stmt = parse_if_statement(p, err);
            break;
        case TOK_WHILE:
            stmt = parse_while_statement(p, err);
            break;
        case TOK_FOR:
            stmt = parse_for_statement(p, err);
            break;
        case TOK_RETURN:
            stmt = parse_return_statement(p, err);
            break;
        case TOK_BREAK:
            stmt = parse_break_statement(p, err);
            break;
        case TOK_CONTINUE:
            stmt = parse_continue_statement(p, err);
            break;
        case TOK_L_BRACE:
            stmt = parse_block(p, err);
            break;
        case TOK_IDENTIFIER: {
            Token *next = peek(p, 1);
            if (!next) {
                if (err) create_parse_error(err, "unexpected end of input after identifier", p);
                return NULL;
            }
            if (next->type == TOK_COLON) {
                /* variable declaration statement */
                stmt = parse_declaration_stmt(p, err);
            } else {
                /* expression statement (covers function calls, assignments, etc.) */
                stmt = parse_expression_statement(p, err);
            }
            break;
        }            
        default:
            /* treat as expression statement */
            stmt = parse_expression_statement(p, err);
            break;
    }

    return stmt;
}

AstNode *parse_if_statement(Parser *p, ParseError *err) {
    if(!consume(p, TOK_IF)) {
        if (err) create_parse_error(err, "expected 'if' keyword", p);
        return NULL;
    }

    AstNode *if_stmt = ast_create_node(AST_IF_STATEMENT);
    if (!if_stmt) {
        if (err) create_parse_error(err, "out of memory creating if statement node", p);
        return NULL;
    }

    Token *tok = current_token(p);
    if (!tok || tok->type != TOK_LPAREN) {
        if (err) create_parse_error(err, "expected '(' after 'if'", p);
        ast_node_free(if_stmt);
        return NULL;
    }
    consume(p, TOK_LPAREN);

    /* parse the condition expression */
    if_stmt->data.if_statement.condition = parse_expression(p, err);
    if (!if_stmt->data.if_statement.condition) {
        ast_node_free(if_stmt);
        return NULL;
    }

    if (!consume(p, TOK_RPAREN)) {
        if (err) create_parse_error(err, "expected ')' after if condition", p);
        ast_node_free(if_stmt);
        return NULL;
    }

    /* parse the then block */
    if_stmt->data.if_statement.then_branch = parse_block(p, err);
    if (!if_stmt->data.if_statement.then_branch) {
        ast_node_free(if_stmt);
        return NULL;
    }

    /* parse the optional else (else if or else block) */
    if (consume(p, TOK_ELSE)) {
        Token *next = current_token(p);
        if (!next) {
            if (err) create_parse_error(err, "unexpected end after 'else'", p);
            ast_node_free(if_stmt);
            return NULL;
        }

        if (next->type == TOK_IF) {
            /* else-if: delegate to parse_if_statement (it will consume 'if') */
            AstNode *else_if = parse_if_statement(p, err);
            if (!else_if) {
                ast_node_free(if_stmt);
                return NULL;
            }
            if_stmt->data.if_statement.else_branch = else_if;
        } else {
            /* else block: parse_block consumes the '{' */
            if_stmt->data.if_statement.else_branch = parse_block(p, err);
            if (!if_stmt->data.if_statement.else_branch) {
                ast_node_free(if_stmt);
                return NULL;
            }
        }
    }

    return if_stmt;
}


AstNode *parse_while_statement(Parser *p, ParseError *err) {
    if (!consume(p, TOK_WHILE)) {
        if (err) create_parse_error(err, "expected 'while' keyword", p);
        return NULL;
    }

    AstNode *while_stmt = ast_create_node(AST_WHILE_STATEMENT);
    if (!while_stmt) {
        if (err) create_parse_error(err, "out of memory creating while statement node", p);
        return NULL;
    }

    Token *tok = current_token(p);
    if (!tok || tok->type != TOK_LPAREN) {
        if (err) create_parse_error(err, "expected '(' after 'while'", p);
        ast_node_free(while_stmt);
        return NULL;
    }
    consume(p, TOK_LPAREN);

    /* parse the condition expression */
    while_stmt->data.while_statement.condition = parse_expression(p, err);
    if (!while_stmt->data.while_statement.condition) {
        ast_node_free(while_stmt);
        return NULL;
    }

    if (!consume(p, TOK_RPAREN)) {
        if (err) create_parse_error(err, "expected ')' after while condition", p);
        ast_node_free(while_stmt);
        return NULL;
    }

    /* parse the body block */
    while_stmt->data.while_statement.body = parse_block(p, err);
    if (!while_stmt->data.while_statement.body) {
        ast_node_free(while_stmt);
        return NULL;
    }

    return while_stmt;
}

AstNode *parse_for_statement(Parser *p, ParseError *err) {
    if (!consume(p, TOK_FOR)) {
        if (err) create_parse_error(err, "expected 'for' keyword", p);
        return NULL;
    }

    AstNode *for_stmt = ast_create_node(AST_FOR_STATEMENT);
    if (!for_stmt) {
        if (err) create_parse_error(err, "out of memory creating for statement node", p);
        return NULL;
    }

    /* expect '(' */
    Token *tok = current_token(p);
    if (!tok || tok->type != TOK_LPAREN) {
        if (err) create_parse_error(err, "expected '(' after 'for'", p);
        ast_node_free(for_stmt);
        return NULL;
    }
    consume(p, TOK_LPAREN);

    /* --- init slot --- */
    for_stmt->data.for_statement.init = NULL;
    tok = current_token(p);
    if (!tok) {
        if (err) create_parse_error(err, "unexpected end of input in for-init", p);
        ast_node_free(for_stmt);
        return NULL;
    }

    if (tok->type == TOK_SEMICOLON) {
        /* empty init */
        consume(p, TOK_SEMICOLON);
    } else if (tok->type == TOK_IDENTIFIER && peek(p, 1) && peek(p,1)->type == TOK_COLON) {
        /* variable declaration form (IDENTIFIER ':' <Type> ...) */
        AstNode *decl = parse_variable_declaration(p, err);
        if (!decl) {
            ast_node_free(for_stmt);
            return NULL; /* parse_variable_declaration sets err */
        }
        for_stmt->data.for_statement.init = decl;

        /* require the semicolon separator after a declaration */
        if (!consume(p, TOK_SEMICOLON)) {
            if (err) {
                err->underline_previous_token_line = 1;
                create_parse_error(err, "expected ';' after for-init declaration", p);
            }
            ast_node_free(for_stmt); /* this will also free decl via for_stmt->init */
            return NULL;
        }
    }else {
        /* single expression init */
        AstNode *init_expr = parse_expression(p, err);
        if (!init_expr) {
            ast_node_free(for_stmt);
            return NULL; /* parse_expression sets err */
        }
        for_stmt->data.for_statement.init = init_expr;

        if (!consume(p, TOK_SEMICOLON)) {
            if (err) {
                err->underline_previous_token_line = 1;
                create_parse_error(err, "expected ';' after for-init expression", p);
            }
            ast_node_free(for_stmt); /* this will also free init_expr via for_stmt->init */
            return NULL;
        }
    }

    /* --- condition slot --- */
    for_stmt->data.for_statement.condition = NULL;
    tok = current_token(p);
    if (!tok) {
        if (err) create_parse_error(err, "unexpected end of input in for-condition", p);
        ast_node_free(for_stmt);
        return NULL;
    }

    if (tok->type == TOK_SEMICOLON) {
        /* empty condition => treated as true */
        consume(p, TOK_SEMICOLON);
        for_stmt->data.for_statement.condition = NULL;
    } else {
        AstNode *cond = parse_expression(p, err);
        if (!cond) {
            ast_node_free(for_stmt);
            return NULL; /* parse_expression sets err */
        }
        for_stmt->data.for_statement.condition = cond;

        if (!consume(p, TOK_SEMICOLON)) {
            if (err) {
                err->underline_previous_token_line = 1;
                create_parse_error(err, "expected ';' after for-condition", p);
            }
            ast_node_free(for_stmt); /* this will also free cond via for_stmt->condition */
            return NULL;
        }
    }

    /* --- post slot --- */
    for_stmt->data.for_statement.post = NULL;
    tok = current_token(p);
    if (!tok) {
        if (err) create_parse_error(err, "unexpected end of input in for-post", p);
        ast_node_free(for_stmt);
        return NULL;
    }

    if (tok->type == TOK_RPAREN) {
        /* empty post */
        consume(p, TOK_RPAREN);
    } else {
        AstNode *post_expr = parse_expression(p, err);
        if (!post_expr) {
            ast_node_free(for_stmt);
            return NULL; /* parse_expression sets err */
        }

        /* expect closing ')' */
        if (!consume(p, TOK_RPAREN)) {
            if (err) create_parse_error(err, "expected ')' after for-post expression", p);
            ast_node_free(post_expr);
            ast_node_free(for_stmt);
            return NULL;
        }
        for_stmt->data.for_statement.post = post_expr;
    }

    /* --- body --- */
    AstNode *body = parse_block(p, err);
    if (!body) {
        ast_node_free(for_stmt);
        return NULL; /* parse_block sets err */
    }

    return for_stmt;
}


AstNode *parse_return_statement(Parser *p, ParseError *err) {
    if(!consume(p, TOK_RETURN)){
        if (err) create_parse_error(err, "expected 'return' keyword", p);
        return NULL;
    }

    AstNode *return_stmt = ast_create_node(AST_RETURN_STATEMENT);
    if (!return_stmt) {
        if (err) create_parse_error(err, "out of memory creating return statement node", p);
        return NULL;
    }

    /* parse optional expression, then require semicolon */
    Token *tok = current_token(p);
    if (tok && tok->type != TOK_SEMICOLON) {
        /* parse expression (not expression-statement, because that would consume semicolon itself) */
        return_stmt->data.return_statement.expression = parse_expression(p, err);
        if (!return_stmt->data.return_statement.expression) {
            ast_node_free(return_stmt);
            return NULL;
        }
    } else {
        return_stmt->data.return_statement.expression = NULL;
    }

    /* require terminating semicolon */
    if (!consume(p, TOK_SEMICOLON)) {
        if (err) {
            err->underline_previous_token_line = 1;
            create_parse_error(err, "expected ';' after return", p);
        }
        ast_node_free(return_stmt);
        return NULL;
    }

    return return_stmt;
}


AstNode *parse_break_statement(Parser *p, ParseError *err) {
    if (!consume(p, TOK_BREAK)) {
        if (err) create_parse_error(err, "expected 'break' keyword", p);
        return NULL;
    }

    AstNode *break_stmt = ast_create_node(AST_BREAK_STATEMENT);
    if (!break_stmt) {
        if (err) create_parse_error(err, "out of memory creating break statement node", p);
        return NULL;
    }

    /* require semicolon */
    if (!consume(p, TOK_SEMICOLON)) {
        if (err) {
            err->underline_previous_token_line = 1; // underline previous token line
            create_parse_error(err, "expected ';' after break", p);
        }
        ast_node_free(break_stmt);
        return NULL;
    }

    return break_stmt;
}

AstNode *parse_continue_statement(Parser *p, ParseError *err) {
    if (!consume(p, TOK_CONTINUE)) {
        if (err) create_parse_error(err, "expected 'continue' keyword", p);
        return NULL;
    }

    AstNode *continue_stmt = ast_create_node(AST_CONTINUE_STATEMENT);
    if (!continue_stmt) {
        if (err) create_parse_error(err, "out of memory creating continue statement node", p);
        return NULL;
    }

    /* require semicolon */
    if (!consume(p, TOK_SEMICOLON)) {
        if (err) {
            err->underline_previous_token_line = 1; // underline previous token line
            create_parse_error(err, "expected ';' after continue", p);
        }
        ast_node_free(continue_stmt);
        return NULL;
    }

    return continue_stmt;
}

