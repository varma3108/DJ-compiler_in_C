#include "codegen.h"
#include "symtbl.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// External function from typecheck.c
extern int isSubtype(int sub, int super);

//GLOBALS
#define MAX_DISM_ADDR 65535
FILE *fout;
unsigned int SP = 0, FP = 0, HP = 0, FPOld = 0;
int loopCount = 0, lessCount = 0, equalityCount = 0, andCount = 0, notCount = 0, ifCount = 0;
int methCount = 0;

//structs
typedef struct pair{
    int meth, submeth;
}pair;
typedef struct vstruct{
    int sub;
    int mic;
    pair* methpairs;
}vstruct;

//my helper funcs
void codeGenExpr(ASTree *t, int classNumber, int methodNumber);
void codeGenExprs(ASTree *expList, int classNumber, int methodNumber);
void codeGenExprsLeave(ASTree *expList, int classNumber, int methodNumber);
int getNumObjectFields(int type);
void *safeMalloc(size_t size, const char *location);
int varInClassField(char *var, int classNumber);
void genPrologue(int class, int meth);
void genEpilogue(int class, int meth);
void genVT();
void genArgs(ASTree *t, int classNumber, int methodNumber);

void decSP() {
    --SP;
    fprintf(fout, "mov 1 1       ;DECSP-START\n");
    fprintf(fout, "sub 6 6 1     ;decsp-sub\n");
    fprintf(fout, "blt 6 5 #END  ;decsp-memcheck\n");
    fprintf(fout, "beq 6 5 #END  ;DECSP-END\n");
}

void incSP() {
    ++SP;
    fprintf(fout, "mov 1 1    ;INCSP-START\n");
    fprintf(fout, "add 6 6 1  ;INCSP-END\n");
}

// Helper function for looking up variables in main
int varNumMain(char *var) {
    int i = 0;
    while(i < numMainBlockLocals) {
        if(!strcmp(var, mainBlockST[i].varName))
            return i;
        i++;
    }
    return -4;
}
// Function to find a field in a class or its superclasses
int varInClassField(char *var, int classNumber) {
    if (classNumber <= 0 || var == NULL) {
        return -4;
    }
    
    // Search in current class
    int i;
    for (i = 0; i < classesST[classNumber].numVars; i++) {
        if (!strcmp(var, classesST[classNumber].varList[i].varName)) {
            return i;  // Return field index
        }
    }
    
    // Search in superclass if not found
    int superclass = classesST[classNumber].superclass;
    if (superclass > 0) {
        return varInClassField(var, superclass);
    }
    
    return -4;  // Not found
}
// Helper function for safe memory allocation
void *safeMalloc(size_t size, const char *location) {
    void *ptr = malloc(size);
    if (ptr == NULL) {
        fprintf(stderr, "ERROR: Memory allocation failed in %s\n", location);
        exit(-1);
    }
    return ptr;
}

// Helper function for looking up local variables in methods
int varNumMeth(char *var, int c, int m) {
    if (c <= 0 || m < 0 || m >= classesST[c].numMethods) {
        return -4;
    }
    
    int i = 0;
    while(i < classesST[c].methodList[m].numLocals) {
        if(!strcmp(var, classesST[c].methodList[m].localST[i].varName))
            return i;
        i++;
    }
    return -4;
}

// Helper function for looking up method parameters
int varNumMethParam(char *var, int c, int m) {
    // Make sure we're looking at a valid method and class
    if (c <= 0 || m < 0 || m >= classesST[c].numMethods) {
        return -4;
    }
    
    // Check if this method has a parameter and if it matches the variable name
    if (classesST[c].methodList[m].paramName != NULL && 
        !strcmp(var, classesST[c].methodList[m].paramName)) {
        return 0;  // We only have one parameter, so return index 0
    }
    return -4;
}
// Helper function to count field offset in class hierarchy
int fieldOffset(char *var, int classNumber) {
    if (classNumber <= 0 || var == NULL) {
        return -4;
    }
    
    // Search in current class
    int i;
    for (i = 0; i < classesST[classNumber].numVars; i++) {
        if (!strcmp(var, classesST[classNumber].varList[i].varName)) {
            return i + 1;  // +1 for object type tag
        }
    }
    
    // Search in superclass
    int superclass = classesST[classNumber].superclass;
    if (superclass > 0) {
        int superOffset = fieldOffset(var, superclass);
        if (superOffset != -4) {
            return superOffset;  // Inherited field retains its offset
        }
    }
    
    return -4;  // Not found
}

// Helper function to get the type number of a class
int getObjType(char *var) {
    if (var == NULL) return -1;
    
    int i = 0;
    while(i < numClasses) {
        if(!strcmp(var, classesST[i].className))
            return i;
        i++;
    }
    return -1;  // Return -1 for not found
}

// Helper function to count the number of fields in a class hierarchy
int numFields(int type) {
    int count = 0;
    while(type > 0) {
        count += classesST[type].numVars;
        type = classesST[type].superclass;
    }
    return count;
}

// Improved method call resolution
pair methCallClass(char *meth, int c) {
    pair cm;
    cm.meth = -4;
    cm.submeth = -4;
    
    // Validate input
    if (meth == NULL || c <= 0 || c >= numClasses) {
        return cm;
    }
    
    int i;
    // First search in the current class
    for(i = 0; i < classesST[c].numMethods; i++) {
        if(!strcmp(meth, classesST[c].methodList[i].methodName)) {
            cm.meth = c;
            cm.submeth = i;
            return cm;
        }
    }
    
    // Then search in superclass hierarchy
    int currentClass = classesST[c].superclass;
    while(currentClass > 0) {
        for(i = 0; i < classesST[currentClass].numMethods; i++) {
            if(!strcmp(meth, classesST[currentClass].methodList[i].methodName)) {
                cm.meth = currentClass;
                cm.submeth = i;
                return cm;
            }
        }
        currentClass = classesST[currentClass].superclass;
    }
    
    // Not found
    return cm;
}

