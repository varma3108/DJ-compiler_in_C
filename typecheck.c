/* typecheck.c */
#include "typecheck.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define DEBUG printf("\nDEBUG\n");

// Extended semantic error codes:
typedef enum {
    MULTI_CLASS_DECL, MULTI_VAR_DECL, CLASS_SUPER, VAR_DECL_TYPE, CLASS_SUPER_SAME,
    CLASS_VAR_TYPE, METH_RET, METH_VARS, METH_PARAM, CLASS_CYCLE, ASSIGN, ID_DNE,
    NOT, NEW, THIS_MAIN, PLUS, MINUS, TIMES, AND, LESS, EQUALITY, DOT_ID_L, DOT_ID_R,
    CLASS_VAR_DNE, PRINT, DOT_ASSIGN_L, DOT_ASSIGN_R, WHILE, CLASS_VAR_DUPE,
    CLASS_VAR_DUPE_SUPER, IF_COND, IF_EL, METH_CLASS_UNIQUE, METH_SIG, METH_RET_MIS,
    PARAM_DUPE, METH_LOCAL_DUPE, DOT_METH_CALL, OBJ_INIT,
    FINAL_CLASS_EXTENDS,   // error for extending a final class
    METH_OVERRIDE_FINAL    // error for overriding a final method
} SemError;

void semerror(SemError er, int l) {
    printf("Semantic analysis error on line %d:\n", l);
    switch(er) {
        case MULTI_CLASS_DECL: printf("Class declared multiple times\n"); break;
        case MULTI_VAR_DECL: printf("Variable declared multiple times\n"); break;
        case CLASS_SUPER: printf("Class's Superclass does not exist\n"); break;
        case VAR_DECL_TYPE: printf("Variable's class/object type does not exist\n"); break;
        case CLASS_SUPER_SAME: printf("Class can't be superclass of itself\n"); break;
        case CLASS_VAR_TYPE: printf("Class field type does not exist\n"); break;
        case METH_RET: printf("Method return type does not exist\n"); break;
        case METH_VARS: printf("Method local type does not exist\n"); break;
        case METH_PARAM: printf("Method parameter type does not exist\n"); break;
        case CLASS_CYCLE: printf("Class has cycle with its super\n"); break;
        case ASSIGN: printf("Assignment to incompatible types\n"); break;
        case ID_DNE: printf("ID never declared\n"); break;
        case NOT: printf("Non-nat type in NOT expression\n"); break;
        case NEW: printf("Invalid class for new expression\n"); break;
        case THIS_MAIN: printf("This can't be used in main\n"); break;
        case PLUS: printf("Plus expression using non-nat types\n"); break;
        case MINUS: printf("Minus expression using non-nat types\n"); break;
        case TIMES: printf("Times expression using non-nat types\n"); break;
        case AND: printf("And expression using non-nat types\n"); break;
        case LESS: printf("Less expression using non-nat types\n"); break;
        case EQUALITY: printf("Equality expression type mismatch\n"); break;
        case DOT_ID_L: printf("Class on left side of dot id invalid type\n"); break;
        case DOT_ID_R: printf("Id in dot id is not valid\n"); break;
        case CLASS_VAR_DNE: printf("Class/Object variable is not valid\n"); break;
        case PRINT: printf("printNat() argument invalid type, must be nat\n"); break;
        case DOT_ASSIGN_L: printf("Class on left side is not valid\n"); break;
        case DOT_ASSIGN_R: printf("Type on right side of assign not compatible with left side\n"); break;
        case WHILE: printf("While conditional isn't type nat\n"); break;
        case CLASS_VAR_DUPE: printf("Class variable duplicate\n"); break;
        case CLASS_VAR_DUPE_SUPER: printf("Class variable duplicate of a superclass\n"); break;
        case IF_COND: printf("If conditional doesn't evaluate to a nat type\n"); break;
        case IF_EL: printf("If then-else expr list type mismatch\n"); break;
        case METH_CLASS_UNIQUE: printf("Duplicate method names in class\n"); break;
        case METH_SIG: printf("Method signature mismatch with superclass method\n"); break;
        case METH_RET_MIS: printf("Last method expression is not compatible with its return type\n"); break;
        case PARAM_DUPE: printf("Parameter variable name in method is a duplicate\n"); break;
        case METH_LOCAL_DUPE: printf("Method local variable is a duplicate\n"); break;
        case DOT_METH_CALL: printf("Invalid dot-method call\n"); break;
        case OBJ_INIT: printf("Object hasn't been initialized yet\n"); break;
        case FINAL_CLASS_EXTENDS: printf("Cannot extend a final class\n"); break;
        case METH_OVERRIDE_FINAL: printf("Overriding a final method\n"); break;
        default: printf("Generic Error, check line\n");
    }
    exit(-1);
}

