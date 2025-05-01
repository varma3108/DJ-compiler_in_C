#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "ast.h"

void printError(char *reason) {
  printf("AST Error: %s\n", reason);
  exit(-1);
}

ASTree *newAST(ASTNodeType t, ASTree *child, unsigned int natAttribute, 
  char *idAttribute, unsigned int lineNum) {

  //creates tree root and sets type
  ASTree *toReturn = malloc(sizeof(ASTree));
  if(toReturn==NULL) printError("malloc in newAST()");
    toReturn->typ = t;
    
  //creates child list
  ASTList *childList = malloc(sizeof(ASTList));
  if(childList==NULL) printError("malloc in newAST()");
    
  //sets first child and sets next
  childList->data = child;
  childList->next = NULL;

  //sets children head and tail
  toReturn->children = childList;
  toReturn->childrenTail = childList;

  //sets line number
  toReturn->lineNumber = lineNum;

  //sets attribute nat
  toReturn->natVal = natAttribute;
    
  //sets attribute id
  if(idAttribute == NULL)
    toReturn->idVal = NULL;
  else {
    char *copyStr = malloc(strlen(idAttribute)+1);
    if(copyStr==NULL) printError("malloc in newAST()");
    strcpy(copyStr, idAttribute);
    toReturn->idVal = copyStr;
  }
  
  // Initialize static class and member attributes to 0
  toReturn->staticClassNum = 0;
  toReturn->staticMemberNum = 0;

  //returns new tree
  return toReturn;
}

void appendToChildrenList(ASTree *parent, ASTree *newChild) {

  //checks parameters
  if(parent==NULL) printError("append called with null parent");
  if(parent->children==NULL || parent->childrenTail==NULL)
  printError("append called with bad parent");
  if(newChild==NULL) printError("append called with null newChild");

  //replace empty tail
  if(parent->childrenTail->data == NULL)
    parent->childrenTail->data = newChild;

  //append to current tail
  else {
    
    //creates new list
    ASTList *newList = malloc(sizeof(ASTList));
    if(newList==NULL) printError("malloc in appendAST()");

    //appends to tail list
    newList->data = newChild;
    newList->next = NULL;
    parent->childrenTail->next = newList;
    parent->childrenTail = newList;
  }
}

/* Print the type of this node and any node attributes */
void printNodeTypeAndAttribute(ASTree *t) {
  if(t==NULL) return;
  switch(t->typ) {
    case PROGRAM: printf("PROGRAM   (ends on line %u)", t->lineNumber); break;
    case CLASS_DECL_LIST: printf("CLASS_DECL_LIST   (ends on line %u)", t->lineNumber); break;
    case FINAL_CLASS_DECL: printf("FINAL_CLASS_DECL   (ends on line %u)", t->lineNumber); break;
    case NONFINAL_CLASS_DECL: printf("NONFINAL_CLASS_DECL   (ends on line %u)", t->lineNumber); break;
    case VAR_DECL_LIST: printf("VAR_DECL_LIST   (ends on line %u)", t->lineNumber); break;
    case VAR_DECL: printf("VAR_DECL   (ends on line %u)", t->lineNumber); break;
    case METHOD_DECL_LIST: printf("METHOD_DECL_LIST   (ends on line %u)", t->lineNumber); break;
    case FINAL_METHOD_DECL: printf("FINAL_METHOD_DECL   (ends on line %u)", t->lineNumber); break;
    case NONFINAL_METHOD_DECL: printf("NONFINAL_METHOD_DECL   (ends on line %u)", t->lineNumber); break;
    case NAT_TYPE: printf("NAT_TYPE   (ends on line %u)",t->lineNumber); break;
    case AST_ID: printf("AST_ID(%s)   (ends on line %u)",t->idVal, t->lineNumber); break;
    case EXPR_LIST: printf("EXPR_LIST   (ends on line %u)", t->lineNumber); break;

    //id expressions
    case DOT_METHOD_CALL_EXPR: printf("DOT_METHOD_CALL_EXPR   (ends on line %u)", t->lineNumber); break;
    case METHOD_CALL_EXPR: printf("METHOD_CALL_EXPR   (ends on line %u)", t->lineNumber); break;
    case DOT_ID_EXPR: printf("DOT_ID_EXPR   (ends on line %u)", t->lineNumber); break;
    case ID_EXPR: printf("ID_EXPR   (ends on line %u)", t->lineNumber); break;
    case DOT_ASSIGN_EXPR: printf("DOT_ASSIGN_EXPR   (ends on line %u)", t->lineNumber); break;
    case ASSIGN_EXPR: printf("ASSIGN_EXPR   (ends on line %u)", t->lineNumber); break;

    case PLUS_EXPR: printf("PLUS_EXPR   (ends on line %u)", t->lineNumber); break;
    case MINUS_EXPR: printf("MINUS_EXPR   (ends on line %u)", t->lineNumber); break;
    case TIMES_EXPR: printf("TIMES_EXPR   (ends on line %u)", t->lineNumber); break;
    case EQUALITY_EXPR: printf("EQUALITY_EXPR   (ends on line %u)", t->lineNumber); break;
    case LESS_THAN_EXPR: printf("LESS_THAN_EXPR   (ends on line %u)", t->lineNumber); break;

    case NOT_EXPR: printf("NOT_EXPR   (ends on line %u)", t->lineNumber); break;
    case OR_EXPR: printf("OR_EXPR   (ends on line %u)", t->lineNumber); break;
    case ASSERT_EXPR: printf("ASSERT_EXPR   (ends on line %u)", t->lineNumber); break;
    case IF_THEN_ELSE_EXPR: printf("IF_THEN_ELSE_EXPR   (ends on line %u)", t->lineNumber); break;
    case WHILE_EXPR: printf("WHILE_EXPR   (ends on line %u)", t->lineNumber); break;
    case PRINT_EXPR: printf("PRINT_EXPR   (ends on line %u)", t->lineNumber); break;

    case READ_EXPR: printf("READ_EXPR   (ends on line %u)", t->lineNumber); break;
    case THIS_EXPR: printf("THIS_EXPR   (ends on line %u)", t->lineNumber); break;
    case NEW_EXPR: printf("NEW_EXPR   (ends on line %u)", t->lineNumber); break;
    case NULL_EXPR: printf("NULL_EXPR   (ends on line %u)", t->lineNumber); break;
    case NAT_LITERAL_EXPR: printf("NAT_LITERAL_EXPR(%u)   (ends on line %u)", t->natVal, t->lineNumber); break;

    default: printError("unknown node type in printNodeTypeAndAttribute()");
  }  
}

/* print tree in preorder */
void printASTree(ASTree *t, int depth) {
  if(t==NULL) return;
  printf("%d:",depth);
  int i=0;
  for(; i<depth; i++) printf("  ");
  printNodeTypeAndAttribute(t);
  printf("\n");
  //recursively print all children
  ASTList *childListIterator = t->children;
  while(childListIterator!=NULL) {
    printASTree(childListIterator->data, depth+1);
    childListIterator = childListIterator->next;
  }
}

/* Print the AST to stdout with indentations marking tree depth. */
void printAST(ASTree *t) { printASTree(t, 0); }