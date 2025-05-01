/* File codegen.h: Header File for Code Generator for DJ compiler */

#ifndef CODEGEN_H 
#define CODEGEN_H

#include <stdio.h>

/* Perform code generation for the compiler's input program.
   The code generation is based on the enhanced symbol tables built
   in setupSymbolTables, which is declared in symtbl.h.

   This method writes DISM code for the whole program to the 
   specified outputFile (which must be open and ready for writes 
   before calling generateDISM).

   This method assumes that setupSymbolTables(), declared in 
   symtbl.h, and typecheckProgram(), declared in typecheck.h, 
   have already executed.
*/
void generateDISM(FILE *outputFile);

#endif