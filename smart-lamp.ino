#include "pgmspace.h"
#include "Constants.h"
#include <FastLED.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include "CaptivePortalManager.h"
#include <WiFiUdp.h>
#include <EEPROM.h>
#include "Types.h"
#include "timerMinim.h"
#include <GyverButton.h>
#include "fonts.h"
#include <NTPClient.h>
#include <Timezone.h>
#include <TimeLib.h>
#include "TimerManager.h"
#include "FavoritesManager.h"
#include "EepromManager.h"


CRGB leds[NUM_LEDS];
WiFiManager wifiManager;
WiFiServer wifiServer(ESP_HTTP_PORT);
WiFiUDP Udp;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, NTP_ADDRESS, 0, NTP_INTERVAL);
TimeChangeRule localTime = { LOCAL_TIMEZONE_NAME, LOCAL_WEEK_NUM, LOCAL_WEEKDAY, LOCAL_MONTH, LOCAL_HOUR, LOCAL_OFFSET };
Timezone localTimeZone(localTime);

timerMinim timeTimer(3000);
bool ntpServerAddressResolved = false;
bool timeSynched = false;
uint32_t lastTimePrinted = 0U;

GButton touch(BTN_PIN, LOW_PULL, NORM_OPEN);

uint16_t localPort = ESP_UDP_PORT;
char packetBuffer[MAX_UDP_BUFFER_SIZE];
char inputBuffer[MAX_UDP_BUFFER_SIZE];
static const uint8_t maxDim = max(WIDTH, HEIGHT);

ModeType modes[MODE_AMOUNT];
AlarmType alarms[7];

static const uint8_t dawnOffsets[] PROGMEM = {5, 10, 15, 20, 25, 30, 40, 50, 60};
uint8_t dawnMode;
bool dawnFlag = false;
uint32_t thisTime;
bool manualOff = false;

int8_t currentMode = 0;
bool loadingFlag = true;
bool ONflag = false;
uint32_t eepromTimeout;
bool settChanged = false;
bool buttonEnabled = true;

unsigned char matrixValue[8][16];

bool TimerManager::TimerRunning = false;
bool TimerManager::TimerHasFired = false;
uint8_t TimerManager::TimerOption = 1U;
uint64_t TimerManager::TimeToFire = 0ULL;

uint8_t FavoritesManager::FavoritesRunning = 0;
uint16_t FavoritesManager::Interval = DEFAULT_FAVORITES_INTERVAL;
uint16_t FavoritesManager::Dispersion = DEFAULT_FAVORITES_DISPERSION;
uint8_t FavoritesManager::UseSavedFavoritesRunning = 0;
uint8_t FavoritesManager::FavoriteModes[MODE_AMOUNT] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
uint32_t FavoritesManager::nextModeAt = 0UL;

bool CaptivePortalManager::captivePortalCalled = false;


