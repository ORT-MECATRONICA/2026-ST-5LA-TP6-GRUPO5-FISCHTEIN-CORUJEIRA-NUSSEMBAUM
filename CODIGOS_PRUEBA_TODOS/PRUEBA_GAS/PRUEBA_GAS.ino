//Grupo 5: Sol Corujeira, Benjamin Fischtein y Lara Nussembaum
#define GAS 12

void setup() {
  Serial.begin(115200);
  pinMode(GAS,INPUT);
}

void loop() {
  int valorGas = analogRead(GAS);
  valorGas = map(valorGas,0,4095,1,100);
  Serial.println("Valor: " + String(valorGas));
}