/* Use the globals defined in symtbl.c */
extern ASTree *wholeProgram;
extern ASTree *mainExprs;
extern int numMainBlockLocals;
extern VarDecl *mainBlockST;
extern int numClasses;
extern ClassDecl *classesST;

int *mainInits;
int **classInits;

/* Forward declarations for functions used in methInClass */
int isSubtype(int sub, int super);
int typeExpr(ASTree *t, int classContainingExpr, int methodContainingExpr);

/* A structure to hold two integers */
typedef struct {
    int c;
    int m;
} Pair;

/* Helper Functions */
int join(int t1, int t2) {
    if(isSubtype(t1, t2))
        return t2;
    if(isSubtype(t2, t1))
        return t1;
    return join(classesST[t1].superclass, t2);
}

int inMainST(char *var, int l) {
    int i;
    for(i = 0; i < numMainBlockLocals; i++) {
        if(!strcmp(var, mainBlockST[i].varName))
            return mainBlockST[i].type;
    }
    semerror(ID_DNE, l);
    exit(-1);
}

int varHash(char *var, int c) {
    int i;
    if(c == 0)
        return -1;
    if(c < 0) {
        for(i = 0; i < numMainBlockLocals; i++) {
            if(!strcmp(var, mainBlockST[i].varName))
                return i;
        }
    }
    if(c > 0) {
        for(i = 0; i < classesST[c].numVars; i++) {
            if(!strcmp(var, classesST[c].varList[i].varName))
                return i;
        }
    }
    return -1;
}

int inClassST(char *var) {
    int i;
    for(i = 0; i < numClasses; i++) {
        if(!strcmp(var, classesST[i].className))
            return i;
    }
    return -1;
}

int varInClass(int c, char* var) {
    if(c == 0)
        return -4;
    int i;
    for(i = 0; i < classesST[c].numVars; i++) {
        if(!strcmp(classesST[c].varList[i].varName, var))
            return classesST[c].varList[i].type;
    }
    return varInClass(classesST[c].superclass, var);
}

/* Revised to use a single parameter */
int varInParam(int c, int m, char* var) {
    if (classesST[c].methodList[m].paramName != NULL &&
        strcmp(classesST[c].methodList[m].paramName, var) == 0)
         return classesST[c].methodList[m].paramType;
    return -4;
}

int varInClassDupeChk(int c, char* var) {
    if(c == 0)
        return 0;
    int i;
    for(i = 0; i < classesST[c].numVars; i++) {
        if(!strcmp(classesST[c].varList[i].varName, var))
            return 1;
    }
    return varInClassDupeChk(classesST[c].superclass, var);
}

/* 
   Check for method conflicts in the superclass chain.
   Returns:
     0 if no conflicting method is found,
     1 if a method with the same name but a different signature is found,
     2 if the method in the superclass is marked final.
*/
int methUnique(int c, MethodDecl meth) {
    if(c == 0)
        return 0;
    int i;
    int thisHasParam, methHasParam;
    for(i = 0; i < classesST[c].numMethods; i++) {
        if(!strcmp(classesST[c].methodList[i].methodName, meth.methodName)) {
            if(classesST[c].methodList[i].isFinal)
                return 2;  /* Error: overriding a final method */
            if(classesST[c].methodList[i].returnType != meth.returnType)
                return 1;
            thisHasParam = (classesST[c].methodList[i].paramName != NULL);
            methHasParam = (meth.paramName != NULL);
            if(thisHasParam != methHasParam)
                return 1;
            if(thisHasParam && (classesST[c].methodList[i].paramType != meth.paramType))
                return 1;
        }
    }
    return methUnique(classesST[c].superclass, meth);
}

