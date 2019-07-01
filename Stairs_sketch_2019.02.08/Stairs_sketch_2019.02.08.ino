#include "Tlc5940.h"                                                                // подключить библиотеку обслуживания LED-драйвера
#include <EEPROM.h>                                                                 // подключить библиотеку работы с памятью EEPROM

#define Sonar_Bottom_Trig  8                                                               // вход "Trig" "нижнего" сонара (имеет смысл только при использовании СОНАРОВ)
#define Sonar_Bottom_Echo  2                                                               // выход "Echo" "нижнего" сонара (имеет смысл только при использовании СОНАРОВ)
#define Sonar_Bottom_Reset 4                                                               // управление перезагрузкой "нижнего" сонара (имеет смысл только при использовании СОНАРОВ)

#define Sonar_Top_Trig  6                                                               // вход "Trig" "верхнего" сонара (имеет смысл только при использовании СОНАРОВ)
#define Sonar_Top_Echo  7                                                               // выход "Echo" "верхнего" сонара (имеет смысл только при использовании СОНАРОВ)
#define Sonar_Top_Reset 5                                                               // управление перезагрузкой "верхнего" сонара (имеет смысл только при использовании СОНАРОВ)

#define PIR_Bottom 8                                                                   // выход "нижнего" PIR (имеет смысл только при использовании PIR-сенсоров)
#define PIR_Top 6                                                                   // выход "верхнего" PIR (имеет смысл только при использовании PIR-сенсоров)

#define Sonar_Bottom_Limit 30                                                              // см, если обнаружена дистанция меньше, чем это число, то сонар считается сработавшим
#define Sonar_Top_Limit 30                                                              // см, если обнаружена дистанция меньше, чем это число, то сонар считается сработавшим

// === ПОЛЬЗОВАТЕЛЬСКИЕ НАСТРОЙКИ =============================================================================

bool SensorPIR = false;                                                             // выберите тип сенсора: FALSE = сонары SRF05,  TRUE = PIR сенсоры
bool UseResetSonar = true;                                                          // выберите, использовать ли механизм reset (имеет смысл только при использовании СОНАРОВ)
int N = 8;                                                                        	// 2...16 - установите количество подсвечиваемых ступеней
int Speed = 10;                                                                     // > 0 - задайте скорость включения/выключения освещения ступеней
int PWM_Initial_Value = 500;                                                       	// 0...4095 - задайте уровень яркости первой и последней ступеней в режиме ожидания
int Light_Time = 7000;                                                          // мс, задайте продолжительность освещения лестницы после включения последней ступени

// ============================================================================================================

int StairsPWMValue[16];                                                            	// массив значений уровней яркости (0...4096) ступеней. Все операции только с ним, далее через PWM_Output передаются в LED-драйвер 

boolean Sensor_Set[8];                                                               // массив битовых настроек датчиков 

byte Sensor_Bottom_Ignore = 0;                                                       // счетчик игнорирования срабатывания сенсора 1 (счетчик спускающихся по лестнице)
byte Sensor_Top_Ignore = 0;                                                       // счетчик игнорирования срабатывания сенсора 2 (счетчик поднимающихся по лестнице)

byte Sensor_Bottom_Count = 0;
byte Sensor_Top_Count = 0;

boolean Sensor_Bottom_Out = false;                                                         	  // сенсор 1 зафиксировал движение 
boolean Sensor_Bottom_Memory = false;                                                            // память сенсора 1
boolean Sensor_Bottom_ON = false;                                                         // управляющий сигнал сенсора 1

boolean Sensor_Top_Out = false;                                                         	  // сенсор 2 зафиксировал движение
boolean Sensor_Top_Memory = false;                                                            // память сенсора 2
boolean Sensor_Top_ON = false;                                                         // управляющий сигнал сенсора 2

boolean All_Stairs_On = false;                                                         // освещение включено полностью, требуется ожидание в таком состоянии
boolean ON_Bottom_Top = false;                                                       // требуется включение ступеней снизу вверх
boolean OFF_Bottom_Top = false;                                                      // требуется выключение ступеней снизу вверх
boolean ON_Top_Bottom = false;                                                       // требуется включение ступеней сверху вниз
boolean OFF_Top_Bottom = false;                                                      // требуется выключение ступеней сверху вниз
boolean Initial_State = true;                                                        // "дежурный" режим лестницы, т.н. исходное состояние
boolean direct = false;                                                             // направление : false = снизу вверх, true = сверху вниз

unsigned long PreTime;                                                              // отметка системного времени в предыдущем цикле

