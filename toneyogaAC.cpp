
#include <WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <UniversalTelegramBot.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <DHT.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <ir_Coolix.h>


#define WIRELESSID "#####"      
#define CONTRASENHA "######" 

const uint16_t kIrLed = 4; 
IRCoolixAC ac(kIrLed);   

const char *BOTtoken = "######"; 
const char *CHAT_ID = "######AA";
const char *CHAT_ID_Lari = "######BB"; // 2nd ID
String chatty = "";
WiFiClientSecure net_ssl;
UniversalTelegramBot bot(BOTtoken, net_ssl);

// ntp time & date
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

// DHT11 Temp y Hum ambiente y sus variables globales
DHT dht(18, DHT11);
float tempAmb = 0;
float humAmb = 0;
float humProm = 0;
float tempProm = 0;
float humAndTempPromCounter = 0;
int MAXhumAndTempPromCounter = 120;

float lastTimeSensorsUpdate;
float timeSensorsDelay = 30000;

float lastTimeUpdate;
float timeRequestDelay = 60000;

float botRequestDelay = 1000; // cada 1 segundo
float lastTimeBotRan;
float lastTimeDataSent;

boolean sendFlag = false;
boolean sendSutra = false;

unsigned long previousMillis = 0;
unsigned long interval = 60000;

void setup()
{
  sendFlag = false;
  Serial.begin(115200);
  Serial.println("Remote IR has started");
  ac.begin();
  dht.begin();
  Serial.println("DHT TEMPERATURE & HUMIDITY SENSOR STARTED");
  Get_HumTempAmb();
  Serial.println(" Temp: " + String(tempAmb) + " \n Hum: " + String(humAmb));

  WiFi.begin(WIRELESSID, CONTRASENHA);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(1000);
    Serial.println("ConnectinG to WiFI..");
  }

  configTime(0, 0, "pool.ntp.org"); // get UTC time via NTP
  net_ssl.setCACert(TELEGRAM_CERTIFICATE_ROOT);
  timeClient.begin();
  timeClient.setTimeOffset(-10800);
  Serial.println("Iniciando telegram y demas chiches...");

  String WELCOME = "TONE YOGA Bot se ha reiniciado \n\n";
  String CONFIGKEY = "[[\"/Datos\"],[\"/On\", \"/Off\"],[\"/20\", \"/21\", \"/22\"],[\"/23\",\"/24\",\"/25\"],[\"/26\", \"/27\",\"/28\"],[\"/Sutras\"]]";
  WELCOME += "Elija el comando que desea enviar al aire acondicionado de la sala:";

  bot.sendMessageWithReplyKeyboard(CHAT_ID, WELCOME, "", CONFIGKEY, true);
  bot.sendMessageWithReplyKeyboard(CHAT_ID_Lari, WELCOME, "", CONFIGKEY, true);

  timeClient.update();
}

void loop()
{
  unsigned long currentMillis = millis();

  // if WiFi is down, try reconnecting every CHECK_WIFI_TIME seconds
  if ((WiFi.status() != WL_CONNECTED) && (currentMillis - previousMillis >= interval))
  {
    Serial.print(millis());
    Serial.println("Reconnecting to WiFi...");
    WiFi.disconnect();
    WiFi.reconnect();
    previousMillis = currentMillis;
  }
  /* Send IR signal*/
  if (sendFlag == true)
  {
    sendFlag = false;
#if SEND_COOLIX
    // Now send the IR signal.
    ac.send();
    bot.sendMessage(chatty, "El comando fue enviado.", "");
#else  // SEND
    Serial.println("Can't send because SEND_COOLIX has been disabled.");
    bot.sendMessage(chatty, "El comando no fue enviado. Por favor intente nuevamente", "");
#endif // SEND_ARGO
  }

  if (sendSutra == true)
  {
    sendSutra = false;
    RandomizeSutra();
  }

  /* sensors readings*/
  if (millis() > lastTimeSensorsUpdate + timeSensorsDelay)
  {
    Get_HumTempAmb();
    lastTimeSensorsUpdate = millis();
  };
  /*END sensors readings*/

  /* clock functions*/
  if (millis() > lastTimeUpdate + timeRequestDelay)
  {
    UpdateTime();
    lastTimeUpdate = millis();
  };
  /* END clock functions*/

  /* CHATBOT TELEGRAM */
  if (millis() > lastTimeBotRan + botRequestDelay)
  {

    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);

    while (numNewMessages)
    {
      Serial.println("got a request!");
      handleNewMessages(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }
    lastTimeBotRan = millis();
  }
  /* FIN CHATBOT */
}

void Get_HumTempAmb()
{
  Serial.println(humAmb = dht.readHumidity());
  Serial.println(tempAmb = dht.readTemperature());
  if (isnan(humAmb) || isnan(tempAmb))
  {
    humAmb = 1;
    tempAmb = 1;
    Serial.println("Error de lectura de temperatura o humedad relativa. Verificar sensor");
  };
  if ((humAmb != 1) || (tempAmb != 1))
  {
    if (humAndTempPromCounter < MAXhumAndTempPromCounter)
    {
      humProm += humAmb;
      tempProm += tempAmb;
      humAndTempPromCounter++;
    }
    else if (humAndTempPromCounter == MAXhumAndTempPromCounter)
    {
      humProm = 0;
      tempProm = 0;
      humAndTempPromCounter = 0;
    }
  }
}