int varInMeth(int c, int m, char *var) {
    int i;
    for(i = 0; i < classesST[c].methodList[m].numLocals; i++) {
        if(!strcmp(classesST[c].methodList[m].localST[i].varName, var))
            return classesST[c].methodList[m].localST[i].type;
    }
    return -4;
}

int varInMethNum(int c, int m, char *var) {
    int i;
    for(i = 0; i < classesST[c].methodList[m].numLocals; i++) {
        if(!strcmp(classesST[c].methodList[m].localST[i].varName, var))
            return i;
    }
    return -4;
}

Pair methInClass(int c, char *var, ASTree* argl, int ogclass, int ogmeth) {
    Pair ar;
    ar.c = -1;  // Default to error values
    ar.m = -1;
    
    if(c < 1) {  // If no valid class exists, return error
        return ar;
    }
    
    int i, argCount = 0;
    
    // Count arguments if any exist
    if(argl != NULL && argl->children != NULL) {
        ASTList *p = argl->children;
        while(p && p->data) {
            argCount++;
            p = p->next;
        }
    }
    
    // Search for matching method in this class
    for(i = 0; i < classesST[c].numMethods; i++) {
        if(!strcmp(classesST[c].methodList[i].methodName, var)) {
            // Found a method with matching name
            int signatureMatches = 1;  // Assume match until proven otherwise
            ar.c = c;
            ar.m = i;
            
            // Check parameter compatibility
            if(classesST[c].methodList[i].paramName == NULL) {
                // Method has no parameters, verify call has no arguments
                if(argCount != 0) {
                    signatureMatches = 0;  // Arguments provided when none expected
                }
            } else {
                // Method has one parameter, verify call has one argument of compatible type
                if(argCount != 1) {
                    signatureMatches = 0;  // Wrong number of arguments
                } else {
                    // Check type compatibility of the argument
                    int argType = typeExpr(argl->children->data, ogclass, ogmeth);
                    int paramType = classesST[c].methodList[i].paramType;
                    
                    // Check if argument type is compatible with parameter type
                    if(paramType >= 0) {  // Object type
                        if(!isSubtype(argType, paramType)) {
                            signatureMatches = 0;  // Incompatible type
                        }
                    } else if(paramType != argType) {  // Primitive type
                        signatureMatches = 0;  // Types don't match
                    }
                }
            }
            
            if(signatureMatches) {
                return ar;  // Found a match
            }
        }
    }
    
    // If no match in this class, check superclass
    return methInClass(classesST[c].superclass, var, argl, ogclass, ogmeth);
}

int getMethRet(int c, int m) {
    return classesST[c].methodList[m].returnType;
}

void updateST(int classContainingExpr, int methodContainingExpr, int rhs, char *var) {
    int i;
    if(classContainingExpr < 0) {
        for(i = 0; i < numMainBlockLocals; i++) {
            if(!strcmp(mainBlockST[i].varName, var))
                mainBlockST[i].type = rhs;
        }
    }
    if(classContainingExpr > 0) {
        for(i = 0; i < classesST[classContainingExpr].numVars; i++) {
            if(!strcmp(classesST[classContainingExpr].varList[i].varName, var))
                classesST[classContainingExpr].varList[i].type = rhs;
        }
    }
}

