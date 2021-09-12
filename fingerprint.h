
#ifndef FINGERPRINT_H
#define FINGERPRINT_H

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
 * Fritz Feichtinger, 2014
 * 
 * referred to datasheet ZFM-20 from ZhianTec, Sep 2008, Ver: 1.4
 * 
 */


#include <QObject>
#include <QSerialPort>


class Fingerprint : public QObject
{
	
public:								
	
	enum Slot {SLOT_1=1, SLOT_2=2};											// indices of character file buffers
	
	enum PacketType {NONE=0, COMMAND=0x01, DATA=0x02, ACK=0x07, END=0x08};	// codes for serial packet types
	
	// status and error codes
	enum Status {OK=0x00, PACKETRECIEVEERR=0x01, NOFINGER=0x02, IMAGEFAIL=0x03, IMAGEMESS=0x06, FEATUREFAIL=0x07, NOMATCH=0x08, NOTFOUND=0x09,
				 ENROLLMISMATCH=0x0A, BADPAGEID=0x0B, INVALIDTEMPLATE=0x0C, UPLOADFEATUREFAIL=0x0D, PACKETRECEIVEFAIL=0x0E, UPLOADFAIL=0x0F,
				 DELETEFAIL=0x10, DBCLEARFAIL=0x11/*, PASSFAIL=0x13*/, INVALIDIMAGE=0x15, FLASHERR=0x18, NODEFERR=0x19, INVALIDREG=0x1A,
				 REGCONFERR=0x1B, NOTEPADERR=0x1C, COMMPORTERR=0x1D/*, ADDRCODE=0x20, PASSVERIFY=0x21*/, TIMEOUT=0xFF, BADPACKET=0xFE};
	
	// command codes
	enum Command {GENIMAGE=0x01, IMAGE2TZ=0x02, MATCH=0x03, SEARCH=0x04, REGMODEL=0x05, STORE=0x06, LOADCHAR=0x07, UPCHAR=0x08, DOWNCHAR=0x09,
				  UPIMAGE=0x0A, DOWNIMAGE=0x0B, DELETE=0x0C, EMPTY=0x0D, SETSYSPARA=0x0E, READSYSPARA=0x0F/*, VERIFYPASSWORD=0x13*/, RANDOM=0x14,
				  SETADDR=0x15, HANDSHAKE=0x17, WRITENOTEPAD=0x18, READNOTEPAD=0x19/*, HISPEEDSEARCH=0x1B*/, TEMPLATECOUNT=0x1D};
	
	enum SystemParam {N_BAUD=4, SECURITY_LEVEL=5, SIZE_CODE=6};
	
	Fingerprint();
	~Fingerprint();
	
	// call this at start
	bool start();


	// commands
	Status setSysPara(SystemParam param, uint8_t value);
	Status readSysPara(uint16_t& statusReg, uint16_t& systemID, uint16_t& librarySize, uint16_t& securityLevel,
					   uint32_t& deviceAddress, uint16_t& sizeCode, uint16_t& nBaud);
	
	Status genImage(void);
	Status image2Tz(Slot slot);
	Status createModel(void);
	
	Status storeModel(Slot slot, uint16_t id);
	Status loadModel(Slot slot, uint16_t id);
	Status search(Slot slot, uint16_t start_id, uint16_t count, uint16_t& id, uint16_t& score);
	Status deleteModel(uint16_t id, uint16_t count);
	Status emptyDatabase(void);
	Status upChar(Slot slot, QByteArray& model);
	Status downChar(Slot slot, QByteArray model);
	//Status getTemplateCount(void);
	
	void printError(Status status);
	
	
private:
	
	bool tryToOpenSerial();	
	bool writePacket(uint32_t addr, PacketType type, QByteArray data);
	PacketType getReply(QByteArray& data);
	
	QSerialPort* serial;
	
	// configuration
	int SERIAL_TIMEOUT;		// (seconds) timeout for serial port communication
	uint16_t MAX_FINGERS;	// capacitiy of fingerprint library

};

#endif
