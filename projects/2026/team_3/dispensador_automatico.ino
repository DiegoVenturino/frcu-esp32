// --- Configuración de Pines Físicos ---
const int PIN_TRIG = 19;
const int PIN_ECHO = 18;
const int PIN_RELAY = 14; 
const int PIN_BOTON = 13;

// Pines LED RGB
const int PIN_LED_R = 17;
const int PIN_LED_G = 16; 
const int PIN_LED_B = 4;  

// --- Parámetros de Seguridad ---
const float DISTANCIA_PISO = 17.5; 
const float UMBRAL_VACIO = 16;   

// Lógica Física: PULLUP forzado. 3.3V en reposo, 0V al presionar.
const int ESTADO_PRESIONADO = LOW; 

// --- Variables de la Máquina de Estados ---
enum EstadoDispensador {
  ESPERANDO_VASO,
  ESPERANDO_RETIRO_TAPA,
  LLENANDO
};

EstadoDispensador estadoActual = ESPERANDO_VASO;
float distanciaBorde = 0.0;
float distanciaFondo = 0.0;
float ultimaDistanciaReportada = 0.0;

// Variables de Filtro de Ruido
int lecturasCorte = 0; 
int lecturasEmergencia = 0;

void setup() {
  Serial.begin(115200);
  
  pinMode(PIN_TRIG, OUTPUT);
  pinMode(PIN_ECHO, INPUT);
  pinMode(PIN_RELAY, OUTPUT);
  
  // Forzamos PULLUP para eliminar la estática y el bloqueo del botón
  pinMode(PIN_BOTON, INPUT_PULLUP); 
  
  pinMode(PIN_LED_R, OUTPUT);
  pinMode(PIN_LED_G, OUTPUT);
  pinMode(PIN_LED_B, OUTPUT);
  
  digitalWrite(PIN_RELAY, LOW);
  Serial.println("--- Dispensador Físico: Filtros Anti-Ruido Iniciados ---");
}

void colorLED(int r, int g, int b) {
  digitalWrite(PIN_LED_R, r);
  digitalWrite(PIN_LED_G, g);
  digitalWrite(PIN_LED_B, b);
}

float medirDistancia() {
  digitalWrite(PIN_TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(PIN_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);
  
  long duracion = pulseIn(PIN_ECHO, HIGH, 30000); 
  if (duracion == 0) return 999.0; 
  return (duracion * 0.0343) / 2.0;
}

void loop() {
  float distanciaActual = medirDistancia();
  bool botonPresionado = (digitalRead(PIN_BOTON) == ESTADO_PRESIONADO); 

  switch (estadoActual) {
    
    case ESPERANDO_VASO:
      colorLED(HIGH, LOW, LOW); // Rojo
      
      if (botonPresionado) {
        
        while(digitalRead(PIN_BOTON) == ESTADO_PRESIONADO) {
          delay(10);
        }
        
        Serial.println("Escaneando nivel de referencia...");
        delay(2000); 
        
        distanciaBorde = medirDistancia();
        
        if (distanciaBorde >= UMBRAL_VACIO) {
          Serial.println("ERROR: No hay vaso. Piso detectado.");
          for(int i = 0; i < 4; i++) {
            colorLED(HIGH, LOW, LOW);
            delay(150);
            colorLED(LOW, LOW, LOW);
            delay(150);
          }
          break; 
        }
        
        Serial.print("Borde del vaso a: ");
        Serial.print(distanciaBorde);
        Serial.println(" cm.");
        
        colorLED(LOW, LOW, HIGH); // Azul
        estadoActual = ESPERANDO_RETIRO_TAPA;
        delay(1000); 
      }
      break;

    case ESPERANDO_RETIRO_TAPA:
      if (distanciaActual > (distanciaBorde + 3.0)) {
        Serial.println("Movimiento detectado. Esperando estabilización...");
        delay(3000); 
        
        distanciaFondo = medirDistancia();
        
        if (distanciaFondo >= UMBRAL_VACIO) {
          Serial.println("ERROR CRÍTICO: Vaso retirado.");
          for(int i = 0; i < 4; i++) {
            colorLED(HIGH, LOW, LOW);
            delay(150);
            colorLED(LOW, LOW, LOW);
            delay(150);
          }
          estadoActual = ESPERANDO_VASO;
          break; 
        }
        
        ultimaDistanciaReportada = distanciaFondo;
        
        // Reinicio de filtros antes de encender la bomba
        lecturasCorte = 0;
        lecturasEmergencia = 0;
        
        Serial.println("INICIANDO BOMBEO...");
        colorLED(LOW, HIGH, LOW); // Verde
        digitalWrite(PIN_RELAY, HIGH);
        
        estadoActual = LLENANDO;
      }
      break;

    case LLENANDO:
      // --- MODIFICACIÓN: Impresión continua en vivo ---
      Serial.print("[EN VIVO] Distancia al sensor: ");
      Serial.print(distanciaActual);
      Serial.println(" cm");
      // ------------------------------------------------

      // Filtro 1: Nivel Máximo Alcanzado (Exige 3 lecturas confirmadas)
      if (distanciaActual <= (distanciaBorde + 1.5)) {
        lecturasCorte++;
        if (lecturasCorte >= 3) {
          digitalWrite(PIN_RELAY, LOW);
          Serial.println("Nivel máximo alcanzado. Bombeo finalizado.");
          delay(2000);
          estadoActual = ESPERANDO_VASO;
        }
      } else {
        lecturasCorte = 0; // Si fue un falso positivo, reinicia el contador
      }
      
      // Filtro 2: Sobrepaso Crítico (Exige 3 lecturas confirmadas)
      if (distanciaActual < distanciaBorde) {
          lecturasEmergencia++;
          if (lecturasEmergencia >= 3) {
            digitalWrite(PIN_RELAY, LOW);
            Serial.println("ERROR: Límite sobrepasado. Corte de emergencia.");
            delay(2000);
            estadoActual = ESPERANDO_VASO;
          }
      } else {
          lecturasEmergencia = 0;
      }
      break;
  }
  
  delay(100); 
}