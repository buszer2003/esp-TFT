#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266Ping.h>
#include <Wire.h>
#include <SPI.h>
#include <TFT_eSPI.h> // Hardware-specific library
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <Adafruit_BME280.h>
#include <TaskScheduler.h>
#include <EEPROM.h>

#define __FIXEDDATE false
const String version	= "1.0.1";
float temperature		= 0.00;
float humidity			= 0.00;
float pressure			= 0.00;
uint8_t maxPage			= 3;
uint16_t color1			= 2031;
uint16_t color2			= 38888;
uint8_t page			= 1;
uint8_t center			= 120;
uint16_t year			= 0;
uint8_t month			= 0;
uint8_t day				= 0;
uint8_t hour			= 0;
uint8_t minute			= 0;
uint8_t second			= 0;
uint8_t old_hour		= 0;
uint8_t old_minute		= 0;
uint8_t old_second		= 0;
uint8_t old_day			= 0;
uint8_t dow				= 0;
uint8_t dateFontSize	= 4;
uint8_t timeOut			= 0;
int key					= 0;
bool flagChange			= false;
bool flagTouch			= false;
bool flagTimeout		= false;
bool flagWifi			= false;
bool flagFirstBoot		= true;

unsigned long lastTouchTime = 0;

uint8_t yearAddr		= 0;
uint8_t monthAddr		= 2;
uint8_t dayAddr			= 3;
uint8_t dowAddr			= 4;
uint8_t hourAddr		= 5;
uint8_t minuteAddr		= 6;
uint8_t secondAddr		= 7;

const char *ssid     = "R&D_2";
const char *password = "1qaz7ujmrd2";

int startDay = 0; // Sunday's value is 0, Saturday is 6
int monthLength = 0;
String shortWeekDays[7] = {
	"S",
	"M",
	"T",
	"W",
	"T",
	"F",
	"S"
};
String weekDays[7] = {
	"Sun",
	"Mon",
	"Tue",
	"Wed",
	"Thu",
	"Fri",
	"Sat"
};
String months[12] = {
	"Jan",
	"Feb",
	"Mar",
	"Apr",
	"May",
	"Jun",
	"Jul",
	"Aug",
	"Sep",
	"Oct",
	"Nov",
	"Dec"
};
uint8_t dayOff[2][12][31] = {
	{// 2023
		{// Jan
			0
		},
		{// Feb
			0
		},
		{// Mar
			0
		},
		{// Apr
			0
		},
		{// May
			0
		},
		{// Jun
			0
		},
		{// Jul
			0
		},
		{// Aug
			0
		},
		{// Sep
			0
		},
		{// Oct
			0
		},
		{// Nov
			0
		},
		{// Dec
			4, 5, 28, 29, 30
		}

	},
	{// 2024
		{// Jan
			4, 1, 2, 13, 27
		},
		{// Feb
			2, 10, 26
		},
		{// Mar
			2, 9, 23
		},
		{// Apr
			6, 8, 12, 13, 15, 16, 17
		},
		{// May
			4, 1, 4, 18, 22
		},
		{// Jun
			3, 1, 15, 29
		},
		{// Jul
			3, 13, 20, 27
		},
		{// Aug
			3, 10, 12, 24
		},
		{// Sep
			2, 7, 21
		},
		{// Oct
			3, 5, 19, 23
		},
		{// Nov
			3, 2, 16, 30
		},
		{// Dec
			4, 5, 28, 30, 31
		}
	}
};

Scheduler runner;
TFT_eSPI tft = TFT_eSPI();       // Invoke custom library
Adafruit_BME280 bme280;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");

void updateNtp();
void readSensor();
void updateTime();
void changePage();
void updateDisplay();
void showTime(uint8_t _type);	// 1 is large, 2 is small
void showDay();
void showDate();
uint8_t startDayOfWeek(uint16_t y, uint8_t m, uint8_t d);
void offlineMode();
void showCalendar();
void showSensor();
bool isDayOff(uint16_t yyyy, uint8_t mm, uint8_t dd);

