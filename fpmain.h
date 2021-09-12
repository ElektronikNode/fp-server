#ifndef FPMAIN_H
#define FPMAIN_H

#include <QObject>
#include <QTimer>
#include <QtMqtt/QtMqtt>
#include <QTimer>

#include "fpthread.h"

class FpMain : public QObject
{
	Q_OBJECT
public:
	explicit FpMain(QObject *parent = nullptr);
	~FpMain();
	
signals:
	void enroll(bool run);
	void del(int id);
	
private slots:
	void mqttStateChanged();
	void fpMatch(int id, int score, bool button);
	void fpEnrollFinished(int id, bool success);
	void mqttReceive(const QByteArray &message, const QMqttTopicName &topic);
	void unlock(bool keepOpen);
	void lock();
	
private:
	FpThread fpThread;
	QMqttClient mClient;
	QTimer lockTimer;

	// configuration
	uint32_t SINGLE_OPEN_TIME;		// (seconds) unlock time for single access
	uint32_t BUZZ_OPEN_PWM;			// PWM duty cycle (1024=max) used for keeping door buzzer open
	uint32_t BUZZ_PULSE_TIME;		// (milliseconds) initial pulse time to open door buzzer
	
};

#endif // FPMAIN_H
