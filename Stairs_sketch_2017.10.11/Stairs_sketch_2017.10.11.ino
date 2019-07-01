#include "Tlc5940.h"                                                                // подключить библиотеку обслуживания LED-драйвера

#define Sonar1trig  8                                                               // вход "Trig" "нижнего" сонара (имеет смысл только при использовании СОНАРОВ)
#define Sonar1echo  2                                                               // выход "Echo" "нижнего" сонара (имеет смысл только при использовании СОНАРОВ)
#define Sonar1reset 4                                                               // управление перезагрузкой "нижнего" сонара (имеет смысл только при использовании СОНАРОВ)

#define Sonar2trig  6                                                               // вход "Trig" "верхнего" сонара (имеет смысл только при использовании СОНАРОВ)
#define Sonar2echo  7                                                               // выход "Echo" "верхнего" сонара (имеет смысл только при использовании СОНАРОВ)
#define Sonar2reset 5                                                               // управление перезагрузкой "верхнего" сонара (имеет смысл только при использовании СОНАРОВ)

#define PIR1out 8                                                                   // выход "нижнего" PIR (имеет смысл только при использовании PIR-сенсоров)
#define PIR2out 6                                                                   // выход "верхнего" PIR (имеет смысл только при использовании PIR-сенсоров)

#define Sonar1limit 30                                                              // см, если обнаружена дистанция меньше, чем это число, то сонар считается сработавшим
#define Sonar2limit 30                                                              // см, если обнаружена дистанция меньше, чем это число, то сонар считается сработавшим

// === ПОЛЬЗОВАТЕЛЬСКИЕ НАСТРОЙКИ =============================================================================

#define SensorPIR false                                                             // выберите тип сенсора: FALSE = сонары SRF05,  TRUE = PIR сенсоры
#define UseResetSonar true                                                          // выберите, использовать ли механизм reset (имеет смысл только при использовании СОНАРОВ)
#define N 8                                                                        	// 2...16 - установите количество подсвечиваемых ступеней
#define Speed 2.0                                                                  // >=1.2 - задайте скорость включения/выключения освещения ступеней
#define PWM_InitialValue 500                                                       	// 0...4095 - задайте уровень яркости первой и последней ступеней в режиме ожидания
#define LightOFF_Delay 7000                                                         // мс, задайте время задержки отключения освещения после включения последней ступени

// ============================================================================================================

int StairsPWMValue[16];                                                            	// массив значений уровней яркости (0...4096) ступеней. Все операции только с ним, далее через PWM_Output передаются в LED-драйвер 

byte Sensor1_IgnoreCount = 0;                                                       // счетчик игнорирования срабатывания сенсора 1 (счетчик спускающихся по лестнице)
byte Sensor2_IgnoreCount = 0;                                                       // счетчик игнорирования срабатывания сенсора 2 (счетчик поднимающихся по лестнице)

boolean Sensor1 = false;                                                         	  // сенсор 1 зафиксировал движение 
boolean Memory1 = false;                                                            // память сенсора 1
boolean Sensor1_ON = false;                                                         // управляющий сигнал сенсора 1

boolean Sensor2 = false;                                                         	  // сенсор 2 зафиксировал движение
boolean Memory2 = false;                                                            // память сенсора 2
boolean Sensor2_ON = false;                                                         // управляющий сигнал сенсора 2

boolean AllLightOn = false;                                                         // освещение включено полностью, требуется ожидание в таком состоянии
boolean ON_BottomTop = false;                                                       // требуется включение ступеней снизу вверх
boolean OFF_BottomTop = false;                                                      // требуется выключение ступеней снизу вверх
boolean ON_TopBottom = false;                                                       // требуется включение ступеней сверху вниз
boolean OFF_TopBottom = false;                                                      // требуется выключение ступеней сверху вниз
boolean InitialState = true;                                                        // "дежурный" режим лестницы, т.н. исходное состояние
boolean direct = false;                                                             // направление : false = снизу вверх, true = сверху вниз

unsigned long PreTime;                                                              // отметка системного времени в предыдущем цикле
unsigned long ActTime;                                                              // отметка системного времени в текущем цикле

long Cycle;                                                                         // мс, длительность цикла
long AllLight_OffDelay;                                                             // мс, оставшееся время до выключения освещения лестницы


// ===== ПОДГОТОВКА ===========================================================================================

