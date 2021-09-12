/*************************************************** 
  This is a library for our optical Fingerprint sensor

  Designed specifically to work with the Adafruit Fingerprint sensor 
  ----> http://www.adafruit.com/products/751

  These displays use TTL Serial to communicate, 2 pins are required to 
  interface
  Adafruit invests time and resources providing this open source code, 
  please support Adafruit and open-source hardware by purchasing 
  products from Adafruit!

  Written by Limor Fried/Ladyada for Adafruit Industries.  
  BSD license, all text above must be included in any redistribution
 ****************************************************/
 
/*
 * modified and improved by
 * Friedrich Feichtinger, 2017
 * 
 * referred to datasheet ZFM-20 from ZhianTec, Sep 2008, Ver: 1.4
 * 
 */

#include <stdbool.h>
#include <stdint.h>

#include <QDebug>
#include <QSerialPortInfo>
#include <QSettings>
#include <QThread>

#include "fingerprint.h"
#include "defs.h"


#define STARTCODE 0xEF01				// packet start code
#define THEADDRESS 0xFFFFFFFF			// default sensor address
#define TEMPSIZE 512					// size of template file in bytes



Fingerprint::Fingerprint()
{
	QSettings conf(CONFIG_FILE, QSettings::IniFormat, this);
	
	SERIAL_TIMEOUT = conf.value("SERIAL_TIMEOUT", 5).toInt();
	MAX_FINGERS = conf.value("MAX_FINGERS", 1000).toInt();
	
	serial = new QSerialPort(conf.value("SERIAL_PORT", "/dev/ttyS0").toString());
}


bool Fingerprint::start()
{
	if(tryToOpenSerial())
	{
		qDebug() << "Fingerprint: serial port" << serial->portName() << "open.";
		return true;
	}
	else
	{
		return false;
	}
}


Fingerprint::~Fingerprint()
{
	serial->close();
	delete(serial);
}


/*
 * write system parameters
 */
Fingerprint::Status Fingerprint::setSysPara(SystemParam param, uint8_t value)
{
	writePacket(THEADDRESS, COMMAND, QByteArray().append(SETSYSPARA).append((uint8_t)param).append(value));
	
	QByteArray ack;
	PacketType type=getReply(ack);

	if(!(type==ACK && ack.size()==1))
	{
		return BADPACKET;
	}
	return (Status)ack.at(0);
}


/*
 * read system parameters
 */
Fingerprint::Status Fingerprint::readSysPara(uint16_t& statusReg, uint16_t& systemID, uint16_t& librarySize, uint16_t& securityLevel,
				   uint32_t& deviceAddress, uint16_t& sizeCode, uint16_t& nBaud)
{
	writePacket(THEADDRESS, COMMAND, QByteArray().append(READSYSPARA));
	
	QByteArray ack;
	PacketType type=getReply(ack);

	if(!(type==ACK && ack.size()==17))
	{
		return BADPACKET;
	}
	
	statusReg = ((uint16_t)ack[1])<<8;
	statusReg |= (uint8_t)ack[2];
	
	systemID = ((uint16_t)ack[3])<<8;
	systemID |= (uint8_t)ack[4];
	
	librarySize = ((uint16_t)ack[5])<<8;
	librarySize |= (uint8_t)ack[6];
	
	securityLevel = ((uint16_t)ack[7])<<8;
	securityLevel |= (uint8_t)ack[8];
	
	deviceAddress = ((uint32_t)ack[9])<<24;
	deviceAddress |= ((uint32_t)ack[10])<<16;
	deviceAddress |= ((uint32_t)ack[11])<<8;
	deviceAddress |= (uint8_t)ack[12];
	
	sizeCode = ((uint16_t)ack[13])<<8;
	sizeCode |= (uint8_t)ack[14];
	
	nBaud = ((uint16_t)ack[15])<<8;
	nBaud |= (uint8_t)ack[16];
	
	return (Status)ack.at(0);
}


/*
 * generate image of finger
 */
Fingerprint::Status Fingerprint::genImage(void)
{
	writePacket(THEADDRESS, COMMAND, QByteArray().append(GENIMAGE));
	
	QByteArray ack;
	PacketType type=getReply(ack);

	if(!(type==ACK && ack.size()==1))
	{
		return BADPACKET;
	}
	return (Status)ack.at(0);
}


