#pragma once

#include "ast.h"
#include "parser.h"

/* ---------------------------
   Top-level
   ---------------------------
   <Program> ::= { <Declaration> }
*/
AstNode *parse_program(Parser *p, ParseError *err);

/* ---------------------------
   Declarations
   ---------------------------
   <Declaration> ::= <VariableDeclarationStmt>
                   | <FunctionDeclaration>

   <VariableDeclaration> ::=
       IDENTIFIER COLON [ CONST ] <Type> [ ASSIGN ( <Expression> | <InitializerList> ) ] 
   <VariableDeclarationStmt> ::= <VariableDeclaration> SEMICOLON

   <FunctionDeclaration> ::= FN IDENTIFIER LPAREN [ <ParamList> ] RPAREN
                             [ ARROW <Type> ] <Block>
*/
AstNode *parse_declaration(Parser *p, ParseError *err);
AstNode *parse_declaration_stmt(Parser *p, ParseError *err);

AstNode *parse_variable_declaration(Parser *p, ParseError *err);
AstNode *parse_function_declaration(Parser *p, ParseError *err);

/* ---------------------------
   Parameters & arguments
   ---------------------------
   <ParamList> ::= <Param> { COMMA <Param> }
   <Param>     ::= IDENTIFIER COLON <Type>

   <ArgList> ::= (<Expression> | <InitializerList>) { COMMA (<Expression> | <InitializerList>) }
   (FunctionCall: IDENTIFIER LPAREN <ArgList> RPAREN)
*/
int parse_parameter_list(Parser *p, AstNode *func_decl, ParseError *err);
int parse_argument_list(Parser *p, AstNode *call, ParseError *err);

/* ---------------------------
   Types
   ---------------------------
   <Type>        ::= [ CONST ] <TypeAtom> { <TypeSuffix> }
   <TypeAtom>    ::= BaseType | LPAREN <Type> RPAREN | FunctionType
   <TypeSuffix>  ::= STAR | L_SQB [ <ConstExpr> ] R_SQB
   FunctionType  ::= FN LPAREN [ <TypeList> ] RPAREN [ ARROW <Type>]
   <TypeList>    ::= <Type> { COMMA <Type> }
   BaseType      ::= I32 | I64 | BOOL | F32 | F64 | IDENTIFIER
*/
AstNode *parse_type(Parser *p, ParseError *err);
int parse_type_atom(Parser *p, AstNode *type_node, ParseError *err);
int parse_function_type_atom(Parser *p, AstNode *type_node, ParseError *err);
int parse_function_type_inline(Parser *p, AstNode *type_node, ParseError *err);
int parse_type_suffix(Parser *p, AstNode *type_node, ParseError *err);
int parse_postfix_type(Parser *p, AstNode *type_node, ParseError *err);

/* ---------------------------
   Initializers
   ---------------------------
   <InitializerList> ::= L_BRACE [ <InitElements> ] R_BRACE

   <InitElements> ::= <InitElement> { COMMA <InitElement> }
   <InitElement>  ::= <Expression> | <InitializerList>
*/
AstNode *parse_initializer_list(Parser *p, AstNode *type_node, ParseError *err);

/* ---------------------------
   Blocks & statements
   ---------------------------
   <Block> ::= L_BRACE { <Statement> } R_BRACE

   <Statement> ::= <Block>        | <IfStmt> 
                 | <WhileStmt>    | <ForStmt> 
                 | <ReturnStmt>   | <BreakStmt> 
                 | <ContinueStmt> | <VariableDeclarationStmt> 
                 | <ExprStmt>     | <FunctionCall>

   <IfStmt>    ::= IF LPAREN <Expression> RPAREN <Block> [ ELSE <Block> ]
   <WhileStmt> ::= WHILE LPAREN <Expression> RPAREN <Block>
   <ForStmt>   ::= FOR LPAREN [ <ForInit> ] SEMICOLON [ <Expression> ] SEMICOLON [ <Expression> ] RPAREN <Block>
                 where <ForInit> ::= <VariableDeclaration> | <Expression>

   <ReturnStmt>   ::= RETURN [ <Expression> ] SEMICOLON
   <BreakStmt>    ::= BREAK SEMICOLON
   <ContinueStmt> ::= CONTINUE SEMICOLON
   <ExprStmt>     ::= <Expression> SEMICOLON
*/
AstNode *parse_block(Parser *p, ParseError *err);
AstNode *parse_statement(Parser *p, ParseError *err);
AstNode *parse_expression_statement(Parser *p, ParseError *err);

