// Ejemplo de uso de continue en un bucle for() 
// Objetivo: saltarse la impresión del número 5 

int main()
{
  int i;

  for(i=0; i<10; i=i+1) {
    
    if(i == 5) {
      continue;
    }
    print(i);
    print("");
  }
  
  puts("");
  print("Fuera del ciclo con valor i: ");
  print(i);
  puts("");

  return 0;
}
