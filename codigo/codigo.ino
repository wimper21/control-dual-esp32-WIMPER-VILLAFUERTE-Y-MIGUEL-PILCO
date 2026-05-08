#define PIN_LED1       25   
#define PIN_LED2       26  
#define PIN_POT1       34   
#define PIN_POT2       35   
#define PIN_BTN1       18   
#define PIN_BTN2       19   

#define LEDC_CHANNEL   0
#define LEDC_FREQ_HZ   5000
#define LEDC_BITS      8  

volatile bool timerFlag = false;   
volatile bool estadoSistema  = true;    
volatile int  modo = 1;      
volatile unsigned long lastBtn1 = 0;
volatile unsigned long lastBtn2 = 0;
#define DEBOUNCE_US 200000UL      
volatile bool estadoLed2 = false;

hw_timer_t* timer = NULL;

void IRAM_ATTR onTimer() {
  timerFlag = true;
}
void IRAM_ATTR isrBtn1() {
  unsigned long ahora = micros();
  if (ahora - lastBtn1 > DEBOUNCE_US) {
    lastBtn1 = ahora;
    if (estadoSistema) {
      modo = (modo == 1) ? 2 : 1;
    }
  }
}
void IRAM_ATTR isrBtn2() {
  unsigned long ahora = micros();
  if (ahora - lastBtn2 > DEBOUNCE_US) {
    lastBtn2 = ahora;
    estadoSistema = !estadoSistema;
  }
}

void configurarTimer(uint32_t freqHz) {
  if (timer != NULL) {
    timerEnd(timer);
    timer = NULL;
  }

  timer = timerBegin(1000000);          

  timerAttachInterrupt(timer, &onTimer);
  uint64_t ticks = 1000000UL / freqHz;
  timerAlarm(timer, ticks, true, 0);    // true = auto-reload
}

// ─────────────────────────────────────────────
//  SETUP
// ─────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  Serial.println("=== SISTEMA ESP32 INICIADO ===");

  // Pines de salida
  pinMode(PIN_LED1, OUTPUT);
  pinMode(PIN_LED2, OUTPUT);

  // Pines de entrada
  pinMode(PIN_BTN1, INPUT_PULLUP);
  pinMode(PIN_BTN2, INPUT_PULLUP);

  // Configurar PWM para LED1 — ESP32 Core v3
  // ledcAttach(pin, frecuencia, bits_resolución)
  ledcAttach(PIN_LED1, LEDC_FREQ_HZ, LEDC_BITS);

  // Interrupciones externas por pulsadores (flanco de bajada)
  attachInterrupt(digitalPinToInterrupt(PIN_BTN1), isrBtn1, FALLING);
  attachInterrupt(digitalPinToInterrupt(PIN_BTN2), isrBtn2, FALLING);

  // Timer inicial para Modo 2 → 2 Hz (parpadeo lento)
  configurarTimer(2);

  Serial.println("Hardware Timer configurado: 2 Hz");
  Serial.println("Modo inicial: 1 (Control PWM)");
}

// ─────────────────────────────────────────────
//  LOOP  — Sin delay(), sin millis()
// ─────────────────────────────────────────────
void loop() {

  // ── SISTEMA DESACTIVADO ──────────────────────
  if (!estadoSistema) {
    ledcWrite(PIN_LED1, 0);     // apagar LED1 (PWM en 0)
    digitalWrite(PIN_LED2, LOW); // apagar LED2
    Serial.println("SISTEMA DESACTIVADO");
    // Pequeña pausa pasiva para no saturar el serial;
    // aquí sí podríamos usar delay SOLO para el mensaje,
    // pero el enunciado prohíbe delay(). Usamos un bucle
    // vacío que se rompe cuando cambia el estado.
    while (!estadoSistema) {
      // Espera activa: el sistema sigue verificando la
      // variable global modificada por ISR.
      // No se ejecuta ninguna lógica bloqueante.
    }
    Serial.println("=== SISTEMA REACTIVADO ===");
    return;
  }

  // ── MODO 1: Control Inteligente PWM ─────────
  if (modo == 1) {
    // Apagar LED2 residual del modo anterior
    digitalWrite(PIN_LED2, LOW);

    // Leer potenciómetro 1 (ADC 12 bits → 0 a 4095)
    int valorADC = analogRead(PIN_POT1);

    // Mapear a rango PWM 8 bits (0 a 255)
    int valorPWM = map(valorADC, 0, 4095, 0, 255);

    // Aplicar PWM al LED1 (ledcWrite = analogWrite en Core v3)
    ledcWrite(PIN_LED1, valorPWM);

    // Monitor Serial
    Serial.print("MODO 1 | ADC: ");
    Serial.print(valorADC);
    Serial.print("  |  PWM: ");
    Serial.println(valorPWM);
  }

  // ── MODO 2: Sistema de Alerta Adaptativo ────
  else if (modo == 2) {
    // Apagar LED1 residual
    ledcWrite(PIN_LED1, 0);

    // Leer potenciómetro 2
    int valorPot2 = analogRead(PIN_POT2);

    // Mapear ADC a frecuencia de parpadeo
    // Menor valor ADC → mayor frecuencia (más rápido)
    // Rango: 1 Hz (lento) a 20 Hz (rápido)
    int freqParpadeo = map(valorPot2, 0, 4095, 20, 1);

    // Reconfigurar el timer con la nueva frecuencia
    // NOTA: solo se reconfigura si cambió significativamente
    // para evitar interrupciones innecesarias.
    static int freqAnterior = 0;
    if (freqParpadeo != freqAnterior) {
      configurarTimer(freqParpadeo);
      freqAnterior = freqParpadeo;
    }

    // Actuar sobre la bandera del timer (base de tiempo precisa)
    if (timerFlag) {
      timerFlag = false;                  // limpiar bandera
      estadoLed2 = !estadoLed2;          // alternar estado
      digitalWrite(PIN_LED2, estadoLed2 ? HIGH : LOW);

      // Monitor Serial
      Serial.print("MODO 2 | ADC: ");
      Serial.print(valorPot2);
      Serial.print("  |  Frecuencia: ");
      Serial.print(freqParpadeo);
      Serial.print(" Hz  |  LED2: ");
      Serial.println(estadoLed2 ? "ON" : "OFF");
    }
  }
}