void handleNewMessages(int numNewMessages)
{

  for (int i = 0; i < numNewMessages; i++)
  {
    // Chat id of the requester
    String chat_id = String(bot.messages[i].chat_id);
    if ((chat_id != CHAT_ID) && (chat_id != CHAT_ID_Lari))
    {
      bot.sendMessage(chat_id, "Unauthorized user", "");
      continue;
    }

    // set that chat_id for the response
    chatty = chat_id;

    // Print the received message
    String text = bot.messages[i].text;
    delay(10);

    String from_name = bot.messages[i].from_name;

    if (text == "/Datos")
    {
      String tects = "Peticion procesada, " + from_name + ".\n";
      tects += "Últimas lecturas en sensores: \n\n";
      tects += "\n  Temp=";
      tects += String(tempAmb);
      tects += "\n  Hum=";
      tects += String(humAmb);
      tects += "\n\n";

      float humProm2 = 0;
      humProm2 = humProm / humAndTempPromCounter;
      float tempProm2 = 0;
      tempProm2 = tempProm / humAndTempPromCounter;

      tects += "Lectura promedio en sensores: \n\n";
      tects += "\n  Temp=";
      if (isnan(tempProm2))
      {
        tects += String(tempAmb);
      }
      else
      {
        tects += String(tempProm2);
      };
      tects += "\n  Hum=";
      if (isnan(humProm2))
      {
        tects += String(humAmb);
      }
      else
      {
        tects += String(humProm2);
      };
      tects += "\n (Resultado promedio de " + String(humAndTempPromCounter) + " lecturas).";

      bot.sendMessage(chat_id, tects, "");
    }
    if (text == "/Start")
    {
      sendFlag = true;
      String CONFIGKEY = "[[\"/Datos\"],[\"/On\", \"/Off\"],[\"/20\", \"/21\", \"/22\"],[\"/23\",\"/24\",\"/25\"],[\"/26\", \"/27\",\"/28\"],[\"/Sutras\"]]";
      bot.sendMessageWithReplyKeyboard(chat_id, "Datos para saber Temperatura y Humedad,o seleccioná el comando que quieras enviar... \n\n", "", CONFIGKEY, true);
    }
    if (text == "/On")
    {
      sendFlag = true;
      String CONFIGKEY = "[[\"/Datos\"],[\"/On\", \"/Off\"],[\"/20\", \"/21\", \"/22\"],[\"/23\",\"/24\",\"/25\"],[\"/26\", \"/27\",\"/28\"],[\"/Sutras\"]]";
      ac.setPower(true);
      ac.setFan(kCoolixFanAuto);
      ac.setMode(kCoolixCool);
      bot.sendMessageWithReplyKeyboard(chat_id, "Seleccionaste ENCENDER. \n\n", "", CONFIGKEY, true);
    }
    if (text == "/Off")
    {
      sendFlag = true;
      String CONFIGKEY = "[[\"/Datos\"],[\"/On\", \"/Off\"],[\"/20\", \"/21\", \"/22\"],[\"/23\",\"/24\",\"/25\"],[\"/26\", \"/27\",\"/28\"],[\"/Sutras\"]]";
      ac.setPower(false);
      bot.sendMessageWithReplyKeyboard(chat_id, "Seleccionaste APAGAR. \n\n", "", CONFIGKEY, true);
    }

    if (text == "/29")
    {
      sendFlag = true;
      String CONFIGKEY = "[[\"/Datos\"],[\"/On\", \"/Off\"],[\"/20\", \"/21\", \"/22\"],[\"/23\",\"/24\",\"/25\"],[\"/26\", \"/27\",\"/28\"],[\"/Sutras\"]]";
      ac.setPower(true);
      ac.setFan(kCoolixFanAuto);
      ac.setMode(kCoolixCool);
      ac.setTemp(29);
      bot.sendMessageWithReplyKeyboard(chat_id, "Seleccionaste 29 grados. \n\n", "", CONFIGKEY, true);
    }
    if (text == "/28")
    {
      sendFlag = true;
      String CONFIGKEY = "[[\"/Datos\"],[\"/On\", \"/Off\"],[\"/20\", \"/21\", \"/22\"],[\"/23\",\"/24\",\"/25\"],[\"/26\", \"/27\",\"/28\"],[\"/Sutras\"]]";
      ac.setPower(true);
      ac.setFan(kCoolixFanAuto);
      ac.setMode(kCoolixCool);
      ac.setTemp(28);
      bot.sendMessageWithReplyKeyboard(chat_id, "Seleccionaste 28 grados. \n\n", "", CONFIGKEY, true);
    }
    if (text == "/27")
    {
      sendFlag = true;
      String CONFIGKEY = "[[\"/Datos\"],[\"/On\", \"/Off\"],[\"/20\", \"/21\", \"/22\"],[\"/23\",\"/24\",\"/25\"],[\"/26\", \"/27\",\"/28\"],[\"/Sutras\"]]";
      ac.setPower(true);
      ac.setFan(kCoolixFanAuto);
      ac.setMode(kCoolixCool);
      ac.setTemp(27);
      bot.sendMessageWithReplyKeyboard(chat_id, "Seleccionaste 27 grados. \n\n", "", CONFIGKEY, true);
    }
    if (text == "/26")
    {
      sendFlag = true;
      String CONFIGKEY = "[[\"/Datos\"],[\"/On\", \"/Off\"],[\"/20\", \"/21\", \"/22\"],[\"/23\",\"/24\",\"/25\"],[\"/26\", \"/27\",\"/28\"],[\"/Sutras\"]]";
      ac.setPower(true);
      ac.setFan(kCoolixFanAuto);
      ac.setMode(kCoolixCool);
      ac.setTemp(26);
      bot.sendMessageWithReplyKeyboard(chat_id, "Seleccionaste 26 grados. \n\n", "", CONFIGKEY, true);
    }
    if (text == "/25")
    {
      sendFlag = true;
      String CONFIGKEY = "[[\"/Datos\"],[\"/On\", \"/Off\"],[\"/20\", \"/21\", \"/22\"],[\"/23\",\"/24\",\"/25\"],[\"/26\", \"/27\",\"/28\"],[\"/Sutras\"]]";
      ac.setPower(true);
      ac.setFan(kCoolixFanAuto);
      ac.setMode(kCoolixCool);
      ac.setTemp(25);
      bot.sendMessageWithReplyKeyboard(chat_id, "Seleccionaste 25 grados. \n\n", "", CONFIGKEY, true);
    }
    if (text == "/24")
    {
      sendFlag = true;
      String CONFIGKEY = "[[\"/Datos\"],[\"/On\", \"/Off\"],[\"/20\", \"/21\", \"/22\"],[\"/23\",\"/24\",\"/25\"],[\"/26\", \"/27\",\"/28\"],[\"/Sutras\"]]";
      ac.setPower(true);
      ac.setFan(kCoolixFanAuto);
      ac.setMode(kCoolixCool);
      ac.setTemp(24);
      bot.sendMessageWithReplyKeyboard(chat_id, "Seleccionaste 24 grados. \n\n", "", CONFIGKEY, true);
    }
    if (text == "/23")
    {
      sendFlag = true;
      String CONFIGKEY = "[[\"/Datos\"],[\"/On\", \"/Off\"],[\"/20\", \"/21\", \"/22\"],[\"/23\",\"/24\",\"/25\"],[\"/26\", \"/27\",\"/28\"],[\"/Sutras\"]]";
      ac.setPower(true);
      ac.setFan(kCoolixFanAuto);
      ac.setMode(kCoolixCool);
      ac.setTemp(23);
      bot.sendMessageWithReplyKeyboard(chat_id, "Seleccionaste 23 grados. \n\n", "", CONFIGKEY, true);
    }
    if (text == "/22")
    {
      sendFlag = true;
      String CONFIGKEY = "[[\"/Datos\"],[\"/On\", \"/Off\"],[\"/20\", \"/21\", \"/22\"],[\"/23\",\"/24\",\"/25\"],[\"/26\", \"/27\",\"/28\"],[\"/Sutras\"]]";
      ac.setPower(true);
      ac.setFan(kCoolixFanAuto);
      ac.setMode(kCoolixCool);
      ac.setTemp(22);
      bot.sendMessageWithReplyKeyboard(chat_id, "Seleccionaste 22 grados. \n\n", "", CONFIGKEY, true);
    }
    if (text == "/21")
    {
      sendFlag = true;
      String CONFIGKEY = "[[\"/Datos\"],[\"/On\", \"/Off\"],[\"/20\", \"/21\", \"/22\"],[\"/23\",\"/24\",\"/25\"],[\"/26\", \"/27\",\"/28\"],[\"/Sutras\"]]";
      ac.setPower(true);
      ac.setFan(kCoolixFanAuto);
      ac.setMode(kCoolixCool);
      ac.setTemp(21);
      bot.sendMessageWithReplyKeyboard(chat_id, "Seleccionaste 21 grados. \n\n", "", CONFIGKEY, true);
    }
    if (text == "/20")
    {
      sendFlag = true;
      String CONFIGKEY = "[[\"/Datos\"],[\"/On\", \"/Off\"],[\"/20\", \"/21\", \"/22\"],[\"/23\",\"/24\",\"/25\"],[\"/26\", \"/27\",\"/28\"],[\"/Sutras\"]]";
      ac.setPower(true);
      ac.setFan(kCoolixFanAuto);
      ac.setMode(kCoolixCool);
      ac.setTemp(20);
      bot.sendMessageWithReplyKeyboard(chat_id, "Seleccionaste 20 grados. \n\n", "", CONFIGKEY, true);
    }
    if (text == "/Sutras")
    {
      sendSutra = true;
    }
  }

}

void UpdateTime()
{
  timeClient.update();
  Serial.println("[UPDATE] Time is:");
  Serial.println(timeClient.getFormattedTime());
  int horaActual = timeClient.getHours();
  int minActual = timeClient.getMinutes();
  lastTimeUpdate = millis();
}