/*
 * generate character file from image and store in <slot>
 */
Fingerprint::Status Fingerprint::image2Tz(Slot slot)
{	
	writePacket(THEADDRESS, COMMAND, QByteArray().append(IMAGE2TZ).append(slot));
	
	QByteArray ack;
	PacketType type=getReply(ack);

	if(!(type==ACK && ack.size()==1))
	{
		return BADPACKET;
	}
	return (Status)ack.at(0);
}


/*
 * use character files in both slots to create model and store back in both slots
 */
Fingerprint::Status Fingerprint::createModel(void)
{
	writePacket(THEADDRESS, COMMAND, QByteArray().append(REGMODEL));
	
	QByteArray ack;
	PacketType type=getReply(ack);

	if(!(type==ACK && ack.size()==1))
	{
		return BADPACKET;
	}
	return (Status)ack.at(0);
}


/*
 * store model from <slot> in library at <id>
 */
Fingerprint::Status Fingerprint::storeModel(Slot slot, uint16_t id)
{
	writePacket(THEADDRESS, COMMAND, QByteArray().append(STORE).append(slot).append(id>>8).append(id & 0xFF));
	
	QByteArray ack;
	PacketType type=getReply(ack);

	if(!(type==ACK && ack.size()==1))
	{
		return BADPACKET;
	}
	return (Status)ack.at(0);
}


/*
 * load model from library at <id> into <slot>
 */
Fingerprint::Status Fingerprint::loadModel(Slot slot, uint16_t id)
{
	writePacket(THEADDRESS, COMMAND, QByteArray().append(LOADCHAR).append(slot).append(id>>8).append(id & 0xFF));
	
	QByteArray ack;
	PacketType type=getReply(ack);

	if(!(type==ACK && ack.size()==1))
	{
		return BADPACKET;
	}
	return (Status)ack.at(0);
}


/*
 * Search the library for a model that matches the one in <slot>
 * start at <start_id> and test <count> templates
 * additional return parameters:
 *	* id of the matching model (if any)
 *  * match score (0 at mismatch)
 */
Fingerprint::Status Fingerprint::search(Slot slot, uint16_t start_id, uint16_t count,
										uint16_t& id, uint16_t& score)
{
	//qDebug() << "search()";
	writePacket(THEADDRESS, COMMAND, QByteArray().append(SEARCH).append(slot)
				.append(start_id>>8).append(start_id & 0xFF).append(count>>8).append(count & 0xFF));
	
	QByteArray ack;
	PacketType type=getReply(ack);
	
	//qDebug() << "reply:" << ack.toHex(':');
	
	if(!(type==ACK && ack.size()==5))
	{
		return BADPACKET;
	}
	
	id=((uint16_t)ack[1])<<8;
	id|=(uint8_t)ack[2];
	
	score=((uint16_t)ack[3])<<8;
	score|=(uint8_t)ack[4];
	
	return (Status)ack.at(0);
}


/*
 * delete <count> models from library starting with <id>
 */
Fingerprint::Status Fingerprint::deleteModel(uint16_t id, uint16_t count)
{
	writePacket(THEADDRESS, COMMAND, QByteArray().append(DELETE).append(id>>8).append(id & 0xFF)
				.append(count>>8).append(count & 0xFF));
	
	QByteArray ack;
	PacketType type=getReply(ack);

	if(!(type==ACK && ack.size()==1))
	{
		return BADPACKET;
	}
	return (Status)ack.at(0);
}


/*
 * delete all models from library
 */
Fingerprint::Status Fingerprint::emptyDatabase(void)
{
	writePacket(THEADDRESS, COMMAND, QByteArray().append(EMPTY));
	
	QByteArray ack;
	PacketType type=getReply(ack);

	if(!(type==ACK && ack.size()==1))
	{
		return BADPACKET;
	}
	return (Status)ack.at(0);
}


/*
 * upload model file form <slot>
 * additional return parameter:
 *	* model of fingerprint
 */