void generateDISM(FILE *fp) {
    //sets globals
    fout = fp;
    SP = FP = MAX_DISM_ADDR;
    HP = 1;

    //sets sp, hp, fp
    fprintf(fout, "#INITPTRS: mov 0 0\n");
    fprintf(fout, "mov 5 %d  ;sets HP\n", HP);
    fprintf(fout, "mov 6 %d  ;sets SP\n", SP);
    fprintf(fout, "mov 7 %d  ;sets FP\n", FP);

    //sets up main locals
    fprintf(fout, "#INITMAIN: mov 0 0\n");
    int i;
    for(i = 0; i < numMainBlockLocals; i++) {
        fprintf(fout, "str 6 0 0  ;sets main var %d to zero\n", i);
        decSP();
    }

    //main exprs
    fprintf(fout, "#MAINEXPRS: mov 0 0\n");
    codeGenExprs(mainExprs, 0, 0);

    //MAIN END
    fprintf(fout, "ptn 6\n"); //debug
    fprintf(fout, "#END: hlt 0  ;END OF PROGRAM\n");

    // Add assertion failure handler
    fprintf(fout, "#ASSERTFAIL: mov 0 0  ;assertion-failed\n");
    fprintf(fout, "mov 1 55     ;set-error-code-for-assert-failure\n");
    fprintf(fout, "hlt 1        ;halt-on-assertion-failure\n");

    //setup methods
    int j, temp;
    for(i = 1; i < numClasses; i++) {
        for(j = 0; j < classesST[i].numMethods; j++) {
            fprintf(fout, "#Class%dMethod%d: mov 0 0  ;c%dm%d-setup\n", i, j, i, j);
            genPrologue(i, j);
            
            //pushes old FP
            fprintf(fout, "str 6 0 7\n");
            decSP();

            //sets new FP
            fprintf(fout, "mov 1 6\n");
            fprintf(fout, "add 1 1 6\n");
            fprintf(fout, "mov 2 %d\n", classesST[i].methodList[j].numLocals);
            fprintf(fout, "add 1 1 2\n");
            fprintf(fout, "mov 2 1\n"); // Always 1 parameter in DJ
            fprintf(fout, "add 1 1 2\n");
            fprintf(fout, "add 7 1 0\n");

            codeGenExprsLeave(classesST[i].methodList[j].bodyExprs, i, j);

            genEpilogue(i, j);
        }
    }

    //gens vtable vt
    genVT();
}

