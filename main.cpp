#include <QCoreApplication>
#include "fpmain.h"

int main(int argc, char *argv[])
{
	QCoreApplication a(argc, argv);
	
	FpMain fpMain(&a);
	
	return a.exec();
}