Fingerprint::Status Fingerprint::upChar(Slot slot, QByteArray& model)
{	
	writePacket(THEADDRESS, COMMAND, QByteArray().append(UPCHAR).append(slot));
		
	QByteArray ack;
	PacketType type=getReply(ack);

	if(!(type==ACK && ack.size()==1))
	{
		return BADPACKET;
	}
	
	Status status=(Status)ack.at(0);
	if(status!=OK)
	{
		return status;
	}
	
	
	// data is received in multiple DATA packets, terminated by END packet
	QByteArray data;
	
	while(true)
	{
		QByteArray packet;
		PacketType type=getReply(data);
		
		data.append(packet);
		
		if(type==END)
		{
			// end of data packet
			/*if(data.size()!=TEMPSIZE)
			{
				qCritical() << "Fingerprint::upChar(): wrong size of data packet";
				return BADPACKET;
			}*/
			break;
		}
		else if(type!=DATA)
		{
			qCritical() << "Fingerprint::upChar(): invalid packet received";
			return BADPACKET;
		}
		
		/*
		if(data.size()>TEMPSIZE)
		{
			qCritical() << "Fingerprint::upChar(): to long data packet" << data.size() << ">" << TEMPSIZE;
			return BADPACKET;
		}
		*/
	}
	
	//qDebug() << "template size:" << data.size();
	
	model=data;
	
	return OK;
}

/*
 * download model file to <slot>
 */
Fingerprint::Status Fingerprint::downChar(Slot slot, QByteArray model)
{
	writePacket(THEADDRESS, COMMAND, QByteArray().append(DOWNCHAR).append(slot));
		
	QByteArray ack;
	PacketType type=getReply(ack);

	if(!(type==ACK && ack.size()==1))
	{
		return BADPACKET;
	}
	
	Status status=(Status)ack.at(0);
	if(status!=OK)
	{
		return status;
	}
	
	// ready to send data packet
	int pos;
	int packetsize=128;
	QByteArray packet;
	
	for(pos=0; pos+packetsize<TEMPSIZE; pos+=packetsize)
	{
		packet=model.mid(pos, packetsize);
		writePacket(THEADDRESS, DATA, packet);
	}
	
	// end of packet
	packet=model.mid(pos, packetsize);
	writePacket(THEADDRESS, END, packet);
	
	// TODO upload again to verify download
	
	return OK;
}


void Fingerprint::printError(Status status)
{
	qWarning() << "Fingerprint Error:" << QString("0x%1").arg((int)status, 2, 16, QChar('0'));
	
	switch(status)
	{
		case PACKETRECIEVEERR:	qCritical()	<< "\t error when receiving data packet"; break;
		case IMAGEFAIL:			qWarning()	<< "\t fail to enroll finger"; break;
		case IMAGEMESS:			qWarning()	<< "\t disordered fingerprint"; break;
		case FEATUREFAIL:		qWarning()	<< "\t too small fingerprint"; break;
		case ENROLLMISMATCH:	qWarning()	<< "\t enroll mismatch (could not combine the 2 samples)"; break;
		case BADPAGEID:			qCritical()	<< "\t invalid ID (out of memory)"; break;
		case FLASHERR:			qCritical()	<< "\t error writing flash"; break;
		case DELETEFAIL:		qCritical()	<< "\t failed to delete template"; break;
		case DBCLEARFAIL:		qCritical()	<< "\t failed to clear database"; break;
		case UPLOADFEATUREFAIL:	qCritical()	<< "\t error when uploading template"; break;
		case BADPACKET:			qCritical()	<< "\t packet error"; break;
		default:				qCritical()	<< "\t unknown error"; break;
	}
}

/************************************************************/
/*					private functions:						*/
/************************************************************/

bool Fingerprint::tryToOpenSerial()
{
	/*
	// list available serial ports
	auto ports = QSerialPortInfo::availablePorts();
	for(auto p : ports)
	{
		qDebug() << p.portName() << p.description();
	}
	*/
	
	// check for errors and clear them
	if(serial->error()!=QSerialPort::NoError)
	{
		qWarning() << "Fingerprint: SerialPort Error:" << serial->error() << serial->errorString();
		serial->clearError();
		serial->close();
	}
	
	// try to open
	if(!serial->isOpen())
	{
		if(!serial->open(QIODevice::ReadWrite /*| QIODevice::Unbuffered*/))
		{
			qCritical() << "Fingerprint: cannot open serial port" << serial->portName() << "error:" << serial->error() << serial->errorString();
			return false;
		}
		serial->setBaudRate(QSerialPort::Baud57600);
	}
	
	//qDebug() << "Fingerprint: serial port" << serial->portName() << "open.";
	
	return true;
}