void setup()
{
  Serial.begin(115200);
  Serial.println();
  ESP.wdtEnable(WDTO_8S);

  touch.setStepTimeout(BUTTON_STEP_TIMEOUT);
  touch.setClickTimeout(BUTTON_CLICK_TIMEOUT);
  delay(1000);                                            // ожидание инициализации модуля кнопки ttp223 (по спецификации 250мс)
  if (digitalRead(BTN_PIN))
  {
    wifiManager.resetSettings();                          // сброс сохранённых SSID и пароля при старте с зажатой кнопкой, если разрешено
    LOG.println(F("Настройки WiFiManager сброшены"));
  }
  buttonEnabled = true;                                   // при сбросе параметров WiFi сразу после старта с зажатой кнопкой, также разблокируется кнопка, если была заблокирована раньше
  EepromManager::SaveButtonEnabled(&buttonEnabled);
  ESP.wdtFeed();

  FastLED.addLeds<WS2812B, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.setBrightness(BRIGHTNESS);
  if (CURRENT_LIMIT > 0)
  {
    FastLED.setMaxPowerInVoltsAndMilliamps(5, CURRENT_LIMIT);
  }
  FastLED.clear();
  FastLED.show();


  // EEPROM
  EepromManager::InitEepromSettings(
    modes, alarms, &espMode, &ONflag, &dawnMode, &currentMode, &buttonEnabled,
    &(FavoritesManager::ReadFavoritesFromEeprom),
    &(FavoritesManager::SaveFavoritesToEeprom));
  LOG.printf_P(PSTR("Рабочий режим лампы: ESP_MODE = %d\n"), espMode);

  // WI-FI
  wifiManager.setDebugOutput(WIFIMAN_DEBUG);
  CaptivePortalManager *captivePortalManager = new CaptivePortalManager(&wifiManager);

  LOG.println(F("Старт в режиме WiFi клиента (подключение к роутеру)"));

  if (WiFi.SSID().length())
  {
    LOG.printf_P(PSTR("Подключение к WiFi сети: %s\n"), WiFi.SSID().c_str());

    if (sizeof(STA_STATIC_IP))                            // ВНИМАНИЕ: настраивать статический ip WiFi клиента можно только при уже сохранённых имени и пароле WiFi сети (иначе проявляется несовместимость библиотек WiFiManager и WiFi)
    {
      LOG.print(F("Сконфигурирован статический IP адрес: "));
      LOG.printf_P(PSTR("%u.%u.%u.%u\n"), STA_STATIC_IP[0], STA_STATIC_IP[1], STA_STATIC_IP[2], STA_STATIC_IP[3]);
      wifiManager.setSTAStaticIPConfig(
        IPAddress(STA_STATIC_IP[0], STA_STATIC_IP[1], STA_STATIC_IP[2], STA_STATIC_IP[3]),// статический IP адрес ESP в режиме WiFi клиента
        IPAddress(STA_STATIC_IP[0], STA_STATIC_IP[1], STA_STATIC_IP[2], 1),               // первый доступный IP адрес сети (справедливо для 99,99% случаев; для сетей меньше чем на 255 адресов нужно вынести в константы)
        IPAddress(255, 255, 255, 0));                                                     // маска подсети (справедливо для 99,99% случаев; для сетей меньше чем на 255 адресов нужно вынести в константы)
    }
  }
  else
  {
    LOG.println(F("WiFi сеть не определена, запуск WiFi точки доступа для настройки параметров подключения к WiFi сети..."));
    CaptivePortalManager::captivePortalCalled = true;
    wifiManager.setBreakAfterConfig(true);                // перезагрузка после ввода и сохранения имени и пароля WiFi сети
    showWarning(CRGB::Yellow, 1000U, 500U);               // мигание жёлтым цветом 0,5 секунды (1 раз) - нужно ввести параметры WiFi сети для подключения
  }

  wifiManager.setConnectTimeout(ESP_CONN_TIMEOUT);        // установка времени ожидания подключения к WiFi сети, затем старт WiFi точки доступа
  wifiManager.setConfigPortalTimeout(ESP_CONF_TIMEOUT);   // установка времени работы WiFi точки доступа, затем перезагрузка; отключить watchdog?
  wifiManager.autoConnect(AP_NAME, AP_PASS);              // пытаемся подключиться к сохранённой ранее WiFi сети; в случае ошибки, будет развёрнута WiFi точка доступа с указанными AP_NAME и паролем на время ESP_CONN_TIMEOUT секунд; http://AP_STATIC_IP:ESP_HTTP_PORT (обычно http://192.168.0.1:80) - страница для ввода SSID и пароля от WiFi сети роутера

  delete captivePortalManager;
  captivePortalManager = NULL;

  if (WiFi.status() != WL_CONNECTED)                      // подключение к WiFi не установлено
  {
    if (CaptivePortalManager::captivePortalCalled)        // была показана страница настройки WiFi ...
    {
      if (millis() < (ESP_CONN_TIMEOUT + ESP_CONF_TIMEOUT) * 1000U) // пользователь ввёл некорректное имя WiFi сети и/или пароль или запрошенная WiFi сеть недоступна
      {
        LOG.println(F("Не удалось подключиться к WiFi сети\nУбедитесь в корректности имени WiFi сети и пароля\nРестарт для запроса нового имени WiFi сети и пароля...\n"));
        wifiManager.resetSettings();
      }
      else                                                // пользователь не вводил имя WiFi сети и пароль
      {
        LOG.println(F("Время ожидания ввода SSID и пароля от WiFi сети или подключения к WiFi сети превышено\nЛампа будет перезагружена в режиме WiFi точки доступа!\n"));

        espMode = (espMode == 0U) ? 1U : 0U;
        EepromManager::SaveEspMode(&espMode);

        LOG.printf_P(PSTR("Рабочий режим лампы изменён и сохранён в энергонезависимую память\nНовый рабочий режим: ESP_MODE = %d, %s\nРестарт...\n"),
                     espMode, espMode == 0U ? F("WiFi точка доступа") : F("WiFi клиент (подключение к роутеру)"));
      }
    }
    else                                                  // страница настройки WiFi не была показана, не удалось подключиться к ранее сохранённой WiFi сети (перенос в новую WiFi сеть)
    {
      LOG.println(F("Не удалось подключиться к WiFi сети\nВозможно, заданная WiFi сеть больше не доступна\nРестарт для запроса нового имени WiFi сети и пароля...\n"));
      wifiManager.resetSettings();
    }

    showWarning(CRGB::Red, 1000U, 500U);                  // мигание красным цветом 0,5 секунды (1 раз) - ожидание ввода SSID'а и пароля WiFi сети прекращено, перезагрузка
    ESP.restart();
  }

  if (CaptivePortalManager::captivePortalCalled &&        // первое подключение к WiFi сети после настройки параметров WiFi на странице настройки - нужна перезагрузка для применения статического IP
      sizeof(STA_STATIC_IP) &&
      WiFi.localIP() != IPAddress(STA_STATIC_IP[0], STA_STATIC_IP[1], STA_STATIC_IP[2], STA_STATIC_IP[3]))
  {
    LOG.println(F("Рестарт для применения заданного статического IP адреса..."));
    delay(100);
    ESP.restart();
  }

  LOG.print(F("IP адрес: "));
  LOG.println(WiFi.localIP());

  ESP.wdtFeed();

  LOG.printf_P(PSTR("Порт UDP сервера: %u\n"), localPort);
  Udp.begin(localPort);

  timeClient.begin();
  ESP.wdtFeed();

  memset(matrixValue, 0, sizeof(matrixValue));
  randomSeed(micros());
  changePower();
  loadingFlag = true;
}


void loop() {

  parseUDP();
  effectsTick();

  EepromManager::HandleEepromTick(&settChanged, &eepromTimeout, &ONflag,
                                  &currentMode, modes, &(FavoritesManager::SaveFavoritesToEeprom));

  timeTick();
  if (buttonEnabled) {
    buttonTick();
  }

  TimerManager::HandleTimer(&ONflag, &settChanged,
                            &eepromTimeout, &changePower);

  if (FavoritesManager::HandleFavorites(
        &ONflag,
        &currentMode,
        &loadingFlag,
        &dawnFlag
      ))
  {
    FastLED.setBrightness(modes[currentMode].Brightness);
    FastLED.clear();
    delay(1);
  }

  ESP.wdtFeed();
}