void setup() {  

//  Serial.begin(9600);
  Tlc.init();                                                                       // инициализация LED-драйвера
  delay(500);                                                                       // задержка для выполнения инициализации LED-драйвера 
  for (byte i = 1; i < 16; i++) StairsPWMValue[i] = 0;                              // обнулить массив значений яркости ступеней начиная со 2-й ступени
  StairsPWMValue[0] = PWM_InitialValue;                                             // выставление дефолтной яркости 1-ой ступени
  StairsPWMValue[N - 1] = PWM_InitialValue;                                         // выставление дефолтной яркости последней используемой ступени
  PWM_Output();                                                                     // передать в LED-драйвер начальные уровни яркости

 // - инициализация сенсора 1  

  if (SensorPIR) pinMode (PIR1out, INPUT);                                          // PIR сенсор
  else {                                                                            // сонар SRF05
    pinMode (Sonar1trig, OUTPUT);
    pinMode (Sonar1echo, INPUT);
    pinMode (Sonar1reset, OUTPUT);
    digitalWrite (Sonar1trig, LOW);
  }
  Sensor1 = false;
  Memory1 = false;
  
// - инициализация сенсора 2 

  if (SensorPIR) pinMode (PIR2out, INPUT);                                          // PIR сенсор
  else {                                                                            // сонар SRF05
    pinMode (Sonar2trig, OUTPUT);
    pinMode (Sonar2echo, INPUT);
    pinMode (Sonar2reset, OUTPUT);
    digitalWrite (Sonar2trig, LOW);
  }
  Sensor2 = false;
  Memory2 = false;

// - инициализация состояния 

  ON_BottomTop = false;                                                        
  ON_TopBottom = false;
  
  PreTime = millis();                                                               // сохранить системное время
}  

// ===== ОСНОВНОЙ ЦИКЛ ========================================================================================

