#include "fpthread.h"
#include "fingerprint.h"
#include "defs.h"
#include <QDebug>
#include <QThread>
#include <QSettings>
#include <QProcess>
#include <QtSql>
#include <QDateTime>
#include <QTime>


FpThread::FpThread(QObject *parent) : QThread(parent)
{
	mode = NORMAL;
	
	QSettings conf(CONFIG_FILE, QSettings::IniFormat, this);
	MAX_FINGERS = uint16_t(conf.value("MAX_FINGERS", 1000).toInt());
	ENROLL_TIMEOUT = conf.value("ENROLL_TIMEOUT", 600).toUInt();
	DATABASE_NAME = conf.value("DATABASE_NAME", "minutiae").toString();
	DATABASE_USER = conf.value("DATABASE_USER", "fp-server").toString();
	DATABASE_PASSWD = conf.value("DATABASE_PASSWD", "DY50").toString();
}


void FpThread::run()
{
	Fingerprint* fp = new Fingerprint();
	fingerIds = new QSet<int>();
	
	if(!fp->start())
	{
		qCritical() << "FpThread: startup failed!";
		return;
	}
	
	QThread::sleep(1);		// give FP sensor some time to start
	
	Fingerprint::Status status;
	

	// connect to database
	QSqlDatabase db = QSqlDatabase::addDatabase("QMYSQL");
	db.setHostName("localhost");
	db.setDatabaseName(DATABASE_NAME);
	db.setUserName(DATABASE_USER);
	db.setPassword(DATABASE_PASSWD);
	bool ok = db.open();
	if(ok)
	{
		qDebug() << "connected to database";
	}
	else
	{
		qCritical() << "could not connect to database:" << db.lastError().text();
		return;
	}

	
	/*
	status = fp->setSysPara(Fingerprint::SECURITY_LEVEL, 3);
	
	// check for errors
	if(status!=Fingerprint::OK)
	{
		fp->printError(status);
		return;
	}
	*/
	
	// read system parameters
	uint16_t statusReg;
	uint16_t systemID;
	uint16_t librarySize;
	uint16_t securityLevel;
	uint32_t deviceAddress;
	uint16_t sizeCode;
	uint16_t nBaud;

	do
	{
		status = fp->readSysPara(statusReg, systemID, librarySize, securityLevel, deviceAddress, sizeCode, nBaud);

		// check for errors
		if(status!=Fingerprint::OK)
		{
			fp->printError(status);
			continue;	// try again
		}

		qDebug() << "fingerprint sensor status register:";
		qDebug() << "busy:" << (statusReg & 1);
		qDebug() << "pass:" << ((statusReg>>1) & 1);
		qDebug() << "PWD:" << ((statusReg>>2) & 1);
		qDebug() << "ImgBufStat:" << ((statusReg>>3) & 1);
		qDebug() << "";

		qDebug() << "systemID 0x" << hex << systemID;
		qDebug() << "librarySize" << librarySize;
		qDebug() << "securityLevel" << securityLevel;
		qDebug() << "deviceAddress 0x" << hex << deviceAddress;
		qDebug() << "sizeCode" << sizeCode;
		qDebug() << "baudrate" << nBaud*9600 << "baud";
		qDebug() << "";

	} while(status != Fingerprint::OK);


	qDebug() << "clear sensor library";
	status = fp->emptyDatabase();
	if(status!=Fingerprint::OK)
	{
		fp->printError(status);
	}
	
	qDebug() << "read fingerprint templates from database...";
	QSqlQuery query;
	if(query.exec("SELECT * FROM fingerprint"))
	{
		while(query.next())
		{
			int id = query.value(0).toInt();
			QByteArray fpTemplate = query.value(1).toByteArray();
			qDebug() << "\tID:" << id;

			if(id < 0 || id >= MAX_FINGERS)
			{
				qCritical() << "invalid id in database, ignored";
				continue;
			}

			status = fp->downChar(Fingerprint::SLOT_1, fpTemplate);
			if(status!=Fingerprint::OK)
			{
				// report error
				fp->printError(status);
				continue;
			}

			status = fp->storeModel(Fingerprint::SLOT_1, uint16_t(id));
			if(status!=Fingerprint::OK)
			{
				// report error
				fp->printError(status);
				continue;
			}

			fingerIds->insert(id);
			//qDebug() << "\ttemplate stored";
		}
	}
	else
	{
		qCritical() << "SQL query failed:" << query.lastError().text();
	}
	qDebug() << "finished!";


	while(true)
	{
		//QThread::msleep(100);
		
		switch(mode)
		{
			case NORMAL:
			{
				normalMode(fp);
				break;
			}
				
			case ENROLL:
			{
				enrollMode(fp);
				break;
			}
				
			case DELETE:
			{
				deleteMode(fp);
				break;
			}
		}
	}
}


