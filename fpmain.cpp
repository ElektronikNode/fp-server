#include "fpmain.h"
#include "fpthread.h"
#include "defs.h"

#include <QDebug>
#include <QJsonDocument>
#include <stdlib.h>

FpMain::FpMain(QObject *parent) : QObject(parent)
{
	// read config
	QSettings conf(CONFIG_FILE, QSettings::IniFormat, this);
	SINGLE_OPEN_TIME = conf.value("SINGLE_OPEN_TIME", 5).toUInt();
	BUZZ_OPEN_PWM = conf.value("BUZZ_OPEN_PWM", 256).toUInt();
	BUZZ_PULSE_TIME = conf.value("BUZZ_PULSE_TIME", 100).toUInt();
	
	// start fingerprint thread
	connect(this, SIGNAL(enroll(bool)), &fpThread, SLOT(enroll(bool)));
	connect(this, SIGNAL(del(int)), &fpThread, SLOT(del(int)));
	connect(&fpThread, SIGNAL(match(int,int,bool)), this, SLOT(fpMatch(int,int,bool)));
	connect(&fpThread, SIGNAL(enrollFinished(int, bool)), this, SLOT(fpEnrollFinished(int, bool)));
	fpThread.start();
	
	// start MQTT connection
	connect(&mClient, SIGNAL(stateChanged(ClientState)), this, SLOT(mqttStateChanged()));
	connect(&mClient, SIGNAL(messageReceived(QByteArray,QMqttTopicName)), this, SLOT(mqttReceive(QByteArray,QMqttTopicName)));
	mClient.setHostname("localhost");
	mClient.setPort(1883);
	mClient.connectToHost();

	// configure GPIO
	system("gpio mode 1 pwm");		// door buzzer
	system("gpio mode 4 out");		// green LED
	system("gpio mode 5 out");		// red LED
	system("gpio mode 7 in");		// sensor button

	lock();
}


void FpMain::mqttStateChanged()
{
	auto state = mClient.state();
	switch(state)
	{
		case QMqttClient::Disconnected: qDebug() << "MQTT disconnected"; break;
		case QMqttClient::Connecting: qDebug() << "MQTT connecting"; break;
		case QMqttClient::Connected:
		{
			qDebug() << "MQTT connected, subscribe to topics";
			
			// subscribe to MQTT topics
			mClient.subscribe(QMqttTopicFilter("ENROLL"), 1);
			mClient.subscribe(QMqttTopicFilter("DELETE"), 1);
			mClient.subscribe(QMqttTopicFilter("UNLOCK"), 1);
			mClient.subscribe(QMqttTopicFilter("LOCK"), 1);
			
			break;
		}
	}
}


void FpMain::fpMatch(int id, int score, bool button)
{
	//qDebug() << "fpMatch";
	QJsonObject obj(
	{
		{"pattern", "MATCH"},
		{"data", QJsonObject(
		{
			{"externalFingerId", id},
			{"score", score},
			{"button", button}
		})
		}
	});
	QJsonDocument doc(obj);
	mClient.publish(QMqttTopicName("MATCH"), doc.toJson(), 1);
}


void FpMain::fpEnrollFinished(int id, bool success)
{
	//qDebug() << "fpEnrollFinished";
	QJsonObject obj(
	{
		{"pattern", "ENROLL_FINISHED"},
		{"data", QJsonObject(
		{
			{"externalFingerId", id},
			{"success", success}
		})
		}
	});
	QJsonDocument doc(obj);
	mClient.publish(QMqttTopicName("ENROLL_FINISHED"), doc.toJson(), 1);
}


void FpMain::mqttReceive(const QByteArray &message, const QMqttTopicName &topic)
{
	//qDebug() << "MQTT message received";
	
	QJsonParseError error;
	QJsonDocument doc = QJsonDocument::fromJson(message, &error);
	if(doc.isNull())
	{
		qWarning() << "mqttReceive(): JSON parsing error at offset" << error.offset << ":" << error.errorString();
		return;
	}
	if(!doc.isObject())
	{
		qWarning() << "mqttReceive(): message does not contain a JSON object";
		return;
	}
	QJsonObject obj=doc.object();
	if(!obj.contains("data"))
	{
		qWarning() << "mqttReceive() message does not contain 'data'";
		return;
	}
	obj = obj["data"].toObject();
	
	
	if(topic.name() == "ENROLL")
	{
		if(obj.contains("run"))
		{
			bool run = obj["run"].toBool();
			emit enroll(run);
		}
		else
		{
			qWarning() << "mqttReceive(): ENROLL: 'run' not found";
		}
	}
	else if(topic.name() == "DELETE")
	{
		if(obj.contains("externalFingerId"))
		{
			int id = obj["externalFingerId"].toInt();
			emit del(id);
		}
		else
		{
			qWarning() << "mqttReceive(): DELETE: 'externalFingerId' not found";
		}
	}
	else if(topic.name() == "UNLOCK")
	{
		bool keepOpen = false;
		if(obj.contains("keepOpen"))
		{
			keepOpen = obj["keepOpen"].toBool();
		}
		unlock(keepOpen);
	}
	else if(topic.name() == "LOCK")
	{
		lock();
	}
	else
	{
		qWarning() << "mqttReceive(): unknown topic" << topic.name();
	}
}


void FpMain::unlock(bool keepOpen)
{
	qDebug() << "UNLOCK, keepOpen:" << keepOpen;
	system("gpio pwm 1 1024");		// door buzzer full power
	system("gpio write 4 1");		// green LED on
	system("gpio write 5 0");		// red LED off
	QThread::msleep(BUZZ_PULSE_TIME);

	system(QString("gpio pwm 1 %1").arg(BUZZ_OPEN_PWM).toStdString().c_str());		// reduce buzzer pwm to minimize power dissipation
	
	if(!keepOpen)
	{
		QTimer::singleShot(int32_t(SINGLE_OPEN_TIME) * 1000, this, SLOT(lock()));
	}
}


void FpMain::lock()
{
	qDebug() << "LOCK";
	system("gpio pwm 1 0");			// buzzer off
	system("gpio write 4 0");		// green LED off
	system("gpio write 5 1");		// red LED on
}


FpMain::~FpMain()
{
	mClient.disconnectFromHost();
}
