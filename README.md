# Various Arduino applications
A small collection of my personal Arduino, ESP32, applications.

If you find these helpful, a small donation is appreciated, [![Donate](https://www.paypalobjects.com/en_US/i/btn/btn_donate_LG.gif)](https://www.paypal.com/cgi-bin/webscr?cmd=_s-xclick&hosted_button_id=K8XPMSEBERH3W).

---
## weatherStation
A simple weather system, which retrieves weatherdata from Wunderground and displays them on a Nextion 4.3 display, furthermore an BME280 sensor has been added to give indoor temperature and humidity readings. It's build around the Arduino Wemos Lolin32 Lite. The commincation to Nextion 4.3 is hardware serial, while it's SDA/SCL to BME280.

Highlights:
* Current weather (weather icon, temperature, wind, wind direction, high and low temperature, pressure, humidity, UV Index)
* 3 day forecast (weather icon, high and low temperature)
* Sunrise and sunset time
* Moon coverage
* Indoor (temperature and humidity)
* 9 random screensavers to avoid display burnin
* Information page (ip, last weather update)
* Simple loading screen

A 3D figure is available to mount everything into a casing created with Autodesk Fusion 360.

---
## waterWateringSystem
todo