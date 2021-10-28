#ifndef FPTHREAD_H
#define FPTHREAD_H

#include <QThread>
#include <QSet>
#include <QDateTime>
#include "fingerprint.h"

class FpThread : public QThread
{
	Q_OBJECT
public:
	explicit FpThread(QObject *parent = nullptr);
	~FpThread();
	
	enum Mode {NORMAL = 0, ENROLL = 1, DELETE = 2};
	
public slots:
	void enroll(bool run);
	void del(int id);
	
signals:
	void match(int id, int score, bool button);
	void enrollFinished(int id, bool success);
	
	
private:
	
	volatile Mode mode;
	volatile uint16_t tempID;
	QSet<int>* fingerIds;
	QDateTime enrollStartTime;
	
	void run();
	void normalMode(Fingerprint* fp);
	void enrollMode(Fingerprint* fp);
	void deleteMode(Fingerprint* fp);
	
	// configuration
	uint16_t MAX_FINGERS;		// capacitiy of fingerprint library
	uint32_t ENROLL_TIMEOUT;	// (seconds) timeout for enroll mode
	QString DATABASE_NAME;		// name of database
	QString DATABASE_USER;		// user name for database
	QString DATABASE_PASSWD;	// password for database user
};

#endif // FPTHREAD_H