long Cycle;                                                                         // мс, длительность цикла
long Light_Off_Delay;                                                             // мс, оставшееся время до выключения освещения лестницы

// ===== ПОДГОТОВКА ===========================================================================================

void setup() {  

//  Serial.begin(9600);

// - инициализация переменных
  
  EEPROM.get(0, SensorSet);
  if (SensorSet[8]) EEPROM_Save();                                                  // значения в памяти EEPROM отсутствуют, необходимо записать данные в EEPROM  
  else {                                                                            // иначе, присвоить переменным значения из EEPROM
    Sensor_PIR = SensorSet[0];                                                       // инициализация тип сенсора: FALSE = сонары SRF05,  TRUE = PIR сенсоры
    Use_Reset_Sonar = SensorSet[1];       
    EEPROM.get(2, N);                                                               // инициализация количества подсвечиваемых ступеней
    EEPROM.get(4, Speed);                                                           // инициализация скорости включения/выключения освещения ступеней
    EEPROM.get(6, PWM_Initial_Value);                                                // инициализация уровня яркости первой и последней ступеней в режиме ожидания
    EEPROM.get(8, Light_Time);                                                  	// инициализация продолжительности освещения лестницы после включения последней ступени
  }
  
// - инициализация начального состояния освещения
  
  Tlc.init();                                                                       // инициализация LED-драйвера
  delay(500);                                                                       // задержка для выполнения инициализации LED-драйвера 
  for (byte i = 1; i < 16; i++) StairsPWMValue[i] = 0;                              // обнулить массив значений яркости ступеней начиная со 2-й ступени
  StairsPWMValue[0] = PWM_InitialValue;                                             // выставление дефолтной яркости 1-ой ступени
  StairsPWMValue[N - 1] = PWM_InitialValue;                                         // выставление дефолтной яркости последней используемой ступени
  PWM_Output();                                                                     // передать в LED-драйвер начальные уровни яркости

 // - инициализация нижнего датчика  

  if (Sensor_PIR) {																	// PIR сенсор
    pinMode (PIR_Bottom, INPUT);
  }	
  else {                                                                            // сонар SRF05
    pinMode (Sonar_Bottom_Trig, OUTPUT);
    pinMode (Sonar_Bottom_Echo, INPUT);
    pinMode (Sonar_Bottom_Reset, OUTPUT);
    digitalWrite (Sonar_Bottom_Trig, LOW);
  }
  Sensor_Bottom_Out = false;
  Sensor_Bottom_Memory = false;
  
// - инициализация верхнего датчика 

  if (Sensor_PIR)  {																// PIR сенсор
    pinMode (PIR_Top, INPUT);                                       
  }	
  else {                                                                            // сонар SRF05
    pinMode (Sonar_Top_Trig, OUTPUT);
    pinMode (Sonar_Top_Echo, INPUT);
    pinMode (Sonar_Top_Reset, OUTPUT);
    digitalWrite (Sonar_Top_Trig, LOW);
  }
  Sensor_Top_Out = false;
  Sensor_Top_Memory = false;

// - инициализация состояния 
  
  ON_Bottom_Top = false;                                                        
  ON_Top_Bottom = false;
  
  PreTime = millis();                                                               // сохранить системное время
}  

// ===== ОСНОВНОЙ ЦИКЛ ========================================================================================

