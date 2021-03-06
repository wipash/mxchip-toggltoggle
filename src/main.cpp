#include <Arduino.h>
#undef max
#undef min
#include "AzureIotHub.h"
#include "ArduinoJson.h"
#include "AZ3166WiFi.h"
#include "config.h"
#include "http_client.h"
#include "rBase64.h"
#include "RGB_LED.h"
#include "SystemTickCounter.h"

static RGB_LED rgbLed;

static bool hasWiFi;

static uint32_t status = WL_IDLE_STATUS;

static uint32_t duration;

static uint32_t state = 2;
// 0 = stopped
// 1 = running
// 2 = updating

volatile bool buttonPressed = false;

static uint32_t checkIntervalMs;

const char TOGGL_BASE_URI[] = "https://www.toggl.com/api/v8/time_entries/";
const char TOGGL_CURRENT_URI[] = "https://www.toggl.com/api/v8/time_entries/current";
const char TOGGL_START_URI[] = "https://www.toggl.com/api/v8/time_entries/start";
const char TOGGL_SUMM_URI[] = "https://toggl.com/reports/api/v2/summary";

static uint32_t interval = FASTINTERVAL;

char authHeaderString[150];
char currentEntryID[10];

// Display buffers
static char runningTimeBuffer[10];
static char totalTimeBuffer[10];