/* Main typechecking function */
void typecheckProgram() {
    int i, j, k, l;
    char *si, *sj;
    
    mainInits = (int*)malloc(sizeof(int) * numMainBlockLocals);
    memset(mainInits, 0, sizeof(int) * numMainBlockLocals);

    classInits = (int**)malloc(sizeof(int*) * numClasses);
    for(i = 0; i < numClasses; i++) {
        j = classesST[i].numVars;
        classInits[i] = (int*)malloc(sizeof(int) * j);
        memset(classInits[i], 0, sizeof(int) * j);
    }
    
    /* Check that no class extends a final class */
    for(i = 1; i < numClasses; i++) {
        int super = classesST[i].superclass;
        if(super >= 0 && classesST[super].isFinal)
            semerror(FINAL_CLASS_EXTENDS, classesST[i].classNameLineNumber);
    }
    
    /* Validate classes: unique names, valid superclasses, cycles, and types */
    for(i = 0; i < numClasses; i++) {
        si = classesST[i].className;
        for(j = i + 1; j < numClasses; j++) {
            sj = classesST[j].className;
            if(i == 0 && j == classesST[j].superclass)
                semerror(CLASS_SUPER_SAME, classesST[j].classNameLineNumber);
            if(i == 0 && classesST[j].superclass < 0)
                semerror(CLASS_SUPER, classesST[j].classNameLineNumber);
            if(!strcmp(si, sj))
                semerror(MULTI_CLASS_DECL, classesST[j].classNameLineNumber);
        }
        if(!isSubtype(i, 0))
            semerror(CLASS_CYCLE, classesST[i].superclassLineNumber);
        for(j = 0; j < classesST[i].numVars; j++) {
            if(classesST[i].varList[j].type < -1)
                semerror(CLASS_VAR_TYPE, classesST[i].varList[j].typeLineNumber);
        }
        for(j = 0; j < classesST[i].numMethods; j++) {
            if(classesST[i].methodList[j].returnType < -1)
                semerror(METH_RET, classesST[i].methodList[j].returnTypeLineNumber);
            for(k = 0; k < classesST[i].methodList[j].numLocals; k++) {
                if(classesST[i].methodList[j].localST[k].type < -1)
                    semerror(METH_VARS, classesST[i].methodList[j].localST[k].varNameLineNumber);
            }
            if(classesST[i].methodList[j].paramName != NULL &&
               classesST[i].methodList[j].paramType < -1)
                semerror(METH_PARAM, classesST[i].methodList[j].paramTypeLineNumber);
        }
    }
    
    /* Check class variables for duplicates */
    for(i = 0; i < numClasses; i++) {
        for(j = 0; j < classesST[i].numVars; j++) {
            for(k = j + 1; k < classesST[i].numVars; k++) {
                if(!strcmp(classesST[i].varList[j].varName, classesST[i].varList[k].varName))
                    semerror(CLASS_VAR_DUPE, classesST[i].varList[k].varNameLineNumber);
            }
            if(varInClassDupeChk(classesST[i].superclass, classesST[i].varList[j].varName))
                semerror(CLASS_VAR_DUPE_SUPER, classesST[i].varList[j].varNameLineNumber);
        }
    }
    
    /* Check method signature uniqueness and correctness */
    for(i = 0; i < numClasses; i++) {
        for(j = 0; j < classesST[i].numMethods; j++) {
            for(k = j + 1; k < classesST[i].numMethods; k++) {
                if(!strcmp(classesST[i].methodList[j].methodName, classesST[i].methodList[k].methodName))
                    semerror(METH_CLASS_UNIQUE, classesST[i].methodList[k].methodNameLineNumber);
            }
            k = methUnique(classesST[i].superclass, classesST[i].methodList[j]);
            if(k == 1)
                semerror(METH_SIG, classesST[i].methodList[j].methodNameLineNumber);
            else if(k == 2)
                semerror(METH_OVERRIDE_FINAL, classesST[i].methodList[j].methodNameLineNumber);
            
            /* Check that the method's body evaluates to a type matching its return type */
            k = typeExprs(classesST[i].methodList[j].bodyExprs, i, j);
            if(classesST[i].methodList[j].returnType != k &&
               !(isSubtype(k, classesST[i].methodList[j].returnType))) {
                int errLine = classesST[i].methodList[j].methodNameLineNumber;
                if(classesST[i].methodList[j].bodyExprs && 
                   classesST[i].methodList[j].bodyExprs->childrenTail &&
                   classesST[i].methodList[j].bodyExprs->childrenTail->data)
                    errLine = classesST[i].methodList[j].bodyExprs->childrenTail->data->lineNumber;
                semerror(METH_RET_MIS, errLine);
            }
        }
    }
    
    /* Check method parameters do not duplicate local variables */
    for(i = 0; i < numClasses; i++) {
        for(j = 0; j < classesST[i].numMethods; j++) {
            if(classesST[i].methodList[j].paramName != NULL) {
                int dupIdx = varInMethNum(i, j, classesST[i].methodList[j].paramName);
                if(dupIdx != -4)
                    semerror(PARAM_DUPE, classesST[i].methodList[j].localST[dupIdx].varNameLineNumber);
            }
            for(k = 0; k < classesST[i].methodList[j].numLocals; k++) {
                for(l = k + 1; l < classesST[i].methodList[j].numLocals; l++) {
                    if(!strcmp(classesST[i].methodList[j].localST[k].varName,
                                classesST[i].methodList[j].localST[l].varName))
                        semerror(METH_LOCAL_DUPE, classesST[i].methodList[j].localST[l].varNameLineNumber);
                }
            }
        }
    }
    
    /* Check main block variables for duplicates and valid types */
    for(i = 0; i < numMainBlockLocals; i++) {
        si = mainBlockST[i].varName;
        if(i == 0 && mainBlockST[i].type < -1)
            semerror(VAR_DECL_TYPE, mainBlockST[i].typeLineNumber);
        for(j = i + 1; j < numMainBlockLocals; j++) {
            sj = mainBlockST[j].varName;
            if(i == 0 && mainBlockST[j].type < -1)
                semerror(VAR_DECL_TYPE, mainBlockST[j].typeLineNumber);
            if(!strcmp(si, sj))
                semerror(MULTI_VAR_DECL, mainBlockST[j].varNameLineNumber);
        }
    }
    
    /* Typecheck the main expression list */
    typeExprs(mainExprs, -1, -1);
}