void loop() { 

  InitialState = !((ON_TopBottom) || (OFF_TopBottom) || (ON_BottomTop) || (OFF_BottomTop) || (AllLightOn));
  if (InitialState) {                                                               // если лестница находится в исходном (выключенном) состоянии, сбросить флаги-"потеряшки" на всякий случай
    Sensor1_IgnoreCount = 0;                                                        // счетчик игнорирования сенсора 1
    Sensor2_IgnoreCount = 0;                                                        // счетчик игнорирования сенсора 2
  }

// - проверить состояние "НИЖНЕГО" сенсора 

  if (!SensorPIR) {                													                        // сканирование пространства перед сонаром 1
    digitalWrite(Sonar1trig, HIGH);
    delayMicroseconds(10);
    digitalWrite(Sonar1trig, LOW);
    byte distance = pulseIn(Sonar1echo, HIGH, 5000) / 58 ;                  				// расчет дистанции до препятствия (5000 - таймаут, то есть не ждать сигнала более 5мс) 
	Sensor1 = (distance > 0) && (distance <= Sonar1limit);							              // SRF05 : сонар 1 зафиксировал присутствие
	if (UseResetSonar && !(distance > 0)) digitalWrite(Sonar1reset, LOW);			        // перезапуск зависшего сонара - отключить питание до конца цикла
	}
  else Sensor1 = (digitalRead(PIR1out) == LOW);										                  // PIR : сенсор 1 зафиксировал присутствие

  Sensor1_ON = Sensor1 && !(Sensor1_IgnoreCount > 0);                           	  // счетчик игнорирования сенсора 1 не более 0 - сформировать управляющий сигнал 
  if (Sensor1 && !Memory1) {														                            // фронт срабатывания сенсора 1
    if (Sensor1_IgnoreCount > 0) Sensor1_IgnoreCount-- ;                            // если требуется игнорировть сонар 1, декремент счетчика игнорирования сенсора 1 (вероятно, кто-то закончил спуск по лестнице)
    else Sensor2_IgnoreCount++ ;                                                    // иначе, инкремент счетчика игнорирования сенсора 2 (вероятно, кто-то начал подниматься по лестнице)    
  }
  Memory1 = Sensor1 ;																                                // запомнить состояние сенсора
 
// - проверить состояние "ВЕРХНЕГО" сенсора 

  if (!SensorPIR) {                													                        // сканирование пространства перед сонаром 2
    digitalWrite(Sonar2trig, HIGH);
    delayMicroseconds(10);
    digitalWrite(Sonar2trig, LOW);
    byte distance = pulseIn(Sonar2echo, HIGH, 5000) / 58 ;                  				// расчет дистанции до препятствия (5000 - таймаут, то есть не ждать сигнала более 5мс) 
	Sensor2 = (distance > 0) && (distance <= Sonar2limit);						                // SRF05 : сонар 2 зафиксировал присутствие
	if (UseResetSonar && !(distance > 0)) digitalWrite(Sonar2reset, LOW);			        // перезапуск зависшего сонара - отключить питание до конца цикла
	}
  else Sensor2 = (digitalRead(PIR2out) == LOW);										                  // PIR : сенсор 2 зафиксировал присутствие

  Sensor2_ON = Sensor2 && !(Sensor2_IgnoreCount > 0);                           	  // счетчик игнорирования сенсора 2 не более 0 - сформировать управляющий сигнал 
  if (Sensor2 && !Memory2) {														                            // фронт срабатывания сенсора 2
    if (Sensor2_IgnoreCount > 0) Sensor2_IgnoreCount-- ;                            // если требуется игнорировть сонар 2, декремент счетчика игнорирования сенсора 2 (вероятно, кто-то закончил подъем по лестнице)
    else Sensor1_IgnoreCount++ ;                                                    // иначе, инкремент счетчика игнорирования сенсора 1 (вероятно, кто-то начал спускаться по лестнице)    
  }
  Memory2 = Sensor2 ;																                                // запомнить состояние сенсора
  
// - освещение лестницы включено

  if (AllLightOn) {                                                                	// если лестница находится во включенном состоянии, можно сбросить флаги-"потеряшки" на всякий случай
    ON_BottomTop = false;                                                        	  // включение освещения снизу вверх
    ON_TopBottom = false;                                                        	  // включение освещения сверху вниз
  }
  
   
// - действия при срабатывания "НИЖНЕГО" сенсора 
  
  if (Sensor1_ON && InitialState) ON_BottomTop = true;                              // при срабатывании сенсора из исходного состояния лестницы начать включение освещения ступеней снизу вверх
  else if (Sensor1_ON && AllLightOn) AllLight_OffDelay = LightOFF_Delay ;           // если освещение уже полностью включено, обновить отсчет задержки выключения освещения   	
  else if (Sensor1_ON && OFF_BottomTop) {                                           // если происходит выключение освещения ступеней снизу вверх,
    OFF_BottomTop = false;                                                          // прекратить выключение освещения
    ON_BottomTop = true;                                                            // вновь начать включение освещения ступеней снизу вверх
  }
/*
  else if (Sensor1_ON && ON_TopBottom) {                                            // если происходит включение освещения ступеней сверху вниз
    ON_TopBottom = false;                                                           // прекратить включение освещения сверху вниз
    ON_BottomTop = true;                                                            // начать включение освещения ступеней снизу вверх
  }
*/  
  else if (Sensor1_ON && OFF_TopBottom) {                                           // если происходит выключение освещения ступеней сверху вниз
    OFF_TopBottom = false;                                                          // прекратить выключение освещения сверху вниз
    ON_BottomTop = true;                                                            // начать включение освещения ступеней снизу вверх
  }

// - действия при срабатывания "ВЕРХНЕГО" сенсора 

  if (Sensor2_ON && InitialState) ON_TopBottom = true;                              // при срабатывании сенсора из исходного состояния лестницы начать включение освещения ступеней сверху вниз
  else if (Sensor2_ON && AllLightOn) AllLight_OffDelay = LightOFF_Delay ;           // если освещение уже полностью включено, обновить отсчет задержки выключения освещения   	
  else if (Sensor2_ON && OFF_TopBottom) {                                       	  // если происходит выключение освещения ступеней сверху вниз
    OFF_TopBottom = false;                                                          // прекратить выключение освещения
    ON_TopBottom = true;                                                            // вновь начать включение освещения ступеней сверху вниз
  }
/*  
  else if (Sensor2_ON && ON_BottomTop) {                                        	  // если происходит включение освещения ступеней снизу вверх
    ON_BottomTop = false;                                                           // прекратить включение освещения снизу вверх
    ON_TopBottom = true;                                                            // начать включение освещения ступеней сверху вниз
  }
*/  
  else if (Sensor2_ON && OFF_BottomTop) {                                       	  // если происходит выключение освещения ступеней снизу вверх
    OFF_BottomTop = false;                                                          // прекратить выключение освещения снизу вверх
    ON_TopBottom = true;                                                            // начать включение освещения ступеней сверху вниз
  }

// - определение необходимости и направления выключения освещения

	if (AllLightOn && !(AllLight_OffDelay > 0)) {                                  	  // пора гасить освещение
    OFF_BottomTop = !direct;                                                        // снизу вверх
    OFF_TopBottom = direct;                                                         // сверху вниз
  }
 
// - управление включением/выключением освещения ступеней

  if (ON_BottomTop) BottomTopLightON();                                             // вызов процедуры включения ступеней снизу вверх
  if (OFF_BottomTop) BottomTopLightOFF();                                           // вызов процедуры выключения ступеней снизу вверх
  if (ON_TopBottom) TopBottomLightON();                                             // вызов процедуры включения ступеней сверху вниз
  if (OFF_TopBottom) TopBottomLightOFF();                                           // выключение ступеней сверху вниз
  PWM_Output();                                                                     // передать в LED-драйвер актуальные уровни яркости

// - завершение цикла

  delay(50);                                                                        // задержка для обеспечения паузы перед следующим опросом сонаров
  if (!SensorPIR) {                                                                 // восстановление питания, если был перезапуск сонаров
    digitalWrite (Sonar1reset, HIGH);                                               // всегда должен быть HIGH, для перезапуска сонара 1 установить в LOW
    digitalWrite (Sonar1reset, HIGH);                                               // всегда должен быть HIGH, для перезапуска сонара 2 установить в LOW
  } 
  
  ActTime = millis();                                                               // отметка времени для расчета длительности цикла
  Cycle = ActTime - PreTime;                                                        // прошло времени с предыдущего цикла программы 

  if (AllLight_OffDelay > 0) AllLight_OffDelay = AllLight_OffDelay - Cycle;         // если время освещения лестницы не истекло, уменьшить на длительность цикла
  else AllLight_OffDelay = 0;                                                       // иначе, время до начала отключения освещения = 0
 
  PreTime = millis();                                                               // отметка времени для расчета длительности цикла

}


