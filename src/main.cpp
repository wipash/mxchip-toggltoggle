#include <Arduino.h>
#include "ArduinoJson.h"
#include "AZ3166WiFi.h"
#include "config.h"
#include "time.h"
#include "http_client.h"
#include "OledDisplay.h"
#include "rBase64.h"
#include "RGB_LED.h"
#include "SystemTickCounter.h"
#include "utility.h"

static RGB_LED rgbLed;

static bool hasWiFi;

static int status = WL_IDLE_STATUS;

static int duration;

static int state = 2;
// 0 = stopped
// 1 = running
// 2 = updating

static int lastButtonAState;
static int buttonAState;

static uint64_t check_interval_ms;

const char togglBaseURI[] = "https://www.toggl.com/api/v8/time_entries/";
const char togglCurrentURI[] = "https://www.toggl.com/api/v8/time_entries/current";
const char togglStartURI[] = "https://www.toggl.com/api/v8/time_entries/start";
const char togglSummURI[] = "https://toggl.com/reports/api/v2/summary";

char auth[150];
char togglId[10];

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

void InitWifi()
{
  Screen.print(0, "Connecting WiFi");
  if (WiFi.begin(WIFISSID, WIFIPWD) == WL_CONNECTED)
  {
    IPAddress ip = WiFi.localIP();
    Screen.clean();
    Screen.print(3, ip.get_address());
    hasWiFi = true;
  }
  else
  {
    Screen.clean();
    Screen.print(1, "No Wi-Fi");
  }
}

void update_state(int newstate)
{
  if (state != newstate)
  {
    state = newstate;
    Screen.clean();
  }
}

struct tm convert_seconds_to_tm(long seconds)
{
  int hh, mm, ss;
  hh = seconds / 3600;

  seconds %= 3600;

  mm = seconds / 60;

  seconds %= 60;

  ss = seconds;

  struct tm tm;
  tm.tm_year = 2000 - 1900;
  tm.tm_mon = 1 - 1;
  tm.tm_mday = 1;
  tm.tm_hour = hh;
  tm.tm_min = mm;
  tm.tm_sec = ss;
  tm.tm_isdst = 0;

  return tm;
}

void start_entry()
{
  state = 2;
  HTTPClient client(SSL_CA_PEM, HTTP_POST, togglStartURI);
  client.set_header("Authorization", auth);
  client.set_header("Content-Type", "application/json");

  Screen.clean();
  Screen.print(0, "Starting");
  Serial.println("Starting new entry");

  time_t now;
  time(&now);
  char buf[sizeof "2011-10-08T07:07:09Z"];
  strftime(buf, sizeof buf, "%FT%TZ", gmtime(&now));

  const size_t capacity = JSON_OBJECT_SIZE(1) + JSON_OBJECT_SIZE(4);
  DynamicJsonBuffer jsonBuffer(capacity);
  JsonObject &root = jsonBuffer.createObject();
  JsonObject &time_entry = root.createNestedObject("time_entry");
  time_entry["description"] = TOGGLDESCRIPTION;
  time_entry["pid"] = TOGGLPID;
  time_entry["created_with"] = "toggltoggle";
  time_entry["start"] = buf;

  root.printTo(Serial);
  
  String body;
  root.printTo(body);

  const Http_Response *result = client.send(body.c_str(), body.length());
  if (result == NULL)
  {
    Serial.print("Error Code: ");
    Serial.println(client.get_error());
    Screen.print(1, "Failed");
  }
  else
  {
    Serial.println("Started");
    Serial.println(result->body);
    Screen.print(0, "Started");
  }
}

void stop_entry()
{
  state = 2;
  char requestURI[70];
  sprintf(requestURI, "%s%s/stop", togglBaseURI, togglId);
  HTTPClient client(SSL_CA_PEM, HTTP_PUT, requestURI);
  client.set_header("Authorization", auth);
  client.set_header("Content-Type", "application/json");
  Screen.clean();
  Screen.print(0, "Stopping");
  Serial.println("Stopping");
  Serial.println(requestURI);

  const Http_Response *result = client.send(NULL, 0);
  if (result == NULL)
  {
    Serial.print("Error Code: ");
    Serial.println(client.get_error());
  }
  else
  {
    Serial.println(result->body);
    Serial.println("Stopped");
  }
}

void DatePlusHours(struct tm* date, int hours)
{
  const time_t ONE_HOUR = 60*60;
  time_t date_seconds = mktime(date) + (hours * ONE_HOUR);
  *date = *localtime( &date_seconds);
}

