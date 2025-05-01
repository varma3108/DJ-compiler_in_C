/* DJ PARSER */

%code provides {
  #include <stdio.h>
  #include "lex.yy.c"
  #include "ast.h"
  #include "symtbl.h"
  #include "typecheck.h"
  #include "codegen.h"  /* Include codegen.h header */

  /* Symbols in this grammar are represented as ASTs */ 
  #define YYSTYPE ASTree *

  /* Declare global AST for entire program */
  ASTree *pgmAST;

  /* Function for printing generic syntax-error messages */
  void yyerror(const char *str) {
    printf("Syntax error on line %d at token %s\n", yylineno, yytext);
    printf("(This version of the compiler exits after finding the first ");
    printf("syntax error.)\n");
    exit(-1);
  }
}

%token MAIN CLASS EXTENDS NATTYPE IF ELSE WHILE
%token PRINTNAT READNAT THIS NEW NUL NATLITERAL 
%token ID ASSIGN PLUS MINUS TIMES EQUALITY LESS
%token OR NOT DOT SEMICOLON COMMA LBRACE RBRACE 
%token LPAREN RPAREN ENDOFFILE FINAL ASSERT

%start pgm

/* operator association and precedence */
/* Note: we want assignment to have the lowest precedence, OR next, then
   LESS, and finally EQUALITY (so that EQUALITY binds tighter than LESS),
   followed by additive and multiplicative operators; then unary NOT;
   then ASSERT and finally DOT. */
%right ASSIGN
%left OR
%nonassoc LESS
%nonassoc EQUALITY
%left PLUS MINUS
%left TIMES
%right NOT
%nonassoc ASSERT /* Add ASSERT at a high precedence to avoid conflicts */
%left DOT

%%

/* program start */
pgm : cdec MAIN LBRACE vardec elist RBRACE ENDOFFILE 
      { $$ = newAST(PROGRAM, $1, 0, NULL, yylineno); pgmAST = $$; 
        appendToChildrenList($$, $4);
        appendToChildrenList($$, $5);
        return 0; }
    ;

/* class declarations */
cdec : cdec CLASS id EXTENDS id LBRACE vardec methdec RBRACE
       { appendToChildrenList($1, newAST(NONFINAL_CLASS_DECL, $3, 0, NULL, yylineno)); 
         appendToChildrenList($1->childrenTail->data, $5);
         appendToChildrenList($1->childrenTail->data, $7);
         appendToChildrenList($1->childrenTail->data, $8); }
     | cdec FINAL CLASS id EXTENDS id LBRACE vardec methdec RBRACE
       { appendToChildrenList($1, newAST(FINAL_CLASS_DECL, $4, 0, NULL, yylineno)); 
         appendToChildrenList($1->childrenTail->data, $6);
         appendToChildrenList($1->childrenTail->data, $8);
         appendToChildrenList($1->childrenTail->data, $9); }
     | cdec CLASS id EXTENDS id LBRACE vardec RBRACE
       { appendToChildrenList($1, newAST(NONFINAL_CLASS_DECL, $3, 0, NULL, yylineno)); 
         appendToChildrenList($1->childrenTail->data, $5);
         appendToChildrenList($1->childrenTail->data, $7);
         appendToChildrenList($1->childrenTail->data, newAST(METHOD_DECL_LIST, NULL, 0, NULL, yylineno)); }
     | cdec FINAL CLASS id EXTENDS id LBRACE vardec RBRACE
       { appendToChildrenList($1, newAST(FINAL_CLASS_DECL, $4, 0, NULL, yylineno)); 
         appendToChildrenList($1->childrenTail->data, $6);
         appendToChildrenList($1->childrenTail->data, $8);
         appendToChildrenList($1->childrenTail->data, newAST(METHOD_DECL_LIST, NULL, 0, NULL, yylineno)); }
     |
       { $$ = newAST(CLASS_DECL_LIST, NULL, 0, NULL, yylineno); }
     ;

/* var declarations */
vardec : vardec nat id SEMICOLON
         { appendToChildrenList($1, newAST(VAR_DECL, $2, 0, NULL, yylineno)); 
           appendToChildrenList($1->childrenTail->data, $3); }
       | vardec id id SEMICOLON
         { appendToChildrenList($1, newAST(VAR_DECL, $2, 0, NULL, yylineno));
           appendToChildrenList($1->childrenTail->data, $3); }
       |
         { $$ = newAST(VAR_DECL_LIST, NULL, 0, NULL, yylineno); }
       ;

