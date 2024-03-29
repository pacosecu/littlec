/****** Internal Library Functions *******/

/* Add more of your own, here. */
#include <stdio.h>
#include <stdlib.h>

extern char *prog; /* points to current location in program */
extern char token[80]; /* holds string representation of token */
extern char token_type; /* contains type of token */
extern char keyword_token; /* holds the internal representation of token */

enum token_types {DELIMITER, IDENTIFIER, NUMBER, KEYWORD,
                TEMP, STRING, BLOCK};

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
      WHILE_EXPECTED, UNTIL_EXPECTED, QUOTE_EXPECTED, NOT_STRING,
      TOO_MANY_LVARS, DIVISION_BY_ZERO};

int get_token(void);
void syntax_error(int error), eval_exp(int *result);
void putback(void);

/* Get a character from console. (Use getchar() if
   your compiler does not support getche().) */
int call_getche()
{
  char ch;
  ch = getchar();
  while(*prog!=')') prog++;
  prog++;   /* advance to end of line */
  return ch;
}

/* Put a character to the display. */
int call_putch()
{
  int value;

  eval_exp(&value);
  printf("%c", value);
  return value;
}

/* Call puts(). */
int call_puts(void)
{
  get_token();
  if(*token!='(') syntax_error(PAREN_EXPECTED);
  get_token();
  if(token_type!=STRING) syntax_error(QUOTE_EXPECTED);
  puts(token);
  get_token();
  if(*token!=')') syntax_error(PAREN_EXPECTED);
  
  get_token();
  if(*token!=';') syntax_error(SEMICOLON_EXPECTED);
  putback();
  return 0;
}

/* A built-in console output function. */
int print(void)
{
  int i;

  get_token();
  if(*token!='(')  syntax_error(PAREN_EXPECTED);

  get_token();
  if(token_type==STRING) { /* output a string */
    printf("%s", token);
  }
  else {  /* output a number */
   putback();
   eval_exp(&i);
   printf("%d", i);
  }

  get_token();

  if(*token!=')') syntax_error(PAREN_EXPECTED);

  get_token();
  if(*token!=';') syntax_error(SEMICOLON_EXPECTED);
  putback();
  return 0;
}

/* Read an integer from the keyboard. */
int getnum(void)
{
  char s[80];
	
  gets(s);
  while(*prog!=')') prog++;
    prog++;  /* advance to end of line */
  }
  return atoi(s);
}



