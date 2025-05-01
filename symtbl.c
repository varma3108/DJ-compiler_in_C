/* symtbl.c */
#include "symtbl.h"
#include <string.h>
#include <stdlib.h>

/* Forward declaration for internal helper */
static int typeNameToNumber(char *typeName);

/* Global variables */
ASTree *wholeProgram;
ASTree *mainExprs;
int numMainBlockLocals;
VarDecl *mainBlockST;
int numClasses;
ClassDecl *classesST;

/* Utility function: counts the number of children in an AST node list */
int getNum(ASTree *t) {
    ASTList *p = t->children;
    int count = 0;
    while(p && p->data) {
        count++;
        p = p->next;
    }
    return count;
}

/* Utility function: extracts variable declarations from an AST node into an array */
void getVars(ASTree *t, VarDecl *v, int num) {
    ASTList *p = t->children;
    int i;
    for(i = 0; i < num; i++) {
        /* Extract the type from the first child */
        v[i].type = typeNameToNumber(p->data->children->data->idVal);
        v[i].typeLineNumber = p->data->children->data->lineNumber;
        /* Extract the variable name from the second child */
        v[i].varName = p->data->children->next->data->idVal;
        v[i].varNameLineNumber = p->data->children->next->data->lineNumber;
        p = p->next;
    }
}

/* Sets up the global symbol tables using the full program AST */
void setupSymbolTables(ASTree *fullProgramAST) {

    /* Set the global pointer */
    wholeProgram = fullProgramAST;

    /* Count the number of classes: Object is always present so start at 1 */
    numClasses = 1;
    ASTList *p = wholeProgram->children->data->children;
    while(p && p->data) {
        numClasses++;
        p = p->next;
    }

    /* Allocate the symbol table for classes; index 0 is reserved for Object */
    classesST = (ClassDecl*)calloc(numClasses, sizeof(ClassDecl));
    classesST[0].className = "Object";
    classesST[0].classNameLineNumber = -1;
    classesST[0].superclass = -3;  /* Object has no superclass */
    classesST[0].superclassLineNumber = -1;
    classesST[0].isFinal = 0;
    classesST[0].numVars = 0;
    classesST[0].varList = NULL;
    classesST[0].numMethods = 0;
    classesST[0].methodList = NULL;

    /* Process the remaining classes */
    int i, j, check;
    ASTList *mvp;
    p = wholeProgram->children->data->children;
    for(i = 1; i < numClasses; i++) {

        classesST[i].className = p->data->children->data->idVal;
        classesST[i].classNameLineNumber = p->data->children->data->lineNumber;
        /* Determine final status by checking the AST node type:
           FINAL_CLASS_DECL means final; NONFINAL_CLASS_DECL means not final. */
        classesST[i].isFinal = (p->data->typ == FINAL_CLASS_DECL) ? 1 : 0;
        /* Get the number of variables and allocate variable table */
        classesST[i].numVars = getNum(p->data->children->next->next->data);
        classesST[i].varList = (VarDecl*)calloc(classesST[i].numVars, sizeof(VarDecl));
        /* Get the number of methods and allocate method table */
        classesST[i].numMethods = getNum(p->data->children->next->next->next->data);
        classesST[i].methodList = (MethodDecl*)calloc(classesST[i].numMethods, sizeof(MethodDecl));

        p = p->next;
    }

    /* Fill in the superclass and class variable information */
    p = wholeProgram->children->data->children;
    for(i = 1; i < numClasses; i++) {

        check = 0;
        classesST[i].superclassLineNumber = p->data->children->next->data->lineNumber;
        for(j = 0; j < numClasses; j++) {
            if(!strcmp(p->data->children->next->data->idVal, classesST[j].className)) {
                classesST[i].superclass = j;
                check = 1;
                break;
            }
        }
        if(!check)
            classesST[i].superclass = -4;  /* Superclass not found */

        /* Extract the class's variables */
        getVars(p->data->children->next->next->data, classesST[i].varList, classesST[i].numVars);
        p = p->next;
    }

    /* Process methods for each class */
    p = wholeProgram->children->data->children;
    for(i = 1; i < numClasses; i++) {
        /* Assume the method list is in the following AST node */
        mvp = p->data->children->next->next->next->data->children;
        for(j = 0; j < classesST[i].numMethods; j++) {

            /* Process method name and return type */
            classesST[i].methodList[j].methodName = mvp->data->children->next->data->idVal;
            classesST[i].methodList[j].methodNameLineNumber = mvp->data->children->next->data->lineNumber;
            classesST[i].methodList[j].returnType = typeNameToNumber(mvp->data->children->data->idVal);
            classesST[i].methodList[j].returnTypeLineNumber = mvp->data->children->data->lineNumber;
            /* Determine if the method is final by checking its AST node type.
               FINAL_METHOD_DECL means final; NONFINAL_METHOD_DECL means not final. */
            classesST[i].methodList[j].isFinal = (mvp->data->typ == FINAL_METHOD_DECL) ? 1 : 0;
            
            /* Process the parameter list:
               According to the header, there is at most one parameter. */
            ASTree *paramList = mvp->data->children->next->next->data;
            if(getNum(paramList) > 0) {
                classesST[i].methodList[j].paramType =
                    typeNameToNumber(paramList->children->data->children->data->idVal);
                classesST[i].methodList[j].paramTypeLineNumber =
                    paramList->children->data->children->data->lineNumber;
                classesST[i].methodList[j].paramName =
                    paramList->children->data->children->next->data->idVal;
                classesST[i].methodList[j].paramNameLineNumber =
                    paramList->children->data->children->next->data->lineNumber;
            } else {
                classesST[i].methodList[j].paramName = NULL;
                classesST[i].methodList[j].paramNameLineNumber = -1;
                classesST[i].methodList[j].paramType = -3;   /* Indicates no parameter type */
                classesST[i].methodList[j].paramTypeLineNumber = -1;
            }
            
            /* Process method local variables */
            classesST[i].methodList[j].numLocals = getNum(mvp->data->children->next->next->next->data);
            classesST[i].methodList[j].localST = (VarDecl*)calloc(classesST[i].methodList[j].numLocals, sizeof(VarDecl));
            getVars(mvp->data->children->next->next->next->data,
                    classesST[i].methodList[j].localST,
                    classesST[i].methodList[j].numLocals);

            /* Set the method body expression list */
            classesST[i].methodList[j].bodyExprs =
                mvp->data->children->next->next->next->next->data;

            mvp = mvp->next;
        }
        p = p->next;
    }

    /* Set up main block information */
    mainExprs = wholeProgram->children->next->next->data;
    numMainBlockLocals = getNum(wholeProgram->children->next->data);
    mainBlockST = (VarDecl*)calloc(numMainBlockLocals, sizeof(VarDecl));
    getVars(wholeProgram->children->next->data, mainBlockST, numMainBlockLocals);
}

/* Converts a type name into the appropriate type number.
   Returns 0 for Object; a positive integer for declared classes;
   -1 for nat; -2 for any-object; -3 for illegal type; and -4 if not found. */
static int typeNameToNumber(char *typeName) {
    if(!typeName)
        return -1;
    int i;
    for(i = 0; i < numClasses; i++) {
        if(!strcmp(typeName, classesST[i].className))
            return i;
    }
    return -4;
}


