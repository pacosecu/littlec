/* A Little C interpreter. 

   Minor fixes incorporated as of 1/4/96.
*/

#include <stdio.h>
#include <setjmp.h>
#include <math.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#define NUM_FUNC        100
#define NUM_GLOBAL_VARS 100
#define NUM_LOCAL_VARS  200
#define NUM_BLOCK       100
#define ID_LEN          31
#define FUNC_CALLS      31
#define NUM_PARAMS      31
#define PROG_SIZE       10000
#define LOOP_NEST       31

enum token_types {DELIMITER, IDENTIFIER, NUMBER, KEYWORD,
                TEMP, STRING, BLOCK};

/* add additional C keyword tokens here */
enum keyword_tokens {ARG, CHAR, INT, IF, ELSE, FOR, DO, WHILE, REPEAT, UNTIL, BREAK, CONTINUE, 
             SWITCH, RETURN, FINISHED, END};

/* add additional double operators here (such as ->) */
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

char *prog;  /* current location in source code */
char *p_buf; /* points to start of program buffer */
jmp_buf e_buf; /* hold environment for longjmp() */

/* An array of these structures will hold the info
   associated with global variables.
*/
struct var_type {
  char v_name[ID_LEN];
  int v_type;
  int v_value;
};

struct var_type global_vars_table[NUM_GLOBAL_VARS];

struct var_type local_vars_stack[NUM_LOCAL_VARS];

struct func_type {
  char func_name[ID_LEN];
  int func_ret_type;
  char *func_loc;  /* location of entry point in file */
} func_table[NUM_FUNC];

int call_stack[NUM_FUNC];

struct commands { /* keyword lookup table */
  char command[20];
  char keyword_token;
} table[] = { /* Commands must be entered lowercase */
  "if", IF, /* in this table. */
  "else", ELSE,
  "for", FOR,
  "do", DO,
  "while", WHILE,
  "repeat", REPEAT,
  "until", UNTIL,
  "break", BREAK,
  "continue", CONTINUE,
  "char", CHAR,
  "int", INT,
  "return", RETURN,
  "", END
};

char token[80];
char token_type, keyword_token;

int functos;    /* index to top of function call stack */
int func_index; /* index into function table */
int gvar_index; /* index into global variable table */
int lvartos;    /* index into local variable stack */

int ret_value; /* function return value */

void print(void), prescan(void);
void declare_global(void), call(void), putback(void);
void declare_local(void), local_push(struct var_type i);
void eval_exp(int *value), syntax_error(int error);
void execute_if(void), find_eob(void), execute_for(void);
void get_params(void), get_args(void);
void execute_while(void),  func_push(int i);
void execute_do(void), execute_repeat(void);
void assign_var(char *v_name, int v_value);
int load_program(char *p, char *fname), find_var(char *s);
void interpret_block(void), func_ret(void);
int func_pop(void), is_var(char *s), get_token(void);
char *find_func(char *name);

int main(int argc, char **argv)
{
  if(argc!=2) {
    printf("Usage: littlec <filename>\n");
    exit(1);
  }

  /* allocate memory for the program */
  if((p_buf=(char *) malloc(PROG_SIZE))==NULL) {
    printf("Allocation Failure");
    exit(1);
  }

  /* load the program to execute */
  if(!load_program(p_buf, argv[1])) exit(1);
  if(setjmp(e_buf)) exit(1); /* initialize long jump buffer */

  gvar_index = 0;  /* initialize global variable index */

  /* set program pointer to start of program buffer */
  prog = p_buf;
  prescan(); /* find the location of all functions
                and global variables in the program */

  lvartos = 0;     /* initialize local variable stack index */
  functos = 0;      /* initialize the CALL stack index */

  /* setup call to main() */
  prog = find_func("main");  /* find program starting point */

  if(!prog) { /* incorrect or missing main() function in program */
    printf("main() not found.\n");
    exit(1);
  }

  prog--; /* back up to opening ( */
  strcpy(token, "main");
  call();  /* call main() to start interpreting */

  return 0;
}

