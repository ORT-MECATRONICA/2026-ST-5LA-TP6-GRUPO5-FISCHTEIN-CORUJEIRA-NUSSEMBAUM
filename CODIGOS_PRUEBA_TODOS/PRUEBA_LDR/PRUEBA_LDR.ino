//Grupo 5: Sol Corujeira, Benjamin Fischtein y Lara Nussembaum
#define LDR 23

void setup() {
  Serial.begin(115200);
  pinMode(LDR,INPUT);
}

void loop() {
  int valor = map(analogRead(LDR),0,4095,0,100);
  Serial.println(valor);
}
