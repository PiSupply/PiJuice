
# PiJuice Weather Station

This project is designed to show the capabilities fo the PiJuice HAT for a remote weather station application. Used in conjunction with the Raspberry Pi it be used as an off grid weather station provided it has a data connection. This weather station is powered using the PiJuice HAT with the default battery and a 22W solar panel for recharging the battery and allowing continues monitoring over night and when sunlight conditions are low.

This project itself is very simple, with only a few low-cost components we are able to monitor Light levels, temperature and humidity as well as monitoring the status of the PiJuice battery levels. The front end of the weather station uses a web server provided by Flask and programmed in Python. The interface is done using a javascript plugin called [JustGauge](http://justgage.com), which is fully customisable.

The project is a prototype and is designed to get you started with building your very own weather station with the Raspberry Pi and PiJuice HAT. Once tested on a breadboard you can then move it on to our PiCrust Pro HAT for a permanent fixture.

## Hardware setup

For this weather station project you will require the following parts:

- PiJuice HAT Maker Kit (Excludes components)
- Raspberry Pi computer
- Wi-Fi or Ethernet network capability
- 8GB microSD card with Raspbian OS
- DHT22 Temperature & Humidity sensor
  - 4.7k resistor
- TMP36 Temperature sensor
- LDR sensor
- 2.2K resistor
- PiJuice HAT
- PiJuice Solar Panel
- PiCrust Pro prototyping HAT (optional)


## Software Installation

This project runs on the latest version of [Raspbian OS](https://www.raspberrypi.org/downloads/) for the Raspberry Pi. Make sure you run `sudo apt-get update` before installing the following libraries. You will need to run the following commands in the terminal window to install the libraries for the weather sensors.

### Auto Installation

Just run the following line in the terminal to automatically install all the libraries and project files to the Raspberry Pi.

Raspbian OS Install Script:
```bash
# Run this line and the weather station will be setup and installed
curl -sSL https://raw.githubusercontent.com/ChristopherRush/weather/master/install.sh | sudo bash
```
Raspbian Lite Install script:
```bash
curl -sSL https://raw.githubusercontent.com/ChristopherRush/weather/master/install_lite.sh | sudo bash
```

### Adafruit DHT22 Library

```bash
pip install Adafruit_DHT
```

### Flask

[Flask](http://flask.pocoo.org) is a lightweight web framework that runs using Python programming language. We will be using Flask to create a web server that can host a web page locally on the Raspberry Pi and then can be accessible over the network from any other device on that same network.

By default Flask is already installed on the latest version of [Raspbian OS](https://www.raspberrypi.org/downloads/), however if the package is not there then you can type the following in the terminal window to install Flask:
```bash
sudo apt-get install python3-flask
```

Flask had a specific file structure that needs to be met in order for all the files to be located for the web server. Here is the file structure in its simplest terms:

- app.py
- config.py
- requirements.txt
  - static/
    - css/
      - style.css
    - javascript/
  - templates
    - index.html

For further information visit http://exploreflask.com/en/latest/organizing.html

running 'debug=True' causes the server to run with the reloader, therefore the app is restarted in a new process by the reloader process.

### Weather Station Project install

To download the weather station project files to your Raspberry Pi type the following in the terminal window:

```bash
git clone https://github.com/ChristopherRush/weather.git

```

To run the Flask web server:

```bash
cd weather
sudo python weather.py
```

To view the webpage you will need to go to the Raspberry Pi's IP address on your local network such as http://192.168.0.23:5000 yours may differ. You can find your IP address from the terminal window on the Raspberry Pi by typing in the following command:

```bash
#Wi-Fi connection
ifconfig wlan0

#Ethernet
ifconfig eth0
```

![ipaddress](https://www.pi-supply.com/wp-content/uploads/2018/02/Screen-Shot-2018-02-14-at-11.11.06.png)

## Changing web page refresh rate

By default this project has the ability to refresh the web page every 5 seconds to get the latest update value from the sensors. You can change the refresh rate in the index.html page by changing the content value in the following line:

```html
<meta http-equiv="refresh" content="5">
```
## JustGauge Javascript Plugin

[Justgauge](http://justgage.com/) is a handy JavaScript plugin for generating and animating nice & clean gauges. It is based on Raphaël library for vector drawing, so it’s completely resolution independent and self-adjusting. This plugin is a nice clean way to display the values from our weather station in its simplest form.

The JavaScript files have already been added to the project files in static/javascript/ . To add JavaScript to the index.html file you must do so in the following format not the regular html format:

```html
<script src="{{url_for('static', filename='javascript/justgage.js')}}"></script>
<script src="{{url_for('static', filename='javascript/raphael-2.1.4.min.js')}}"></script>
```
To add a new gauge you will need to create a new div with id and class.

```html
<div id="gauge" class="gauge">
```

Add the following to the css file to style each gauge element. The id element will change the individual gauge block and as such will require a unique id where as the class will style all gauge elements the same. Note: currently the id is not used in this project.

```css
.gauge {
     width: 300px;
     height: 300px;
     display: inline-block;
     margin: 5px;
 }
 ```
Finally add these parameters changing the value to the variable that gets passed through from the Python script.

```html
<script>
var gauge = new JustGage({
  id: "gauge",
  value: {{temp}},
  symbol: '\u2103', #can be added as plain text or hex code
  levelColors: ['#1B94FF', '#FDDC00', '#FF9E00', '#FF3F00'],
  min: 0, #minimum range value
  decimals: 1, #decimal places
  max: 50, #maximum range value
  title: "Temperature" #text to be displayed above the gauge
});
</script>
```

JustGage plugin is licensed under the MIT license