// ===== ПРОЦЕДУРА ВКЛЮЧЕНИЯ СНИЗУ ВВЕРХ - ПОСЛЕДОВАТЕЛЬНОЕ УВЕЛИЧЕНИЕ УРОВНЕЙ ЯРКОСТИ СТУПЕНЕЙ ===============

void BottomTopLightON() { 
  direct = false;                                                                   // запомнить направление = снизу вверх
  for (byte i = 0; i < N; i++) {                                                    // перебор ступеней снизу вверх
    if (StairsPWMValue[i] < 1) {													                          // если очередная ступень выключена,
	  StairsPWMValue[i] = 5; 	  									                                    // задать для неё начальный уровень яркости
	  return;																		                                      // и выйти из процедуры
	  }  
	  else if (StairsPWMValue[i] < 4095 ){                                     		    // иначе, если уровень яркости ступени меньше максимального,
      StairsPWMValue[i] = StairsPWMValue[i] * Speed ; 	  								          // увеличить уровень яркости  
	  if (StairsPWMValue[i] > 4095 ) StairsPWMValue[i] = 4095 ;               		    // ограничить максимальную яркость
      return;                                                                       // и выйти из процедуры                            
    }
  }  
	AllLightOn = true;                                                            	   // освещение лестницы полностью включено
  AllLight_OffDelay = LightOFF_Delay;
  ON_BottomTop = false;                                                         	   // закончить включение ступеней снизу вверх
}

// ===== ПРОЦЕДУРА ВЫКЛЮЧЕНИЯ СНИЗУ ВВЕРХ - ПОСЛЕДОВАТЕЛЬНОЕ СНИЖЕНИЕ УРОВНЕЙ ЯРКОСТИ СТУПЕНЕЙ ===============