/* nat type */
nat : NATTYPE
      { $$ = newAST(NAT_TYPE, NULL, 0, NULL, yylineno); }
    ;

/* id */
id : ID
     { $$ = newAST(AST_ID, NULL, 0, yytext, yylineno); }
   ;

/* method declarations - modified to always have exactly one parameter */
methdec : methdec nat id LPAREN spdec RPAREN LBRACE vardec elist RBRACE
          { appendToChildrenList($1, newAST(NONFINAL_METHOD_DECL, $2, 0, NULL, yylineno));
            appendToChildrenList($1->childrenTail->data, $3);
            appendToChildrenList($1->childrenTail->data, $5);
            appendToChildrenList($1->childrenTail->data, $8);
            appendToChildrenList($1->childrenTail->data, $9); }
        | methdec FINAL nat id LPAREN spdec RPAREN LBRACE vardec elist RBRACE
          { appendToChildrenList($1, newAST(FINAL_METHOD_DECL, $3, 0, NULL, yylineno));
            appendToChildrenList($1->childrenTail->data, $4);
            appendToChildrenList($1->childrenTail->data, $6);
            appendToChildrenList($1->childrenTail->data, $9);
            appendToChildrenList($1->childrenTail->data, $10); }
        | methdec id id LPAREN spdec RPAREN LBRACE vardec elist RBRACE
          { appendToChildrenList($1, newAST(NONFINAL_METHOD_DECL, $2, 0, NULL, yylineno));
            appendToChildrenList($1->childrenTail->data, $3);
            appendToChildrenList($1->childrenTail->data, $5);
            appendToChildrenList($1->childrenTail->data, $8);
            appendToChildrenList($1->childrenTail->data, $9); }
        | methdec FINAL id id LPAREN spdec RPAREN LBRACE vardec elist RBRACE
          { appendToChildrenList($1, newAST(FINAL_METHOD_DECL, $3, 0, NULL, yylineno));
            appendToChildrenList($1->childrenTail->data, $4);
            appendToChildrenList($1->childrenTail->data, $6);
            appendToChildrenList($1->childrenTail->data, $9);
            appendToChildrenList($1->childrenTail->data, $10); }
        | nat id LPAREN spdec RPAREN LBRACE vardec elist RBRACE
          { $$ = newAST(METHOD_DECL_LIST, newAST(NONFINAL_METHOD_DECL, $1, 0, NULL, yylineno), 0, NULL, yylineno);
            appendToChildrenList($$->childrenTail->data, $2);
            appendToChildrenList($$->childrenTail->data, $4);
            appendToChildrenList($$->childrenTail->data, $7);
            appendToChildrenList($$->childrenTail->data, $8); }
        | FINAL nat id LPAREN spdec RPAREN LBRACE vardec elist RBRACE
          { $$ = newAST(METHOD_DECL_LIST, newAST(FINAL_METHOD_DECL, $2, 0, NULL, yylineno), 0, NULL, yylineno);
            appendToChildrenList($$->childrenTail->data, $3);
            appendToChildrenList($$->childrenTail->data, $5);
            appendToChildrenList($$->childrenTail->data, $8);
            appendToChildrenList($$->childrenTail->data, $9); }
        | id id LPAREN spdec RPAREN LBRACE vardec elist RBRACE
          { $$ = newAST(METHOD_DECL_LIST, newAST(NONFINAL_METHOD_DECL, $1, 0, NULL, yylineno), 0, NULL, yylineno);
            appendToChildrenList($$->childrenTail->data, $2);
            appendToChildrenList($$->childrenTail->data, $4);
            appendToChildrenList($$->childrenTail->data, $7);
            appendToChildrenList($$->childrenTail->data, $8); }
        | FINAL id id LPAREN spdec RPAREN LBRACE vardec elist RBRACE
          { $$ = newAST(METHOD_DECL_LIST, newAST(FINAL_METHOD_DECL, $2, 0, NULL, yylineno), 0, NULL, yylineno);
            appendToChildrenList($$->childrenTail->data, $3);
            appendToChildrenList($$->childrenTail->data, $5);
            appendToChildrenList($$->childrenTail->data, $8);
            appendToChildrenList($$->childrenTail->data, $9); }
        ;

