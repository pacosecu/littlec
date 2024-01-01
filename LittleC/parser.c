/* Recursive descent parser for integer expressions
   which may include variables and function calls.

   Minor fixes incorporated as of 1/4/96.

   Herbert Schildt's Little C Interpreter Modified by paco secades
   Adds support for C++ comments
   Add support for repeat until loop structure
   Add suport for &&, || and !
   Aadd support for break and continue in loop structures
   Please enhanced it and share it!
   enjoy!

 */
#include <setjmp.h>
#include <math.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define NUM_FUNC        100
#define NUM_GLOBAL_VARS 100
#define NUM_LOCAL_VARS  200
#define ID_LEN          31
#define FUNC_CALLS      31
#define PROG_SIZE       10000
#define FOR_NEST        31

enum token_types {DELIMITER, IDENTIFIER, NUMBER, KEYWORD, TEMP,
                STRING, BLOCK};

enum keyword_tokens {ARG, CHAR, INT, IF, ELSE, FOR, DO, WHILE, REPEAT, UNTIL, BREAK, CONTINUE, 
                     SWITCH, RETURN, FINISHED, END};

enum double_ops {LT=1, LE, GT, GE, EQ, NE, AND, OR, NOT};

/* These are the constants used to call syntax_error() when
   a syntax error occurs. Add more if you like.
   NOTE: SYNTAX is a generic error message used when
   nothing else seems appropriate.
*/
enum error_msg
     {SYNTAX, UNBAL_PARENS, NO_EXPRESSION, EQUALS_EXPECTED,
      NOT_VAR, PARAM_ERR, SEMICOLON_EXPECTED,
      UNBAL_BRACES, FUNC_UNDEF, TYPE_EXPECTED,
      NEST_FUNC, RET_NOCALL, PAREN_EXPECTED,
      WHILE_EXPECTED, UNTIL_EXPECTED, QUOTE_EXPECTED, NOT_TEMP,
      TOO_MANY_LVARS, DIVISION_BY_ZERO};

extern char *prog;  /* current location in source code */
extern char *p_buf;  /* points to start of program buffer */
extern jmp_buf e_buf; /* hold environment for longjmp() */

/* An array of these structures will hold the info
   associated with global variables.
*/
extern struct var_type {
  char v_name[32];
  int v_type;
  int v_value;
};

extern struct var_type global_vars_table[NUM_GLOBAL_VARS];

/*  This is the function call stack. */
extern struct func_type {
  char func_name[32];
  int func_ret_type;
  char *func_loc;  /* location of function entry point in file */
} func_stack[NUM_FUNC];

/* Keyword table */
extern struct commands {
  char command[20];
  char keyword_token;
} table[];

/* "Standard library" functions are declared here so
   they can be put into the internal function table that
   follows.
 */
int call_getche(void), call_putch(void);
int call_puts(void), print(void), getnum(void);

struct internal_function_type {
  char *f_name; /* function name */
  int (*p)();  /* pointer to the function */
} internal_function_table[] = {
  "getche", call_getche,
  "putch", call_putch,
  "puts", call_puts,
  "print", print,
  "getnum", getnum,
  "", 0  /* null terminate the list */
};

extern char token[80]; /* string representation of token */
extern char token_type; /* contains type of token */
extern char keyword_token; /* internal representation of token */

extern int ret_value; /* function return value */

void eval_exp(int *value);
void eval_exp0(int *value), eval_exp1(int *value);
void eval_exp2(int *value), eval_exp3(int *value);
void eval_exp4(int *value), eval_exp5(int *value);
void eval_exp6(int *value), atom(int *value);

void syntax_error(int error), putback(void);
void assign_var(char *v_name, int value);
int isdelim(char c), look_up(char *s), iswhite(char c);
int find_var(char *s), get_token(void);
int internal_function(char *s);
int is_var(char *s);
char *find_func(char *name);
void call(void);

/* Entry point into parser. */
void eval_exp(int *value)
{
  get_token();
  if(!*token) {
    syntax_error(NO_EXPRESSION);
    /* return; */
  }
  if(*token==';') {
    *value = 0; /* empty expression */
    return;
  }
  eval_exp0(value);
  putback(); /* return last token read to input stream */
}

/* Process an assignment expression */
void eval_exp0(int *value)
{
  char temp[ID_LEN];  /* holds name of var receiving
                         the assignment */
  register int temp_token_type;

  if(token_type==IDENTIFIER) {
    if(is_var(token)) {  /* if a var, see if assignment */
      strcpy(temp, token);
      temp_token_type = token_type;
      get_token();
      if(*token=='=') {  /* is an assignment */
        get_token();
        eval_exp0(value);  /* get value to assign */
        assign_var(temp, *value);  /* assign the value */
        return;
      }
      else {  /* not an assignment */
        putback();  /* restore original token */
        strcpy(token, temp);
        token_type = temp_token_type;
      }
    }
  }
  eval_exp1(value);
}

/* This array is used by eval_exp1(). Because
   some compilers cannot initialize an array within a
   function it is defined as a global variable.
*/
char logops[4] = {
  AND, OR, NOT, 0
};