void FpThread::normalMode(Fingerprint* fp)
{
	Fingerprint::Status status;
	
	// try to generate image of finger
	status=fp->genImage();
	
	// skip the trivial case NO_FINGER
	if(status!=Fingerprint::NOFINGER)
	{
		// check for errors
		if(status!=Fingerprint::OK)
		{
			// report error and try again next time
			fp->printError(status);
			return;
		}
		
		qDebug() << "finger detected, checking for match...";
		
		// try to create feature file from image
		status=fp->image2Tz(Fingerprint::SLOT_1);
		if(status!=Fingerprint::OK)
		{
			// report error and try again next time
			fp->printError(status);
			return;
		}
		
		uint16_t id=0;
		uint16_t score=0;
		status=fp->search(Fingerprint::SLOT_1, 0, MAX_FINGERS, id, score);
		if(status==Fingerprint::OK)
		{
			// found a match
			
			// check for button
			QProcess process;
			process.start("gpio read 7");
			process.waitForFinished(1000);
			int butRead = QString::fromUtf8(process.readAllStandardOutput()).toInt();

			bool button = (butRead == 0);		// invert button signal
			
			qDebug() << "MATCH, id:" << id << "score:" << score << "button:" << button;
			emit match(id, score, button);
			//QThread::msleep(500);
		}
		else if(status==Fingerprint::NOTFOUND)
		{
			qDebug() << "no match";
			return;
		}
		else
		{
			fp->printError(status);
			return;
		}
	}
	else
	{
		// update routine
		// this is done on a regular basis to check updates of the database

		static QDateTime lastTime = QDateTime::currentDateTime();
		if(QDateTime::currentDateTime() > lastTime.addSecs(5))		// check every 5s
		{
			lastTime = QDateTime::currentDateTime();

			//qDebug() << "check database for update";

			QSqlQuery query;
			if(!query.exec("SELECT id FROM fingerprint"))
			{
				qCritical() << "update: failed to read IDs from database:" << query.lastError().text();
				return;
			}
			QSet<int> dbIds;
			while(query.next())
			{
				dbIds.insert(query.value(0).toInt());
			}

			// compare sets of finger IDs

			// this is the set of fingers that are in the db but not on the sensor -> load to sensor
			auto newIds = dbIds - *fingerIds;

			// this is the set of fingers that are on the sensor but not in the db -> delete them from sensor
			auto oldIds = *fingerIds - dbIds;

			// load one new template
			if(!newIds.isEmpty())
			{
				int newId = newIds.toList().first();
				if(newId < 0 || newId >= MAX_FINGERS)
				{
					qCritical() << "update: invalid ID in database:" << newId;
					return;
				}

				query.prepare("SELECT template FROM fingerprint WHERE id=:id");
				query.bindValue(":id", newId);
				if(!query.exec())
				{
					qCritical() << "update: could not read template from database:" << query.lastError().text();
					return;
				}
				if(!query.next())
				{
					return;
				}
				QByteArray fpTemplate = query.value(0).toByteArray();

				status = fp->downChar(Fingerprint::SLOT_1, fpTemplate);
				if(status!=Fingerprint::OK)
				{
					fp->printError(status);
					return;
				}

				status = fp->storeModel(Fingerprint::SLOT_1, uint16_t(newId));
				if(status!=Fingerprint::OK)
				{
					fp->printError(status);
					return;
				}

				fingerIds->insert(newId);

				qDebug() << "new template ID:" << newId << "loaded";
				return;
			}

			// delete one old template
			if(!oldIds.isEmpty())
			{
				int oldId = oldIds.toList().first();
				if(oldId < 0 || oldId >= MAX_FINGERS)
				{
					qCritical() << "update: invalid ID:" << oldId;
					return;
				}

				// try to delete template on sensor
				status=fp->deleteModel(uint16_t(oldId), 1);
				if(status!=Fingerprint::OK)
				{
					fp->printError(status);
					return;
				}

				fingerIds->remove(oldId);

				qDebug() << "removed old template ID:" << oldId;
				return;
			}
		}
	}
}