//code gen for expressions
void codeGenExpr(ASTree *t, int classNumber, int methodNumber) {
    // Add safety check for NULL nodes
    if (t == NULL) {
        printf("ERROR: Null AST node in codeGenExpr\n");
        exit(-1);
    }

    int temp, type, n;
    pair cm;

    switch(t->typ) {
        case THIS_EXPR: 
            fprintf(fout, "mov 0 0  ;THIS-START\n");
            
            //caller heap addr
            fprintf(fout, "mov 1 1\n");
            fprintf(fout, "sub 1 7 1\n");
            fprintf(fout, "lod 2 1 0\n");

            //put heap addr on the stack
            fprintf(fout, "str 6 0 2\n");
            decSP();

            fprintf(fout, "mov 0 0  ;THIS-END\n");
            break;

        case METHOD_CALL_EXPR: 
            if (t->children == NULL || t->children->data == NULL) {
                printf("ERROR: Invalid METHOD_CALL_EXPR node at line %d\n", t->lineNumber);
                exit(-1);
            }
            
            fprintf(fout, "mov 0 0  ;METH-CALL-START\n");
            cm = methCallClass(t->children->data->idVal, classNumber);
            if(cm.meth == -4) {
                printf("\nERROR: Method '%s' not found in class %d or its superclasses\n", 
                       t->children->data->idVal, classNumber);
                exit(-1);
            }

            //caller heap addr
            fprintf(fout, "mov 1 1\n");
            fprintf(fout, "sub 1 7 1\n");
            fprintf(fout, "lod 2 1 0\n");

            //stores ra on the stack
            fprintf(fout, "mov 1 #METHRET%d  ;meth-dot-mov-ra\n", methCount);
            fprintf(fout, "str 6 0 1  ;meth-dot-push-ra\n");
            decSP();

            //pushes dyn caller heap addr on stack
            fprintf(fout, "str 6 0 2  ;meth-dot-push-caller-addr\n");
            decSP();

            //pushes static class
            fprintf(fout, "mov 1 %d\n", t->staticClassNum);
            fprintf(fout, "str 6 0 1\n");
            decSP();

            //pushes static method number
            fprintf(fout, "mov 1 %d\n", t->staticMemberNum);
            fprintf(fout, "str 6 0 1\n");
            decSP();

            genArgs(t->childrenTail->data, classNumber, methodNumber);

            fprintf(fout, "jmp 0 #Class%dMethod%d\n", cm.meth, cm.submeth);
            fprintf(fout, "#METHRET%d: mov 0 0  ;METH-DOT-END\n", methCount++);
            fprintf(fout, "mov 0 0  ;METH-CALL-END\n");
            break;

        case DOT_METHOD_CALL_EXPR: 
            if (t->children == NULL || t->children->data == NULL) {
                printf("ERROR: Invalid DOT_METHOD_CALL_EXPR node at line %d\n", t->lineNumber);
                exit(-1);
            }
            
            fprintf(fout, "mov 0 0  ;METH-DOT-START\n");
            codeGenExpr(t->children->data, classNumber, methodNumber);
            fprintf(fout, "lod 2 6 1  ;meth-dot-load-var-addr\n");
            fprintf(fout, "beq 2 0 #END  ;meth-dot-null-check\n");

            //stores ra on the stack
            fprintf(fout, "mov 1 #METHRET%d  ;meth-dot-mov-ra\n", methCount);
            fprintf(fout, "str 6 0 1  ;meth-dot-push-ra\n");
            decSP();

            //pushes dyn caller heap addr on stack
            fprintf(fout, "str 6 0 2  ;meth-dot-push-caller-addr\n");
            decSP();

            //pushes static class
            fprintf(fout, "mov 1 %d\n", t->staticClassNum);
            fprintf(fout, "str 6 0 1\n");
            decSP();

            //pushes static method number
            fprintf(fout, "mov 1 %d\n", t->staticMemberNum);
            fprintf(fout, "str 6 0 1\n");
            decSP();

            //pushes arglist and arg count
            genArgs(t->childrenTail->data, classNumber, methodNumber);

            //goes to jmp table
            fprintf(fout, "jmp 0 #VTABLE\n");

            fprintf(fout, "#METHRET%d: mov 0 0  ;METH-DOT-END\n", methCount++);
            break;

        case DOT_ASSIGN_EXPR: 
            if (t->children == NULL || t->childrenTail == NULL) {
                printf("ERROR: Invalid DOT_ASSIGN_EXPR node at line %d\n", t->lineNumber);
                exit(-1);
            }
            
            fprintf(fout, "mov 0 0  ;DOT-ASSIGN-START\n");
            codeGenExpr(t->childrenTail->data, classNumber, methodNumber);
            codeGenExpr(t->children->data, classNumber, methodNumber);
            //checks for null
            fprintf(fout, "lod 1 6 1  ;dot-assign-null-check\n");
            fprintf(fout, "beq 1 0 #END  ;dot-assign-halt-if-null\n");
            fprintf(fout, "lod 2 6 2  ;dot-assign-load-value2store\n");
            fprintf(fout, "mov 3 %d    ;dot-assign-offset\n", t->staticMemberNum + 1);
            fprintf(fout, "sub 1 1 3  ;dot-assign-heap-address2store\n");
            fprintf(fout, "str 1 0 2  ;dot-assign-store-val\n");
            incSP();
            fprintf(fout, "mov 0 0  ;DOT-ASSIGN-END\n");
            break;

        case DOT_ID_EXPR: 
            if (t->children == NULL) {
                printf("ERROR: Invalid DOT_ID_EXPR node at line %d\n", t->lineNumber);
                exit(-1);
            }
            
            fprintf(fout, "mov 0 0  ;DOT-ID-START\n");
            codeGenExpr(t->children->data, classNumber, methodNumber);
            fprintf(fout, "lod 1 6 1  ;dot-id-null-check\n");
            fprintf(fout, "beq 1 0 #END  ;dot-id-halt-if-null\n");
            fprintf(fout, "mov 2 %d  ;dot-id-sub-num\n", t->staticMemberNum + 1);
            fprintf(fout, "sub 1 1 2  ;dot-id-heap-address2read\n");
            fprintf(fout, "lod 1 1 0   ;dot-id-loads-val\n");
            fprintf(fout, "str 6 1 1  ;dot-id-push-val-stack\n");
            fprintf(fout, "mov 0 0  ;DOT-ID-END\n");
            break;

        case ASSIGN_EXPR: 
            if (t->children == NULL || t->children->next == NULL) {
                printf("ERROR: Invalid ASSIGN_EXPR node at line %d\n", t->lineNumber);
                exit(-1);
            }
            
            if(t->staticClassNum <= 0) {
                if(classNumber <= 0) {
                    temp = varNumMain(t->children->data->idVal);
                    fprintf(fout, "mov 0 0    ;ASSIGN-START\n");
                    codeGenExpr(t->children->next->data, 0, 0);
                    fprintf(fout, "mov 1 %d    ;assign-dest-address\n", MAX_DISM_ADDR - temp);
                    fprintf(fout, "lod 2 6 1  ;assign-expr-load\n");
                    fprintf(fout, "str 1 0 2  ;ASSIGN-END\n");
                    break;
                }
                
                temp = varNumMeth(t->children->data->idVal, classNumber, methodNumber);
                if(temp == -4) {
                    temp = varNumMethParam(t->children->data->idVal, classNumber, methodNumber);
                    if(temp == -4) {
                        // Check if it's a class field
                        int fieldIndex = varInClassField(t->children->data->idVal, classNumber);
                        if (fieldIndex != -4) {
                            // Generate code to assign to the field in 'this'
                            fprintf(fout, "mov 0 0    ;ASSIGN-CLASS-FIELD-START\n");
                            codeGenExpr(t->children->next->data, classNumber, methodNumber);
                            fprintf(fout, "mov 2 1\n");
                            fprintf(fout, "sub 1 7 2\n");  // Get 'this' pointer
                            fprintf(fout, "lod 1 1 0\n");
                            fprintf(fout, "beq 1 0 #END  ;null-check\n");  // Check if 'this' is null
                            fprintf(fout, "lod 2 6 1\n");  // Get value to assign
                            fprintf(fout, "mov 3 %d\n", fieldIndex + 1);  // Field offset
                            fprintf(fout, "sub 1 1 3\n");
                            fprintf(fout, "str 1 0 2\n");  // Store value in field
                            fprintf(fout, "mov 0 0    ;ASSIGN-CLASS-FIELD-END\n");
                            break;
                        }
                        
                        printf("\nID ERROR IN CLASS/METHOD: var '%s' not found in class %d, method %d\n", 
                            t->children->data->idVal, classNumber, methodNumber);
                        exit(-1);
                    }
                    temp += 4;
                    fprintf(fout, "mov 0 0    ;ASSIGN-START\n");
                    codeGenExpr(t->children->next->data, classNumber, methodNumber);                                
                    fprintf(fout, "mov 1 %d\n", temp); //offset
                    fprintf(fout, "sub 1 7 1\n"); //addr to store
                    fprintf(fout, "lod 2 6 1\n"); //loads value to store
                    fprintf(fout, "str 1 0 2\n"); //saves
                    fprintf(fout, "mov 0 0    ;ASSIGN-END\n");
                    break;
                }
                temp += 5 + 1; // Fixed: 1 parameter in methods
                fprintf(fout, "mov 0 0    ;ASSIGN-START\n");
                codeGenExpr(t->children->next->data, classNumber, methodNumber);
                fprintf(fout, "mov 1 %d\n", temp);
                fprintf(fout, "sub 1 7 1\n");
                fprintf(fout, "lod 2 6 1\n");
                fprintf(fout, "str 1 0 2\n");
                fprintf(fout, "mov 0 0    ;ASSIGN-END\n");
                break;
            }

            fprintf(fout, "mov 0 0    ;ASSIGN-START\n");
            codeGenExpr(t->children->next->data, classNumber, methodNumber);
            fprintf(fout, "mov 2 1\n");
            fprintf(fout, "sub 1 7 2\n");
            fprintf(fout, "lod 1 1 0\n");
            fprintf(fout, "mov 2 %d\n", t->staticMemberNum + 1);
            fprintf(fout, "sub 1 1 2\n");
            fprintf(fout, "lod 2 6 1\n");
            fprintf(fout, "str 1 0 2\n");
            fprintf(fout, "mov 0 0    ;ASSIGN-END\n");
            break;

        case ID_EXPR: 
            if (t->children == NULL || t->children->data == NULL) {
                printf("ERROR: Invalid ID_EXPR node at line %d\n", t->lineNumber);
                exit(-1);
            }
            
            if(t->staticClassNum <= 0 && classNumber <= 0) {
                temp = varNumMain(t->children->data->idVal);
                fprintf(fout, "mov 0 0    ;ID-START-LOCAL\n");
                fprintf(fout, "mov 1 %d    ;id-load-stack-location\n", MAX_DISM_ADDR - temp);
                fprintf(fout, "lod 1 1 0  ;id-load-from-stack\n");
                fprintf(fout, "str 6 0 1  ;ID-END\n");
                decSP();
                break;
            }
            if(t->staticClassNum <= 0 && t->staticMemberNum <= 0 && classNumber > 0) {
                temp = varNumMeth(t->children->data->idVal, classNumber, methodNumber);
                //if in params
                if(temp == -4) {
                    temp = varNumMethParam(t->children->data->idVal, classNumber, methodNumber);
                    if(temp == -4) {
                        // Check if it's a class field
                        int fieldIndex = varInClassField(t->children->data->idVal, classNumber);
                        if (fieldIndex != -4) {
                            // Generate code to access the field from 'this'
                            fprintf(fout, "mov 0 0    ;ID-CLASS-FIELD-START\n");
                            fprintf(fout, "mov 1 1\n");
                            fprintf(fout, "sub 1 7 1\n");  // Get 'this' pointer
                            fprintf(fout, "lod 1 1 0\n");
                            fprintf(fout, "beq 1 0 #END  ;null-check\n");  // Check if 'this' is null
                            fprintf(fout, "mov 2 %d\n", fieldIndex + 1);  // Field offset (add 1 for type tag)
                            fprintf(fout, "sub 1 1 2\n");
                            fprintf(fout, "lod 1 1 0\n");  // Load the field value
                            fprintf(fout, "str 6 0 1\n");  // Push onto stack
                            decSP();
                            fprintf(fout, "mov 0 0    ;ID-CLASS-FIELD-END\n");
                            break;
                        }
                        
                        printf("\nID ERROR IN CLASS/METHOD: var '%s' not found in class %d, method %d\n", 
                            t->children->data->idVal, classNumber, methodNumber);
                        exit(-1);
                    }
                    temp += 4;
                    fprintf(fout, "mov 1 %d\n", temp);
                    fprintf(fout, "sub 1 7 1\n");
                    fprintf(fout, "lod 1 1 0\n");
                    fprintf(fout, "str 6 0 1\n");
                    decSP();
                    break;
                }

                temp += 5 + 1; // Fixed: 1 parameter in methods
                fprintf(fout, "mov 1 %d\n", temp);
                fprintf(fout, "sub 1 7 1\n");
                fprintf(fout, "lod 1 1 0\n");
                fprintf(fout, "str 6 0 1\n");
                decSP();
                break;      
            }

            fprintf(fout, "mov 2 1\n");
            fprintf(fout, "sub 1 7 2\n");
            fprintf(fout, "lod 1 1 0\n");
            fprintf(fout, "mov 2 %d\n", t->staticMemberNum + 1);
            fprintf(fout, "sub 1 1 2\n");
            fprintf(fout, "lod 1 1 0\n");
            fprintf(fout, "str 6 0 1\n");
            decSP();
            break;

            fprintf(fout, "mov 2 1\n");
            fprintf(fout, "sub 1 7 2\n");
            fprintf(fout, "lod 1 1 0\n");
            fprintf(fout, "mov 2 %d\n", t->staticMemberNum + 1);
            fprintf(fout, "sub 1 1 2\n");
            fprintf(fout, "lod 1 1 0\n");
            fprintf(fout, "str 6 0 1\n");
            decSP();
            break;
                      
        case PRINT_EXPR: 
            if (t->children == NULL) {
                printf("ERROR: Invalid PRINT_EXPR node at line %d\n", t->lineNumber);
                exit(-1);
            }
            
            codeGenExpr(t->children->data, classNumber, methodNumber);
            fprintf(fout, "mov 0 0    ;PRINT-START\n");
            fprintf(fout, "lod 1 6 1  ;print-ready\n");
            fprintf(fout, "ptn 1      ;PRINT-END\n");
            break;

        case READ_EXPR: 
            fprintf(fout, "mov 0 0    ;READ-START\n");
            fprintf(fout, "rdn 1      ;reads in\n");
            fprintf(fout, "str 6 0 1  ;READ-END\n");
            decSP();
            break;

        case NAT_LITERAL_EXPR: 
            fprintf(fout, "mov 0 0    ;NATLIT-START\n");
            fprintf(fout, "mov 1 %d    ;natlit-set\n", t->natVal);
            fprintf(fout, "str 6 0 1  ;NATLIT-END\n");
            decSP();
            break;

        case PLUS_EXPR: 
            if (t->children == NULL || t->children->next == NULL) {
                printf("ERROR: Invalid PLUS_EXPR node at line %d\n", t->lineNumber);
                exit(-1);
            }
            
            codeGenExpr(t->children->data, classNumber, methodNumber);
            codeGenExpr(t->children->next->data, classNumber, methodNumber);
            fprintf(fout, "mov 0 0    ;PLUS-START\n");
            fprintf(fout, "lod 1 6 2  ;plus-load e1\n");
            fprintf(fout, "lod 2 6 1  ;plus-load e2\n");
            fprintf(fout, "add 1 1 2  ;plus-add e1+e2\n");
            fprintf(fout, "str 6 2 1  ;PLUS-END\n");
            incSP();
            break;

        case MINUS_EXPR: 
            if (t->children == NULL || t->children->next == NULL) {
                printf("ERROR: Invalid MINUS_EXPR node at line %d\n", t->lineNumber);
                exit(-1);
            }
            
            codeGenExpr(t->children->data, classNumber, methodNumber);
            codeGenExpr(t->children->next->data, classNumber, methodNumber);
            fprintf(fout, "mov 0 0    ;MINUS-START\n");
            fprintf(fout, "lod 1 6 2  ;minus-load e1\n");
            fprintf(fout, "lod 2 6 1  ;minus-load e2\n");
            fprintf(fout, "sub 1 1 2  ;minus-minus e1+e2\n");
            fprintf(fout, "str 6 2 1  ;MINUS-END\n");
            incSP();
            break;

        case TIMES_EXPR: 
            if (t->children == NULL || t->children->next == NULL) {
                printf("ERROR: Invalid TIMES_EXPR node at line %d\n", t->lineNumber);
                exit(-1);
            }
            
            codeGenExpr(t->children->data, classNumber, methodNumber);
            codeGenExpr(t->children->next->data, classNumber, methodNumber);
            fprintf(fout, "mov 0 0    ;TIMES-START\n");
            fprintf(fout, "lod 1 6 2  ;times-load e1\n");
            fprintf(fout, "lod 2 6 1  ;times-load e2\n");
            fprintf(fout, "mul 1 1 2  ;times-times e1+e2\n");
            fprintf(fout, "str 6 2 1  ;TIMES-END\n");
            incSP();
            break;

        case LESS_THAN_EXPR: 
            if (t->children == NULL || t->childrenTail == NULL) {
                printf("ERROR: Invalid LESS_THAN_EXPR node at line %d\n", t->lineNumber);
                exit(-1);
            }
            
            temp = lessCount++;
            codeGenExpr(t->children->data, classNumber, methodNumber);
            codeGenExpr(t->childrenTail->data, classNumber, methodNumber);
            fprintf(fout, "lod 1 6 2           ;LESS-START-load-e1\n");
            fprintf(fout, "lod 2 6 1           ;less-load-e2\n");
            fprintf(fout, "blt 1 2 #LESS%d      ;less-cond-check\n", temp);
            fprintf(fout, "str 6 2 0           ;less-stores-zero\n");
            fprintf(fout, "jmp 0 #LESSEND%d     ;less-jmp-to-end\n", temp);
            fprintf(fout, "#LESS%d: mov 1 1     ;less-sets-1\n", temp);
            fprintf(fout, "str 6 2 1           ;less-store-1\n");
            fprintf(fout, "#LESSEND%d: mov 0 0  ;less-end-noop\n", temp);
            incSP();
            break;

        case EQUALITY_EXPR: 
            if (t->children == NULL || t->childrenTail == NULL) {
                printf("ERROR: Invalid EQUALITY_EXPR node at line %d\n", t->lineNumber);
                exit(-1);
            }
            
            temp = equalityCount++;
            codeGenExpr(t->children->data, classNumber, methodNumber);
            codeGenExpr(t->childrenTail->data, classNumber, methodNumber);
            fprintf(fout, "lod 1 6 2            ;EQUAL-START-load-e1\n");
            fprintf(fout, "lod 2 6 1            ;equal-load-e2\n");
            fprintf(fout, "beq 1 2 #EQUAL%d      ;equal-cond-check\n", temp);
            fprintf(fout, "str 6 2 0            ;equal-stores-zero\n");
            fprintf(fout, "jmp 0 #EQUALEND%d     ;equal-jmp-to-end\n", temp);
            fprintf(fout, "#EQUAL%d: mov 1 1     ;equal-sets-1\n", temp);
            fprintf(fout, "str 6 2 1            ;equal-store-1\n");
            fprintf(fout, "#EQUALEND%d: mov 0 0  ;equal-end-noop\n", temp);
            incSP();
            break;

        case OR_EXPR: 
            if (t->children == NULL || t->childrenTail == NULL) {
                printf("ERROR: Invalid OR_EXPR node at line %d\n", t->lineNumber);
                exit(-1);
            }
            
            temp = andCount++;  // Reusing andCount counter for OR
            codeGenExpr(t->children->data, classNumber, methodNumber);
            fprintf(fout, "lod 1 6 1            ;OR-START-load-e1\n");
            fprintf(fout, "mov 2 0              ;or-mov-0-into-r2\n");
            fprintf(fout, "beq 1 2 #ORSECOND%d   ;or-check-if-first-is-false\n", temp);
            // First expression is true (non-zero), result is true
            fprintf(fout, "mov 2 1              ;or-set-1-first-true\n");
            fprintf(fout, "str 6 1 2            ;or-store-result\n");
            fprintf(fout, "jmp 0 #OREND%d        ;or-skip-second-expr\n", temp);
            // First expression was false, evaluate second expression
            fprintf(fout, "#ORSECOND%d: mov 0 0  ;or-eval-second\n", temp);
            incSP();
            codeGenExpr(t->childrenTail->data, classNumber, methodNumber);
            fprintf(fout, "lod 1 6 1            ;or-load-second-result\n");
            fprintf(fout, "mov 2 0              ;or-compare-to-zero\n");
            fprintf(fout, "beq 1 2 #ORFALSE%d    ;or-check-if-second-false\n", temp);
            // Second expression is true
            fprintf(fout, "mov 2 1              ;or-set-1-second-true\n");
            fprintf(fout, "str 6 1 2            ;or-store-result\n");
            fprintf(fout, "jmp 0 #OREND%d        ;or-jmp-to-end\n", temp);
            // Both expressions were false
            fprintf(fout, "#ORFALSE%d: str 6 1 0 ;or-set-result-false\n", temp);
            fprintf(fout, "#OREND%d: mov 0 0     ;OR-END\n", temp);
            break;


        case NOT_EXPR: 
            if (t->children == NULL) {
                printf("ERROR: Invalid NOT_EXPR node at line %d\n", t->lineNumber);
                exit(-1);
            }
            
            temp = notCount++;
            codeGenExpr(t->children->data, classNumber, methodNumber);
            fprintf(fout, "lod 1 6 1            ;NOT-START-load-expr\n");
            fprintf(fout, "beq 1 0 #NOT%d        ;not-cond\n", temp);
            fprintf(fout, "str 6 1 0            ;not-set-zero\n");
            fprintf(fout, "jmp 0 #NOTEND%d       ;not-jmp-end\n", temp);
            fprintf(fout, "#NOT%d: mov 1 1       ;not-set-r1=1\n", temp);
            fprintf(fout, "str 6 1 1            ;not-set-1\n");
            fprintf(fout, "#NOTEND%d: mov 0 0    ;NOT-END\n", temp);
            break;

        case ASSERT_EXPR: 
            if (t->children == NULL) {
                printf("ERROR: Invalid ASSERT_EXPR node at line %d\n", t->lineNumber);
                exit(-1);
            }
            
            // Generate code for the expression to be asserted
            codeGenExpr(t->children->data, classNumber, methodNumber);
            
            // Check if the assertion is false (0)
            fprintf(fout, "mov 0 0    ;ASSERT-START\n");
            fprintf(fout, "lod 1 6 1  ;assert-load-condition\n");
            fprintf(fout, "mov 2 0    ;assert-compare-with-0\n");
            fprintf(fout, "beq 1 2 #ASSERTFAIL  ;assert-check-if-false\n");
            
            // If we get here, assertion passed - continue execution
            fprintf(fout, "mov 0 0    ;ASSERT-END\n");
            break;
        

        case IF_THEN_ELSE_EXPR: 
            if (t->children == NULL || t->children->next == NULL || t->childrenTail == NULL) {
                printf("ERROR: Invalid IF_THEN_ELSE_EXPR node at line %d\n", t->lineNumber);
                exit(-1);
            }
            
            temp = ifCount++;
            codeGenExpr(t->children->data, classNumber, methodNumber);
            fprintf(fout, "lod 1 6 1      ;IF-START\n");
            fprintf(fout, "beq 1 0 #IF%d   ;if-cond\n", temp);
            codeGenExprsLeave(t->children->next->data, classNumber, methodNumber);
            fprintf(fout, "lod 1 6 1\n");
            fprintf(fout, "str 6 2 1\n");
            incSP();
            fprintf(fout, "jmp 0 #IFEND%d  ;if-jmp-end\n", temp);
            fprintf(fout, "#IF%d: mov 0 0  ;if-else-start\n", temp);
            codeGenExprsLeave(t->childrenTail->data, classNumber, methodNumber);
            fprintf(fout, "lod 1 6 1\n");
            fprintf(fout, "str 6 2 1\n");
            incSP();
            fprintf(fout, "#IFEND%d: mov 0 0 ;IF-END\n", temp);
            break;

        case WHILE_EXPR: 
            if (t->children == NULL || t->childrenTail == NULL) {
                printf("ERROR: Invalid WHILE_EXPR node at line %d\n", t->lineNumber);
                exit(-1);
            }
            
            temp = loopCount++;
            fprintf(fout, "#LOOP%d: mov 0 0       ;WHILE-START\n", temp);
            codeGenExpr(t->children->data, classNumber, methodNumber);
            fprintf(fout, "mov 1 1               ;while-cond-start\n");
            fprintf(fout, "lod 2 6 1             ;while-load-conditional-outcome\n");
            fprintf(fout, "blt 2 1 #LOOPEND%d     ;while-cond-end\n", temp);
            incSP();
            codeGenExprs(t->childrenTail->data, classNumber, methodNumber);
            fprintf(fout, "jmp 0 #LOOP%d          ;loop-jmp-end\n", temp);
            fprintf(fout, "#LOOPEND%d: str 6 1 0  ;WHILE-END\n", temp);
            break;

        case NEW_EXPR:
            if (t->children == NULL || t->children->data == NULL) {
                printf("ERROR: Invalid NEW_EXPR node at line %d\n", t->lineNumber);
                exit(-1);
            }
            
            fprintf(fout, "mov 0 0  ;NEW-START\n");
            type = getObjType(t->children->data->idVal);
            if (type == -1) {
                printf("ERROR: Unknown class type '%s' in NEW_EXPR at line %d\n", 
                       t->children->data->idVal, t->lineNumber);
                exit(-1);
            }
            
            n = numFields(type);
            fprintf(fout, "mov 1 1  ;new-set-1\n");
            temp = 0;
            while(temp < n) {
                fprintf(fout, "str 5 0 0  ;new-alloc-%d\n", temp);
                fprintf(fout, "add 5 5 1  ;new-incHP-%d\n", temp);
                HP++;
                temp++;
            }
            fprintf(fout, "mov 2 %d    ;new-r2=type\n", type);
            fprintf(fout, "str 5 0 2  ;new-store-type\n");
            fprintf(fout, "str 6 0 5  ;new-push-stack\n");
            fprintf(fout, "add 5 5 1  ;new-incHP\n");
            HP++;
            decSP();
            fprintf(fout, "mov 0 0  ;NEW-END\n");
            break;

        case NULL_EXPR: 
            fprintf(fout, "mov 0 0    ;NULL-START\n");
            fprintf(fout, "str 6 0 0  ;null-push-zero\n");
            decSP();
            fprintf(fout, "mov 0 0    ;NULL-END\n");
            break;

        default: 
            printf("\nERROR_CODE-80085!!! line#%d, type = %d\n", t->lineNumber, t->typ);
            exit(-1);
    }
}