Task task_updateNtp(100, TASK_FOREVER, &updateNtp);
Task task_readSensor(5000, TASK_FOREVER, &readSensor);
Task task_updateTime(1000, TASK_FOREVER, &updateTime);
Task task_changePage(100, TASK_FOREVER, &changePage);
Task task_updateDisplay(1000, TASK_FOREVER, &updateDisplay);

void setup() {
	EEPROM.begin(16);
	pinMode(A0, INPUT);
	tft.init();
	tft.setRotation(0);
	tft.fillScreen(TFT_BLACK);
	tft.drawCentreString(version, 120, 310, 1);
	uint16_t calData[5] = {358, 3388, 357, 3372, 7};
	tft.setTouch(calData);
	if (analogRead(A0) < 1) {
		offlineMode();
	}
	else {
		String waitingDot;
		tft.println("Connecting to " + String(ssid));
		uint8_t tryCount = 0;
		WiFi.begin(ssid, password);
		while (WiFi.status() != WL_CONNECTED && tryCount++ < 60) {
			delay(500);
			tft.print(String(waitingDot));
			waitingDot += ".";
			if (analogRead(A0) < 1) {
				flagWifi = false;
				offlineMode();
				break;
			}
		}
		if (WiFi.status() == WL_CONNECTED) {
			tft.println(); tft.println("Connected Pinging...");
			IPAddress ip (8, 8, 8, 8);
			bool ret = Ping.ping(ip);
			if (ret == true) flagWifi = true, tft.println("Done");
			else WiFi.disconnect(true), tft.println("Failed");
		}
	}
	if (bme280.begin(0x76)) tft.println("Init BME280 done");
	if (flagWifi == true) {
		tft.println("Init NTP");
		timeClient.begin();
		timeClient.setTimeOffset(25200);
	}
	tft.println("Init Task Scheduler");
	runner.addTask(task_updateNtp);
	runner.addTask(task_readSensor);
	runner.addTask(task_updateTime);
	runner.addTask(task_changePage);
	runner.addTask(task_updateDisplay);
	if (flagWifi == true) task_updateNtp.enable();
	task_readSensor.enable();
	task_updateTime.enable();
	task_changePage.enable();
	task_updateDisplay.enable();
	randomSeed(bme280.readTemperature());
	tft.println("All done!"); delay(1000);

	if (__FIXEDDATE == true) {
		year	= 2024;
		month	= 4;
		day		= 2;
		dow		= startDayOfWeek(year, month, day);
		hour	= 10;
		minute	= 10;
		second	= 10;
	}
}

void loop() {
	runner.execute();
	
	if (page == 9) timeOut = 5;
	else timeOut = 120;
	if (millis() - lastTouchTime > timeOut * 1000 && flagTimeout == false && page != 1) {
		page = 1;
		flagChange = true;
		flagTimeout = true;
		updateDisplay();
	}
	
	if (year > 2000 && flagFirstBoot == true) {
		flagChange = true;
		flagFirstBoot = false;
		updateDisplay();
	}
}

void offlineMode() {
	tft.println("Offline!");
	EEPROM.get(yearAddr, year);
	EEPROM.get(monthAddr, month);
	EEPROM.get(dayAddr, day);
	EEPROM.get(dowAddr, dow);
	EEPROM.get(hourAddr, hour);
	EEPROM.get(minuteAddr, minute);
	EEPROM.get(secondAddr, second);
}

void updateNtp() {
	if (timeClient.update()) {
		task_updateNtp.disable();
		WiFi.disconnect(true);
	}
}

void readSensor() {
	temperature	= bme280.readTemperature();
	humidity	= bme280.readHumidity();
	pressure	= bme280.readPressure();
}

