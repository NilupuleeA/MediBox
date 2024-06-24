// Include libraries
#include <Wire.h>
#include <WiFi.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHTesp.h>

// Define OLED parameters
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

// Define pin assignments
#define BUZZER 5
#define LED_1 15  
#define LED_2 2   
#define PB_CANCEL 34
#define PB_OK 32
#define PB_UP 33
#define PB_DOWN 35
#define DHTPIN 12

// Real-time clock parameters
const char* NTP_SERVER = "pool.ntp.org";
const int UTC_OFFSET_DST = 0;
int UTC_OFFSET = 0;

// Declare objects
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
DHTesp dhtSensor;

// Time-related variables
int days = 0;
int hours = 0;
int minutes = 0;
int seconds = 0;

// Alarm-related variables
bool alarm_enabled = true;
int n_alarms =3;
int alarm_hours[] = {0,15, 13};
int alarm_minutes[] = {1, 10, 55};
bool alarm_triggered[] = {false, false, false};

// Alarm tone frequencies
int n_notes =8;
int C = 262;
int D = 294;
int E = 330;
int F = 349;
int G = 392;
int A = 440;
int B = 494;
int C_H = 523;
int notes[] = {C, D, E, F, G, A, B, C_H};

// Running modes
int current_mode = 0;
int max_modes = 5;
String modes[] = {"1 - Set Time", "2 - Set Alarm 1", "3 - Set Alarm 2", "4 - Set Alarm 3", "5 - Disable Alarms"};

void setup(){
  pinMode(BUZZER, OUTPUT);   
  pinMode(LED_1, OUTPUT);   // LED to indicate alarm activation
  pinMode(LED_2, OUTPUT);   // LED to indicate non-optimal temperature or humidity levels

  pinMode(PB_CANCEL, INPUT);
  pinMode(PB_OK, INPUT);
  pinMode(PB_UP, INPUT);
  pinMode(PB_DOWN, INPUT);

  // Set up DHT sensor
  dhtSensor.setup(DHTPIN, DHTesp::DHT22);

  // Initialize SSD1306 display with internal voltage generation
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)){
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }
  display.display();   // Display buffer on the screen
  delay(500);

  // Connect to WiFi 
  WiFi.begin("Wokwi-GUEST", "", 6);
  while (WiFi.status() != WL_CONNECTED) {
    delay(250);
    display.clearDisplay();
    print_line("Connecting to WIFI", 0, 0, 2);
  }
  display.clearDisplay();
  print_line("Connected to WIFI", 0, 0, 2);   //Indicate successful WiFi connection

  // Configure time synchronization with NTP server
  configTime(UTC_OFFSET, UTC_OFFSET_DST, NTP_SERVER);

  // Display welcome message on the screen
  display.clearDisplay();
  print_line("Welcome to Medibox!", 10, 20, 2);
  delay(1000);
  display.clearDisplay();
}

void loop(){
  update_time_with_check_alarm();

  if (digitalRead((PB_OK)) == LOW){
    delay(200);
    go_to_menu();
  }  

  check_temp();
}

// Function to print a line of text on the OLED display
void print_line(String text, int column, int row, int text_size){
  display.setTextSize(text_size);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(column, row);
  display.println(text);
  display.display();
}

// Function to add a leading zero to a single-digit number
String AddLeadingZero(uint8_t x){
  String AddLeadingZeroText;
  if(x<10) AddLeadingZeroText="0"; 
  else AddLeadingZeroText=""; 
  AddLeadingZeroText = AddLeadingZeroText + x;
  return AddLeadingZeroText;
}

// Function to print the current time on the OLED display
void print_time_now(void){
  //display.clearDisplay();   // Added this line later in the code
  print_line(AddLeadingZero(days), 0, 0, 2);
  print_line(":", 20, 0, 2);
  print_line(AddLeadingZero(hours), 30, 0, 2);
  print_line(":", 50, 0, 2);
  print_line(AddLeadingZero(minutes), 60, 0, 2);
  print_line(":", 80, 0, 2);
  print_line(AddLeadingZero(seconds), 90, 0, 2);
}