void get_day_total(long current_duration)
{
  char time_buffer[11];
  time_t now = time(NULL);
  struct tm tm;
  tm = *gmtime(&now);

  // Add the UTC offset
  DatePlusHours( &tm, UTCOFFSET);

  strftime(time_buffer, sizeof(time_buffer), "%Y-%m-%d", &tm);

  Serial.println("Current duration");
  delay(100);
  char requestURI[120];
  sprintf(requestURI, "%s?user_agent=toggltoggle&workspace_id=%s&since=%s", togglSummURI, TOGGLWID, time_buffer);
  HTTPClient client(SSL_CA_PEM, HTTP_GET, requestURI);
  client.set_header("Authorization", auth);
  client.set_header("Content-Type", "application/json");
  Serial.println(requestURI);

  const Http_Response *result = client.send(NULL, 0);
  if (result == NULL)
  {
    Serial.print("Error Code: ");
    Serial.println(client.get_error());
  }
  else
  {
    Serial.println(result->body);
    const size_t capacity = 4 * JSON_ARRAY_SIZE(1) + JSON_OBJECT_SIZE(1) + 2 * JSON_OBJECT_SIZE(2) + 2 * JSON_OBJECT_SIZE(4) + 2 * JSON_OBJECT_SIZE(5) + 360;
    DynamicJsonBuffer jsonBuffer(capacity);
    const char *json = result->body;
    JsonObject &root = jsonBuffer.parseObject(json);
    long total_grand = root["total_grand"];
    long daily_total = (total_grand / 1000) + current_duration;

    struct tm tm = convert_seconds_to_tm(daily_total);

    char time_buffer[10];
    strftime(time_buffer, sizeof(time_buffer), "%T", &tm);
    Serial.println(time_buffer);
    Screen.print(2, "Today's Total");
    Screen.print(3, time_buffer);
  }
}

void get_current_duration()
{
  rgbLed.setColor(RGB_LED_BRIGHTNESS, 0, 0);
  HTTPClient client(SSL_CA_PEM, HTTP_GET, togglCurrentURI);
  client.set_header("Authorization", auth);
  client.set_header("Content-Type", "application/json");
  Serial.print("Requesting ");
  Serial.println(togglCurrentURI);
  rgbLed.setColor(RGB_LED_BRIGHTNESS, RGB_LED_BRIGHTNESS, 0);
  const Http_Response *result = client.send(NULL, 0);
  rgbLed.setColor(RGB_LED_BRIGHTNESS, 0, 0);

  if (result == NULL)
  {
    Serial.print("Error Code: ");
    Serial.println(client.get_error());
  }
  else
  {
    Serial.println(result->body);

    const size_t capacity = JSON_OBJECT_SIZE(1) + JSON_OBJECT_SIZE(11) + 270;
    DynamicJsonBuffer jsonBuffer(capacity);
    const char *json = result->body;
    rgbLed.setColor(RGB_LED_BRIGHTNESS, 0, RGB_LED_BRIGHTNESS);
    JsonObject &root = jsonBuffer.parseObject(json);
    rgbLed.setColor(RGB_LED_BRIGHTNESS, 0, 0);
    if (!root.success())
    {
      Serial.println("parseObject() failed");
    }
    long data_duration = root["data"]["duration"];
    long data_id = root["data"]["id"];
    sprintf(togglId, "%lu", data_id);

    Serial.println(data_duration);
    if (data_duration < 0)
    {
      update_state(1);

      time_t currentseconds;
      currentseconds = time(NULL);
      long duration = currentseconds + data_duration;

      get_day_total(duration);
      struct tm tm = convert_seconds_to_tm(duration);

      char time_buffer[10];
      strftime(time_buffer, sizeof(time_buffer), "%T", &tm);
      Serial.println(time_buffer);

      Screen.print(0, "Running");
      Screen.print(1, time_buffer);
      rgbLed.turnOff();
    }
    else
    {
      update_state(0);
      get_day_total(0);
      Screen.print(0, "Stopped");
      rgbLed.turnOff();
    }
  }
}

void setup()
{
  Serial.begin(115200);
  Screen.init();
  hasWiFi = false;
  InitWifi();

  // Create basic auth header
  rbase64.encode(TOGGLAUTHSTRING);
  char *b64enc = rbase64.result();
  sprintf(auth, "Basic %s", b64enc);

  // Button setup
  pinMode(USER_BUTTON_A, INPUT);
  lastButtonAState = digitalRead(USER_BUTTON_A);

  if (hasWiFi)
  {
    delay(2000);
    Screen.clean();
    get_current_duration();
  }

  // Turn off the annouing WIFI LED
  pinMode(LED_WIFI, OUTPUT);
  digitalWrite(LED_WIFI, LOW);
}

void loop()
{

  buttonAState = digitalRead(USER_BUTTON_A);
  if (buttonAState == LOW && lastButtonAState == HIGH)
  {
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
    delay(2000);
  }

  if ((int)SystemTickCounterRead() - check_interval_ms >= getInterval())
  {
    get_current_duration();
    check_interval_ms = SystemTickCounterRead();
  }
}