/* Interpret a single statement or block of code. When
   interpret_block() returns from its initial call, the final
   brace (or a return) in main() has been encountered.
*/
void interpret_block(void)
{
  int value;
  char block = 0;

  do {
    token_type = get_token();

    /* If interpreting single statement, return on first semicolon. */

    /* see what kind of token is up */
    if(token_type==IDENTIFIER) {
      /* Not a keyword, so process expression. */
      putback();   /* restore token to input stream for further processing by eval_exp() */
      eval_exp(&value);  /* process the expression */
      if(*token!=';') syntax_error(SEMICOLON_EXPECTED);

    } else if(token_type==BLOCK) { /* if it's a block */
        if(*token=='{') /* is a block */
          block = 1; /* interpreting block, not statement */
        else return; /* is a }, so return */
    
    } else /* is keyword */
      switch(keyword_token) {
        case CHAR:
        case INT:     /* declare local variables */
          putback();
          declare_local();
          break;
        case RETURN:  /* return from function call */
          func_ret();
          return;
        case IF:      /* process an if statement */
          execute_if();
          break;
        case ELSE:    /* process an else statement */
          find_eob(); /* find end of else block and continue execution */
          break;
        case WHILE:   /* process a while loop */
          execute_while();
          break;
        case DO:      /* process a do-while loop */
          execute_do();
          break;
        case REPEAT:  /* process a repeat-until */
          execute_repeat();
          break;
        case FOR:     /* process a for loop */
          execute_for();
          break;
        case END:
          exit(0);
     }
  } while(keyword_token != FINISHED && keyword_token != BREAK && keyword_token != CONTINUE && block);
}

/* Load a program. */
int load_program(char *p, char *fname)
{
  FILE *fp;
  int i=0;

  if((fp=fopen(fname, "rb"))==NULL) return 0;

  i = 0;
  do {
    *p = getc(fp);
    p++; i++;
  } while(!feof(fp) && i<PROG_SIZE);
  if(*(p-2)==0x1a) *(p-2) = '\0'; /* null terminate the program */
  else *(p-1) = '\0';
  fclose(fp);
  return 1;
}

/* Find the location of all functions in the program
and store global variables. */
void prescan(void)
{
  char *p, *tp;
  char temp[32];
  int datatype;
  int brace = 0; /* When 0, this var tells us that current source position is outside
                    of any function. */
  p = prog;
  func_index = 0;

  do {
    while(brace) { /* bypass code inside functions */
      get_token();
      if(*token == '{') brace++;
      if(*token == '}') brace--;
    }
    tp = prog; /* save current position */
    get_token();
    /* global var type or function return type */
    if(keyword_token==CHAR || keyword_token==INT) {
      datatype = keyword_token; /* save data type */
      get_token();
      if(token_type == IDENTIFIER) {
        strcpy(temp, token);
        get_token();
        if(*token != '(') { /* must be global var */
          prog = tp; /* return to start of declaration */
          declare_global();
        } else if(*token == '(') { /* must be a function */
          func_table[func_index].func_loc = prog;
          func_table[func_index].func_ret_type = datatype;
          strcpy(func_table[func_index].func_name, temp);
          func_index++;
          while(*prog != ')') prog++;
          prog++;
          /* now prog points to opening curly brace of function */
        } else putback();
      }
    } else if(*token == '{') brace++;
  } while(keyword_token != FINISHED); 
  prog = p;
}


/* Return the entry point of the specified function.
   Return NULL if not found.
*/
char *find_func(char *name)
{
  register int i;

  for(i=0; i<func_index; i++)
    if(!strcmp(name, func_table[i].func_name))
      return func_table[i].func_loc;

  return NULL;
 }

/* Declare a global variable. */
void declare_global(void)
{
  int vartype;

  get_token();  /* get type */

  vartype = keyword_token; /* save var type */

  do { /* process comma-separated list */
    global_vars_table[gvar_index].v_type = vartype;
    global_vars_table[gvar_index].v_value = 0;  /* init to 0 */
    get_token();  /* get name */
    strcpy(global_vars_table[gvar_index].v_name, token);
    get_token();
    gvar_index++;
  } while(*token==',');
  if(*token!=';') syntax_error(SEMICOLON_EXPECTED);
}

/* Declare a local variable. */
void declare_local(void)
{
  struct var_type i;

  get_token();  /* get type */

  i.v_type = keyword_token;
  i.v_value = 0;  /* init to 0 */

  do { /* process comma-separated list */
    get_token(); /* get var name */
    strcpy(i.v_name, token);
    local_push(i);
    get_token();
  } while(*token==',');
  if(*token!=';') syntax_error(SEMICOLON_EXPECTED);
}