/* Single parameter declaration - exactly one parameter required */
spdec : nat id
        { $$ = newAST(VAR_DECL_LIST, newAST(VAR_DECL, $1, 0, NULL, yylineno), 0, NULL, yylineno); 
          appendToChildrenList($$->childrenTail->data, $2); }
     | id id
       { $$ = newAST(VAR_DECL_LIST, newAST(VAR_DECL, $1, 0, NULL, yylineno), 0, NULL, yylineno); 
         appendToChildrenList($$->childrenTail->data, $2); }
     ;

/* expression list */
elist : elist expr SEMICOLON
        { appendToChildrenList($1, $2); }
      | expr SEMICOLON
        { $$ = newAST(EXPR_LIST, $1, 0, NULL, yylineno); }
      ;

/* argument expression list */
alist : expr
        { $$ = newAST(EXPR_LIST, $1, 0, NULL, yylineno); }
      |
        { $$ = newAST(EXPR_LIST, NULL, 0, NULL, yylineno); }
      ;

/* expressions */
expr : expr PLUS expr
       { $$ = newAST(PLUS_EXPR, $1, 0, NULL, yylineno); 
         appendToChildrenList($$, $3); }
     | expr MINUS expr
       { $$ = newAST(MINUS_EXPR, $1, 0, NULL, yylineno); 
         appendToChildrenList($$, $3); }
     | expr TIMES expr
       { $$ = newAST(TIMES_EXPR, $1, 0, NULL, yylineno); 
         appendToChildrenList($$, $3); }
     | expr EQUALITY expr
       { $$ = newAST(EQUALITY_EXPR, $1, 0, NULL, yylineno); 
         appendToChildrenList($$, $3); }
     | expr LESS expr
       { $$ = newAST(LESS_THAN_EXPR, $1, 0, NULL, yylineno); 
         appendToChildrenList($$, $3); }
     | NOT expr
       { $$ = newAST(NOT_EXPR, $2, 0, NULL, yylineno); }
     | expr OR expr
       { $$ = newAST(OR_EXPR, $1, 0, NULL, yylineno);
         appendToChildrenList($$, $3); }
     | ASSERT expr %prec ASSERT
       { $$ = newAST(ASSERT_EXPR, $2, 0, NULL, yylineno); }
     | NATLITERAL
       { $$ = newAST(NAT_LITERAL_EXPR, NULL, atoi(yytext), NULL, yylineno); }
     | NUL
       { $$ = newAST(NULL_EXPR, NULL, 0, NULL, yylineno); }
     | IF LPAREN expr RPAREN LBRACE elist RBRACE ELSE LBRACE elist RBRACE
       { $$ = newAST(IF_THEN_ELSE_EXPR, $3, 0, NULL, yylineno);
         appendToChildrenList($$, $6);
         appendToChildrenList($$, $10); }
     | WHILE LPAREN expr RPAREN LBRACE elist RBRACE
       { $$ = newAST(WHILE_EXPR, $3, 0, NULL, yylineno);
         appendToChildrenList($$, $6); }
     | NEW id LPAREN RPAREN
       { $$ = newAST(NEW_EXPR, $2, 0, NULL, yylineno); }
     | THIS
       { $$ = newAST(THIS_EXPR, NULL, 0, NULL, yylineno); }
     | PRINTNAT LPAREN expr RPAREN
       { $$ = newAST(PRINT_EXPR, $3, 0, NULL, yylineno); }
     | READNAT LPAREN RPAREN
       { $$ = newAST(READ_EXPR, NULL, 0, NULL, yylineno); }
     | id
       { $$ = newAST(ID_EXPR, $1, 0, NULL, yylineno); }
     | expr DOT id
       { $$ = newAST(DOT_ID_EXPR, $1, 0, NULL, yylineno);
         appendToChildrenList($$, $3); }
     | id ASSIGN expr
       { $$ = newAST(ASSIGN_EXPR, $1, 0, NULL, yylineno);
         appendToChildrenList($$, $3); }
     | expr DOT id ASSIGN expr
       { $$ = newAST(DOT_ASSIGN_EXPR, $1, 0, NULL, yylineno); 
         appendToChildrenList($$, $3); 
         appendToChildrenList($$, $5); }
     | id LPAREN alist RPAREN
       { $$ = newAST(METHOD_CALL_EXPR, $1, 0, NULL, yylineno); 
         appendToChildrenList($$, $3); }
     | expr DOT id LPAREN alist RPAREN
       { $$ = newAST(DOT_METHOD_CALL_EXPR, $1, 0, NULL, yylineno); 
         appendToChildrenList($$, $3);
         appendToChildrenList($$, $5); }
     | LPAREN expr RPAREN
       { $$ = $2; }
     ;

%%


