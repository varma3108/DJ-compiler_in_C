/* File ast.h: Abstract-syntax-tree data structure for DJ */

#ifndef AST_H
#define AST_H

#include <stdlib.h>

/* define types of AST nodes */
typedef enum {
  /* program, class, field, and method declarations: */
  PROGRAM, 
  CLASS_DECL_LIST, /* class declarations */
  FINAL_CLASS_DECL,
  NONFINAL_CLASS_DECL,
  VAR_DECL_LIST, /* variable declarations */
  VAR_DECL,   
  METHOD_DECL_LIST, /* method declarations */
  FINAL_METHOD_DECL,
  NONFINAL_METHOD_DECL, 
  /* types, including generic IDs: */
  NAT_TYPE, 
  AST_ID, 
  /* expression-lists: */
  EXPR_LIST,
  /* expressions: */
  DOT_METHOD_CALL_EXPR, /* E.ID(E) */
  METHOD_CALL_EXPR,     /* ID(E) */
  DOT_ID_EXPR,          /* E.ID */
  ID_EXPR,              /* ID */
  DOT_ASSIGN_EXPR,      /* E.ID = E */
  ASSIGN_EXPR,          /* ID = E */
  PLUS_EXPR,            /* E + E */
  MINUS_EXPR,           /* E - E */
  TIMES_EXPR,           /* E * E */
  EQUALITY_EXPR,        /* E==E */
  LESS_THAN_EXPR,       /* E < E */
  NOT_EXPR,             /* !E */
  OR_EXPR,              /* E||E */
  ASSERT_EXPR,          /* assert E */
  IF_THEN_ELSE_EXPR,    /* if(E) {Es} else {Es} */
  WHILE_EXPR,           /* while(E) {Es} */
  PRINT_EXPR,           /* printNat(E) */
  READ_EXPR,            /* readNat() */
  THIS_EXPR,            /* this */
  NEW_EXPR,             /* new */
  NULL_EXPR,            /* null */
  NAT_LITERAL_EXPR,     /* N */
} ASTNodeType;

/* define a list of AST nodes */
typedef struct astlistnode {
  struct astnode *data;
  struct astlistnode *next;
} ASTList;

/* define the actual AST nodes */
typedef struct astnode {
  ASTNodeType typ;
  /* list of children nodes: */
  ASTList *children; /* head of the list of children */
  ASTList *childrenTail;
  /* which source-program line does this node end on: */
  unsigned int lineNumber;
  /* node attributes: */
  unsigned int natVal;
  char *idVal;
  /* Node attributes used on the first 6 kinds of expressions enumerated above
    (E.ID(E), ID(E), E.ID, ID, E.ID = E, and ID = E).
    These attributes get set during type checking and used during code gen,
    so code gen doesn't duplicate the work of the type checker. 
    The attributes store the statically determined class and member number
    of an ID that refers to a method or variable.  
    When these attributes are all 0, the ID refers not to a member of a class
    but instead to a local/parameter variable. */
  unsigned int staticClassNum; /* class number in which this member resides */
  unsigned int staticMemberNum; /* when set to i, this member is the ith 
                                   method/var in the staticClassNum-th class */
} ASTree;


/* METHODS TO CREATE AND MANIPULATE THE AST */
/* Create a new AST node of type t having only one child.
   (That is, create a one-node list of children for the new tree.)
   If the child argument is NULL, then the single list node in the 
   new AST node's list of children will have a NULL data field.
   If t is NAT_LITERAL_EXPR then the proper natAttribute should be
   given; otherwise natAttribute is ignored.
   If t is AST_ID then the proper idAttribute should be given;
   otherwise idAttribute is ignored.
*/
ASTree *newAST(ASTNodeType t, ASTree *child, unsigned int natAttribute, 
  char *idAttribute, unsigned int lineNum);

/* Append an AST node onto a parent's list of children */
void appendToChildrenList(ASTree *parent, ASTree *newChild);

/* Print the AST to stdout with indentations marking tree depth. */
void printAST(ASTree *t);

#endif