/* Call a function. */
void call(void)
{
  char *loc, *temp;
  int lvartemp;

  loc = find_func(token); /* find entry point of function */
  if(loc==NULL)
    syntax_error(FUNC_UNDEF); /* function not defined */
  else {
    lvartemp = lvartos;  /* save local var stack index because local_push() modify it */
    get_args();  /* get function arguments */
    temp = prog; /* save return point of the function */
    func_push(lvartemp);  /* save local var stack index in the call_stack[] */
    prog = loc;  /* reset prog to start of function */
    get_params(); /* load the function's parameters with
                     the values of the arguments */
    interpret_block(); /* interpret the function */
    prog = temp; /* reset the program pointer to its return point */
    lvartos = func_pop(); /* reset the local var stack to its value before the function call */
  }
}

/* Push the arguments to a function onto the local
   variable stack. */
void get_args(void)
{
  int value, count, temp[NUM_PARAMS];
  struct var_type i;

  count = 0;
  get_token();
  if(*token!='(') syntax_error(PAREN_EXPECTED);

  /* process a comma-separated list of values */
  do {
    eval_exp(&value);
    temp[count] = value;  /* save temporarily */
    get_token();
    count++;
  }while(*token==',');
  count--;
  /* now, push on local_vars_stack in reverse order */
  for(; count>=0; count--) {
    i.v_value = temp[count];
    i.v_type = ARG;
    local_push(i);
  }
}

/* Get function parameters. */
void get_params(void)
{
  struct var_type *p;
  int i;

  i = lvartos-1;
  do { /* process comma-separated list of parameters */
    get_token();
    p = &local_vars_stack[i];
    if(*token!=')') {
      if(keyword_token!=INT && keyword_token!=CHAR) syntax_error(TYPE_EXPECTED);
      p->v_type = token_type;
      get_token();

      /* link parameter name with argument already on
         local var stack */
      strcpy(p->v_name, token);
      get_token();
      i--;
    }
    else break;
  } while(*token==',');
  if(*token!=')') syntax_error(PAREN_EXPECTED);
}

/* Return from a function. */
void func_ret(void)
{
  int value;

  value = 0;
  /* get return value, if any */
  eval_exp(&value);

  ret_value = value;
}

/* Push a local variable. */
void local_push(struct var_type i)
{
  if(lvartos>NUM_LOCAL_VARS)
   syntax_error(TOO_MANY_LVARS);

  local_vars_stack[lvartos] = i;
  lvartos++;
}

/* Pop index into local variable stack. */
int func_pop(void)
{
  functos--;
  if(functos<0) syntax_error(RET_NOCALL);
  return(call_stack[functos]);
}

/* Push index of local variable stack. */
void func_push(int i)
{
  if(functos>NUM_FUNC)
    syntax_error(NEST_FUNC);

  call_stack[functos] = i;
  functos++;
}

/* Assign a value to a variable. */
void assign_var(char *v_name, int v_value)
{
  register int i;

  /* first, see if it's a local variable */
  for(i=lvartos-1; i>=call_stack[functos-1]; i--)  {
    if(!strcmp(local_vars_stack[i].v_name, v_name)) {
      local_vars_stack[i].v_value = v_value;
      return;
    }
  }
  if(i < call_stack[functos-1])
  /* if not local, try global var table */
    for(i=0; i<NUM_GLOBAL_VARS; i++)
      if(!strcmp(global_vars_table[i].v_name, v_name)) {
        global_vars_table[i].v_value = v_value;
        return;
      }
  syntax_error(NOT_VAR); /* variable not found */
}

/* Find the value of a variable. */
int find_var(char *s)
{
  register int i;

  /* first, see if it's a local variable */
  for(i=lvartos-1; i>=call_stack[functos-1]; i--)
    if(!strcmp(local_vars_stack[i].v_name, token))
      return local_vars_stack[i].v_value;

  /* otherwise, try global vars */
  for(i=0; i<NUM_GLOBAL_VARS; i++)
    if(!strcmp(global_vars_table[i].v_name, s))
      return global_vars_table[i].v_value;

  syntax_error(NOT_VAR); /* variable not found */
  return -1;
}

/* Determine if an identifier is a variable. Return
   1 if variable is found; 0 otherwise.
*/
int is_var(char *s)
{
  register int i;

  /* first, see if it's a local variable */
  for(i=lvartos-1; i>=call_stack[functos-1]; i--)
    if(!strcmp(local_vars_stack[i].v_name, token))
      return 1;

  /* otherwise, try global vars */
  for(i=0; i<NUM_GLOBAL_VARS; i++)
    if(!strcmp(global_vars_table[i].v_name, s))
      return 1;

  return 0;
}

