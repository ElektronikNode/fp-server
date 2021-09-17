
# fp-server
A driver/server for a fingerprint sensor and door lock using Raspberry Pi and a custom shield.

This is a software component of the [Minutiae fingerprint door lock system](URL).

## DISCLAIMER
This software comes with ABSOLUTELY NO WARRANTY. Use at YOUR OWN RISK.

## What it does
fp-server handles the "low level" tasks such as:

* Initialization and communication with the fingerprint sensor (such as DY-50).
* Continuously try to scan the fingerprint and detect a match.
* Enroll and store new fingerprints.
* Communication with the minutiae backend using MQTT.
* Lock/unlock the door using the electrical door buzzer, drive some LEDs and input button, etc.
* Backup the fingerprint templates in a MariaDB/MySQL database.

When enrolling a new fingerprint, fp-server is switched into enroll mode. The same finger has to be presented to the sensor 2 times. fp-server repeatedly tries to scan and enroll until 2 matching fingerprints were scanned. fp-server switches back to normal mode after the enroll was successful or if enrolling was aborted or timed out.

## MQTT Interface
All MQTT messages are formatted using JSON syntax. The following MQTT topics are received by fp-server:

* ENROLL {"pattern": "ENROLL", "data":{"run": true/false}}	
	Tries to scan an new fingerprint and store it. If "run": false, enrolling is aborted.
	
* DELETE {"pattern": "DELETE", "data":{"externalFingerId": ...}}  
	Delete the fingerprint id from sensor and database.
	
* UNLOCK {"pattern": "UNLOCK", "data":{"keepOpen": true/false}}		
	Unlock the door. If "keepOpen": true the door stays unlocked, otherwise it is locked again after SINGLE_OPEN_TIME.
	
* LOCK {"pattern": "LOCK", "data":{}}	
	Lock the door.

The following MQTT topics are sent by fp-server:

* MATCH {"pattern": "MATCH", "data":{"externalFingerId": ..., "score": ..., "button": true/false}}	
	The finger externalFingerId was detected on the sensor. score is the match quality. If the sensor button was pressed button=true.

* ENROLL_FINISHED {"pattern": "ENROLL_FINISHED", "data":{"externalFingerId": ..., "success": true/false}	
	Enrolling finger externalFingerId is finished. If enrolling failed, success=false.

## Hardware requirements
fp-server is meant to run on a Raspberry Pi using the custom shield (link to project). However it is possible to compile and run on a regular PC running Debian/Ubuntu for testing/debugging purpose. In this case the fingerprint sensor has to be connected using a serial-to-USB adapter. Of course all the GPIO related commands (door buzzer, LEDs, ...) will not work.

## Building
Run this command to install the required debian packages for building fp-server:

	$ sudo apt install git qt5-default qtcreator qtbase5-private-dev libqt5serialport5-dev libqt5sql5-mysql

The QtMqtt library has to be built manually:

	$ git clone https://code.qt.io/qt/qtmqtt.git -b 5.12
	$ cd qtmqtt
	$ qmake -r
	$ make
	$ sudo make install

To build and debug fp-server Qt-Creator can be used. As the Raspberry Pi tends to run out of RAM when building with Qt-Creator it is recommended to build with one thread only (-j1).

## MQTT
mosquitto is recommended as a MQTT broker:

	$ sudo apt install mosquitto

To allow commands from local clients only, append the line:

	listener 1883 localhost
to the file:

	/etc/mosquitto/mosquitto.conf

[MQTT-explorer](http://mqtt-explorer.com/) can be used to debug MQTT messages.


## Database
MariaDB is used as a database in the Minutiae project, therefore it is recommended to install and configure the database during the installation of Minutiae.

## Configuration
fp-server comes with a configuration file named 'fp-server.conf'. This file has to be present in same folder as the executable file. 