// SSL Cert for Comodo CA
const char SSL_CA_PEM[] = "-----BEGIN CERTIFICATE-----\n"
                          "MIIF2DCCA8CgAwIBAgIQTKr5yttjb+Af907YWwOGnTANBgkqhkiG9w0BAQwFADCB\n"
                          "hTELMAkGA1UEBhMCR0IxGzAZBgNVBAgTEkdyZWF0ZXIgTWFuY2hlc3RlcjEQMA4G\n"
                          "A1UEBxMHU2FsZm9yZDEaMBgGA1UEChMRQ09NT0RPIENBIExpbWl0ZWQxKzApBgNV\n"
                          "BAMTIkNPTU9ETyBSU0EgQ2VydGlmaWNhdGlvbiBBdXRob3JpdHkwHhcNMTAwMTE5\n"
                          "MDAwMDAwWhcNMzgwMTE4MjM1OTU5WjCBhTELMAkGA1UEBhMCR0IxGzAZBgNVBAgT\n"
                          "EkdyZWF0ZXIgTWFuY2hlc3RlcjEQMA4GA1UEBxMHU2FsZm9yZDEaMBgGA1UEChMR\n"
                          "Q09NT0RPIENBIExpbWl0ZWQxKzApBgNVBAMTIkNPTU9ETyBSU0EgQ2VydGlmaWNh\n"
                          "dGlvbiBBdXRob3JpdHkwggIiMA0GCSqGSIb3DQEBAQUAA4ICDwAwggIKAoICAQCR\n"
                          "6FSS0gpWsawNJN3Fz0RndJkrN6N9I3AAcbxT38T6KhKPS38QVr2fcHK3YX/JSw8X\n"
                          "pz3jsARh7v8Rl8f0hj4K+j5c+ZPmNHrZFGvnnLOFoIJ6dq9xkNfs/Q36nGz637CC\n"
                          "9BR++b7Epi9Pf5l/tfxnQ3K9DADWietrLNPtj5gcFKt+5eNu/Nio5JIk2kNrYrhV\n"
                          "/erBvGy2i/MOjZrkm2xpmfh4SDBF1a3hDTxFYPwyllEnvGfDyi62a+pGx8cgoLEf\n"
                          "Zd5ICLqkTqnyg0Y3hOvozIFIQ2dOciqbXL1MGyiKXCJ7tKuY2e7gUYPDCUZObT6Z\n"
                          "+pUX2nwzV0E8jVHtC7ZcryxjGt9XyD+86V3Em69FmeKjWiS0uqlWPc9vqv9JWL7w\n"
                          "qP/0uK3pN/u6uPQLOvnoQ0IeidiEyxPx2bvhiWC4jChWrBQdnArncevPDt09qZah\n"
                          "SL0896+1DSJMwBGB7FY79tOi4lu3sgQiUpWAk2nojkxl8ZEDLXB0AuqLZxUpaVIC\n"
                          "u9ffUGpVRr+goyhhf3DQw6KqLCGqR84onAZFdr+CGCe01a60y1Dma/RMhnEw6abf\n"
                          "Fobg2P9A3fvQQoh/ozM6LlweQRGBY84YcWsr7KaKtzFcOmpH4MN5WdYgGq/yapiq\n"
                          "crxXStJLnbsQ/LBMQeXtHT1eKJ2czL+zUdqnR+WEUwIDAQABo0IwQDAdBgNVHQ4E\n"
                          "FgQUu69+Aj36pvE8hI6t7jiY7NkyMtQwDgYDVR0PAQH/BAQDAgEGMA8GA1UdEwEB\n"
                          "/wQFMAMBAf8wDQYJKoZIhvcNAQEMBQADggIBAArx1UaEt65Ru2yyTUEUAJNMnMvl\n"
                          "wFTPoCWOAvn9sKIN9SCYPBMtrFaisNZ+EZLpLrqeLppysb0ZRGxhNaKatBYSaVqM\n"
                          "4dc+pBroLwP0rmEdEBsqpIt6xf4FpuHA1sj+nq6PK7o9mfjYcwlYRm6mnPTXJ9OV\n"
                          "2jeDchzTc+CiR5kDOF3VSXkAKRzH7JsgHAckaVd4sjn8OoSgtZx8jb8uk2Intzna\n"
                          "FxiuvTwJaP+EmzzV1gsD41eeFPfR60/IvYcjt7ZJQ3mFXLrrkguhxuhoqEwWsRqZ\n"
                          "CuhTLJK7oQkYdQxlqHvLI7cawiiFwxv/0Cti76R7CZGYZ4wUAc1oBmpjIXUDgIiK\n"
                          "boHGhfKppC3n9KUkEEeDys30jXlYsQab5xoq2Z0B15R97QNKyvDb6KkBPvVWmcke\n"
                          "jkk9u+UJueBPSZI9FoJAzMxZxuY67RIuaTxslbH9qh17f4a+Hg4yRvv7E491f0yL\n"
                          "S0Zj/gA0QHDBw7mh3aZw4gSzQbzpgJHqZJx64SIDqZxubw5lT2yHh17zbqD5daWb\n"
                          "QOhTsiedSrnAdyGN/4fy3ryM7xfft0kL0fJuMAsaDk527RH89elWsn2/x20Kk4yl\n"
                          "0MC2Hb46TpSi125sC8KKfPog88Tk5c0NqMuRkrF8hey1FGlmDoLnzc7ILaZRfyHB\n"
                          "NVOFBkpdn627G190\n"
                          "-----END CERTIFICATE-----\n";

void init_wifi()
{
  Screen.print(0, "Connecting WiFi");
  if (WiFi.begin(WIFISSID, WIFIPWD) == WL_CONNECTED)
  {
    IPAddress ip = WiFi.localIP();
    Serial.println(ip.get_address());
    hasWiFi = true;
  }
  else
  {
    Screen.clean();
    Screen.print(1, "No Wi-Fi");
  }
}

/**
    Instead of Screen.clean(), so the screen doesn't flicker
 */
void write_only_line(uint8_t line, const char *message)
{
  for (size_t i = 0; i < 4; i++)
  {
    if (i == line)
    {
      Screen.print(i, message);
    }
    else
    {
      Screen.print(i, "                ");
    }
  }
}

void write_main_display()
{
  switch (state)
  {
  case 0: // stopped
    Screen.print(0, "Stopped         ");
    Screen.print(1, "                ");
    break;

  case 1: // running
    Screen.print(0, "Running         ");
    Screen.print(1, runningTimeBuffer);
    break;

  default:
    break;
  }
  Screen.print(2, "Today's Total");
  Screen.print(3, totalTimeBuffer);
}