// Function to update time variables from NTP server
void update_time(){
  struct tm timeinfo;
  getLocalTime(&timeinfo);

  char timeHour[3];
  strftime(timeHour, 3, "%H", &timeinfo);
  hours = atoi(timeHour);

  char timeMinute[3];
  strftime(timeMinute, 3, "%M", &timeinfo);
  minutes = atoi(timeMinute);

  char timeSecond[3];
  strftime(timeSecond, 3, "%S", &timeinfo);
  seconds = atoi(timeSecond);

  char timeDay[3];
  strftime(timeDay, 3, "%d", &timeinfo);
  days = atoi(timeDay);
}

// Function to ring the alarm, activate LED, and play the alarm tone
void ring_alarm(){
  display.clearDisplay();
  print_line("MEDICINE TIME!", 0, 0, 2);

  digitalWrite(LED_1, HIGH);

  bool break_happened = false;

  while (digitalRead(PB_CANCEL) == HIGH){
    for (int i=0; i<n_notes; i++){
      if (digitalRead(PB_CANCEL) == LOW){   // Terminate if cancel button is pressed
        //delay(200);  // Commented as otherwise the alarm starts ringing again
        break_happened = true;
        break;
      }
      tone(BUZZER, notes[i]);
      delay(500);
      noTone(BUZZER);
      delay(2);
    }
  }
  digitalWrite(LED_1, LOW);
  display.clearDisplay();
}

// Function to update time and check if any alarm needs to be triggered
void update_time_with_check_alarm(void){
  update_time();
  print_time_now();

  if (alarm_enabled == true){
    for (int i=0; i<n_alarms; i++){
      if (alarm_triggered[i] == false && alarm_hours[i] == hours && alarm_minutes[i] == minutes){
        ring_alarm();
        alarm_triggered[i] = true;
      }
    }
  } 
}

// Function to wait for a button press and return the pressed button
int wait_for_button_press(){
  while (true){
    if (digitalRead(PB_UP) == LOW){
      delay(200);
      return PB_UP;
    }
    else if (digitalRead(PB_DOWN) == LOW){
      delay(200);
      return PB_DOWN;
    }
    else if (digitalRead(PB_OK) == LOW){
      delay(200);
      return PB_OK;
    }
    else if (digitalRead(PB_CANCEL) == LOW){
      delay(200);
      return PB_CANCEL;
    }
    update_time();
  }
}

// Function to navigate through different modes using buttons
void go_to_menu(){
  while (digitalRead(PB_CANCEL) == HIGH){
    display.clearDisplay();
    print_line(modes[current_mode], 0, 0, 2);

    int pressed = wait_for_button_press();

    if (pressed == PB_UP){
      delay(200);
      current_mode += 1;
      current_mode %= max_modes;
    }
    else if (pressed == PB_DOWN){
      delay(200);
      current_mode -= 1;
      if (current_mode<0){
        current_mode = max_modes-1;
      }
    }
    else if (pressed == PB_OK){
      delay(200);
      Serial.print(current_mode);
      run_mode(current_mode);
    }
    else if (pressed == PB_CANCEL){
      delay(200);
      break;
    }
  }
}

// Function to set the local time offset
void set_time(){
  int temp_offset = UTC_OFFSET;   // Store the current UTC offset
  int temp_hour = temp_offset/3600;   // Calculate the hours part of the current offset
  int temp_minute = (temp_offset-3600*temp_hour)/60;   // Calculate the remaining minutes in the current offset
  
  // Loop to adjust the hour setting
  while (true){
    display.clearDisplay();
    print_line("Enter hour: " + String(temp_hour), 0, 0, 2);

    int pressed = wait_for_button_press();

    if (pressed == PB_UP){
      delay(200);
      temp_hour += 1;
      temp_hour %= 24;
      UTC_OFFSET = temp_hour * 3600;
    }
    else if (pressed == PB_DOWN){
      delay(200);
      temp_hour -= 1;
      temp_hour = (temp_hour + 24) % 24;  // Ensure non-negative value
      UTC_OFFSET = temp_hour * 3600;
    }
    else if (pressed == PB_OK){   // Apply changes only if the user presses the OK button
      delay(200);
      break;
    }
    else if (pressed == PB_CANCEL){   // Keep the previous value if the user presses the CANCEL button
      UTC_OFFSET = temp_offset;
      delay(200);
      break;
    }
  }

  int initial_offset = UTC_OFFSET;   // Store the updated UTC offset after adding the hours offset

  // Loop to adjust the minute setting
  while (true){
    display.clearDisplay();
    print_line("Enter minute: " + String(temp_minute), 0, 0, 2);

    int pressed = wait_for_button_press();
    if (pressed == PB_UP){
      delay(200);
      temp_minute += 1;
      temp_minute %= 60;
      UTC_OFFSET = temp_hour * 3600 + temp_minute * 60;
    }
    else if (pressed == PB_DOWN){
      delay(200);
      temp_minute -= 1;
      temp_minute = (temp_minute + 60) % 60;  // Ensure non-negative value
      UTC_OFFSET = temp_hour * 3600 + temp_minute * 60;
    }
    else if (pressed == PB_OK){   // Apply changes only if the user presses the OK button
      delay(200);
      display.clearDisplay();
      print_line("Time offset is set", 0, 0, 2);
      delay(1000);
      break;
    }
    else if (pressed == PB_CANCEL){   // Keep the previous value if the user presses the CANCEL button
      UTC_OFFSET = initial_offset;
      delay(200);
      break;
    }
  }

  configTime(UTC_OFFSET, UTC_OFFSET_DST, NTP_SERVER);
}


