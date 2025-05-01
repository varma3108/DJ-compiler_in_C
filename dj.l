%option yylineno
%option noyywrap
digit       [0-9]
letter      [a-zA-Z]
id          (_|{letter})({letter}|{digit}|_)*
whitespace  [ \t\r\n]+
%%
"class"     { return scanned(CLASS); }
"extends"   { return scanned(EXTENDS); }
"final"     { return scanned(FINAL); }
"main"      { return scanned(MAIN); }
"nat"       { return scanned(NATTYPE); }
"printNat"  { return scanned(PRINTNAT); }
"readNat"   { return scanned(READNAT); }
"assert"    { return scanned(ASSERT); }
"if"        { return scanned(IF); }
"else"      { return scanned(ELSE); }
"while"     { return scanned(WHILE); }
"null"      { return scanned(NUL); }
"new"       { return scanned(NEW); }
"this"      { return scanned(THIS); }
"=="        { return scanned(EQUALITY); }
"||"        { return scanned(OR); }
"!"         { return scanned(NOT); }
"<"         { return scanned(LESS); }
"="         { return scanned(ASSIGN); }
"+"         { return scanned(PLUS); }
"-"         { return scanned(MINUS); }
"*"         { return scanned(TIMES); }
"."         { return scanned(DOT); }
";"         { return scanned(SEMICOLON); }
"{"         { return scanned(LBRACE); }
"}"         { return scanned(RBRACE); }
"("         { return scanned(LPAREN); }
")"         { return scanned(RPAREN); }
{id}        { return scanned(ID); }
0|[1-9]{digit}*    { return scanned(NATLITERAL); }
{whitespace} { }
"//"[^\n]*  { }
<<EOF>>     { return scanned(ENDOFFILE); }
.           {
  fprintf(stderr, "Lex error on line %d: Illegal character '%s'\n", yylineno, yytext);
  exit(-1);
}
%%
Token scanned(Token t) {
  if (DEBUG) {
    switch(t) {
      case CLASS:      printf("CLASS "); break;
      case EXTENDS:    printf("EXTENDS "); break;
      case FINAL:      printf("FINAL "); break;
      case MAIN:       printf("MAIN "); break;
      case NATTYPE:    printf("NATTYPE "); break;
      case PRINTNAT:   printf("PRINTNAT "); break;
      case READNAT:    printf("READNAT "); break;
      case ASSERT:     printf("ASSERT "); break;
      case IF:         printf("IF "); break;
      case ELSE:       printf("ELSE "); break;
      case WHILE:      printf("WHILE "); break;
      case NUL:        printf("NUL "); break;
      case NEW:        printf("NEW "); break;
      case THIS:       printf("THIS "); break;
      case EQUALITY:   printf("EQUALITY "); break;
      case OR:         printf("OR "); break;
      case NOT:        printf("NOT "); break;
      case LESS:       printf("LESS "); break;
      case ASSIGN:     printf("ASSIGN "); break;
      case PLUS:       printf("PLUS "); break;
      case MINUS:      printf("MINUS "); break;
      case TIMES:      printf("TIMES "); break;
      case DOT:        printf("DOT "); break;
      case SEMICOLON:  printf("SEMICOLON "); break;
      case LBRACE:     printf("LBRACE "); break;
      case RBRACE:     printf("RBRACE "); break;
      case LPAREN:     printf("LPAREN "); break;
      case RPAREN:     printf("RPAREN "); break;
      case ID:         printf("ID(%s) ", yytext); break;
      case NATLITERAL: printf("NATLITERAL(%s) ", yytext); break;
      case ENDOFFILE:  printf("ENDOFFILE\n"); break;
      default:
        printf("ERROR: unknown token %d\n", t);
        exit(-1);
    }
  }
  return t;
}