void loop() { 

  InitialState = !((ON_Top_Bottom) || (OFF_Top_Bottom) || (ON_Bottom_Top) || (OFF_Bottom_Top) || (All_Stairs_On));
  if (InitialState) {                                                               // если лестница находится в исходном (выключенном) состоянии, сбросить флаги-"потеряшки" на всякий случай
    Sensor_Bottom_Ignore_Count = 0;                                                 // счетчик игнорирования сенсора 1
    SensorTop_Ignore_Count = 0;                                                     // счетчик игнорирования сенсора 2
  }

// - проверить состояние нижнего датчика


// -- SRF05 --
  if (!SensorPIR) {                													// сканирование пространства перед сонаром 1
    digitalWrite(Sonar_Bottom_Trig, HIGH);
    delayMicroseconds(10);
    digitalWrite(Sonar_Bottom_Trig, LOW);
    byte distance = pulseIn(Sonar_Bottom_Echo, HIGH, 5000) / 58;                   	// расчет дистанции до препятствия (5000 - таймаут, то есть не ждать сигнала более 5мс) 
	                            
    if ((distance > 0) && (distance <= Sonar_Bottom_Limit)) {
      Sensor_Bottom_Count = Sensor_Bottom_Count + 1;  												// SRF05 : сонар 1 зафиксировал присутствие -> увеличить счетчик срабатываний
	}
    else {
	  Sensor_Bottom_Count = 0;                                                            // иначе, сбросить счетчик в "0"
	}
                                                        
/*
	  if (UseResetSonar && !(distance > 0)) digitalWrite(Sonar1reset, LOW);			        // перезапуск зависшего сонара - отключить питание до конца цикла
*/ 

  }
  
// -- PIR --  
  else {										                  
    if (digitalRead(PIR1out) == LOW) Sensor_Bottom_Count = Sensor_Bottom_Count + 1;                   // PIR : сонар 1 зафиксировал присутствие -> увеличить счетчик срабатываний
    else Sensor_Bottom_Count = 0;                                                            // иначе, сбросить счетчик в "0"  
  }

  if (Sensor_Bottom_Count > 3) Sensor_Bottom_Count = 3;                                               // ограничить счетчик значением 3
  Sensor1 = (Sensor_Bottom_Count > 2); 
  

  Sensor1_ON = Sensor1 && !(Sensor1_IgnoreCount > 0);                           	  // счетчик игнорирования сенсора 1 не более 0 - сформировать управляющий сигнал 
  if (Sensor1 && !Memory1) {														                            // фронт срабатывания сенсора 1
    if (Sensor1_IgnoreCount > 0) Sensor1_IgnoreCount-- ;                            // если требуется игнорировть сонар 1, декремент счетчика игнорирования сенсора 1 (вероятно, кто-то закончил спуск по лестнице)
    else Sensor2_IgnoreCount++;                                                     // иначе, инкремент счетчика игнорирования сенсора 2 (вероятно, кто-то начал подниматься по лестнице)    
  }
  Memory1 = Sensor1;																                                // запомнить состояние сенсора
 
// - проверить состояние "ВЕРХНЕГО" сенсора 

  if (!SensorPIR) {                													                        // сканирование пространства перед сонаром 2
    digitalWrite(Sonar_Top_Trig, HIGH);
    delayMicroseconds(10);
    digitalWrite(Sonar_Top_Trig, LOW);
    byte distance = pulseIn(Sonar_Top_Echo, HIGH, 5000) / 58 ;                  				// расчет дистанции до препятствия (5000 - таймаут, то есть не ждать сигнала более 5мс) 
	Sensor2 = (distance > 0) && (distance <= Sonar2limit);						                // SRF05 : сонар 2 зафиксировал присутствие
	if (UseResetSonar && !(distance > 0)) digitalWrite(Sonar2reset, LOW);			        // перезапуск зависшего сонара - отключить питание до конца цикла
	}
  else Sensor2 = (digitalRead(PIR2out) == LOW);										                  // PIR : сенсор 2 зафиксировал присутствие

  Sensor2_ON = Sensor2 && !(Sensor2_IgnoreCount > 0);                           	  // счетчик игнорирования сенсора 2 не более 0 - сформировать управляющий сигнал 
  if (Sensor2 && !Memory2) {														                            // фронт срабатывания сенсора 2
    if (Sensor2_IgnoreCount > 0) Sensor2_IgnoreCount--;                             // если требуется игнорировть сонар 2, декремент счетчика игнорирования сенсора 2 (вероятно, кто-то закончил подъем по лестнице)
    else Sensor1_IgnoreCount++;                                                     // иначе, инкремент счетчика игнорирования сенсора 1 (вероятно, кто-то начал спускаться по лестнице)    
  }
  Memory2 = Sensor2;																                                // запомнить состояние сенсора
  
// - освещение лестницы включено

  if (AllLightOn) {                                                                	// если лестница находится во включенном состоянии, можно сбросить флаги-"потеряшки" на всякий случай
    ON_BottomTop = false;                                                        	  // включение освещения снизу вверх
    ON_TopBottom = false;                                                        	  // включение освещения сверху вниз
  }
  
   
// - действия при срабатывания "НИЖНЕГО" сенсора 
  
  if (Sensor1_ON && InitialState) ON_BottomTop = true;                              // при срабатывании сенсора из исходного состояния лестницы начать включение освещения ступеней снизу вверх
  else if (Sensor1_ON && AllLightOn) AllLight_OffDelay = LightOFF_Delay;            // если освещение уже полностью включено, обновить отсчет задержки выключения освещения   	
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
  
  Cycle = millis() - PreTime;                                                        // прошло времени с предыдущего цикла программы 

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
      StairsPWMValue[i] = StairsPWMValue[i] * (10 + Speed) / 10; 	  								// увеличить уровень яркости  
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
      StairsPWMValue[i] = 10 * StairsPWMValue[i] / (10 + Speed);                    // уменьшить яркость
	  if (StairsPWMValue[i] < PWM_InitialValue ) StairsPWMValue[i] = PWM_InitialValue ; // ограничить минимальную яркость
      return;                                                                       // и выйти из процедуры	  
    }  
    else if ((i != 0) && (i != N-1) && (StairsPWMValue[i] > 0)) {                   // для остальных ступеней, если уровень яркости выше "0",
      StairsPWMValue[i] = 10 * StairsPWMValue[i] / (10 + Speed);                    // уменьшить яркость
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
  for (int i = 0; i < 16; i++) Tlc.set(i, StairsPWMValue[i]);                      	// яркость ступеней в диапазоне 0...4096 
  Tlc.update();
}