bool Fingerprint::writePacket(uint32_t addr, PacketType type, QByteArray data)
{
	if(!tryToOpenSerial())
		return false;
	
	uint16_t pac_len=data.size()+2;		// length of data including checksum
	uint16_t sum=0;						// checksum
	
	QByteArray packet;
	
	// write packet header
	packet.append((uint8_t)(STARTCODE >> 8));
	packet.append((uint8_t)STARTCODE);
	packet.append((uint8_t)(addr >> 24));
	packet.append((uint8_t)(addr >> 16));
	packet.append((uint8_t)(addr >> 8));
	packet.append((uint8_t)(addr));
	packet.append((uint8_t)type);
	packet.append((uint8_t)(pac_len >> 8));
	packet.append((uint8_t)(pac_len));

	sum += (pac_len>>8) + (pac_len&0xFF) + (uint8_t)type;
	
	// write data and calc checksum
	for(int i=0; i<data.size(); i++)
	{
		packet.append(data[i]);
		sum += data[i];
	}
	
	// write checksum
	packet.append((uint8_t)(sum>>8));
	packet.append((uint8_t)sum);
	
	// send
	if(serial->write(packet)!=packet.size())
	{
		qCritical() << "Fingerprint: could not send serial packet.";
		return false;
	}
	
	QThread::msleep(10);
	
	return true;
}



/*
 * wait for a packet and receive it
 * data (return parameter): received packet content
 * return value: received type of packet, NONE in case of error
 */
Fingerprint::PacketType Fingerprint::getReply(QByteArray& data)
{
	if(!tryToOpenSerial())
		return NONE;
	
	// packet format
	//  0      1      2     3     4     5     6     7    8
	// {START, START, ADDR, ADDR, ADDR, ADDR, TYPE, LEN, LEN, DATA..., SUM, SUM}
	
	uint16_t sum=0;			// calculated checksum of type, len and data
	
	QByteArray buffer;		// serial receive buffer

	while(true)
	{
		// wait for data
		if(!serial->waitForReadyRead(SERIAL_TIMEOUT*1000))
		{
			qCritical() << "Fingerprint: serial port timeout";
			return NONE;
		}
		
		// append data to receive buffer
		buffer.append(serial->readAll());
		
		//qDebug() << "getReply: buffer (size:" << buffer.size() << "):" << hex << buffer.toHex(':');
		
		bool startCodeReceived=false;
		
		// check if there is enough data for the header
		while(buffer.size()>=9)
		{
			// check startcode
			uint16_t startCode = (((uint16_t)buffer[0])<<8) | ((uint16_t)buffer[1]);
			if(startCode == STARTCODE)
			{
				startCodeReceived=true;
				break;
			}
			else
			{
				buffer.remove(0, 1);	// invalid startcode, remove first byte and try again
			}
		}
		
		if(!startCodeReceived)
		{
			continue;	// read more data, try again
		}
		
		// startcode received, buffer.size() >= 9
		
		// check packet type
		PacketType type=(PacketType)buffer.at(6);
		if(!(type==COMMAND || type==DATA || type==ACK || type==END))
		{
			qCritical() << "Fingerprint: invalid packet type received:" << type;
			return NONE;
		}

		sum = 0;
		sum+=(uint8_t)buffer[6];

		// data length (without checksum)
		uint16_t len = (((uint16_t)(buffer[7])<<8) | (uint8_t)buffer[8]) - 2;
		
		//qDebug() << len;
		sum+=(uint8_t)buffer[7];
		sum+=(uint8_t)buffer[8];
		
		// check if there is enough data for the packet
		if(buffer.size() < 9+len+2)
		{
			continue;	// read more data, try again
		}
		
		// read packet data
		for(int i=0; i<len; i++)
		{
			uint8_t d=(uint8_t)buffer[9+i];
			data.append(d);
			sum+=d;
		}
		
		//qDebug() << hex << (uint8_t)buffer[9+len];
		//qDebug() << hex << (uint8_t)buffer[9+len+1];
		
		// read checksum
		uint16_t checksum = (((uint16_t)buffer[9+len])<<8) | (uint8_t)buffer[9+len+1];
		
		if(sum!=checksum)
		{
			qCritical() << "Fingerprint: checksum error:" << sum << "!=" << checksum;
			return NONE;
		}
		
		QThread::msleep(10);
		
		// checksum OK, packet is valid
		return type;
	}
}