//pops the last expr of elist off of stack
void codeGenExprs(ASTree *expList, int classNumber, int methodNumber) {
    if (expList == NULL) {
        printf("ERROR: NULL expression list in codeGenExprs\n");
        exit(-1);
    }
    
    ASTList *p = expList->children;

    while(p && p->data) {
        codeGenExpr(p->data, classNumber, methodNumber);
        incSP();
        p = p->next;
    }
}

//leaves last expr of elist on the stack
void codeGenExprsLeave(ASTree *expList, int classNumber, int methodNumber) {
    if (expList == NULL) {
        printf("ERROR: NULL expression list in codeGenExprsLeave\n");
        exit(-1);
    }
    
    ASTList *p = expList->children;

    while(p && p->data) {
        codeGenExpr(p->data, classNumber, methodNumber);
        if(p->next && p->next->data)
            incSP();
        p = p->next;
    }
}

void genPrologue(int class, int meth) {
    if (class <= 0 || meth < 0 || meth >= classesST[class].numMethods) {
        printf("ERROR: Invalid class or method in genPrologue\n");
        exit(-1);
    }
    
    int i;
    fprintf(fout, "mov 0 0  ;PROLOGUE-START\n");
    for(i = 0; i < classesST[class].methodList[meth].numLocals; i++) {
        fprintf(fout, "str 6 0 0\n");
        decSP();
    }
    fprintf(fout, "mov 0 0  ;PROLOGUE-END\n");
}