void update_state(uint32_t newState)
{
  if (state != newState)
  {
    state = newState;
    switch (state)
    {
    case 0: // stopped
      interval = SLOWINTERVAL;
      break;
    case 1: // running
      interval = FASTINTERVAL;
      break;
    default:
      break;
    }
  }
}

void convert_seconds_to_tm(struct tm *tm, uint32_t seconds)
{
  uint32_t hh, mm, ss;
  hh = seconds / 3600;
  seconds %= 3600;
  mm = seconds / 60;
  seconds %= 60;
  ss = seconds;

  tm->tm_year = 2000 - 1900;
  tm->tm_mon = 1 - 1;
  tm->tm_mday = 1;
  tm->tm_hour = hh;
  tm->tm_min = mm;
  tm->tm_sec = ss;
  tm->tm_isdst = 0;
}

void date_plus_hours(struct tm *date, uint32_t hours)
{
  const time_t ONE_HOUR = 60 * 60;
  time_t dateSeconds = mktime(date) + (hours * ONE_HOUR);
  *date = *localtime(&dateSeconds);
}

void http_request(char *buffer, http_method method, const char *url, const void *body = NULL, int body_size = 0)
{
  HTTPClient client(SSL_CA_PEM, method, url);
  client.set_header("Authorization", authHeaderString);
  client.set_header("Content-Type", "application/json");
  //Serial.printf("Request: %s\n", url);
  switch (method)
  {
  case HTTP_GET:
    rgbLed.setColor(RGB_LED_BRIGHTNESS / 2, RGB_LED_BRIGHTNESS / 2, 0);
    break;

  case HTTP_POST:
    rgbLed.setColor(0, RGB_LED_BRIGHTNESS, 0);
    break;

  case HTTP_PUT:
    rgbLed.setColor(RGB_LED_BRIGHTNESS, 0, 0);
    break;

  default:
    break;
  }
  const Http_Response *result = client.send(body, body_size);
  if (result == NULL)
  {
    Serial.print("Error Code: ");
    Serial.println(client.get_error());
  }
  else
  {
    sprintf(buffer, "%s", result->body);
  }
  rgbLed.turnOff();
}

void start_entry()
{
  state = 2;
  write_only_line(0, "Starting...");
  Serial.println("Starting new entry");

  time_t now;
  time(&now);
  char timeBuffer[21];
  strftime(timeBuffer, sizeof timeBuffer, "%FT%TZ", gmtime(&now));

  const size_t capacity = JSON_OBJECT_SIZE(1) + JSON_OBJECT_SIZE(4);
  DynamicJsonBuffer jsonBuffer(capacity);
  JsonObject &root = jsonBuffer.createObject();
  JsonObject &time_entry = root.createNestedObject("time_entry");
  time_entry["description"] = TOGGLDESCRIPTION;
  time_entry["pid"] = TOGGLPID;
  time_entry["created_with"] = "toggltoggle";
  time_entry["start"] = timeBuffer;

  //root.printTo(Serial);

  String body;
  root.printTo(body);

  char result[500];
  http_request(result, HTTP_POST, TOGGL_START_URI, body.c_str(), body.length());
  if (strlen(result) > 0)
  {
    Serial.println("Started");
  }
}

void stop_entry()
{
  state = 2;
  write_only_line(0, "Stopping...");
  Serial.println("Stopping");

  char requestURI[70];
  sprintf(requestURI, "%s%s/stop", TOGGL_BASE_URI, currentEntryID);
  char result[500];
  http_request(result, HTTP_PUT, requestURI);
  if (strlen(result) > 0)
  {
    Serial.println("Stopped");
  }
}