/* Is an AND or a OR */
void eval_exp1(int *value)
{
  int partial_value;
  register char op;

  eval_exp2(value);
  op = *token;
  if(strchr(logops, op)) {
    get_token();
    eval_exp2(&partial_value);
    switch(op) {  /* perform the relational operation */
      case OR:
        *value = *value || partial_value;
        break;
      case AND:
        *value = *value && partial_value;
        break;
    }
  }
}


/* This array is used by eval_exp2(). Because
   some compilers cannot initialize an array within a
   function it is defined as a global variable.
*/
char relops[7] = {
  LT, LE, GT, GE, EQ, NE, 0
};

/* Is a relational operator */
void eval_exp2(int *value)
{
  int partial_value;
  register char op;

  eval_exp3(value);
  op = *token;
  if(strchr(relops, op)) {
    get_token();
    eval_exp3(&partial_value);
    switch(op) {  /* perform the relational operation */
      case LT:
        *value = *value < partial_value;
        break;
      case LE:
        *value = *value <= partial_value;
        break;
      case GT:
        *value = *value > partial_value;
        break;
      case GE:
        *value = *value >= partial_value;
        break;
      case EQ:
        *value = *value == partial_value;
        break;
      case NE:
        *value = *value != partial_value;
        break;
    }
  }
}


/*  Add or subtract two terms. */
void eval_exp3(int *value)
{
  register char  op;
  int partial_value;

  eval_exp4(value);
  while((op = *token) == '+' || op == '-') {
    get_token();
    eval_exp4(&partial_value);
    switch(op) {  /* add or subtract */
      case '+':
        *value = *value + partial_value;
        break;
      case '-':
        *value = *value - partial_value;
        break;
    }
  }
}

/* Multiply or divide two factors. */
void eval_exp4(int *value)
{
  register char  op;
  int partial_value, t;

  eval_exp5(value);
  while((op = *token) == '*' || op == '/' || op == '%') {
    get_token();
    eval_exp5(&partial_value);
    switch(op) { /* mul, div, or modulus */
      case '*':
        *value = *value * partial_value;
        break;
      case '/':
        if(partial_value == 0.0) {
          syntax_error(DIVISION_BY_ZERO);
          /* exit(1); */
        } else
          *value = (*value) / partial_value;
        break;
      case '%':
        t = (*value) / partial_value;
        *value = *value-(t * partial_value);
        break;
    }
  }
}

/* Is a unary + or - or is a NOT */
void eval_exp5(int *value)
{
  register char  op;

  op = '\0';
  if(*token=='+' || *token=='-' || *token=='!') {
    op = *token;
    get_token();
  }
  eval_exp6(value);
  if(op) {
    if(op=='+') *value = +(*value);
    if(op=='-') *value = -(*value);
    if(op=='!') *value = !(*value);
  }
}


/* Process parenthesized expression. */
void eval_exp6(int *value)
{
  if((*token == '(')) {
    get_token();
    eval_exp0(value);   /* get subexpression */
    if(*token != ')') syntax_error(PAREN_EXPECTED);
    get_token();
  }
  else
    atom(value);
}

/* Find value of number, variable, or function. */
void atom(int *value)
{
  int i;

  switch(token_type) {
  case IDENTIFIER:
    i = internal_function(token);
    if(i != -1) {  /* call "standard library" function */
      *value = (*internal_function_table[i].p)();
    }
    else
    if(find_func(token)){  /* call user-defined function */
      call();
      *value = ret_value;
    }
    else *value = find_var(token);  /* get var's value */
    get_token();
    return;
  case NUMBER: /* is numeric constant */
    *value = atoi(token);
    get_token();
    return;
  case DELIMITER: /* see if character constant */
    if(*token=='\'') {
      *value = *prog;
      prog++;
      if(*prog!='\'') syntax_error(QUOTE_EXPECTED);
      prog++;
      get_token();
      return ;
    }
    if(*token==')') return; /* process empty expression */
    else syntax_error(SYNTAX); /* syntax error */
  default:
    syntax_error(SYNTAX); /* syntax error */
  }
}

/* Display an error message. */
void syntax_error(int error)
{
  char *p, *temp;
  int linecount = 0;
  register int i;

  static char *e[]= {
    "syntax error",
    "unbalanced parentheses",
    "no expression present",
    "equals sign expected",
    "not a variable",
    "parameter error",
    "semicolon expected",
    "unbalanced braces",
    "function undefined",
    "type specifier expected",
    "too many nested function calls",
    "return without call",
    "parentheses expected",
    "while expected",
    "until expected",
    "closing quote expected",
    "not a string",
    "too many local variables",
    "division by zero"
  };
  printf("%s", e[error]);
  p = p_buf;
  while(p != prog) {  /* find line number of error */
    p++;
    if(*p == '\r') {
      linecount++;
    }
  }
  printf(" in line %d\n", linecount);

  temp = p;
  for(i=0; i<20 && p>p_buf && *p!='\n'; i++, p--);
  for(i=0; i<30 && p<=temp; i++, p++) printf("%c", *p);

  longjmp(e_buf, 1); /* return to safe point */
}