/* Execute an if statement. */
void execute_if(void)
{
  int cond;

  eval_exp(&cond); /* get left expression */

  if(cond) { /* is true so process target of IF */
    interpret_block();
  }
  else { /* otherwise skip around IF block and
            process the ELSE, if present */
    find_eob(); /* find end of else block */
    get_token();

    if(keyword_token!=ELSE) {
      putback();  /* restore token if
                     no ELSE is present */
      return;
    }
    interpret_block();
  }
}

/* Execute a while loop. */
void execute_while(void)
{
  int cond;
  char *temp;
  
  putback();
  temp = prog;  /* save location of top of while loop */
  get_token();
  eval_exp(&cond);  /* check the conditional expression */
w:if(cond) {
    interpret_block();  /* if true, interpret */
    if(keyword_token != BREAK && keyword_token != CONTINUE) {
      prog = temp;  /* loop back to top */
    } else if(keyword_token == BREAK) {
      get_token();
      find_eob();
      return;
    } else if(keyword_token == CONTINUE) {
      prog = temp;
      get_token();
      eval_exp(&cond);
      goto w;
    }
  } else {  /* otherwise, skip around loop */
    find_eob(); /* find end of else block */
    return;
  }
}

/* Execute a do loop. */
void execute_do(void)
{
  int cond;
  char *temp;
  
  putback();
  temp = prog;  /* save location of top of do loop */
  
d:get_token(); /* get start of loop */
  interpret_block(); /* interpret loop */
  
  if(keyword_token != BREAK && keyword_token != CONTINUE) {
    get_token();
    if(keyword_token!=WHILE) syntax_error(WHILE_EXPECTED);
    eval_exp(&cond); /* check the loop condition */
    if(cond) prog = temp; /* if true loop; otherwise, continue on */
  } else if(keyword_token == BREAK) {
    while(*prog != ')' && *(prog+1) != ';') get_token();
    get_token();
  } else if(keyword_token == CONTINUE) {
    printf("keyword_token = %d\n", keyword_token);
    prog = temp;
    goto d;
  }
}
/*Execute a repeat loop. */
void execute_repeat(void)
{
  int cond;
  char *temp;

  putback();
  temp = prog;  /* save location of top of repeat loop */

r:get_token(); /* get start of loop */
  interpret_block(); /* interpret loop */
  
  if(keyword_token != BREAK && keyword_token != CONTINUE) {
    get_token();
    if(keyword_token!=UNTIL) syntax_error(UNTIL_EXPECTED);
    eval_exp(&cond); /* check the loop condition */
    if(!cond) prog = temp; /* if true loop; otherwise, continue on */
  } else if(keyword_token == BREAK) {
    while(*prog != ')' && *(prog+1) != ';') get_token();
    get_token();
  } else if(keyword_token == CONTINUE) {
    prog = temp;
    goto r;
  }
}

/* Execute a for loop. */
void execute_for(void)
{
  int cond;
  char *temp, *temp2;
  int brace ;

  get_token();
  eval_exp(&cond);  /* initialization expression */
  if(*token!=';') syntax_error(SEMICOLON_EXPECTED);
  prog++; /* get past the ; */
  temp = prog;
  for(;;) {
  f:eval_exp(&cond);  /* check the condition */
    if(*token!=';') syntax_error(SEMICOLON_EXPECTED);
    prog++; /* get past the ; */
    temp2 = prog;

    /* find the start of the for block */
    brace = 1;
    while(brace) {
      get_token();
      if(*token=='(') brace++;
      if(*token==')') brace--;
    }

    if(cond) {
      interpret_block();  /* if true, interpret */
      if(keyword_token != BREAK && keyword_token != CONTINUE) {
        prog = temp2;
        eval_exp(&cond); /* do the increment */
        prog = temp;  /* loop back to top */
      } else if(keyword_token == BREAK) {
        prog = temp2;
        /* find the start of the for block */
        brace = 1;
        while(brace) {
          get_token();
          if(*token=='(') brace++;
          if(*token==')') brace--;
        }
        find_eob();
        return;
      } else if(keyword_token == CONTINUE) {
        prog = temp2;
        eval_exp(&cond); /* do the increment */
        prog=temp;
        goto f;

      }
    } else {  /* otherwise, skip around loop */
      find_eob(); /* find end of else block */
      return;
    }
  }
}

/* Find the end of a block. */
void find_eob(void)
{
  int brace;

  get_token();
  brace = 1;
  do {
    get_token();
    if(*token=='{') brace++;
    else if(*token=='}') brace--;
  } while(brace);
}