void genEpilogue(int class, int meth) {
    if (class <= 0 || meth < 0 || meth >= classesST[class].numMethods) {
        printf("ERROR: Invalid class or method in genEpilogue\n");
        exit(-1);
    }
    
    fprintf(fout, "mov 0 0  ;EPILOGUE-START\n");
    fprintf(fout, "lod 1 6 1\n"); //result
    fprintf(fout, "lod 2 6 2\n"); //old FP
    fprintf(fout, "lod 3 7 0\n"); //ra

    fprintf(fout, "str 7 0 1\n"); //replaces ra with result
    
    //resets SP to fp
    fprintf(fout, "sub 6 6 6\n");
    fprintf(fout, "add 6 7 0\n");
    
    //sets fp to old fp
    fprintf(fout, "sub 7 7 7\n");
    fprintf(fout, "add 7 2 0\n");
    
    fprintf(fout, "jmp 3 0  ;EPILOGUE-END\n");
}

void genArgs(ASTree *t, int classNumber, int methodNumber) {
    if (t == NULL) {
        // No arguments - push a count of 0
        fprintf(fout, "mov 1 0  ;arlist-mov-count-zero\n");
        fprintf(fout, "str 6 0 1  ;arglist-push-arg-count-zero\n");
        decSP();
        return;
    }
    
    ASTList *p = t->children;
    int count = 0;
    
    while(p && p->data) {
        codeGenExpr(p->data, classNumber, methodNumber);
        count++;
        p = p->next;
    }

    fprintf(fout, "mov 1 %d  ;arlist-mov-count\n", count);
    fprintf(fout, "str 6 0 1  ;arglist-push-arg-count\n");
    decSP();
}

