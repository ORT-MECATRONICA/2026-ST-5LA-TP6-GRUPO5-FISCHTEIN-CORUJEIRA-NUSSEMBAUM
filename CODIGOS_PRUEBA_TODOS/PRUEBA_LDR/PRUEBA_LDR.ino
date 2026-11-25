//Grupo 5: Sol Corujeira, Benjamin Fischtein y Lara Nussembaum
#define LDR 25

void setup() {
  Serial.begin(115200);
  pinMode(LDR,INPUT);
}

void loop() {
  int valor = analogRead(LDR);
  Serial.println(valor);
}