void FpThread::enrollMode(Fingerprint* fp)
{
	static Fingerprint::Slot slot = Fingerprint::SLOT_1;
	
	Fingerprint::Status status;

	if(QDateTime::currentDateTime() > enrollStartTime.addSecs(ENROLL_TIMEOUT))
	{
		qWarning() << "ENROLL timed out";
		emit enrollFinished(-1, false);
		mode = NORMAL;
		return;
	}
	
	// try to generate image of finger
	status=fp->genImage();
	
	// skip the trivial case NO_FINGER
	if(status!=Fingerprint::NOFINGER)
	{
		// check for errors
		if(status!=Fingerprint::OK)
		{
			// report error and try again next time
			fp->printError(status);
			return;
		}
		
		qDebug() << "finger detected, creating feature file in slot" << slot;
		
		// try to create feature file from image
		status=fp->image2Tz(slot);
		if(status!=Fingerprint::OK)
		{
			// report error and try again next time
			fp->printError(status);
			return;
		}
		
		if(slot == Fingerprint::SLOT_1)
		{
			qDebug() << "slot 1 successfull, continue with slot 2...";
			slot = Fingerprint::SLOT_2;
			return;
		}

		if(slot == Fingerprint::SLOT_2)
		{
			qDebug() << "slot 2 successfull, generate template...";

			slot = Fingerprint::SLOT_1;
			
			status = fp->createModel();
			if(status!=Fingerprint::OK)
			{
				// report error and try again next time
				fp->printError(status);
				return;
			}

			qDebug() << "template successfull, find free ID in database...";

			// find free ID
			QSqlQuery query;
			if(!query.exec("SELECT id FROM fingerprint ORDER BY id ASC"))
			{
				qCritical() << "ENROLL: failed to find free ID in database:" << query.lastError().text();
				return;
			}
			uint16_t enrollID = 0;
			while(query.next())
			{
				if(query.value(0) != enrollID)
				{
					break;
				}
				enrollID++;
			}

			if(enrollID >= MAX_FINGERS)
			{
				qWarning() << "ENROLL failed, out of memory!";
				emit enrollFinished(-1, false);
				mode = NORMAL;
			}

			qDebug() << "found free id:" << enrollID << "save template on sensor...";
			
			status = fp->storeModel(Fingerprint::SLOT_1, enrollID);
			if(status!=Fingerprint::OK)
			{
				// report error and try again next time
				fp->printError(status);
				return;
			}

			qDebug() << "template saved on sensor, upload template...";

			QByteArray fpTemplate;
			status = fp->upChar(Fingerprint::SLOT_1, fpTemplate);
			if(status!=Fingerprint::OK)
			{
				// report error and try again next time
				fp->printError(status);
				return;
			}

			qDebug() << "upload successfull," << fpTemplate.size() << "bytes, save template in database...";

			query.prepare("INSERT INTO fingerprint (id, template) VALUES (:id, :template)");
			query.bindValue(":id", enrollID);
			query.bindValue(":template", fpTemplate);
			if(!query.exec())
			{
				qCritical() << "ENROLL: failed to save template in database:" << query.lastError().text();
				return;
			}

			fingerIds->insert(enrollID);

			qDebug() << "ENROLL successfull!";
			emit enrollFinished(enrollID, true);
			mode=NORMAL;
		}
	}
}


void FpThread::deleteMode(Fingerprint* fp)
{
	// remove entry from database
	QSqlQuery query;
	query.prepare("DELETE FROM fingerprint WHERE id=:id");
	query.bindValue(":id", tempID);
	if(!query.exec())
	{
		qCritical() << "DELETE: failed to delete from database:" << query.lastError().text();
		mode = NORMAL;
		return;
	}

	// try to delete template on sensor
	Fingerprint::Status status;
	status=fp->deleteModel(tempID, 1);
	if(status!=Fingerprint::OK)
	{
		// report error
		fp->printError(status);
		mode = NORMAL;
		return;
	}

	fingerIds->remove(tempID);
	
	qDebug() << "DELETE id:" << tempID << "successfull";
	mode = NORMAL;
	return;
}


void FpThread::enroll(bool run)
{
	if(run)
	{
		qDebug() << "ENROLL new finger...";
		enrollStartTime = QDateTime::currentDateTime();
		mode = ENROLL;
	}
	else
	{
		qDebug() << "ENROLL aborted";
		mode = NORMAL;
		emit enrollFinished(-1, false);
	}
}


void FpThread::del(int id)
{
	if(id < 0 || id >= MAX_FINGERS)
	{
		qWarning() << "DELETE: invalid id:" << id;
		return;
	}
	
	tempID = uint16_t(id);
	mode = DELETE;
}



FpThread::~FpThread()
{
	QSqlDatabase db = QSqlDatabase::database();
	db.close();
	QSqlDatabase::removeDatabase(db.connectionName());
}