void genVT() {
    /* gen vtable - with improved implementation */

    //get sub class counts
    int i, j, k, l;
    
    // Count subclasses for each class
    int *subClassCounts = (int*)safeMalloc(sizeof(int)*numClasses, "genVT-subClassCounts");
    memset(subClassCounts, 0, sizeof(int)*numClasses);
    
    for(i = 0; i < numClasses; i++) {
        int next = classesST[i].superclass;
        while(next >= 0) {
            subClassCounts[next]++;
            next = classesST[next].superclass;
        }
    }

    // Allocate arrays for subclasses
    int **subClasses = (int**)safeMalloc(sizeof(int*)*numClasses, "genVT-subClasses");
    
    for(i = 0; i < numClasses; i++) {
        if (subClassCounts[i] > 0) {
            subClasses[i] = (int*)safeMalloc(sizeof(int)*subClassCounts[i], "genVT-subClasses[i]");
            int nextIndex = 0;
            
            // Find all subclasses
            for(j = 0; j < numClasses; j++) {
                if(j != i && isSubtype(j, i)) {
                    if (nextIndex < subClassCounts[i]) {
                        subClasses[i][nextIndex++] = j;
                    }
                }
            }
        } else {
            subClasses[i] = NULL;
        }
    }

    // Count methods in common between classes and their subclasses
    int **numMethodsInCommon = (int**)safeMalloc(sizeof(int*)*numClasses, "genVT-numMethodsInCommon");
    
    for(i = 0; i < numClasses; i++) {
        if (subClassCounts[i] > 0) {
            numMethodsInCommon[i] = (int*)safeMalloc(sizeof(int)*subClassCounts[i], "genVT-numMethodsInCommon[i]");
            memset(numMethodsInCommon[i], 0, sizeof(int)*subClassCounts[i]);
            
            for(j = 0; j < subClassCounts[i]; j++) {
                int subClass = subClasses[i][j];
                
                for(k = 0; k < classesST[i].numMethods; k++) {
                    for(l = 0; l < classesST[subClass].numMethods; l++) {
                        if(classesST[i].methodList[k].methodName && 
                           classesST[subClass].methodList[l].methodName && 
                           strcmp(classesST[i].methodList[k].methodName, 
                                  classesST[subClass].methodList[l].methodName) == 0) {
                            numMethodsInCommon[i][j]++;
                            break;
                        }
                    }
                }
            }
        } else {
            numMethodsInCommon[i] = NULL;
        }
    }

    // Build the virtual method table structure
    vstruct **vtable = (vstruct**)safeMalloc(sizeof(vstruct*)*numClasses, "genVT-vtable");
    
    for(i = 0; i < numClasses; i++) {
        if (subClassCounts[i] > 0) {
            vtable[i] = (vstruct*)safeMalloc(sizeof(vstruct)*subClassCounts[i], "genVT-vtable[i]");
            
            for(j = 0; j < subClassCounts[i]; j++) {
                vtable[i][j].sub = subClasses[i][j];
                vtable[i][j].mic = numMethodsInCommon[i][j];

                if(vtable[i][j].mic > 0) {
                    vtable[i][j].methpairs = (pair*)safeMalloc(sizeof(pair)*vtable[i][j].mic, "genVT-methpairs");
                    int pairIndex = 0;
                    
                    for(k = 0; k < classesST[i].numMethods; k++) {
                        for(l = 0; l < classesST[vtable[i][j].sub].numMethods; l++) {
                            if(strcmp(classesST[i].methodList[k].methodName, 
                                    classesST[vtable[i][j].sub].methodList[l].methodName) == 0) {
                                if (pairIndex < vtable[i][j].mic) {
                                    vtable[i][j].methpairs[pairIndex].meth = k;
                                    vtable[i][j].methpairs[pairIndex].submeth = l;
                                    pairIndex++;
                                }
                                break;
                            }
                        }
                    }
                } else {
                    vtable[i][j].methpairs = NULL;
                }
            }
        } else {
            vtable[i] = NULL;
        }
    }

    // Calculate size of vtable
    int vtsize = 0;
    for(i = 0; i < numClasses; i++) {
        vtsize += classesST[i].numMethods * (1 + subClassCounts[i]);
    }
    
    // Create and populate the virtual table array
    int **vt = NULL;
    if (vtsize > 0) {
        vt = (int**)safeMalloc(sizeof(int*)*vtsize, "genVT-vt");
        
        for(i = 0; i < vtsize; i++) {
            vt[i] = (int*)safeMalloc(sizeof(int)*5, "genVT-vt[i]");
            memset(vt[i], 0, sizeof(int)*5);
        }
    }

    // Fill in the virtual table
    int vtIndex = 0;
    for(i = 0; i < numClasses; i++) {
        for(j = 0; j < classesST[i].numMethods; j++) {
            if (vtIndex < vtsize) {
                vt[vtIndex][0] = i;  // Static class
                vt[vtIndex][1] = j;  // Static method
                vt[vtIndex][2] = i;  // Dynamic class
                vt[vtIndex][3] = i;  // Implementation class
                vt[vtIndex][4] = j;  // Implementation method
                vtIndex++;
            }
            
            for(k = 0; k < subClassCounts[i]; k++) {
                if (vtIndex < vtsize) {
                    vt[vtIndex][0] = i;  // Static class
                    vt[vtIndex][1] = j;  // Static method
                    vt[vtIndex][2] = subClasses[i][k];  // Dynamic class
                    
                    // Check if method is overridden
                    int found = 0;
                    for(l = 0; l < vtable[i][k].mic; l++) {
                        if(j == vtable[i][k].methpairs[l].meth) {
                            vt[vtIndex][3] = subClasses[i][k];  // Implementation class
                            vt[vtIndex][4] = vtable[i][k].methpairs[l].submeth;  // Implementation method
                            found = 1;
                            break;
                        }
                    }
                    
                    if(!found) {
                        vt[vtIndex][3] = i;  // Implementation class (inherited)
                        vt[vtIndex][4] = j;  // Implementation method (inherited)
                    }
                    
                    vtIndex++;
                }
            }
        }
    }

    // Find distinct static classes in vtable
    int scCount = 0;
    int prevClass = -1;
    for(i = 0; i < vtsize; i++) {
        if(vt[i][0] != prevClass) {
            scCount++;
            prevClass = vt[i][0];
        }
    }
    
    int *vtclist = (int*)safeMalloc(sizeof(int)*scCount, "genVT-vtclist");
    int vtcIndex = 0;
    prevClass = -1;
    
    for(i = 0; i < vtsize; i++) {
        if(vt[i][0] != prevClass) {
            vtclist[vtcIndex++] = vt[i][0];
            prevClass = vt[i][0];
        }
    }

    // Generate vtable code
    fprintf(fout, "#VTABLE: mov 0 0 ;VTABLE-START\n");
    
    // Get stack position
    fprintf(fout, "add 4 6 0\n");
    fprintf(fout, "mov 1 1\n");
    fprintf(fout, "add 4 4 1\n"); // r4 = #arg addr
    fprintf(fout, "lod 1 4 0\n");  // r1 = #args
    fprintf(fout, "add 4 4 1\n");

    // Set r3 = static method
    fprintf(fout, "lod 3 4 1\n");

    // Set r2 = static class
    fprintf(fout, "lod 2 4 2\n");

    // Set r1 = dynamic class (from object tag)
    fprintf(fout, "lod 1 4 3\n");
    fprintf(fout, "lod 1 1 0\n");

    // Branch based on static class
    for(i = 0; i < scCount; i++) {
        fprintf(fout, "mov 4 %d\n", vtclist[i]);
        fprintf(fout, "beq 2 4 #VTCLASS%d  ;vt-class-branch\n", vtclist[i]);
    }
    
    fprintf(fout, "mov 4 777  ;vt-class-dne\n");
    fprintf(fout, "ptn 4\n");
    fprintf(fout, "jmp 0 #END\n");

    // Branch based on static method within each class
    for(i = 0; i < scCount; i++) {
        fprintf(fout, "#VTCLASS%d: mov 0 0\n", vtclist[i]);
        for(j = 0; j < classesST[vtclist[i]].numMethods; j++) {
            fprintf(fout, "mov 4 %d\n", j);
            fprintf(fout, "beq 3 4 #VTCLASS%dMETH%d\n", vtclist[i], j);
        }
    }
    
    fprintf(fout, "mov 4 777  ;vt-method-dne\n");
    fprintf(fout, "ptn 4\n");
    fprintf(fout, "jmp 0 #END  ;vt-meth-dne\n");

    // Method implementation lookups
    vtIndex = 0;
    for(i = 0; i < scCount; i++) {
        for(j = 0; j < classesST[vtclist[i]].numMethods; j++) {
            fprintf(fout, "#VTCLASS%dMETH%d: mov 0 0 \n", vtclist[i], j);
            
            // Lookup for this class
            fprintf(fout, "mov 4 %d\n", vt[vtIndex][2]);
            fprintf(fout, "beq 1 4 #Class%dMethod%d\n", vt[vtIndex][3], vt[vtIndex][4]);
            vtIndex++;
            
            // Lookup for subclasses
            for(k = 0; k < subClassCounts[vtclist[i]]; k++) {
                if (vtIndex < vtsize) {
                    fprintf(fout, "mov 4 %d\n", vt[vtIndex][2]);
                    fprintf(fout, "beq 1 4 #Class%dMethod%d\n", vt[vtIndex][3], vt[vtIndex][4]);
                    vtIndex++;
                }
            }
        }
    }
    
    fprintf(fout, "mov 4 777  ;vt-impl-dne\n");
    fprintf(fout, "ptn 4\n");
    fprintf(fout, "jmp 0 #END\n");
    
    // Free allocated memory
    for(i = 0; i < numClasses; i++) {
        if (subClassCounts[i] > 0) {
            for(j = 0; j < subClassCounts[i]; j++) {
                if(vtable[i][j].mic > 0 && vtable[i][j].methpairs != NULL) {
                    free(vtable[i][j].methpairs);
                }
            }
            if (vtable[i] != NULL) free(vtable[i]);
            if (subClasses[i] != NULL) free(subClasses[i]);
            if (numMethodsInCommon[i] != NULL) free(numMethodsInCommon[i]);
        }
    }
    
    // Free the remaining arrays
    free(vtable);
    free(subClasses);
    free(numMethodsInCommon);
    
    // Free the vtable arrays
    if (vt != NULL) {
        for(i = 0; i < vtsize; i++) {
            if (vt[i] != NULL) free(vt[i]);
        }
        free(vt);
    }
    
    free(subClassCounts);
    free(vtclist);
}