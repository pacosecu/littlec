int main()
{

   int n, suma;

   suma = 0;
   repeat {
     print("Introduzca un n�mero entero (-1 para salir, 0 para continuar): ");
     n = getnum();
     puts("");

     if(n == 0) {
       puts("ERROR: El cero no tiene opuesto");
       continue;
       // En el caso de que n sea un cero,
       // la iteraci�n en curso del bucle
       // se interrumpe aqu�.
     }
     print("El opuesto es: ");
     print(-n);
     puts("");
     
     suma = suma + n;
   } until(n>-2 && n<0);

   print("Suma: ");
   print(suma);
   puts("");

   return 0;
}