void BottomTopLightOFF() {  
  AllLightOn = false;                                                               // сбросить признак того, что освещение лестницы полностью включено
  for (byte i = 0; i < N ; i++) {                                                   // перебор ступеней снизу вверх
    if (((i == 0) || (i == N-1)) && (StairsPWMValue[i] > PWM_InitialValue)) {       // если это первая или последняя ступень и уровень яркости выше дежурного уровня,
      StairsPWMValue[i] = StairsPWMValue[i] / Speed ;                          // уменьшить яркость
	  if (StairsPWMValue[i] < PWM_InitialValue ) StairsPWMValue[i] = PWM_InitialValue ; // ограничить минимальную яркость
      return;                                                                       // и выйти из процедуры	  
    }  
    else if ((i != 0) && (i != N-1) && (StairsPWMValue[i] > 0)) {                   // для остальных ступеней, если уровень яркости выше "0",
      StairsPWMValue[i] = StairsPWMValue[i] / Speed ;                               // уменьшить яркость
	  if (StairsPWMValue[i] < 5 ) StairsPWMValue[i] = 0 ; 							              // ограничить минимальную яркость
      return;                                                                       // и выйти из процедуры
    }
  }
  OFF_BottomTop = false;                                                            // закончить выключение ступеней снизу вверх
}

// ===== ПРОЦЕДУРА ВКЛЮЧЕНИЯ СВЕРХУ ВНИЗ - ПОСЛЕДОВАТЕЛЬНОЕ УВЕЛИЧЕНИЕ УРОВНЕЙ ЯРКОСТИ СТУПЕНЕЙ ===============

void TopBottomLightON() { 
  direct = true;                                                                    // запомнить направление = сверху вниз
  for (byte i = N; i > 0; i--) {                                                    // перебор ступеней сверху вниз
    if (StairsPWMValue[i-1] < 1) {                                                 	// если очередная ступень выключена,
 	  StairsPWMValue[i-1] = 5; 	  									                                  // задать для неё начальный уровень яркости
	  return;																		                                      // и выйти из процедуры
	  }  
	  else if (StairsPWMValue[i-1] < 4095 ){                                     		  // иначе, если уровень яркости ступени меньше максимального,
      StairsPWMValue[i-1] = StairsPWMValue[i-1] * Speed ; 	  								      // увеличить уровень яркости  
	    if (StairsPWMValue[i-1] > 4095 ) StairsPWMValue[i-1] = 4095 ;               	// ограничить максимальную яркость
      return;                                                                       // и выйти из процедуры                            
    } 
  } 
  AllLightOn = true;                                                            	  // освещение лестницы полностью включено
  AllLight_OffDelay = LightOFF_Delay;
  ON_TopBottom = false;                                                         	  // закончить включение ступеней сверху вниз
}

// ===== ПРОЦЕДУРА ВЫКЛЮЧЕНИЯ СВЕРХУ ВНИЗ - ПОСЛЕДОВАТЕЛЬНОЕ СНИЖЕНИЕ УРОВНЕЙ ЯРКОСТИ СТУПЕНЕЙ ================

void TopBottomLightOFF() {  
  AllLightOn = false;                                                               // сбросить признак того, что освещение лестницы полностью включено
  for (byte i = N ; i >0; i--) {                                                    // перебор ступеней вниз, начиная с верхней
    if (((i == 1) || (i == N)) && (StairsPWMValue[i-1] > PWM_InitialValue)) {       // если это первая или последняя ступень и уровень яркости выше дежурного уровня,
      StairsPWMValue[i-1] = StairsPWMValue[i-1] / Speed ;                           // уменьшить яркость
	  if (StairsPWMValue[i-1] < PWM_InitialValue ) StairsPWMValue[i-1] = PWM_InitialValue ; // ограничить минимальную яркость
      return;                                                                       // и выйти из процедуры	  
    }  
    else if ((i != 1) && (i != N) && (StairsPWMValue[i-1] > 0)) {                   // для остальных ступеней, если уровень яркости выше "0",
      StairsPWMValue[i-1] = StairsPWMValue[i-1] / Speed ;                           // уменьшить яркость
	  if (StairsPWMValue[i-1] < 5 ) StairsPWMValue[i-1] = 0 ; 							          // ограничить минимальную яркость
      return;                                                                       // и выйти из процедуры
    }
  }
  OFF_TopBottom = false;                                                            // закончить выключение ступеней сверху вниз
}


// ===== ПРОЦЕДУРА ПЕРЕДАЧИ УПРАВЛЯЮЩИХ СИГНАЛОВ В LED-ДРАЙВЕР TLC 5940 =======================================

void PWM_Output() {  
  for (int i = 0; i < 15; i++) Tlc.set(i, StairsPWMValue[i]);                      	// яркость ступеней в диапазоне 0...4096 
  Tlc.update();
}







 
 