void updateTime() {
	if (__FIXEDDATE == false) {
		if (flagWifi == true) {
			time_t epochTime = timeClient.getEpochTime();
			struct tm *ptm = gmtime ((time_t *)&epochTime);
			year	= (ptm->tm_year+1900);
			month	= ptm->tm_mon+1;
			day		= ptm->tm_mday;
			dow		= timeClient.getDay();
			hour	= timeClient.getHours();
			minute	= timeClient.getMinutes();
			second	= timeClient.getSeconds();
			EEPROM.put(yearAddr, year);
			EEPROM.put(monthAddr, month);
			EEPROM.put(dayAddr, day);
			EEPROM.put(dowAddr, dow);
			EEPROM.put(hourAddr, hour);
			EEPROM.put(minuteAddr, minute);
			EEPROM.put(secondAddr, second);
			EEPROM.commit();
		}
	}

	//if (year < 2000 && flagWifi == true) task_updateNtp.enable();
	
	if (day != old_day) {
		// controller 1 - 7, Default Mon = 1
		// library 0 - 6, Default Sun = 0
		uint16_t divider = 0;
		if		(dow == 0) divider = 999;
		else if (dow == 1) divider = 416;
		else if (dow == 2) divider = 381;
		else if (dow == 3) divider = 441;
		else if (dow == 4) divider = 688;
		else if (dow == 5) divider = 625;
		else if (dow == 6) divider = 21;
		key = (((((year % 2000) + day) * 100) + month) * 10000) / divider;
		key = key%1000;
		old_day = day;
	}
}

void changePage() {
	uint16_t x, y;
	if (tft.getTouch(&x, &y) && flagTouch == false) {
		lastTouchTime = millis();
		flagTouch = true;
		flagChange = true;
		page++;
		if (page > maxPage) page = 1;
		if (page > 1) flagTimeout = false;
		updateDisplay();
	}
	else if (analogRead(A0) < 1 && flagTouch == false && page == 3) {
		lastTouchTime = millis();
		flagTouch = true;
		flagChange = true;
		page = 9;
		updateDisplay();
	}
	else if (!tft.getTouch(&x, &y) || analogRead(A0) > 0) flagTouch = false;
}

void updateDisplay() {
	if (page == 1) {
		if (flagChange == true) {
			tft.fillScreen(TFT_BLACK);
			showTime(1);
			showDay();
			showDate();
			flagChange = false;
		}
		
		if (minute != old_minute) {
			tft.setTextColor(color1, TFT_BLACK);
			showTime(1);
			showDay();
			showDate();
			old_minute = minute;
		}
	}
	else if (page == 2) {
		if (flagChange == true) {
			tft.fillScreen(TFT_BLACK);
			showCalendar();
			showTime(2);
			flagChange = false;
		}
	}
	else if (page == 3) {
		if (flagChange == true) {
			tft.fillScreen(TFT_BLACK);
			flagChange = false;
		}
		showSensor();
		showTime(2);
	}
	else if (page == 9) {
		if (flagChange == true) {
			tft.fillScreen(TFT_BLACK);
			tft.setTextColor(TFT_RED);
			tft.setTextSize(1);
			tft.setTextFont(4);
			tft.drawString("Key: " + String(key), 10, 10);
			showTime(2);
			flagChange = false;
		}
	}
}

void showTime(uint8_t _type) {
	String t_hour, t_minute, t_second;
	t_hour = String(hour/10) + String(hour%10);
	t_minute = String(minute/10) + String(minute%10);
	
	if (_type == 1) {
		tft.setTextSize(1);
		tft.setTextFont(8);
		tft.setTextColor(color1, TFT_BLACK);
		tft.drawCentreString(t_hour, 120, 10, 8);
		tft.drawCentreString(t_minute, 120, 100, 8);
	}
	else if (_type == 2) {
		tft.setTextSize(1);
		tft.setTextFont(4);
		tft.setTextColor(TFT_GREENYELLOW, TFT_BLACK);
		tft.drawCentreString(t_hour + ":" + t_minute, 120, 288, 4);
	}
}

void showDay() {
	uint8_t circleWidth				= 30;
	uint8_t circleRadius			= circleWidth / 2;
	uint8_t circleStartx			= 5;
	uint8_t circlePosY				= 240;
	uint8_t weekDaysPosY			= 230;
	uint16_t currentDayColor		= TFT_WHITE;
	uint16_t otherDayColor			= TFT_LIGHTGREY;
	
	for (int i = 0; i < 7; i++) {
		if (i == 0) {
			if (i == dow) tft.fillCircle(circleStartx + circleRadius, circlePosY, circleRadius, currentDayColor);
			else tft.drawCircle(circleStartx + circleRadius, circlePosY, circleRadius, otherDayColor);
			if (i == dow) tft.setTextColor(TFT_BLACK);
			else tft.setTextColor(otherDayColor);
			tft.drawCentreString(shortWeekDays[i], circleStartx + circleRadius, weekDaysPosY, 4);
		}
		else {
			if (i == dow) tft.fillCircle((circleStartx + (circleWidth * i)) + circleRadius + (i * 3), circlePosY, circleRadius, currentDayColor);
			else tft.drawCircle((circleStartx + (circleWidth * i)) + circleRadius + (i * 3), circlePosY, circleRadius, otherDayColor);
			if (i == dow) tft.setTextColor(TFT_BLACK);
			else tft.setTextColor(otherDayColor);
			tft.drawCentreString(shortWeekDays[i], circleStartx + (circleWidth * i) + circleRadius + (i * 3), weekDaysPosY, 4);
		}
	}
}

