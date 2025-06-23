
#include "esp_camera.h"
#include <WiFi.h>
#include "esp_timer.h"
#include "img_converters.h"
#include "Arduino.h"
#include "fb_gfx.h"
#include "soc/soc.h"           // disable brownout problems
#include "soc/rtc_cntl_reg.h"  // disable brownout problems
#include "esp_http_server.h"
#include "htmlcode.h"
// #include <driver/ledc.h>
#include <dht11.h>
#define DHT11PIN 4


#define MOTOR_1_PIN_1 12
#define MOTOR_1_PIN_2 13
#define ENCODER_PIN_A 14
#define ENCODER_PIN_B 15
#define LED 2
// #define PWM_PIN 2
dht11 DHT11;
volatile long encoderCount = 0;
float Hum = 0;
float Temp = 0;
float distance = 0.0;                     // Distance in meters
const float dia = 0.07;                   //mtr = 7cm
const float wheelCircumference = 0.2199;  //3.14159 * 0.07 =     //Adjust based on your wheel size (in meters)
// Replace with your network credentials
const char *ssid = "BOT";
const char *password = "123456789";

#define SERVOMIN 125  // this is the 'minimum' pulse length count (out of 4096)
#define SERVOMAX 525  // this is the 'maximum' pulse length count (out of 4096)

#define PART_BOUNDARY "123456789000000000000987654321"
#define CAMERA_MODEL_AI_THINKER
#if defined(CAMERA_MODEL_AI_THINKER)
#define PWDN_GPIO_NUM 32
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM 0
#define SIOD_GPIO_NUM 26
#define SIOC_GPIO_NUM 27

#define Y9_GPIO_NUM 35
#define Y8_GPIO_NUM 34
#define Y7_GPIO_NUM 39
#define Y6_GPIO_NUM 36
#define Y5_GPIO_NUM 21
#define Y4_GPIO_NUM 19
#define Y3_GPIO_NUM 18
#define Y2_GPIO_NUM 5
#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM 23
#define PCLK_GPIO_NUM 22

#endif

static const char *_STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char *_STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char *_STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

httpd_handle_t camera_httpd = NULL;
httpd_handle_t stream_httpd = NULL;

// PWM properties
const int ledPin = 16;  // GPIO pin connected to the motor driver's enable pin
const int freq = 30000;
const int pwmChannel = 0;
const int pwmresolution = 8;
int dutyCycle = 200;

static esp_err_t distance_handler(httpd_req_t *req) {
  char response[50];
  sprintf(response, "{\"distance\": %.2f}", distance);
  httpd_resp_set_type(req, "application/json");
  return httpd_resp_send(req, response, strlen(response));
}

httpd_uri_t distance_uri = {
  .uri = "/distance",
  .method = HTTP_GET,
  .handler = distance_handler,
  .user_ctx = NULL
};

static esp_err_t Hum_handler(httpd_req_t *req) {
  char response[50];
  sprintf(response, "{\"Hum\": %.2f}", Hum);
  httpd_resp_set_type(req, "application/json");
  return httpd_resp_send(req, response, strlen(response));
}

httpd_uri_t Hum_uri = {
  .uri = "/Hum",
  .method = HTTP_GET,
  .handler = Hum_handler,
  .user_ctx = NULL
};

static esp_err_t Temp_handler(httpd_req_t *req) {
  char response[50];
  sprintf(response, "{\"Temp\": %.2f}", Temp);
  httpd_resp_set_type(req, "application/json");
  return httpd_resp_send(req, response, strlen(response));
}

httpd_uri_t Temp_uri = {
  .uri = "/Temp",
  .method = HTTP_GET,
  .handler = Temp_handler,
  .user_ctx = NULL
};





static esp_err_t index_handler(httpd_req_t *req) {
  httpd_resp_set_type(req, "text/html");
  return httpd_resp_send(req, (const char *)INDEX_HTML, strlen(INDEX_HTML));
}

