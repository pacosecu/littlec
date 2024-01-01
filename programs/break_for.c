int main() {
  int i;
  int num;
  int suma;
  
  suma = 0; 
  for(i=1; i<=4; i=i+1) {
    print("Introduce un número (0 para salir): ");
    num = getnum();
    puts("");

    if(num == 0) {
      break;
    }
    suma = suma + num;
  }

  print("suma = ");
  print(suma);
  puts("");

  return 0;
}