/* Get a token. */
int get_token(void)
{
  register char *temp;

  token_type = 0; keyword_token = 0;

  temp = token;
  *temp = '\0';

 /* skip over white space */
  while(iswhite(*prog) && *prog) ++prog;

  if(*prog=='\r') {
    ++prog;
    ++prog;
    /* skip over white space */
    while(iswhite(*prog) && *prog) ++prog;
  }

  if(*prog=='\0') { /* end of file */
    *token = '\0';
    keyword_token = FINISHED;
    return(token_type=DELIMITER);
  }

  if(strchr("{}", *prog)) { /* block delimiters */
    *temp = *prog;
    temp++;
    *temp = '\0';
    prog++;
    return (token_type = BLOCK);
  }

  /* look for comments */
  if(*prog=='/')
    if(*(prog+1)=='*') { /* is a comment */
      prog += 2;
      do { /* find end of comment */
        while(*prog!='*') prog++;
        prog++;
      } while (*prog!='/');
      prog++;
    }

  /* look for c++ comments */
  if(*prog=='/')
    if(*(prog+1)=='/') { /* is a comment */
      prog += 2;
      do { /* find end of comment */
        prog++;
      } while (*prog!='\r');
      prog++;
      prog++;
    }

  if(strchr("!<>=", *prog)) { /* is or might be
                              a relation operator */
    switch(*prog) {
      case '=': if(*(prog+1)=='=') {
          prog++; prog++;
          *temp = EQ;
          temp++; *temp = EQ; temp++;
          *temp = '\0';
       }
       break;
      case '!': if(*(prog+1)=='=') {
          prog++; prog++;
          *temp = NE;
          temp++; *temp = NE; temp++;
          *temp = '\0';
       }
       break;
      case '<': if(*(prog+1)=='=') {
          prog++; prog++;
          *temp = LE; temp++; *temp = LE;
       }
       else {
          prog++;
          *temp = LT;
       }
       temp++;
       *temp = '\0';
       break;
      case '>': if(*(prog+1)=='=') {
          prog++; prog++;
          *temp = GE; temp++; *temp = GE;
       }
       else {
         prog++;
         *temp = GT;
       }
       temp++;
       *temp = '\0';
       break;
    }
    if(*token) return(token_type = DELIMITER);
  }

 if(strchr("&|", *prog)) { /* is or might be
                              an AND or an OR */
    switch(*prog) {
      case '&': if(*(prog+1)=='&') {
          prog++; prog++;
          *temp = AND;
          temp++; *temp = AND; temp++;
          *temp = '\0';
      }
       break;
      case '|': if(*(prog+1)=='|') {
          prog++; prog++;
          *temp = OR;
          temp++; *temp = OR; temp++;
          *temp = '\0';
       }
       break;
    }
    if(*token) return(token_type = DELIMITER);
  }

  if(strchr("+-*/%!=;(),'", *prog)){ /* delimiter */
    *temp = *prog;
    prog++; /* advance to next position */
    temp++;
    *temp = '\0';
    return (token_type=DELIMITER);
  }

  if(*prog=='"') { /* quoted string */
    prog++;
    while(*prog!='"'&& *prog!='\r') *temp++ = *prog++;
    if(*prog=='\r') syntax_error(SYNTAX);
    prog++; *temp = '\0';
    return(token_type=STRING);
  }

  if(isdigit(*prog)) { /* number */
    while(!isdelim(*prog)) *temp++ = *prog++;
    *temp = '\0';
    return(token_type = NUMBER);
  }

  if(isalpha(*prog)) { /* var or command */
    while(!isdelim(*prog)) *temp++ = *prog++;
    token_type=TEMP;
  }

  *temp = '\0';

  /* see if a string is a command or a variable */
  if(token_type==TEMP) {
    keyword_token = look_up(token); /* convert to internal rep */
    if(keyword_token) token_type = KEYWORD; /* is a keyword */
    else token_type = IDENTIFIER;
  }
  return token_type;
}

/* Return a token to input stream. */
void putback(void)
{
  char *t;

  t = token;
  for(; *t; t++) prog--;
}

/* Look up a token's internal representation in the
   token table.
*/
int look_up(char *s)
{
  register int i;
  char *p;

/* convert to lowercase */
  p = s;
  while(*p){ *p = tolower(*p); p++; }

  /* see if token is in table */
  for(i=0; *table[i].command; i++)
      if(!strcmp(table[i].command, s)) return table[i].keyword_token;
  return 0; /* unknown command */
}

/* Return index of internal library function or -1 if
   not found.
*/
int internal_function(char *s)
{
  int i;

  for(i=0; internal_function_table[i].f_name[0]; i++) {
    if(!strcmp(internal_function_table[i].f_name, s))  return i;
  }
  return -1;
}

/* Return true if c is a delimiter. */
int isdelim(char c)
{
  if(strchr(" ;,+-<>'/*%^&|=!()", c) || c==9 ||
     c=='\r' || c==0) return 1;
  return 0;
}

/* Return 1 if c is space or tab. */
int iswhite(char c)
{
  if(c==' ' || c=='\t') return 1;
  else return 0;
}