static esp_err_t stream_handler(httpd_req_t *req) {
  camera_fb_t *fb = NULL;
  esp_err_t res = ESP_OK;
  size_t _jpg_buf_len = 0;
  uint8_t *_jpg_buf = NULL;
  char *part_buf[64];

  res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
  if (res != ESP_OK) {
    return res;
  }

  while (true) {
    fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Camera capture failed");
      res = ESP_FAIL;
    } else {
      if (fb->width > 200) {
        if (fb->format != PIXFORMAT_JPEG) {
          bool jpeg_converted = frame2jpg(fb, 80, &_jpg_buf, &_jpg_buf_len);
          esp_camera_fb_return(fb);
          fb = NULL;
          if (!jpeg_converted) {
            Serial.println("JPEG compression failed");
            res = ESP_FAIL;
          }
        } else {
          _jpg_buf_len = fb->len;
          _jpg_buf = fb->buf;
        }
      }
    }
    if (res == ESP_OK) {
      size_t hlen = snprintf((char *)part_buf, 64, _STREAM_PART, _jpg_buf_len);
      res = httpd_resp_send_chunk(req, (const char *)part_buf, hlen);
    }
    if (res == ESP_OK) {
      res = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);
    }
    if (res == ESP_OK) {
      res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
    }
    if (fb) {
      esp_camera_fb_return(fb);
      fb = NULL;
      _jpg_buf = NULL;
    } else if (_jpg_buf) {
      free(_jpg_buf);
      _jpg_buf = NULL;
    }
    if (res != ESP_OK) {
      break;
    }
    //Serial.printf("MJPG: %uB\n",(uint32_t)(_jpg_buf_len));
  }
  return res;
}

static esp_err_t cmd_handler(httpd_req_t *req) {
  char *buf;
  size_t buf_len;
  //String variable;
  char variable[32] = {
    0,
  };

  buf_len = httpd_req_get_url_query_len(req) + 1;
  if (buf_len > 1) {
    buf = (char *)malloc(buf_len);
    if (!buf) {
      httpd_resp_send_500(req);
      return ESP_FAIL;
    }
    if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
      if (httpd_query_key_value(buf, "value", variable, sizeof(variable)) == ESP_OK) {
      } else {
        free(buf);
        httpd_resp_send_404(req);
        return ESP_FAIL;
      }
    } else {
      free(buf);
      httpd_resp_send_404(req);
      return ESP_FAIL;
    }
    free(buf);
  } else {
    httpd_resp_send_404(req);
    return ESP_FAIL;
  }

  sensor_t *s = esp_camera_sensor_get();
  int res = 0;
  //need to convert char * to string.
  String variable2 = String(variable);
  // Serial.print("variable : ");
  // Serial.println(variable);
  if (variable[0] == 's') {  // it is for Motor
    Serial.println(variable);
  } else if (!strcmp(variable, "F")) {
    Serial.println("Forward");
    digitalWrite(MOTOR_1_PIN_1, 1);
    digitalWrite(MOTOR_1_PIN_2, 0);

  } else if (!strcmp(variable, "R")) {
    Serial.println("Backward");
    digitalWrite(MOTOR_1_PIN_1, 0);
    digitalWrite(MOTOR_1_PIN_2, 1);

  } else if (!strcmp(variable, "S")) {
    Serial.println("Stop");
    digitalWrite(MOTOR_1_PIN_1, 0);
    digitalWrite(MOTOR_1_PIN_2, 0);
  } else {
    // if (variable.length() >= 3) {
    String str2 = variable;
    String str1 = str2.substring(0, 3);
    Serial.println("spd :" + str1);
    // if (str1.toInt() == 255)
    //   digitalWrite(PWM_PIN, HIGH);
    // else
    //   analogWrite(PWM_PIN, str1.toInt());
  }

  if (res) {
    return httpd_resp_send_500(req);
  }

  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  return httpd_resp_send(req, NULL, 0);
}