AstNode *parse_if_statement(Parser *p, ParseError *err);
AstNode *parse_while_statement(Parser *p, ParseError *err);
AstNode *parse_for_statement(Parser *p, ParseError *err);
AstNode *parse_return_statement(Parser *p, ParseError *err);
AstNode *parse_break_statement(Parser *p, ParseError *err);
AstNode *parse_continue_statement(Parser *p, ParseError *err);

/* ---------------------------
   Expressions
   ---------------------------
   <Expression> ::= <Assignment> | <LogicalOr>
   <Assignment>  ::= <Lvalue> <AssignOp> <Expression>
   <Lvalue>      ::= IDENTIFIER | STAR <Lvalue> | LPAREN <Lvalue> RPAREN
   <AssignOp>    ::= ASSIGN | PLUS_EQ | MINUS_EQ | STAR_EQ | SLASH_EQ | PERCENT_EQ

   <LogicalOr>  ::= <LogicalAnd> { OR_OR <LogicalAnd> }
   <LogicalAnd> ::= <Equality>   { AND_AND <Equality> }
   <Equality>   ::= <Relational> { ( EQ_EQ | BANG_EQ ) <Relational> }
   <Relational> ::= <Additive>   { ( LT | GT | LT_EQ | GT_EQ ) <Additive> }
   <Additive>       ::= <Multiplicative> { ( PLUS | MINUS ) <Multiplicative> }
   <Multiplicative> ::= <Unary>          { ( STAR | SLASH | PERCENT ) <Unary> }

   <Unary>   ::= <PrefixOp> <Unary> | <Postfix>
   <PrefixOp> ::= PLUS | MINUS | BANG | STAR | AMP | PLUSPLUS | MINUSMINUS

   <Postfix> ::= <Primary> { <PostfixOp> }
   <PostfixOp> ::= PLUSPLUS | MINUSMINUS
                | L_SQB <Expression> R_SQB
                | LPAREN [ <ArgList> ] RPAREN

   <Primary> ::= INTEGER | FLOAT | BOOLEAN | CHAR_LITERAL | IDENTIFIER | LPAREN <Expression> RPAREN
*/
AstNode *parse_expression(Parser *p, ParseError *err);
AstNode *parse_assignment(Parser *p, AstNode *lhs, ParseError *err);

AstNode *parse_logical_or(Parser *p, ParseError *err);
AstNode *parse_logical_and(Parser *p, ParseError *err);
AstNode *parse_equality(Parser *p, ParseError *err);
AstNode *parse_relational(Parser *p, ParseError *err);
AstNode *parse_additive(Parser *p, ParseError *err);
AstNode *parse_multiplicative(Parser *p, ParseError *err);

AstNode *parse_unary(Parser *p, ParseError *err);
AstNode *parse_postfix(Parser *p, ParseError *err);
AstNode *parse_primary(Parser *p, ParseError *err);

/* ---------------------------
   Constant expressions (for array sizes, etc.)
   ---------------------------
   <ConstExpr>    ::= <ConstAdd>
   <ConstAdd>     ::= <ConstMul> { ( PLUS | MINUS ) <ConstMul> }
   <ConstMul>     ::= <ConstUnary> { ( STAR | SLASH | PERCENT ) <ConstUnary> }
   <ConstUnary>   ::= ( PLUS | MINUS | PLUSPLUS | MINUSMINUS ) <ConstUnary> | <ConstPostfix>
   <ConstPostfix> ::= <ConstPrimary> { <PostfixOp> }
   <ConstPrimary> ::= INTEGER | IDENTIFIER | LPAREN <ConstExpr> RPAREN
*/
AstNode *parse_const_expr(Parser *p, ParseError *err);