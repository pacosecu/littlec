/* This program demonstrates recursive functions. */
/* return the factorial of n */

int main()
{
  int n;  /* local vars */
  do {
    puts("");
    puts("Factorial de n");
    print("enter n (0 to quit): ");
    n = getnum();
    
    if(n < 0) {
      puts("numbers must be positive, try again");
    }
    else {
      print("Factorial of ");
      print(n);
      print(" is: ");
 	    print(factorial(n));
      puts("");
    }

  } while(n!=0);
  return 0;
  
}

int factorial(int n)
{
	if(n==0) {
		return 1;
	}		
	else {
		return n * factorial(n-1);
	}		
}