void showDate() {
	tft.setTextColor(TFT_GREENYELLOW, TFT_BLACK);
	tft.drawCentreString(months[month-1] + " " + String(day) + ", " + String(year), 120, 288, 4);
}

uint8_t startDayOfWeek(uint16_t y, uint8_t m, uint8_t d) {
	static int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
	y -= m < 3;
	return (y + y/4 - y/100 + y/400 + t[m-1] + d) % 7;
}

void showCalendar() {
	uint8_t rectWidth			= 34;
	uint8_t numDay				= 0;
	uint8_t currentDayRadius	= 17;

	if (month == 1 || month == 3 || month == 5 || month == 7 || month == 8 || month == 10 || month == 12) monthLength = 31;
	else monthLength = 30;
	if (month == 2) monthLength = 28;

	startDay = startDayOfWeek(year, month, 1);
	tft.setTextColor(TFT_WHITE);
	// Start draw day number
	for (int x = startDay; x < 7; x++) {
		numDay++;
		if (startDayOfWeek(year, month, numDay) == 0) {		// Sunday color
			if (numDay == day) {
				tft.setTextColor(TFT_YELLOW);
			}
			else {
				tft.setTextColor(TFT_RED);
			}
		}
		else if (isDayOff(year, month, numDay) == true) {	// Day off color
			if (numDay == day) {
				tft.setTextColor(TFT_YELLOW);
			}
			else {
				tft.setTextColor(TFT_RED);
			}
		}
		else if (numDay == day) {							// Today color
			tft.setTextColor(TFT_GREEN);
		}
		else {
			tft.setTextColor(TFT_WHITE);
		}
		tft.drawCentreString(String(numDay), (rectWidth * x) + currentDayRadius, 42, dateFontSize);
	}
	for (int x = 0; x < 7; x++) {
		numDay++;
		if (startDayOfWeek(year, month, numDay) == 0) {		// Sunday color
			if (numDay == day) {
				tft.setTextColor(TFT_YELLOW);
			}
			else {
				tft.setTextColor(TFT_RED);
			}
		}
		else if (isDayOff(year, month, numDay) == true) {	// Day off color
			if (numDay == day) {
				tft.setTextColor(TFT_YELLOW);
			}
			else {
				tft.setTextColor(TFT_RED);
			}
		}
		else if (numDay == day) {							// Today color
			tft.setTextColor(TFT_GREEN);
		}
		else {
			tft.setTextColor(TFT_WHITE);
		}
		tft.drawCentreString(String(numDay), (rectWidth * x) + currentDayRadius, 76, dateFontSize);
	}
	for (int x = 0; x < 7; x++) {
		numDay++;
		if (startDayOfWeek(year, month, numDay) == 0) {		// Sunday color
			if (numDay == day) {
				tft.setTextColor(TFT_YELLOW);
			}
			else {
				tft.setTextColor(TFT_RED);
			}
		}
		else if (isDayOff(year, month, numDay) == true) {	// Day off color
			if (numDay == day) {
				tft.setTextColor(TFT_YELLOW);
			}
			else {
				tft.setTextColor(TFT_RED);
			}
		}
		else if (numDay == day) {							// Today color
			tft.setTextColor(TFT_GREEN);
		}
		else {
			tft.setTextColor(TFT_WHITE);
		}
		tft.drawCentreString(String(numDay), (rectWidth * x) + currentDayRadius, 110, dateFontSize);
	}
	for (int x = 0; x < 7; x++) {
		numDay++;
		if (startDayOfWeek(year, month, numDay) == 0) {		// Sunday color
			if (numDay == day) {
				tft.setTextColor(TFT_YELLOW);
			}
			else {
				tft.setTextColor(TFT_RED);
			}
		}
		else if (isDayOff(year, month, numDay) == true) {	// Day off color
			if (numDay == day) {
				tft.setTextColor(TFT_YELLOW);
			}
			else {
				tft.setTextColor(TFT_RED);
			}
		}
		else if (numDay == day) {							// Today color
			tft.setTextColor(TFT_GREEN);
		}
		else {
			tft.setTextColor(TFT_WHITE);
		}
		tft.drawCentreString(String(numDay), (rectWidth * x) + currentDayRadius, 144, dateFontSize);
	}
	for (int x = 0; x < 7; x++) {
		numDay++;
		if (startDayOfWeek(year, month, numDay) == 0) {		// Sunday color
			if (numDay == day) {
				tft.setTextColor(TFT_YELLOW);
			}
			else {
				tft.setTextColor(TFT_RED);
			}
		}
		else if (isDayOff(year, month, numDay) == true) {	// Day off color
			if (numDay == day) {
				tft.setTextColor(TFT_YELLOW);
			}
			else {
				tft.setTextColor(TFT_RED);
			}
		}
		else if (numDay == day) {							// Today color
			tft.setTextColor(TFT_GREEN);
		}
		else {
			tft.setTextColor(TFT_WHITE);
		}
		if (numDay <= monthLength) tft.drawCentreString(String(numDay), (rectWidth * x) + currentDayRadius, 178, dateFontSize);
		else break;
	}
	if (numDay <= monthLength) {
		for (int x = 0; x < 7; x++) {
			numDay++;
			if (startDayOfWeek(year, month, numDay) == 0) {		// Sunday color
				if (numDay == day) {
					tft.setTextColor(TFT_YELLOW);
				}
				else {
					tft.setTextColor(TFT_RED);
				}
			}
			else if (isDayOff(year, month, numDay) == true) {	// Day off color
				if (numDay == day) {
					tft.setTextColor(TFT_YELLOW);
				}
				else {
					tft.setTextColor(TFT_RED);
				}
			}
			else if (numDay == day) {							// Today color
				tft.setTextColor(TFT_GREEN);
			}
			else {
				tft.setTextColor(TFT_WHITE);
			}
			if (numDay <= monthLength) tft.drawCentreString(String(numDay), (rectWidth * x) + currentDayRadius, 212, dateFontSize);
			else break;
		}
	} // End of draw day number
	
	for (int y = 0; y < 7; y++) {
		for (int x = 0; x < 7; x++) {
			if (x == 0) tft.setTextColor(64592);
			else tft.setTextColor(TFT_WHITE);
			tft.drawCentreString(weekDays[x], (rectWidth * x) + 17, 8, 2);
			// tft.drawRect((rectWidth * x), (rectHight * y), rectWidth, rectHight, TFT_LIGHTGREY);		// Add line
		}
	}
}

void showSensor() {
	tft.setTextSize(1);
	tft.setTextFont(4);
	tft.setTextColor(TFT_GREEN, TFT_BLACK);
	tft.drawString("Temp", 10, 10);
	tft.drawString(String(temperature), 100, 10);
	tft.drawString("C", 190, 10);
	tft.setTextColor(TFT_CYAN, TFT_BLACK);
	tft.drawString("Humi", 10, 40);
	tft.drawString(String(humidity), 100, 40);
	tft.drawString("%", 190, 40);
	tft.setTextColor(TFT_YELLOW, TFT_BLACK);
	tft.drawString("Press", 10, 70);
	tft.drawString(String(pressure / 1000), 100, 70);
	tft.drawString("hPa", 190, 70);
}

bool isDayOff(uint16_t yyyy, uint8_t mm, uint8_t dd) {
	if (yyyy == 2023) yyyy = 0;
	else if (yyyy == 2024) yyyy = 1;
	uint8_t count = dayOff[yyyy][mm-1][0]+1;
	for (byte i = 1; i < count; i++) {
		if (dayOff[yyyy][mm-1][i] == dd) {
			return true;
		}
	}
	return false;
}