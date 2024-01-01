int main()
{
  int i;
  int valor;
  int suma;

  suma = 0;
  repeat {
    print("Introduce un valor (0 para salir, -1 para salir antes): ");
    valor = getnum();
    puts("");

    if(valor == -1) {
      break;
    }
    suma = suma + valor;
  } until(valor>-1 && valor<1);

  print("suma es: ");
  print(suma);
  puts("");
  
  return 0;
}
