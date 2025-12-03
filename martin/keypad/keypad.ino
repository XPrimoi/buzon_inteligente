#include <Keypad.h>

#define KEYPAD_ROWS 4 // Keypad de 4 filas
#define KEYPAD_COLS 4 // Keypad de 4 columnas
char keypadKeys[KEYPAD_ROWS][KEYPAD_COLS] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};

byte KEYPAD_ROW_PINS[KEYPAD_ROWS] = {A4, A3, A2, A1}; 
byte KEYPAD_COL_PINS[KEYPAD_COLS] = {D13, D12, D11, D10};

Keypad keypad = Keypad(makeKeymap(keypadKeys), KEYPAD_ROW_PINS, KEYPAD_COL_PINS, KEYPAD_ROWS, KEYPAD_COLS);

void setup(){
  // Setup wifi

  Serial.begin(9600);
}
  
void loop(){
  char key = keypad.getKey();
  
  if (key){
    Serial.println(key);
  }

  // Envio de datos con mqtt

  // Recepción de datos con mqtt
}