// ===== ФУНКЦИЯ ОПРОСА ДАТЧИКА ПРИСУТСТВИЯ =======================================

bolean Sensor_Out() {

// -- SRF05 --
  if (!SensorPIR) {                													                        // сканирование пространства перед сонаром 1
    digitalWrite(Sonar_Bottom_Trig, HIGH);
    delayMicroseconds(10);
    digitalWrite(Sonar_Bottom_Trig, LOW);
    byte distance = pulseIn(Sonar_Bottom_Echo, HIGH, 5000) / 58;                   				// расчет дистанции до препятствия (5000 - таймаут, то есть не ждать сигнала более 5мс) 
	                            
    if ((distance > 0) && (distance <= Sonar_Bottom_Limit))  Sensor_Bottom_Count = Sensor_Bottom_Count + 1;  // SRF05 : сонар 1 зафиксировал присутствие -> увеличить счетчик срабатываний
    else Sensor_Bottom_Count = 0;                                                            // иначе, сбросить счетчик в "0"
                                                        
/*
	  if (UseResetSonar && !(distance > 0)) digitalWrite(Sonar1reset, LOW);			        // перезапуск зависшего сонара - отключить питание до конца цикла
*/ 

  }
  
// -- PIR --  
  else {										                  
    if (digitalRead(PIR1out) == LOW) Sensor_Bottom_Count = Sensor_Bottom_Count + 1;                   // PIR : сонар 1 зафиксировал присутствие -> увеличить счетчик срабатываний
    else Sensor_Bottom_Count = 0;                                                            // иначе, сбросить счетчик в "0"  
  }

  if (Sensor_Bottom_Count > 3) Sensor_Bottom_Count = 3;                                               // ограничить счетчик значением 3
  Sensor1 = (Sensor_Bottom_Count > 2); 
  

  Sensor1_ON = Sensor1 && !(Sensor1_IgnoreCount > 0);                           	  // счетчик игнорирования сенсора 1 не более 0 - сформировать управляющий сигнал 
  if (Sensor1 && !Memory1) {														                            // фронт срабатывания сенсора 1
    if (Sensor1_IgnoreCount > 0) Sensor1_IgnoreCount-- ;                            // если требуется игнорировть сонар 1, декремент счетчика игнорирования сенсора 1 (вероятно, кто-то закончил спуск по лестнице)
    else Sensor2_IgnoreCount++;                                                     // иначе, инкремент счетчика игнорирования сенсора 2 (вероятно, кто-то начал подниматься по лестнице)    
  }
  Memory1 = Sensor1;

// ===== ПРОЦЕДУРА ЗАПИСИ ПОЛЬЗОВАТЕЛЬСКИХ ЗНАЧЕНИЙ ПЕРЕМЕННЫХ В EEPROM =======================================

void EEPROM_Save() {
    EEPROM.update(2, N);                                                            // сохранить в EEPROM количество подсвечиваемых ступеней
    EEPROM.update(4, Speed);                                                        // сохранить в EEPROM скорость включения/выключения освещения ступеней
    EEPROM.update(6, PWM_InitialValue);                                             // сохранить в EEPROM уровень яркости первой и последней ступеней в режиме ожидания
    EEPROM.update(8, LightOFF_Delay);                                               // сохранить в EEPROM продолжительность освещения лестницы после включения последней ступени
    SensorSet[0] = SensorPIR;                                                       // сохранить в EEPROM тип сенсора: FALSE = сонары SRF05,  TRUE = PIR сенсоры
    SensorSet[1] = UseResetSonar;                                                   // сохранить в EEPROM использовать ли механизм reset (имеет смысл только при использовании СОНАРОВ)
    SensorSet[8] = 0;                                                               // установить признак записи данных в EEPROM
    EEPROM.update(0, SensorSet);
}




 
 