void get_day_total(uint32_t current_duration)
{
  char sinceTimeBuffer[11];
  time_t now = time(NULL);
  struct tm tmNow;
  tmNow = *gmtime(&now);

  // Add the UTC offset
  date_plus_hours(&tmNow, UTCOFFSET);

  strftime(sinceTimeBuffer, sizeof(sinceTimeBuffer), "%Y-%m-%d", &tmNow);

  char requestURI[120];
  sprintf(requestURI, "%s?user_agent=toggltoggle&workspace_id=%s&since=%s", TOGGL_SUMM_URI, TOGGLWID, sinceTimeBuffer);

  char result[500];
  http_request(result, HTTP_GET, requestURI);
  if (strlen(result) > 0)
  {
    const size_t capacity = 4 * JSON_ARRAY_SIZE(1) + JSON_OBJECT_SIZE(1) + 2 * JSON_OBJECT_SIZE(2) + 2 * JSON_OBJECT_SIZE(4) + 2 * JSON_OBJECT_SIZE(5) + 360;
    DynamicJsonBuffer jsonBuffer(capacity);
    //const char *json = result->body;
    JsonObject &root = jsonBuffer.parseObject(result);
    uint32_t totalGrand = root["total_grand"];
    uint32_t dailyTotal = (totalGrand / 1000) + current_duration;

    struct tm tmTotal;
    convert_seconds_to_tm(&tmTotal, dailyTotal);

    strftime(totalTimeBuffer, sizeof(totalTimeBuffer), "%T", &tmTotal);
    //Serial.println(totalTimeBuffer);
  }
}

void get_current_duration()
{
  char result[500];
  http_request(result, HTTP_GET, TOGGL_CURRENT_URI);

  if (strlen(result) > 0)
  {
    const size_t capacity = JSON_OBJECT_SIZE(1) + JSON_OBJECT_SIZE(11) + 270;
    DynamicJsonBuffer jsonBuffer(capacity);
    //const char *json = result->body;
    JsonObject &root = jsonBuffer.parseObject(result);
    if (!root.success())
    {
      Serial.println("parseObject() failed");
    }
    int32_t data_duration = root["data"]["duration"];
    uint32_t data_id = root["data"]["id"];
    sprintf(currentEntryID, "%lu", data_id);

    //Serial.printf("%d\n", data_duration);
    if (data_duration < 0)
    {
      update_state(1);

      time_t currentSeconds;
      currentSeconds = time(NULL);
      uint32_t duration = currentSeconds + data_duration;

      get_day_total(duration);
      struct tm tmDuration;
      convert_seconds_to_tm(&tmDuration, duration);

      strftime(runningTimeBuffer, sizeof(runningTimeBuffer), "%T", &tmDuration);
      //Serial.println(runningTimeBuffer);
    }
    else
    {
      update_state(0);
      get_day_total(0);
    }
  }
}

void button_isr()
{
  buttonPressed = true;
  digitalWrite(LED_USER, HIGH);
}

void setup()
{
  Serial.begin(115200);
  Screen.init();
  hasWiFi = false;
  init_wifi();

  // Create basic auth header
  rbase64.encode(TOGGLAUTHSTRING);
  char *b64enc = rbase64.result();
  sprintf(authHeaderString, "Basic %s", b64enc);

  if (hasWiFi)
  {
    Screen.print(0, "Initialising...");
    get_current_duration();
    write_main_display();
  }

  // Turn off the annoying WIFI LED
  pinMode(LED_WIFI, OUTPUT);
  digitalWrite(LED_WIFI, LOW);

  pinMode(LED_USER, OUTPUT);
  digitalWrite(LED_USER, LOW);

  // Button setup
  pinMode(USER_BUTTON_A, INPUT);
  attachInterrupt(USER_BUTTON_A, button_isr, FALLING);
}

void loop()
{

  //HttpsClient *client;
  if (buttonPressed)
  {
    buttonPressed = false;
    digitalWrite(LED_USER, LOW);
    switch (state)
    {
    case 0:
      start_entry();
      break;
    case 1:
      stop_entry();
      break;
    default:
      break;
    }
    get_current_duration();
    write_main_display();
    checkIntervalMs = SystemTickCounterRead();
  }

  if ((uint32_t)SystemTickCounterRead() - checkIntervalMs >= interval)
  {
    get_current_duration();
    write_main_display();
    checkIntervalMs = SystemTickCounterRead();
  }
}