int isSubtype(int sub, int super) {
    if(sub == -1 || super == -1)
        return 0;
    if(sub == -2)
        return 1;
    if(sub == super)
        return 1;
    if(sub == 0)
        return 0;
    if(super == -2)
        return 0;
    int ar[numClasses];
    memset(ar, 0, sizeof(int) * numClasses);
    while(sub != super) {
        if(ar[sub])
            return 0;
        ar[sub] = 1;
        sub = classesST[sub].superclass;
    }
    return 1;
}

int typeExpr(ASTree *t, int classContainingExpr, int methodContainingExpr) {
    int lhs, rhs, mid;
    ASTList *p;
    Pair cm;
    switch(t->typ) {
        case ASSIGN_EXPR:
            if(t->children == NULL || t->children->next == NULL)
                semerror(ASSIGN, t->lineNumber);
            rhs = typeExpr(t->children->next->data, classContainingExpr, methodContainingExpr);
            lhs = typeExpr(t->children->data, classContainingExpr, methodContainingExpr);
            if(lhs == rhs || isSubtype(rhs, lhs))
                return lhs;
            semerror(ASSIGN, t->lineNumber);
        case PLUS_EXPR:
            if(t->children == NULL || t->children->next == NULL)
                semerror(PLUS, t->lineNumber);
            lhs = typeExpr(t->children->data, classContainingExpr, methodContainingExpr);
            rhs = typeExpr(t->children->next->data, classContainingExpr, methodContainingExpr);
            if(lhs == -1 && rhs == -1)
                return lhs;
            semerror(PLUS, t->lineNumber);
        case MINUS_EXPR:
            if(t->children == NULL || t->children->next == NULL)
                semerror(MINUS, t->lineNumber);
            lhs = typeExpr(t->children->data, classContainingExpr, methodContainingExpr);
            rhs = typeExpr(t->children->next->data, classContainingExpr, methodContainingExpr);
            if(lhs == -1 && rhs == -1)
                return lhs;
            semerror(MINUS, t->lineNumber);
        case TIMES_EXPR:
            if(t->children == NULL || t->children->next == NULL)
                semerror(TIMES, t->lineNumber);
            lhs = typeExpr(t->children->data, classContainingExpr, methodContainingExpr);
            rhs = typeExpr(t->children->next->data, classContainingExpr, methodContainingExpr);
            if(lhs == -1 && rhs == -1)
                return lhs;
            semerror(TIMES, t->lineNumber);
        case OR_EXPR:
            if(t->children == NULL || t->children->next == NULL)
                semerror(AND, t->lineNumber);
            lhs = typeExpr(t->children->data, classContainingExpr, methodContainingExpr);
            rhs = typeExpr(t->children->next->data, classContainingExpr, methodContainingExpr);
            if(lhs == -1 && rhs == -1)
                return lhs;
            semerror(AND, t->lineNumber);
        case LESS_THAN_EXPR:
            if(t->children == NULL || t->children->next == NULL)
                semerror(LESS, t->lineNumber);
            lhs = typeExpr(t->children->data, classContainingExpr, methodContainingExpr);
            rhs = typeExpr(t->children->next->data, classContainingExpr, methodContainingExpr);
            if(lhs == -1 && rhs == -1)
                return lhs;
            semerror(LESS, t->lineNumber);
        case EQUALITY_EXPR:
            if(t->children == NULL || t->children->next == NULL)
                semerror(EQUALITY, t->lineNumber);
            lhs = typeExpr(t->children->data, classContainingExpr, methodContainingExpr);
            rhs = typeExpr(t->children->next->data, classContainingExpr, methodContainingExpr);
            if(lhs == -1 && rhs == -1)
                return lhs;
            if(isSubtype(lhs, rhs) || isSubtype(rhs, lhs))
                return -1;
            semerror(EQUALITY, t->lineNumber);
        case DOT_ID_EXPR:
            if(t->children == NULL || t->children->next == NULL)
                semerror(DOT_ID_R, t->lineNumber);
            lhs = typeExpr(t->children->data, classContainingExpr, methodContainingExpr);
            if(lhs < 0)
                semerror(DOT_ID_L, t->lineNumber);
            if(t->children->data->typ == THIS_EXPR)
                rhs = typeExpr(t->children->next->data, lhs, -1);
            else
                rhs = typeExpr(t->children->next->data, lhs, methodContainingExpr);
            if(rhs > -2)
                return rhs;
            semerror(DOT_ID_R, t->lineNumber);
        case DOT_ASSIGN_EXPR:
            if(t->children == NULL || t->children->next == NULL || t->childrenTail == NULL)
                semerror(DOT_ASSIGN_R, t->lineNumber);
            lhs = typeExpr(t->children->data, classContainingExpr, methodContainingExpr);
            if(lhs < 0)
                semerror(DOT_ASSIGN_L, t->lineNumber);
            mid = typeExpr(t->children->next->data, lhs, methodContainingExpr);
            rhs = typeExpr(t->childrenTail->data, classContainingExpr, methodContainingExpr);
            if(mid == rhs || isSubtype(rhs, mid))
                return rhs;
            semerror(DOT_ASSIGN_R, t->lineNumber);
        case DOT_METHOD_CALL_EXPR: {
                if(t->children == NULL || t->children->next == NULL) {
                    semerror(DOT_METH_CALL, t->lineNumber);
                }
                
                // Get the type of the object we're calling the method on
                lhs = typeExpr(t->children->data, classContainingExpr, methodContainingExpr);
                if(lhs < 1) {  // Must be a valid class type
                    semerror(DOT_METH_CALL, t->children->data->lineNumber);
                }
                
                // Get the arguments (could be NULL if no arguments)
                ASTree *argl = (t->childrenTail != NULL) ? t->childrenTail->data : NULL;
                
                // Find the method in the class hierarchy
                cm = methInClass(lhs, t->children->next->data->idVal, argl,
                                classContainingExpr, methodContainingExpr);
                if(cm.c == -1) {
                    semerror(DOT_METH_CALL, t->lineNumber);
                }
                
                // Store the resolved class and method for code generation
                t->staticClassNum = cm.c;
                t->staticMemberNum = cm.m;
                
                // Return the method's return type
                return getMethRet(cm.c, cm.m);
            }
        case METHOD_CALL_EXPR: {
                if(t->children == NULL) {
                    semerror(-1, t->lineNumber);
                }
                
                lhs = classContainingExpr;
                if(lhs < 1) {  // Must be in a class context
                    semerror(-1, t->lineNumber);
                }
                
                // Get the arguments (could be NULL if no arguments)
                ASTree *argl = (t->childrenTail != NULL) ? t->childrenTail->data : NULL;
                
                // Find the method in the class hierarchy
                cm = methInClass(lhs, t->children->data->idVal, argl,
                                classContainingExpr, methodContainingExpr);
                if(cm.c == -1) {
                    semerror(-1, t->lineNumber);
                }
                
                // Store the resolved class and method for code generation
                t->staticClassNum = cm.c;
                t->staticMemberNum = cm.m;
                
                return getMethRet(cm.c, cm.m);
            }
        case WHILE_EXPR:
            if(t->children == NULL || t->children->next == NULL)
                semerror(WHILE, t->lineNumber);
            lhs = typeExpr(t->children->data, classContainingExpr, methodContainingExpr);
            if(lhs != -1)
                semerror(WHILE, t->children->data->lineNumber);
            rhs = typeExprs(t->children->next->data, classContainingExpr, methodContainingExpr);
            return -1;
        case IF_THEN_ELSE_EXPR:
            if(t->children == NULL || t->children->next == NULL || t->childrenTail == NULL)
                semerror(IF_EL, t->lineNumber);
            lhs = typeExpr(t->children->data, classContainingExpr, methodContainingExpr);
            if(lhs != -1)
                semerror(IF_COND, t->children->data->lineNumber);
            mid = typeExprs(t->children->next->data, classContainingExpr, methodContainingExpr);
            rhs = typeExprs(t->childrenTail->data, classContainingExpr, methodContainingExpr);
            if(mid == -1 && rhs == -1)
                return rhs;
            if(mid == -1 || rhs == -1)
                semerror(IF_EL, t->lineNumber);
            if(mid == rhs)
                return rhs;
            if(mid < -2 || rhs < -2)
                semerror(IF_EL, t->lineNumber);
            lhs = join(mid, rhs);
            return lhs;
        case NOT_EXPR:
            if(t->children == NULL)
                semerror(NOT, t->lineNumber);
            rhs = typeExpr(t->children->data, classContainingExpr, methodContainingExpr);
            if(rhs != -1)
                semerror(NOT, t->lineNumber);
            return -1;
        case NEW_EXPR:
            if(t->children == NULL)
                semerror(NEW, t->lineNumber);
            rhs = inClassST(t->children->data->idVal);
            if(rhs > -1)
                return rhs;
            semerror(NEW, t->lineNumber);
        case THIS_EXPR:
            if(classContainingExpr < 0)
                semerror(THIS_MAIN, t->lineNumber);
            return classContainingExpr;
        case AST_ID:
            if(classContainingExpr < 0)
                return inMainST(t->idVal, t->lineNumber);
            if(methodContainingExpr < 0) {
                lhs = varInClass(classContainingExpr, t->idVal);
                if(lhs == -4)
                    semerror(CLASS_VAR_DNE, t->lineNumber);
                rhs = varHash(t->idVal, classContainingExpr);
                return lhs;
            }
            lhs = varInParam(classContainingExpr, methodContainingExpr, t->idVal);
            if(lhs >= -1)
                return lhs;
            lhs = varInMeth(classContainingExpr, methodContainingExpr, t->idVal);
            if(lhs >= -1)
                return lhs;
            lhs = varInClass(classContainingExpr, t->idVal);
            if(lhs >= -1)
                return lhs;
            semerror(CLASS_VAR_DNE, t->lineNumber);
        case NULL_EXPR:
            return -2;
        case READ_EXPR:
            return -1;
        case PRINT_EXPR:
            if(t->children == NULL)
                semerror(PRINT, t->lineNumber);
            lhs = typeExpr(t->children->data, classContainingExpr, methodContainingExpr);
            if(lhs == -1)
                return lhs;
            semerror(PRINT, t->lineNumber);
        case NAT_LITERAL_EXPR:
            return -1;
        case ID_EXPR:
            if(t->children == NULL)
                semerror(-1, t->lineNumber);
            return typeExpr(t->children->data, classContainingExpr, methodContainingExpr);
        case EXPR_LIST:
            return typeExprs(t, classContainingExpr, methodContainingExpr);
        case ASSERT_EXPR:
            if(t->children == NULL)
                semerror(METH_RET_MIS, t->lineNumber);
            lhs = typeExpr(t->children->data, classContainingExpr, methodContainingExpr);
            if(lhs != -1)
                semerror(METH_RET_MIS, t->lineNumber);
            return -1;
        default:
            semerror(-1, t->lineNumber);
    }
}

int typeExprs(ASTree *t, int classContainingExprs, int methodContainingExprs) {
    int mofotype;
    ASTList *p = t->children;
    while(p != NULL) {
        mofotype = typeExpr(p->data, classContainingExprs, methodContainingExprs);
        p = p->next;
    }
    return mofotype;
}