void startCameraServer() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = 80;
  httpd_uri_t index_uri = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = index_handler,
    .user_ctx = NULL
  };

  httpd_uri_t cmd_uri = {
    .uri = "/slider",
    .method = HTTP_GET,
    .handler = cmd_handler,
    .user_ctx = NULL
  };
  httpd_uri_t stream_uri = {
    .uri = "/stream",
    .method = HTTP_GET,
    .handler = stream_handler,
    .user_ctx = NULL
  };
  if (httpd_start(&camera_httpd, &config) == ESP_OK) {
    httpd_register_uri_handler(camera_httpd, &index_uri);
    httpd_register_uri_handler(camera_httpd, &cmd_uri);
    httpd_register_uri_handler(camera_httpd, &distance_uri);
    httpd_register_uri_handler(camera_httpd, &Temp_uri);
    httpd_register_uri_handler(camera_httpd, &Hum_uri);
  }
  config.server_port += 1;
  config.ctrl_port += 1;
  if (httpd_start(&stream_httpd, &config) == ESP_OK) {
    httpd_register_uri_handler(stream_httpd, &stream_uri);
  }
}
// Encoder ISR
void IRAM_ATTR handleEncoderA() {
  if (digitalRead(ENCODER_PIN_A) > digitalRead(ENCODER_PIN_B))
    encoderCount--;
  else
    encoderCount++;

  distance = (encoderCount / 1150.0) * wheelCircumference;  // Assuming 20 ticks per rotation; adjust accordingly
  // if (digitalRead(ENCODER_PIN_A) == HIGH) {
  //   encoderCount++;
  // } else {
  //   encoderCount--;
  // }
  // Serial.println(encoderCount);
}

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);  //disable brownout detector
  Serial.begin(115200);

  Serial.begin(115200);
  Serial.setDebugOutput(false);
  pinMode(MOTOR_1_PIN_1, OUTPUT);
  pinMode(MOTOR_1_PIN_2, OUTPUT);
  pinMode(ENCODER_PIN_A, INPUT);
  pinMode(ENCODER_PIN_B, INPUT);
  pinMode(LED,OUTPUT);
  // pinMode(PWM_PIN, OUTPUT);
  digitalWrite(LED, HIGH);





  // Configure LEDC
  // ledcAttachChannel(ledPin, freq, pwmresolution, pwmChannel);
  // ledcAttachPin(ledPin, channel);

  attachInterrupt(digitalPinToInterrupt(ENCODER_PIN_A), handleEncoderA, RISING);

  // attachInterrupt(digitalPinToInterrupt(ENCODER_PIN_A), handleEncoderA, CHANGE);

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  if (psramFound()) {
    config.frame_size = FRAMESIZE_VGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_QVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }

  // Camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }
  // Wi-Fi connection
  WiFi.softAP(ssid, password);

  // WiFi.begin(ssid, password);
  // while (WiFi.status() != WL_CONNECTED) {
  //   delay(500);
  //   Serial.print(".");
  // }
  // Serial.println("");
  // Serial.println("WiFi connected");
  Serial.print("Connect to \n SSID : ");
  Serial.print(ssid);

  Serial.print("\npass: ");
  Serial.println(password);
  Serial.print("Camera Ready! Use 'http://192.168.4.1/");

  // Serial.print("Camera Stream Ready! Go to: http://");
  // Serial.println(WiFi.localIP());



  // Start streaming web server
  startCameraServer();


  delay(1000);
  digitalWrite(LED, LOW);
  // pwm.setPWM(0, 0, angleToPulse(90));
  // pwm.setPWM(4, 0, angleToPulse(70));
  // pwm.setPWM(14, 0, angleToPulse(90));
  // pwm.setPWM(15, 0, angleToPulse(90));
  // for (int i = 45; i<135; i++){
  //   //pwm.setPWM(0, 0, angleToPulse(i));
  //   delay(20);
  // }
}

unsigned long t1 = 2000;
void loop() {
  // Update the distance based on the encoder count
  // if (Serial.available()) {
  //   encoderCount = encoderCount + 50;
  // }

  if (millis() > t1 + 5000) {
  // Serial.println(encoderCount);
    int chk = DHT11.read(DHT11PIN);
    Hum = DHT11.humidity;
    Temp = DHT11.temperature;

    Serial.println("TEMP:" + String(Temp) + " Hum:" + String(Hum));
    t1 = millis();
  }
}