// Function to set an alarm based on user input
void set_alarm(int alarm){
  int temp_hour = alarm_hours[alarm];

  while (true){
    display.clearDisplay();
    print_line("Enter hour: " + String(temp_hour), 0, 0, 2);

    int pressed = wait_for_button_press();
    if (pressed == PB_UP){
      delay(200);
      temp_hour += 1;
      temp_hour %= 24;
    }
    else if (pressed == PB_DOWN){
      delay(200);
      temp_hour -= 1;
      if (temp_hour<0){
        temp_hour = 23;
      }
    }
    else if (pressed == PB_OK){
      delay(200);
      alarm_enabled = true;
      alarm_hours[alarm] = temp_hour;
      break;
    }
    else if (pressed == PB_CANCEL){
      delay(200);
      break;
    }
  }

  int temp_minute = alarm_minutes[alarm];

  while (true){
    display.clearDisplay();
    print_line("Enter minute: " + String(temp_minute), 0, 0, 2);

    int pressed = wait_for_button_press();
    if (pressed == PB_UP){
      delay(200);
      temp_minute += 1;
      temp_minute %= 60;
    }
    else if (pressed == PB_DOWN){
      delay(200);
      temp_minute -= 1;
      if (temp_minute<0){
        temp_minute = 59;
      }
    }
    else if (pressed == PB_OK){
      delay(200);
      display.clearDisplay();
      print_line("Alarm is set", 0, 0, 2);
      delay(1000);
      alarm_enabled = true;
      alarm_minutes[alarm] = temp_minute;
      break;
    }
    else if (pressed == PB_CANCEL){
      delay(200);
      break;
    }
  }
} 

// Function to handle different modes based on user input
void run_mode(int mode){
  if (mode == 0){
    set_time();
  }
  else if (mode == 1 || mode == 2 || mode == 3){
    set_alarm(mode - 1);
  }
  else if (mode == 4){
    alarm_enabled = false;
  }
}

// Function to print temperature and humidity information on the display
void print_temp_hum(String text, int column, int row){
  display.fillRect(0, 28, 128, 64, SSD1306_WHITE);
  display.setTextSize(1);
  display.setTextColor(SSD1306_BLACK);
  display.setCursor(column, row);
  display.println(text);
  display.display();
}

// Function to monitor non-optimal temperature and humidity levels
void check_temp(){
  TempAndHumidity data = dhtSensor.getTempAndHumidity();
  display.clearDisplay();

  if (80 >= data.humidity && data.humidity >= 60 && 32 >= data.temperature && data.temperature >= 26) {
    digitalWrite(LED_2, LOW);
  } 
  else {
    digitalWrite(LED_2, HIGH);
  }
  if (data.temperature > 32){
    print_temp_hum("TEMP_HIGH", 10, 38);
  }
  else if (data.temperature < 26){
    print_temp_hum("TEMP_LOW", 10, 38);
  }
  if (data.humidity > 80){
    print_temp_hum("HUMIDITY_HIGH", 40, 54);
  }
  else if (data.humidity < 60){
    print_temp_hum("HUMIDITY_LOW", 40, 54);
  }
}