void RandomizeSutra()
{
  char *capituloUno_1[5] = {"¿Qué es Yoga? (Yoga Sutras 1.1 -1.4)", "1.1 Ahora, luego de haber hecho la preparación previa por medio de la vida misma y otras prácticas, empieza el estudio y la práctica del Yoga.", "1.2 Yoga es el control (nirodha, regulación, canalización, maestría, integración, coordinación, aquietamiento, silenciamiento, poner a un lado) de las modificaciones (patrones de pensamiento densos y sutiles) del campo mental.", "1.3 Entonces, el Vidente (o El que Ve) reside en sí mismo, yaciendo en su Verdadera Naturaleza. A esto se le llama Auto-Realización.", "1.4 En otras condiciones, al no estar en Auto-Realización, el Vidente parece adquirir la forma de las modificaciones del campo mental y se identifica con esos patrones de pensamiento."};
  char *capituloUno_2[8] = {"Decolorar los pensamientos (Yoga Sutras 1.5 -1.11)", "1.5 Esos patrones de pensamiento densos y sutiles (vrittis) son de cinco variedades, de las cuales algunas están coloreadas (klishta) y otras no están coloreadas (aklishta).", "1.6 Las cinco variedades de patrones de pensamiento a observar son: 1) conocimiento correcto (pramana), 2) conocimiento incorrecto (viparyaya), 3) fantasía o imaginación (vikalpa), 4) el objeto correspondiente al vacío del dormir profundo (nidra) y 5) recuerdo o memoria (smriti).", "1.7 A partir de estos cinco, hay tres maneras de acceder al conocimiento correcto (pramana): 1)percepción, 2) deducción y 3) testimonio o comunicación verbal proveniente de quienes tienen conocimiento.", "1.8 El conocimiento incorrecto o ilusión (viparyaya) es conocimiento falso, lo cual se produce al percibir una cosa como algo distinto de lo que realmente es.", "1.9 La fantasía o imaginación (vikalpa) es un patrón de pensamiento que se puede conocer y expresar verbalmente, pero que no tiene existencia como objeto o realidad.", "1.10 El sueño sin sueños (nidra) es el patrón de pensamiento sutil cuyo objeto es una inactividad, vacío, ausencia o negación de los otros patrones de pensamiento (vrittis).", "1.11 El recuerdo o memoria (smriti) es una modificación mental causada por la reproducción interna de la impresión previa de un objeto, sin agregársele ninguna otra característica proveniente de otras fuentes."};
  char *capituloUno_3[6] = {"Práctica y No-Apego (Yoga Sutras 1.12 -1.16)", "1.12 Estos patrones de pensamiento (vrittis) son dominados (nirodhah, regulados, coordinados, controlados, aquietados, silenciados) a través de la práctica (abhyasa) y el no-apego (vairagya).", "1.13 Práctica (abhyasa) significa elegir, hacer el esfuerzo correspondiente y realizar aquellas acciones que producen un estado estable y tranquilo (sthitau).", "1.14 Cuando tal práctica se hace por largo tiempo, sin descanso y con devoción sincera, ésta se convierte en un cimiento firmemente arraigado, estable y sólido.", "1.15 Cuando la mente ya no tiene deseo ni siquiera por los objetos que se ven o describen en una tradición o en las escrituras, adquiere un estado de total (vashikara) ausencia de deseo llamado no-apego (vairagya).", "1.16 La indiferencia hacia los elementos sutiles, principios constituyentes o cualidades propiamente tales (gunas), lograda gracias al conocimiento de la naturaleza de la consciencia pura (purusha), se llama no-apego supremo (paravairagya)."};
  char *capituloUno_4[3] = {"Tipos de concentración (Yoga Sutras 1.17 -1.18)", "1.17 La absorción profunda de la atención en un objeto es de cuatro clases: 1) burda (vitarka), 2) sutil (vichara), 3) acompañada de gozo (ananda), y 4) con consciencia de la individualidad o personalidad individual (asmita), llamada samprajnata Samadhi.", "1.18 La otra clase de Samadhi es el asamprajnata Samadhi, que carece de objeto en el que absorber la atención, donde sólo permanecen las impresiones latentes; este estado se logra luego de una práctica constante, consistente en permitir que todas las fluctuaciones burdas y sutiles de la mente retornen hacia el campo del cual emergieron."};
  char *capituloUno_5[5] = {"Tareas y compromiso (Yoga Sutras 1.19 -1.22)", "1.19 Algunos que ya han accedido a niveles superiores (videhas) o conocen la naturaleza no manifiesta (prakritilayas) son impulsados a nacer en este mundo debido a sus impresiones latentes remanentes de ignorancia, y alcanzan estos estados de samadhi por naturaleza.", "1.20 Otros siguen un camino sistemático de cinco pasos que son: 1) certeza fiel en el camino, 2)energía dirigida hacia las prácticas, 3) tener presente el camino y el proceso de aquietar la mente de manera reiterada, 4) entrenarse en la concentración profunda y 5) búsqueda del conocimiento real, que es lo que permite lograr el samadhi más elevado (asamprajnata samadhi).", "1.21 Quienes persisten en sus prácticas con intenso sentimiento y vigor y una firme convicción, adquieren concentración y los frutos de las mismas más rápidamente que aquellos que lo hacen con mediana o menor intensidad.", "1.22 Pero incluso aquellos que tienen tal compromiso y convicción muestran diferencias en su progreso, ya que los métodos se pueden aplicar de manera lenta, mediana y rápida, resultando de esto nueve niveles de práctica. "};
  char *capituloUno_6[8] = {"Vía directa a través de (la contemplación de) AUM (OM) (Yoga Sutras 1.23 - 1.29)", "1.23 Producto de un especial proceso de devoción y entrega o abandono en la fuente creativa de la cual emergimos (ishvara pranidhana), el advenimiento de samadhi es inminente.", "1.24 Dicha fuente creativa (ishvara) es una conciencia particular (purusha), que no se ve afectada por los coloridos (kleshas), las acciones (karmas) o los resultados de las acciones que ocurren cuando las impresiones latentes despiertan y las generan.", "1.25 En esa consciencia pura (ishvara) la semilla de la omnisciencia ha alcanzado su máximo desarrollo, imposible de superar.", "1.26 Los maestros más antiguos recibieron sus enseñanzas de esa consciencia (ishvara), dado que no está limitada por las restricciones del tiempo.", "1.27 La palabra sagrada que designa esta fuente creativa es el sonido OM, llamado pranava.", "1.28 Este sonido se recuerda con profundo sentimiento debido al significado de lo que representa.", "1.29 Ese recordar acarrea la realización del Ser individual y remueve los obstáculos."};
  char *capituloUno_7[4] = {"Obstáculos y soluciones (Yoga Sutras 1.30 -1.32)", "1.30 En el camino, es posible encontrar nueve tipos de distracciones consideradas obstáculos, que son: la enfermedad física, la tendencia de la mente a no trabajar eficientemente, la duda o indecisión, negligencia en buscar los medios para conseguir samadhi, flojera de mente y cuerpo, fracaso en regular el deseo por los objetos mundanos, pensamientos o presunciones incorrectas, falla en lograr las etapas de la práctica, e inestabilidad en mantener un nivel de práctica ya obtenido.", "1.31 A partir de estos obstáculos aparecen otras cuatro consecuencias que son: 1) dolor mental o físico, 2) tristeza o abatimiento, 3) inquietud, desequilibrio o ansiedad, y 4) irregularidades en la exhalación e inhalación en la respiración.", "1.32 Para prevenir o hacer frente a estos nueve obstáculos y sus cuatro consecuencias, la recomendación consiste en dirigir la mente en una sola dirección, en entrenarla para que se enfoque en un único objeto o principio."};
  char *capituloUno_8[8] = {"Estabilizar y despejar la mente (Yoga Sutras 1.33 -1.39)", "1.33 En cuanto a las relaciones, la mente se purifica cultivando sentimientos de amabilidad o simpatía hacia quienes están felices, compasión hacia los que sufren, buena disposición hacia aquellos que tienen virtudes, e indiferencia o neutralidad hacia quienes uno percibe como malvados o malintencionados.", "1.34 La mente también se calma regulando la respiración, dando especial atención a la exhalación y al aquietamiento de la respiración que se deriva de esta práctica.", "1.35 La concentración interna en el proceso de la experiencia sensorial, practicada de modo que conduzca a una percepción más sutil y elevada, también produce estabilidad y tranquilidad mental.", "1.36 O concentrarse en un estado interno de lucidez y luminosidad en ausencia de dolor, acarrea estabilidad y tranquilidad. ", "1.37 O contemplar el hecho de tener una mente libre de deseos, estabiliza y tranquiliza la mente.", "1.38 O al enfocarse en la naturaleza del contenido del estado de soñar o en la naturaleza del estado de dormir sin sueños, la mente se vuelve estable y tranquila.", "1.39 O contemplando o concentrándose en cualquier objeto o principio que a uno le guste o hacia el que tenga predisposición, la mente se hace estable y tranquila. "};
  char *capituloUno_9[13] = {"Resultados de estabilizar la mente (Yoga Sutras 1.40 -1.51)", "1.40 Cuando gracias a estas prácticas la mente desarrolla el poder de mantenerse estable, tanto en relación al objeto más pequeño como al más grande, puede realmente decirse que ella se halla bajo control.", "1.41 Cuando las modificaciones de la mente se han debilitado, esta última se vuelve como un cristal transparente, y puede fácilmente adquirir las cualidades de cualquier objeto observado, ya sea éste el observador mismo, los medios a través de los que se observa o un objeto dado, en un proceso de absorción llamado samapatti.", "1.42 Uno de los tipos de absorción (samapatti) es aquel en el que se juntan tres cosas, una palabra o nombre que denomina un objeto, el significado o identidad de tal objeto y el conocimiento asociado a ese objeto; esta absorción se conoce como savitarka samapatti (asociada a objetos densos).", "1.43 Cuando la memoria o depósito de las modificaciones de la mente se purifica, la mente pareciera carecer de su propia naturaleza, y sólo el objeto que se contempla se destaca en primer plano; este tipo de absorción se conoce como nirvitarka samapatti.", "1.44 Tal como estos tipos de absorción se producen con objetos densos o concretos, lo mismo ocurre respecto a objetos sutiles; a estos se les llama savichara y nirvichara samapatti.", "1.45 La gama de dichos objetos sutiles se extiende hasta prakriti no manifestada.", "1.46 Estas cuatro variedades de absorción son las únicas clases de concentración (samadhi) que son objetivas y tienen la semilla de un objeto.", "1.47 A medida que se adquiere la destreza de mantener un flujo sin interferencias en nirvichara, se desarrolla una pureza y luminosidad del instrumento interno de la mente.", "1.48 El conocimiento experimental que se gana en ese estado se relaciona con la sabiduría esencial y está repleto de la verdad.", "1.49 Ese conocimiento difiere del que está asociado a un testimonio o deducción, dado que se refiere a las características específicas del objeto más que a las palabras u otros conceptos.", "1.50 Este tipo de conocimiento que rebosa verdad crea impresiones latentes en el campo mental, las cuales tienden a reducir la formación de otras formas de impresiones latentes habituales menos útiles.", "1.51 Cuando incluso estas impresiones latentes llenas de conocimiento de la verdad se retiran junto con las otras impresiones, se accede a la concentración sin objeto."};

  char *capituloDos_1[10] = {"Minimizar los coloridos más densos (Yoga Sutras 2.1 -2.9)", "2.1 El Yoga en tanto acción (kriya yoga) tiene tres partes: 1) entrenar y purificar los sentidos (tapas), 2) estudio de sí mismo en el contexto de las enseñanzas (svadhyaya) y 3) devoción y entrega o abandono en la fuente creativa de la cual emergimos (ishvara pranidhana).", "2.2 Ese Yoga de la acción (kriya yoga) se practica para dar lugar a samadhi y minimizar los patrones de pensamiento coloreados (kleshas).", "2.3 Hay cinco tipos de coloridos (kleshas): 1) olvido o ignorancia respecto a la verdadera naturaleza de las cosas (avidya), 2) el yo, la individualidad o egoísmo (asmita), 3) apego o adicción a las impresiones mentales o a los objetos (raga), aversión a los patrones de pensamiento o a los objetos (dvesha), y 5) amor a éstos como si fueran la vida misma, y miedo a perderlos como si fueran la muerte.", "2.4 El olvido fundamental o la ignorancia de la naturaleza de las cosas (avidya) es el terreno de cultivo para el resto de los otros coloridos (kleshas), cada uno de los cuales se halla en uno de cuatro estados: 1) dormido o inactivo, 2) atenuado o debilitado, 3) interrumpido o separado momentáneamente o 4) activo o produciendo pensamientos y acciones de distinto grado.", "2.5 La ignorancia (avidya) es de cuatro tipos: 1) considerar lo que es transitorio como eterno, 2) confundir lo impuro con lo puro, 3) creer que lo que acarrea miseria trae felicidad, y 4) estimar lo que es el no ser como si fuera el ser.", "2.6 El colorido (klesha) del yo o egoísmo (asmita) que surge de la ignorancia se debe al error de considerar el intelecto (buddhi, que conoce, decide, juzga y discrimina) como si fuera consciencia pura (purusha).", "2.7 El apego (raga) es una modificación diferente de la mente, que se da cuando aparece el recuerdo del placer, y en la cual se asocian entre sí tres modificaciones, el apego, el placer y el recuerdo del objeto.", "2.8 La aversión (dvesha) es una modificación que resulta de relacionar el sufrimiento con algún recuerdo, siendo las tres modificaciones asociadas en este caso el rechazo, el dolor y el recuerdo del objeto o la experiencia.", "2.9 Incluso quienes conocen de estas cosas tienen un amor permanente y arraigado hacia la continuación de estas modificaciones coloreadas (kleshas), y a la vez miedo de su desaparición o muerte."};
  char *capituloDos_2[3] = {"Abordaje de los pensamientos sutiles (Yoga Sutras 2.10 -2.11)", "2.10 Cuando los cinco tipos de coloridos (kleshas) se hallan en su forma sutil, meramente en potencia, éstos se destruyen por medio de su desaparición o cesación en el campo mental y del campo mental mismo.", "2.11 Cuando las modificaciones aún tienen cierto colorido (klishta), ellas se llevan al estado potencial a través de la meditación (dhyana)."};
  char *capituloDos_3[15] = {"Romper la alianza o asociación con el karma (Yoga Sutras 2.12 -2.25)", "2.12 Las impresiones latentes que están coloreadas (karmashaya) son el resultado de acciones (karmas) que fueron provocadas por los coloridos (kleshas), y se activan y experimentan en la vida presente o en una futura.", "2.13 Mientras esos coloridos (kleshas) permanecen arraigados se producen tres consecuencias: 1) nacimiento, 2) lapso de vida y 3) experiencias en esa vida.", "2.14 Estos tres, dado que las impresiones coloreadas tienen la característica de ser méritos o deméritos, pueden experimentarse en términos de placer o dolor.", "2.15 Una persona sabia, que discrimina, considera todas las experiencias mundanas como dolorosas, porque deduce que todas ellas llevan a más consecuencias, ansiedad y hábitos profundos (samskaras), como también que las cualidades naturales actúan opuestamente entre sí.", "2.16 Debido a que las experiencias mundanas se aprecian como dolorosas, es el dolor que está por venir el que es posible evitar y desechar.", "2.17 La causa o conexión que se ha de evitar es la unión entre el que ve (el sujeto o el que experimenta) y lo visto (el objeto o lo que se experimenta).", "2.18 Los objetos (lo que se puede conocer) según su naturaleza son 1) de iluminación o percepción, 2) de actividad o mutabilidad, o 3) de inercia o estancamiento; ellos están hechos de los elementos y del poder de los sentidos, y existen a fin de experimentar el mundo y para la liberación o iluminación.", "2.19 Hay cuatro estados de los elementos (gunas) que son: 1) diversificado, especializado o particularizado (vishesha), 2) no diversificado, no especializado o no particularizado (avishesha), 3) sólo como señalizador, fenoménicamente indiferenciado, sólo como indicio (linga-matra), y 4) sin señalizador, nóumeno (suprasensible), sin indicio (alingani).", "2.20 El Vidente es el poder de ver en sí, que aparenta ver o experimentar aquello que se le presenta como un principio cognitivo.", "2.21 La esencia o naturaleza de los objetos cognoscibles existe sólo en función de servir como campo objetivo para la consciencia pura.", "2.22 Aunque los objetos cognoscibles dejan de existir en relación a alguien que ha experimentado su verdadera naturaleza fundamental, sin forma, la apariencia de esos objetos no se destruye ya que su existencia sigue siendo compartida por otros, que aún los están observando en sus formas más burdas.", "2.23 El medio necesario para que el Ser posteriormente pueda darse cuenta de la verdadera naturaleza de los objetos, es que haya una alianza o relación entre los objetos y el Ser.", "2.24 Avidya o la ignorancia (2.3 -2.5), la condición de ignorar, es la causa subyacente que permite que esta alianza parezca existir.", "2.25 Cuando no hay avidya o ignorancia existe una ausencia de dicha alianza, lo cual conduce al Vidente a la libertad conocida como estado de liberación o iluminación."};
  char *capituloDos_4[5] = {"Razón para los 8 peldaños (Yoga Sutras 2.26 -2.29)", "2.26 El medio para liberarse de esta alianza o asociación es el conocimiento discriminativo claro, definido, intacto.", "2.27 Quien ha logrado este grado de discriminación accede a siete tipos de insights o comprensión de nivel supremo.", "2.28 Por medio de la práctica de los distintos pasos del Yoga, con los cuales se eliminan las impurezas, emerge una claridad cuya culminación es la sabiduría discriminativa, o iluminación.", "2.29 Los ocho peldaños, escalones o pasos del Yoga son: las reglas de autorregulación o restricción (yamas), cumplimiento o prácticas de autoentrenamiento (niyamas), posturas (asana), expansión de la respiración y el prana (pranayama), retracción de los sentidos (pratyahara), concentración (dharana), meditación (dhyana), y concentración perfecta (samadhi)."};
  char *capituloDos_5[6] = {"Yamas & Niyamas, # 1 y 2 de los 8 peldaños (Yoga Sutras 2.30 -2.34)", "2.30 El primero de los ocho pasos del Yoga, los cinco yamas o normas de autorregulación o restricción, son: no herir o no dañar (ahimsa), veracidad (satya), abstenerse de robar (asteya), mantenerse constantemente consciente de la realidad suprema (brahmacharya), y evitar la posesividad, la codicia, o adueñarse haciendo uso de los sentidos (aparigraha).", "2.31 Estos códigos de comportamiento se convierten en un gran compromiso cuando se viven como reglas generalizadas, en el sentido de abarcar irrestrictamente a todo ser viviente, en todo lugar, tiempo o situación.", "2.32 Limpieza y pureza de cuerpo y mente (shaucha), una actitud de contentamiento (santocha), ascetismo o entrenamiento de los sentidos (tapas), estudio de uno mismo y reflexión en las palabras sagradas (svadhyaya), y una actitud de abandono o entrega a la propia fuente (ishvarapranidhana) son las observancias o prácticas de autoentrenamiento (niyamas), que corresponden al segundo peldaño en la escalera del Yoga.", "2.33 Cuando tanto las normas de autorregulación o restricción (yamas) como las observancias o prácticas de autoentrenamiento (niyamas) se dejan de practicar debido a pensamientos perversos, malsanos, conflictivos o anormales, uno debería cultivar principios o pensamientos opuestos o en dirección contraria.", "2.34 Las acciones provenientes de estos pensamientos negativos son realizadas por uno mismo directamente, inducidas por otros o aprobadas cuando otros las ejecutan. Todas ellas pueden ser precedidas por o derivar de la rabia, la codicia o la ilusión, y ser de grado leve, moderado o intenso. El pensamiento contrario o principio en sentido opuesto recomendado en el sutra previo consiste en tener presente que tales pensamientos negativos y esas acciones son motivo de interminable sufrimiento e ignorancia."};
  char *capituloDos_6[12] = {"Beneficios de los Yamas y Niyamas (Yoga Sutras 2.35 -2.45)", "2.35 Cuando un Yogui consigue estar bien afianzado en no dañar (ahimsa), las personas que se le acercan dejan de experimentar cualquier sentimiento de hostilidad de manera natural.", "2.36 Cuando se logra la veracidad (satya), los frutos de las acciones del Yogui resultan espontáneamente de acuerdo a su voluntad.", "2.37 Cuando se establece la práctica de no robar (asteya), todas las joyas o tesoros se hacen presentes de por sí, o están a la disposición del Yogui.", "2.38 Cuando el mantenerse constantemente consciente de la realidad suprema (brahmacharya) se establece firmemente, se adquiere una gran fuerza, capacidad o vitalidad (virya).", "2.39 Cuando alguien es inquebrantable respecto a evitar la posesividad, la codicia, o adueñarse haciendo uso de los sentidos (aparigraha), emerge el conocimiento del por qué de las encarnaciones pasadas y futuras.", "2.40 A través de la limpieza y pureza del cuerpo y la mente (shausha) se desarrolla una actitud de distanciamiento o desinterés hacia el propio cuerpo, y se reduce la inclinación a contactar el cuerpo de otros.", "2.41 Con la limpieza y pureza del cuerpo y la mente (shausha) se logra además una purificación de la esencia mental sutil (sattva), un sentimiento de agrado, bondad, felicidad, el enfoque absorto en una sola dirección, la conquista o maestría sobre los sentidos, y una aptitud, calificación o capacidad para la Auto-Realización.", "2.42 A partir de la actitud de contento(santosha) se obtiene una felicidad insuperable, bienestar mental, gozo y satisfacción.", " 2.43 A través del ascetismo o entrenamiento de los sentidos(tapas) se produce la destrucción de las impurezas mentales y una consiguiente maestría o perfección respecto al cuerpo y a los órganos mentales de los sentidos y las acciones(indriyas).", " 2.44 Del estudio de uno mismo y la reflexión en las palabras sagradas(svadhyaya) se consigue contacto, comunión o armonización con la realidad o fuerza natural subyacente.", " 2.45 Desde la actitud de abandono o entrega a nuestra propia fuente(ishvarapranidhana) se logra el estado perfecto de concentración(samadhi)."};
  char *capituloDos_7[4] = {"Asana, # 3 de los 8 peldaños (Yoga Sutras 2.46 -2.48)", "2.46 La postura (asana) para la meditación Yoga, -el tercero de los ocho pasos del Yoga-, debería ser firme, estable e inmóvil, y además cómoda.", "2.47 Las formas de perfeccionar la postura consisten en relajar o reducir el esfuerzo, y dejar que la atención se fusione con lo infinito.", "2.48 El logro de la postura perfecta genera una libertad invulnerable y sin obstáculos frente al sufrimiento debido a los pares de opuestos (como calor y frío, bueno y malo, dolor y placer)."};
  char *capituloDos_8[6] = {"Pranayama, # 4 de los 8 peldaños (Yoga Sutras 2.49 -2.53)", "2.49 Luego de conseguir la postura perfecta, se practica el control de la respiración y expansión del prana (pranayama), consistente en enlentecer o frenar la fuerza con que se respira y los movimientos incontrolados de la exhalación e inhalación. Esto lleva a una ausencia deconsciencia de la respiración, y es el cuarto de los ocho pasos.", "2.50 Ese pranayama tiene tres aspectos, el flujo externo o hacia el exterior (exhalación), el flujo interno o hacia el interior (inhalación), y un tercero que es la ausencia de ambos durante la transición entre los dos, conocido como inmovilidad, retención o suspensión. Estos tres aspectos se regulan según lugar, tiempo o duración y número, con lo cual la respiración se hace lenta y sutil.", "2.51 El cuarto pranayama es ese prana continuo que sobrepasa, está más allá o está detrás de los otros que funcionan en los niveles o campos externos e internos.", "2.52 Por medio de este pranayama, el velo de karmasheya (2.12) que cubre la luz o iluminación interna, se adelgaza, disminuye y se desvanece.", "2.53 Gracias a estas prácticas y procesos de pranayama, el cuarto de los ocho pasos, la mente adquiere o desarrolla la aptitud, calificación o capacidad para la verdadera concentración (dharana), que es el sexto de los ocho pasos."};
  char *capituloDos_9[3] = {"Pratyahara, # 5 de los 8 peldaños (Yoga Sutras 2.54 -2.55)", "2.54 Cuando los órganos mentales de los sentidos y las acciones (indriyas) dejan de involucrase con los correspondientes objetos en su nivel mental, y se reincorporan o retornan al campo mental del que emergieron, se habla de pratyahara y es el quinto paso.", "2.55 Debido a esa interiorización de los órganos de los sentidos y acciones (indriyas) también se accede a una suprema capacidad, manejo o dominio sobre esos sentidos, que habitualmente tienen la tendencia a exteriorizarse en dirección a esos objetos."};

  char *capituloTres_1[4] = {"Dharana, Dhyana & Samadhi, # 6, 7 y 8 de los 8 peldaños (Yoga Sutras 3.1 -3.3)", "3.1 Concentración (dharana) es el proceso de mantener o fijar la atención mental en un objeto o lugar, siendo el sexto de los ocho pasos.", "3.2 La mantención continua sobre ese punto único de enfoque o su flujo ininterrumpido se llama absorción en meditación (dhyana), y es el séptimo de los ocho pasos.", "3.3 Cuando sólo la esencia de ese objeto, lugar o punto se destaca en la mente, como desprovista incluso de su propia forma, tal estado de absorción profunda se llamaconcentración profunda o samadhi, el octavo paso."};
  char *capituloTres_2[4] = {"Samyama es la herramienta más refinada (Yoga Sutras 3.4 -3.6)", "3.4 Cuando los tres procesos de dharana, dhyana y samadhi se dan juntos en relación al mismo objeto, lugar o punto, a eso se le llama samyama.", "3.5 Cuando ese proceso triple de samyama se domina, la luz del conocimiento, la comprensión trascendental o la consciencia superior (prajna) amanece, ilumina, destella o se hace visible.", "3.6 Este proceso triple de samyama se va aplicando gradualmente a planos, estados o etapas de la práctica cada vez más sutiles."};
  char *capituloTres_3[3] = {"Lo interno se aprecia como externo (Yoga Sutras 3.7 -3.8)", "3.7 Estas tres prácticas de concentración (dharana), meditación (dhyana) y samadhi son más íntimas o internas que las cinco prácticas previas.", "3.8 Sin embargo son externas y no íntimas comparadas con nirbija samadhi, que es samadhi sin objeto, sin ni siquiera un objeto semilla en el cual concentrarse."};
  char *capituloTres_4[9] = {"Observar las transiciones sutiles (Yoga Sutras 3.9 -3.16)", "3.9 Ese elevado nivel de maestría llamado nirodhah-parinamah sucede en el momento en que se da una convergencia entre la tendencia de las impresiones profundas a emerger, su tendencia a remitir, y la atención del campo mental propiamente tal.", "3.10 El flujo continuo de este estado (nirodhah-parinamah) sigue gracias a la creación de impresiones profundas (samskaras) al hacer la práctica.", "3.11 La maestría llamada samadhi-parinamah es la transición gracias a la cual la tendencia a focalizarse en múltiples direcciones decrece, para dar lugar a la tendencia a la concentración en un solo sentido.", "3.12 La maestría llamada ekagrata-parinamah es la transición en la que la misma focalización en un solo sentido aparece y declina secuencialmente.", "3.13 Estos tres procesos de transición también explican las tres transformaciones relativas a la forma, tiempo y características, y cómo éstas se relacionan a los elementos materiales y a los sentidos.", "3.14 Existe un existencia o sustrato indescriptible, no manifiesto, que es común o está contenido dentro de todas las otras formas o cualidades.", "3.15 El cambio en la secuencia de las características es lo que causa las diferentes apariencias en los resultados, consecuencias o efectos.", "3.16 Por medio de samyama en los cambios de forma, tiempo y características se consigue el conocimiento del pasado y el futuro."};
  char *capituloTres_5[22] = {"Experiencias provenientes de Samyama (Yoga Sutras 3.17 -3.37)", "3.17 El nombre asociado a un objeto, el objeto mismo que ese nombre representa y la existencia conceptual del objeto, en general se interrelacionan o entremezclan entre sí. Haciendo samyama respecto a la distinción entre estos tres, se conoce el significado de los sonidos emitidos por todos los seres.", "3.18 Por medio de la percepción directa de las impresiones latentes (samskaras) se accede al conocimiento de las encarnaciones previas.", "3.19 A través de samyama en relación a las nociones o ideas que se nos transmiten se desarrolla el conocimiento de la mente de otro.", "3.20 Pero el trasfondo o fuente de ese conocimiento (de la mente de otras personas, en 3.19) no es percibido o queda fuera del alcance.", "3.21 Cuando se hace samyama respecto a la forma de nuestro cuerpo físico se suspende la iluminación o las características visuales del mismo, y éste se vuelve invisible a otros.", "3.22 Y tal como se describe en relación a la visión (3.21), es posible interrumpir la capacidad del cuerpo de ser escuchado, tocado, gustado u olido.", "3.23 El karma es de dos clases, uno se manifiesta rápido y el otro lentamente. Por medio de samyama en esos karmas viene el conocimiento anticipado del momento de la muerte.", "3.24 Practicar samyama sobre la amabilidad o simpatía (y las otras actitudes de 1.33) acarrea una gran fuerza en relación a esa actitud.", "3.25 Si se practica samyama sobre la fuerza de los elefantes se adquiere una fuerza similar.", "3.26 Dirigiendo la luz interna proveniente de la percepción sensorial superior es factible obtener conocimiento de los objetos sutiles, de aquellos que están ocultos a la vista y de los que están muy lejos.", "3.27 Con samyama en el sol interno se puede tener el conocimiento de muchos reinos sutiles.", "3.28 Con samyama en la luna se puede acceder al conocimiento de la configuración de las estrellas internas.", "3.29 Con samyama en la estrella polar se puede conseguir el conocimiento del movimiento de esas estrellas.", "3.30 Con samyama en el centro del ombligo se puede tener conocimiento de la disposición de los sistemas del cuerpo.", "3.31 Con samyama en el hueco de la garganta se acaba el hambre y la sed.", "3.32 Con samyama en el canal (energético) de la tortuga, bajo el hueco de la garganta, se adquiere estabilidad.", "3.33 Con samyama en la luz coronal, en la cabeza, se puede ver a los siddhas, los maestros.", "3.34 O, por medio de la luz intuitiva del conocimiento superior, se puede conocer cualquier cosa.", "3.35 Practicando samyama en el corazón se accede al conocimiento de la mente.", "3.36 Las experiencias se producen a partir de una idea que se presenta, sólo cuando se combina el aspecto mental más sutil (sattva) con la consciencia pura (purusha), que en realidad son bastante diferentes. Samyama en la consciencia pura, distinta del aspecto mental más sutil, revela el conocimiento de esa consciencia pura.", "3.37 De la luz del elevado conocimiento de esa consciencia pura o purusha (3.36) surgen la audición, el tacto, la visión, el gusto y el olfato superiores, trascendentales o divinos."};
  char *capituloTres_6[2] = {"¿Qué hacer con las experiencias? (Yoga Sutra 3.38)", "3.38 Todas estas experiencias provenientes de samyama son un obstáculo para samadhi, pero parecen logros o poderes a la mente mundana o que tiende hacia lo externo."};
  char *capituloTres_7[12] = {"Más sobre Samyama (Yoga Sutras 3.39- 3.49)", "3.39 Cuando las causas de la esclavitud y el apego se debilitan o se sueltan, y usando del conocimiento de cómo avanzar por los pasadizos de la mente, se obtiene la capacidad de entrar en otro cuerpo.", "3.40 Con la maestría sobre udana, el prana vayu que fluye hacia arriba, deja de haber contacto con el barro, el agua, las espinas y otros objetos similares, cuya consecuencia es la elevación o levitación del cuerpo.", "3.41 Con la maestría sobre samana, el prana que fluye en el área del ombligo, se produce una brillantez, resplandor o fuego.", "3.42 Con samyama sobre la relación entre el espacio y la capacidad de escuchar se consigue el poder divino de escuchar.", "3.43 Con samyama en la relación entre el cuerpo y el espacio (akasha) y por concentración en la levedad del algodón, puede uno ser capaz de movilizarse en el espacio.", "3.44 Cuando los patrones de pensamiento sin forma de la mente se proyectan fuera del cuerpo, a eso se le llama maha-videha, un gran desencarnado. Haciendo samyama en esa proyección externa se remueve el velo que cubre la luz espiritual.", "3.45 Haciendo samyama en las cinco formas de los elementos (bhutas) que son la forma densa, la esencia, la sutileza, la interconectividad y su propósito, se logra la maestría sobre esos bhutas.", "3.46 A través de la maestría sobre los elementos se consigue la capacidad de hacer el cuerpo anatómicamente pequeño, perfecto e indestructible en sus características o componentes, y también otros poderes.", "3.47 Esta perfección del cuerpo incluye belleza, gracia, fuerza y una inquebrantable dureza para recibir golpes.", "3.48 Con samyama en el proceso de percepción y acción, la esencia, la individualidad, la conectividad, y el propósito de los sentidos y las acciones se accede a la maestría sobre dichos sentidos y acciones (indriyas).", "3.49 A través de esa maestría sobre los sentidos y las acciones (indriyas) se consigue rapidez mental, percepción con los instrumentos físicos de percepción y maestría sobre la causa primordial de la que toda manifestación emerge."};
  char *capituloTres_8[4] = {"La renunciación que lleva a la liberación (Yoga Sutras 3.50 -3.52)", "3.50 Quien conoce perfectamente la distinción entre el aspecto más puro de la mente y la consciencia propiamente tal adquiere supremacía sobre todas las formas o estados de existencia, como también sobre todas las formas de conocimiento.", "3.51 Con no-apego o ausencia de deseo incluso respecto a dicha supremacía sobre las formas, estados de existencia y la omnisciencia (3.50), las semillas que son la raíz de esa esclavitud se destruyen y se logra la liberación total.", "3.52 Cuando uno es invitado por seres celestiales, no debería permitirse nada que haga que la mente acepte la oferta o esboce una sonrisa de orgullo por la invitación, porque dejar que esos pensamientos surjan puede crear de nuevo la posibilidad de repetir palabras y acciones indeseables."};
  char *capituloTres_9[5] = {"Discriminación superior a través de Samyama (Yoga Sutras 3.53 -3.56)", "3.53 La práctica de samyama en relación a los momentos en el tiempo y su sucesión acarrea el conocimiento superior proveniente de la discriminación.", "3.54 Desde ese conocimiento discriminativo (3.53) uno puede darse cuenta de la diferencia o distinción entre dos objetos similares, que normalmente no es posible distinguir según categoría, características o posición en el espacio.", "3.55 Ese conocimiento superior es intuitivo y trascendente, y nace de la discriminación; incluye todos los objetos dentro de su campo, todas las condiciones relacionadas a esos objetos, y trasciende cualquier sucesión.", "3.56 Cuando se logra la igualdad entre el más puro aspecto del buddhi sátvico y la consciencia pura de purusha, se produce la liberación absoluta, ese es el final."};

  char *capituloCuatro_1[4] = {"Maneras de conseguir experiencias (Yoga Sutras 4.1 -4.3)", "4.1 Los logros sutiles se traen al nacer o se consiguen por medio de hierbas, mantra, austeridad o concentración.", "4.2 La transición o transformación en otra forma o tipo de nacimiento ocurre gracias a un rellenado de su propia naturaleza.", "4.3 Causas o acciones casuales, fortuitas, no llevan a generar logros o realización, lo que se consigue más bien removiendo los obstáculos, de modo similar a cuando un agricultor retira una barrera (esclusa), lo que permitirá el regadío de su campo naturalmente."};
  char *capituloCuatro_2[4] = {"Uso avanzado de la mente (Yoga Sutras 4.4 -4.6)", "4.4 Los campos mentales que van emergiendo brotan desde la individualidad del Yo (asmita).", "4.5 Si bien las actividades de los campos mentales emergentes pueden ser diversas, la mente única es la que dirige toda esa diversidad.", "4.6 De todos estos campos mentales, el que nace de la meditación está libre de cualquier impresión latente que pudiera producir karma."};
  char *capituloCuatro_3[3] = {"Acciones y Karma (Yoga Sutras 4.7 -4.8)", "4.7 Las acciones de los yoguis no son ni blanco ni negro, mientras que las de otros son de tres clases.", "4.8 Estas acciones de tres tipos dan como resultado impresiones latentes (vasanas), que posteriormente emergerán dando frutos, directamente correspondientes a esas impresiones."};
  char *capituloCuatro_4[5] = {"Impresiones subconscientes (Yoga Sutras 4.9 -4.12)", "4.9 Dado que los recuerdos (smriti) y los patrones de hábitos profundos (samskaras) son iguales en apariencia, existe una ininterrumpida continuidad en la manifestación de éstos, a pesar de que puede haber un intervalo en la ubicación, tiempo y estado de vida.", "4.10 No hay un comienzo en el proceso de estos patrones de hábitos profundos (samskaras) debido a la naturaleza eterna de la voluntad de vivir.", "4.11 Como las impresiones (4.10) se mantienen unidas por causa, motivo, sustrato y objeto, estos cuatro últimos desaparecen cuando las impresiones profundas lo hacen.", "4.12 El pasado y el futuro existen en la realidad presente, y aparecen como diferentes debido a tener diferentes características o formas."};
  char *capituloCuatro_5[3] = {"Los objetos y las 3 gunas (Yoga Sutras 4.13 -4.14)", "4.13 Ya sea que estas características o formas siempre presentes sean manifiestas o sutiles, ellas están compuestas de los elementos primarios llamados las gunas, que son tres.", "4.14 Las características de un objeto dan la apariencia de una unidad, al manifestarse de modo uniforme a partir de los elementos subyacentes."};
  char *capituloCuatro_6[4] = {"La mente percibe los objetos (Yoga Sutras 4.15 -4.17)", "4.15 Aunque los mismos objetos pueden ser percibidos por diferentes mentes, ellos se perciben de distintas formas porque esas mentes se manifiestan diferentemente.", "4.16 Sin embargo, el objeto mismo no depende de mente alguna, porque si así fuera, ¿qué le pasaría al objeto cuando no estuviera siendo experimentado por esa mente?", "4.17 Los objetos se conocen o no de acuerdo a la manera en que los coloridos de ese objeto inciden en los coloridos de la mente que los observa."};
  char *capituloCuatro_7[10] = {"Iluminación de la mente (Yoga Sutras 4.18 -4.21)", "4.18 La consciencia pura siempre conoce las actividades de la mente, ya que es superior a, soporte de, y la que domina a esta última.", "4.19 Esta mente no es iluminada en sí misma, dado que es el objeto de conocimiento y percepción de la consciencia pura.", "4.20 Ni pueden la mente y el proceso de iluminación conocerse simultáneamente.", "4.21 Si una mente fuera iluminada por otra, que hiciera las veces de maestra, entonces habría una interminable y absurda progresión de cogniciones y también de confusión. Buddhi y liberación (Yoga Sutras 4.22 -4.26)", "4.22 Cuando la consciencia inalterable parece adoptar la forma del aspecto más sutil de la mente (4.18), entonces es posible tener la experiencia del propio proceso de cognición.", "4.23 Por tanto, el campo mental que está coloreado por el que ve como por lo visto, tiene el potencial de percibir cualquier objeto y todos ellos.", "4.24 Ese campo mental, aunque está lleno de incontables impresiones, existe para beneficio de otra consciencia que observa, ya que el campo mental opera sólo en combinación con esas impresiones.", "4.25 Para alguien que ha experimentado la diferencia entre el que ve y la mente más sutil, las identidades falsas e incluso la curiosidad respecto a la naturaleza del propio ser se acaban.", "4.26 Entonces la mente se torna hacia la discriminación más elevada y gravita hacia la absoluta liberación entre el que ve y lo visto."};
  char *capituloCuatro_8[3] = {"Brechas en la iluminación (Yoga Sutras 4.27 -4.28)", "4.27 Cuando se producen brechas o interrupciones en esa elevada discriminación, otras impresiones emergen desde el inconsciente profundo.", "4.28 La remoción de esos patrones de pensamiento interferentes se realiza a través de los mismos medios con los cuales se removieron los coloridos inicialmente."};
  char *capituloCuatro_9[3] = {"Iluminación permanente (Yoga Sutras 4.29 -4.30)", "4.29 Cuando yo no existe ningún interés, ni siquiera en la omnisciencia, esa discriminación permite el samadhi, que trae una abundancia de virtudes, tal como las nubes traen lluvia.", "4.30 Después de ese dharma-meghah-samadhi, los coloridos de los kleshas y los karmas son removidos."};
  char *capituloCuatro_10[2] = {"Queda casi nada por conocer (Yoga Sutra 4.31)", "4.31 Entonces, al eliminar esos velos de imperfección, se hace presente la experiencia del infinito, y la realización de que queda casi nada por conocer."};
  char *capituloCuatro_11[4] = {"Las gunas después de la liberación (Yoga Sutras 4.32 -4.34)", "4.32 También como producto de ese dharma-meghah-samadhi (4.29), los tres elementos primarios o gunas (4.13 -4.14) habrán cumplido su propósito, y dejando de experimentar su sucesiva transformación retornan de vuelta a su esencia.", "4.33 El proceso secuencial de momentos e impresiones equivale a los momentos de tiempo, lo cual se comprende en el punto final de la secuencia.", "4.34 Cuando esos elementos primarios involucionan o se resuelven, regresando a aquello de lo cual emergieron originalmente, ocurre la liberación, y el poder de la consciencia pura se establece en su verdadera naturaleza."};

  int randomNumber = random(1, 41);

  printf("Random number: %d\n", randomNumber);
  String randomSutra = "";
  switch (randomNumber)
  {
  case 1:
  {
    int arrSize = sizeof(capituloUno_1) / sizeof(int);
    int min = 1;
    int max = arrSize - 1;
    int randomIndex = random(min, max);
    char *randomElement = capituloUno_1[randomIndex];
    randomSutra += "Libro Primero \n";
    randomSutra += "Titulo: ";
    randomSutra += String(capituloUno_1[0]);
    randomSutra += "\n Sutra Numero ";
    randomSutra += String(randomElement);
    Serial.println(randomSutra);
    break;
  }
  case 2:
  {
    int arrSize = sizeof(capituloUno_2) / sizeof(int);
    int min = 1;
    int max = arrSize - 1;
    int randomIndex = random(min, max);
    char *randomElement = capituloUno_2[randomIndex];
    randomSutra += "Libro Primero \n";
    randomSutra += "Titulo: ";
    randomSutra += String(capituloUno_2[0]);
    randomSutra += "\n Sutra Numero ";
    randomSutra += String(randomElement);
    Serial.println(randomSutra);
    break;
  }
  case 3:
  {
    int arrSize = sizeof(capituloUno_3) / sizeof(int);
    int min = 1;
    int max = arrSize - 1;
    int randomIndex = random(min, max);
    char *randomElement = capituloUno_3[randomIndex];
    randomSutra += "Libro Primero \n";
    randomSutra += "Titulo: ";
    randomSutra += String(capituloUno_3[0]);
    randomSutra += "\n Sutra Numero ";
    randomSutra += String(randomElement);
    Serial.println(randomSutra);
    break;
  }
  case 4:
  {
    int arrSize = sizeof(capituloUno_4) / sizeof(int);
    int min = 1;
    int max = arrSize - 1;
    int randomIndex = random(min, max);
    char *randomElement = capituloUno_4[randomIndex];
    randomSutra += "Libro Primero \n";
    randomSutra += "Titulo: ";
    randomSutra += String(capituloUno_4[0]);
    randomSutra += "\n Sutra Numero ";
    randomSutra += String(randomElement);
    Serial.println(randomSutra);
    break;
  }
  case 5:
  {
    int arrSize = sizeof(capituloUno_5) / sizeof(int);
    int min = 1;
    int max = arrSize - 1;
    int randomIndex = random(min, max);
    char *randomElement = capituloUno_5[randomIndex];
    randomSutra += "Libro Primero \n";
    randomSutra += "Titulo: ";
    randomSutra += String(capituloUno_5[0]);
    randomSutra += "\n Sutra Numero ";
    randomSutra += String(randomElement);
    Serial.println(randomSutra);
    break;
  }
  case 6:
  {
    int arrSize = sizeof(capituloUno_6) / sizeof(int);
    int min = 1;
    int max = arrSize - 1;
    int randomIndex = random(min, max);
    char *randomElement = capituloUno_6[randomIndex];
    randomSutra += "Libro Primero \n";
    randomSutra += "Titulo: ";
    randomSutra += String(capituloUno_6[0]);
    randomSutra += "\n Sutra Numero ";
    randomSutra += String(randomElement);
    Serial.println(randomSutra);
    break;
  }
  case 7:
  {
    int arrSize = sizeof(capituloUno_7) / sizeof(int);
    int min = 1;
    int max = arrSize - 1;
    int randomIndex = random(min, max);
    char *randomElement = capituloUno_7[randomIndex];
    randomSutra += "Libro Primero \n";
    randomSutra += "Titulo: ";
    randomSutra += String(capituloUno_7[0]);
    randomSutra += "\n Sutra Numero ";
    randomSutra += String(randomElement);
    Serial.println(randomSutra);
    break;
  }
  case 8:
  {
    int arrSize = sizeof(capituloUno_8) / sizeof(int);
    int min = 1;
    int max = arrSize - 1;
    int randomIndex = random(min, max);
    char *randomElement = capituloUno_8[randomIndex];
    randomSutra += "Libro Primero \n";
    randomSutra += "Titulo: ";
    randomSutra += String(capituloUno_8[0]);
    randomSutra += "\n Sutra Numero ";
    randomSutra += String(randomElement);
    Serial.println(randomSutra);
    break;
  }
  case 9:
  {
    int arrSize = sizeof(capituloUno_9) / sizeof(int);
    int min = 1;
    int max = arrSize - 1;
    int randomIndex = random(min, max);
    char *randomElement = capituloUno_9[randomIndex];
    randomSutra += "Libro Primero \n";
    randomSutra += "Titulo: ";
    randomSutra += String(capituloUno_9[0]);
    randomSutra += "\n Sutra Numero ";
    randomSutra += String(randomElement);
    Serial.println(randomSutra);
    break;
  }
  case 10:
  {
    int arrSize = sizeof(capituloDos_1) / sizeof(int);
    int min = 1;
    int max = arrSize - 1;
    int randomIndex = random(min, max);
    char *randomElement = capituloDos_1[randomIndex];
    randomSutra += "Libro Segundo \n";
    randomSutra += "Titulo: ";
    randomSutra += String(capituloDos_1[0]);
    randomSutra += "\n Sutra Numero ";
    randomSutra += String(randomElement);
    Serial.println(randomSutra);
    break;
  }
  case 11:
  {
    int arrSize = sizeof(capituloDos_2) / sizeof(int);
    int min = 1;
    int max = arrSize - 1;
    int randomIndex = random(min, max);
    char *randomElement = capituloDos_2[randomIndex];
    randomSutra += "Libro Segundo \n";
    randomSutra += "Titulo: ";
    randomSutra += String(capituloDos_2[0]);
    randomSutra += "\n Sutra Numero ";
    randomSutra += String(randomElement);
    Serial.println(randomSutra);
    break;
  }
  case 12:
  {
    int arrSize = sizeof(capituloDos_3) / sizeof(int);
    int min = 1;
    int max = arrSize - 1;
    int randomIndex = random(min, max);
    char *randomElement = capituloDos_3[randomIndex];
    randomSutra += "Libro Segundo \n";
    randomSutra += "Titulo: ";
    randomSutra += String(capituloDos_3[0]);
    randomSutra += "\n Sutra Numero ";
    randomSutra += String(randomElement);
    Serial.println(randomSutra);
    break;
  }
  case 13:
  {
    int arrSize = sizeof(capituloDos_4) / sizeof(int);
    int min = 1;
    int max = arrSize - 1;
    int randomIndex = random(min, max);
    char *randomElement = capituloDos_4[randomIndex];
    randomSutra += "Libro Segundo \n";
    randomSutra += "Titulo: ";
    randomSutra += String(capituloDos_4[0]);
    randomSutra += "\n Sutra Numero ";
    randomSutra += String(randomElement);
    Serial.println(randomSutra);
    break;
  }
  case 14:
  {
    int arrSize = sizeof(capituloDos_5) / sizeof(int);
    int min = 1;
    int max = arrSize - 1;
    int randomIndex = random(min, max);
    char *randomElement = capituloDos_5[randomIndex];
    randomSutra += "Libro Segundo \n";
    randomSutra += "Titulo: ";
    randomSutra += String(capituloDos_5[0]);
    randomSutra += "\n Sutra Numero ";
    randomSutra += String(randomElement);
    Serial.println(randomSutra);
    break;
  }
  case 15:
  {
    int arrSize = sizeof(capituloDos_6) / sizeof(int);
    int min = 1;
    int max = arrSize - 1;
    int randomIndex = random(min, max);
    char *randomElement = capituloDos_6[randomIndex];
    randomSutra += "Libro Segundo \n";
    randomSutra += "Titulo: ";
    randomSutra += String(capituloDos_6[0]);
    randomSutra += "\n Sutra Numero ";
    randomSutra += String(randomElement);
    Serial.println(randomSutra);
    break;
  }
  case 17:
  {
    int arrSize = sizeof(capituloDos_7) / sizeof(int);
    int min = 1;
    int max = arrSize - 1;
    int randomIndex = random(min, max);
    char *randomElement = capituloDos_7[randomIndex];
    randomSutra += "Libro Segundo \n";
    randomSutra += "Titulo: ";
    randomSutra += String(capituloDos_7[0]);
    randomSutra += "\n Sutra Numero ";
    randomSutra += String(randomElement);
    Serial.println(randomSutra);
    break;
  }
  case 18:
  {
    int arrSize = sizeof(capituloDos_8) / sizeof(int);
    int min = 1;
    int max = arrSize - 1;
    int randomIndex = random(min, max);
    char *randomElement = capituloDos_8[randomIndex];
    randomSutra += "Libro Segundo \n";
    randomSutra += "Titulo: ";
    randomSutra += String(capituloDos_8[0]);
    randomSutra += "\n Sutra Numero ";
    randomSutra += String(randomElement);
    Serial.println(randomSutra);
    break;
  }
  case 19:
  {
    int arrSize = sizeof(capituloDos_9) / sizeof(int);
    int min = 1;
    int max = arrSize - 1;
    int randomIndex = random(min, max);
    char *randomElement = capituloDos_9[randomIndex];
    randomSutra += "Libro Segundo \n";
    randomSutra += "Titulo: ";
    randomSutra += String(capituloDos_9[0]);
    randomSutra += "\n Sutra Numero ";
    randomSutra += String(randomElement);
    Serial.println(randomSutra);
    break;
  }
  case 20:
  {
    int arrSize = sizeof(capituloTres_1) / sizeof(int);
    int min = 1;
    int max = arrSize - 1;
    int randomIndex = random(min, max);
    char *randomElement = capituloTres_1[randomIndex];
    randomSutra += "Libro Tercero \n";
    randomSutra += "Titulo: ";
    randomSutra += String(capituloTres_1[0]);
    randomSutra += "\n Sutra Numero ";
    randomSutra += String(randomElement);
    Serial.println(randomSutra);
    break;
  }
  case 21:
  {
    int arrSize = sizeof(capituloTres_2) / sizeof(int);
    int min = 1;
    int max = arrSize - 1;
    int randomIndex = random(min, max);
    char *randomElement = capituloTres_2[randomIndex];
    randomSutra += "Libro Tercero \n";
    randomSutra += "Titulo: ";
    randomSutra += String(capituloTres_2[0]);
    randomSutra += "\n Sutra Numero ";
    randomSutra += String(randomElement);
    Serial.println(randomSutra);
    break;
  }
  case 22:
  {
    int arrSize = sizeof(capituloTres_3) / sizeof(int);
    int min = 1;
    int max = arrSize - 1;
    int randomIndex = random(min, max);
    char *randomElement = capituloTres_3[randomIndex];
    randomSutra += "Libro Tercero \n";
    randomSutra += "Titulo: ";
    randomSutra += String(capituloTres_3[0]);
    randomSutra += "\n Sutra Numero ";
    randomSutra += String(randomElement);
    Serial.println(randomSutra);
    break;
  }
  case 23:
  {
    int arrSize = sizeof(capituloTres_4) / sizeof(int);
    int min = 1;
    int max = arrSize - 1;
    int randomIndex = random(min, max);
    char *randomElement = capituloTres_4[randomIndex];
    randomSutra += "Libro Tercero \n";
    randomSutra += "Titulo: ";
    randomSutra += String(capituloTres_4[0]);
    randomSutra += "\n Sutra Numero ";
    randomSutra += String(randomElement);
    Serial.println(randomSutra);
    break;
  }
  case 24:
  {
    int arrSize = sizeof(capituloTres_5) / sizeof(int);
    int min = 1;
    int max = arrSize - 1;
    int randomIndex = random(min, max);
    char *randomElement = capituloTres_5[randomIndex];
    randomSutra += "Libro Tercero \n";
    randomSutra += "Titulo: ";
    randomSutra += String(capituloTres_5[0]);
    randomSutra += "\n Sutra Numero ";
    randomSutra += String(randomElement);
    Serial.println(randomSutra);
    break;
  }
  case 25:
  {
    int arrSize = sizeof(capituloTres_6) / sizeof(int);
    int min = 1;
    int max = arrSize - 1;
    int randomIndex = random(min, max);
    char *randomElement = capituloTres_6[randomIndex];
    randomSutra += "Libro Tercero \n";
    randomSutra += "Titulo: ";
    randomSutra += String(capituloTres_6[0]);
    randomSutra += "\n Sutra Numero ";
    randomSutra += String(randomElement);
    Serial.println(randomSutra);
    break;
  }
  case 26:
  {
    int arrSize = sizeof(capituloTres_7) / sizeof(int);
    int min = 1;
    int max = arrSize - 1;
    int randomIndex = random(min, max);
    char *randomElement = capituloTres_7[randomIndex];
    randomSutra += "Libro Tercero \n";
    randomSutra += "Titulo: ";
    randomSutra += String(capituloTres_7[0]);
    randomSutra += "\n Sutra Numero ";
    randomSutra += String(randomElement);
    Serial.println(randomSutra);
    break;
  }
  case 27:
  {
    int arrSize = sizeof(capituloTres_8) / sizeof(int);
    int min = 1;
    int max = arrSize - 1;
    int randomIndex = random(min, max);
    char *randomElement = capituloTres_8[randomIndex];
    randomSutra += "Libro Tercero \n";
    randomSutra += "Titulo: ";
    randomSutra += String(capituloTres_8[0]);
    randomSutra += "\n Sutra Numero ";
    randomSutra += String(randomElement);
    Serial.println(randomSutra);
    break;
  }
  case 28:
  {
    int arrSize = sizeof(capituloTres_9) / sizeof(int);
    int min = 1;
    int max = arrSize - 1;
    int randomIndex = random(min, max);
    char *randomElement = capituloTres_9[randomIndex];
    randomSutra += "Libro Tercero \n";
    randomSutra += "Titulo: ";
    randomSutra += String(capituloTres_9[0]);
    randomSutra += "\n Sutra Numero ";
    randomSutra += String(randomElement);
    Serial.println(randomSutra);
    break;
  }
  case 29:
  {
    int arrSize = sizeof(capituloCuatro_1) / sizeof(int);
    int min = 1;
    int max = arrSize - 1;
    int randomIndex = random(min, max);
    char *randomElement = capituloCuatro_1[randomIndex];
    randomSutra += "Libro Cuarto \n";
    randomSutra += "Titulo: ";
    randomSutra += String(capituloCuatro_1[0]);
    randomSutra += "\n Sutra Numero ";
    randomSutra += String(randomElement);
    Serial.println(randomSutra);
    break;
  }
  case 30:
  {
    int arrSize = sizeof(capituloCuatro_2) / sizeof(int);
    int min = 1;
    int max = arrSize - 1;
    int randomIndex = random(min, max);
    char *randomElement = capituloCuatro_2[randomIndex];
    randomSutra += "Libro Cuarto \n";
    randomSutra += "Titulo: ";
    randomSutra += String(capituloCuatro_2[0]);
    randomSutra += "\n Sutra Numero ";
    randomSutra += String(randomElement);
    Serial.println(randomSutra);
    break;
  }
  case 31:
  {
    int arrSize = sizeof(capituloCuatro_3) / sizeof(int);
    int min = 1;
    int max = arrSize - 1;
    int randomIndex = random(min, max);
    char *randomElement = capituloCuatro_3[randomIndex];
    randomSutra += "Libro Cuarto \n";
    randomSutra += "Titulo: ";
    randomSutra += String(capituloCuatro_3[0]);
    randomSutra += "\n Sutra Numero ";
    randomSutra += String(randomElement);
    Serial.println(randomSutra);
    break;
  }
  case 32:
  {
    int arrSize = sizeof(capituloCuatro_4) / sizeof(int);
    int min = 1;
    int max = arrSize - 1;
    int randomIndex = random(min, max);
    char *randomElement = capituloCuatro_4[randomIndex];
    randomSutra += "Libro Cuarto \n";
    randomSutra += "Titulo: ";
    randomSutra += String(capituloCuatro_4[0]);
    randomSutra += "\n Sutra Numero ";
    randomSutra += String(randomElement);
    Serial.println(randomSutra);
    break;
  }
  case 33:
  {
    int arrSize = sizeof(capituloCuatro_5) / sizeof(int);
    int min = 1;
    int max = arrSize - 1;
    int randomIndex = random(min, max);
    char *randomElement = capituloCuatro_5[randomIndex];
    randomSutra += "Libro Cuarto \n";
    randomSutra += "Titulo: ";
    randomSutra += String(capituloCuatro_5[0]);
    randomSutra += "\n Sutra Numero ";
    randomSutra += String(randomElement);
    Serial.println(randomSutra);
    break;
  }
  case 34:
  {
    int arrSize = sizeof(capituloCuatro_6) / sizeof(int);
    int min = 1;
    int max = arrSize - 1;
    int randomIndex = random(min, max);
    char *randomElement = capituloCuatro_6[randomIndex];
    randomSutra += "Libro Cuarto \n";
    randomSutra += "Titulo: ";
    randomSutra += String(capituloCuatro_6[0]);
    randomSutra += "\n Sutra Numero ";
    randomSutra += String(randomElement);
    Serial.println(randomSutra);
    break;
  }
  case 35:
  {
    int arrSize = sizeof(capituloCuatro_7) / sizeof(int);
    int min = 1;
    int max = arrSize - 1;
    int randomIndex = random(min, max);
    char *randomElement = capituloCuatro_7[randomIndex];
    randomSutra += "Libro Cuarto \n";
    randomSutra += "Titulo: ";
    randomSutra += String(capituloCuatro_7[0]);
    randomSutra += "\n Sutra Numero ";
    randomSutra += String(randomElement);
    Serial.println(randomSutra);
    break;
  }
  case 36:
  {
    int arrSize = sizeof(capituloCuatro_8) / sizeof(int);
    int min = 1;
    int max = arrSize - 1;
    int randomIndex = random(min, max);
    char *randomElement = capituloCuatro_8[randomIndex];
    randomSutra += "Libro Cuarto \n";
    randomSutra += "Titulo: ";
    randomSutra += String(capituloCuatro_8[0]);
    randomSutra += "\n Sutra Numero ";
    randomSutra += String(randomElement);
    Serial.println(randomSutra);
    break;
  }
  case 37:
  {
    int arrSize = sizeof(capituloCuatro_9) / sizeof(int);
    int min = 1;
    int max = arrSize - 1;
    int randomIndex = random(min, max);
    char *randomElement = capituloCuatro_9[randomIndex];
    randomSutra += "Libro Cuarto \n";
    randomSutra += "Titulo: ";
    randomSutra += String(capituloCuatro_9[0]);
    randomSutra += "\n Sutra Numero ";
    randomSutra += String(randomElement);
    Serial.println(randomSutra);
    break;
  }
  case 38:
  {
    int arrSize = sizeof(capituloCuatro_10) / sizeof(int);
    int min = 1;
    int max = arrSize - 1;
    int randomIndex = random(min, max);
    char *randomElement = capituloCuatro_10[randomIndex];
    randomSutra += "Libro Cuarto \n";
    randomSutra += "Titulo: ";
    randomSutra += String(capituloCuatro_10[0]);
    randomSutra += "\n Sutra Numero ";
    randomSutra += String(randomElement);
    Serial.println(randomSutra);
    break;
  }
  case 39:
  {
    int arrSize = sizeof(capituloCuatro_11) / sizeof(int);
    int min = 1;
    int max = arrSize - 1;
    int randomIndex = random(min, max);
    char *randomElement = capituloCuatro_11[randomIndex];
    randomSutra += "Libro Cuarto \n";
    randomSutra += "Titulo: ";
    randomSutra += String(capituloCuatro_11[0]);
    randomSutra += "\n Sutra Numero ";
    randomSutra += String(randomElement);
    Serial.println(randomSutra);
    break;
  }
  case 40:
  {
    randomSutra += "Caramba! \n Este NO es un Sutra! \n Es apenas un mensaje sencillo, para recordarte lo que sos y que valores con mucho amor lo lejos que te trajiste! Mira hasta donde llegaste! Dale!";
    Serial.println(randomSutra);
    break;
  }
  case 41:
  {
    randomSutra += "Caramba! \n Este NO es un Sutra! \n Es apenas un mensaje sencillo, para recordarte que te quiero mucho!!";
    Serial.println(randomSutra);
    break;
  }
  }
  bot.sendMessage(chatty, randomSutra, "");
}
