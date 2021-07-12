/* Dump the annotations of an edf file */

#include <QApplication>
// #include <iostream>
#include <QDebug>

typedef float EventDataType;

#include "../dumpSTR/edfparser.h"

// using namespace std;

void dumpHeader( const EDFHeaderQT  hdr ) {
    qDebug() << "EDF Header:";
    qDebug() << "Version " << hdr.version << " Patient >" << hdr.patientident << "<";
    qDebug() << "Recording >" << hdr.recordingident << "<";
    qDebug() << "Date: " << hdr.startdate_orig.toString();
    qDebug() << "Header size (bytes): " << hdr.num_header_bytes;
    qDebug() << "EDF type: >" << hdr.reserved44 << "<";
    qDebug() << "Duration(secs): " << hdr.duration_Seconds;
    qDebug() << "Number of Signals: " << hdr.num_signals << "\n";
}

void ifprint( QString label, QString text) {
    if ( text.isEmpty() )
        return;
    qDebug() << label << ": " << text;
    return;
}

void dumpSignals( const QVector<EDFSignal>  sigs ) {
    int i = 1;
    for (auto sig: sigs) {
        qDebug() << "Signal #" << i++;
        qDebug() << "Label: " << sig.label;
        ifprint( "Transducer", sig.transducer_type );
        ifprint( "Dimension", sig.physical_dimension );
        qDebug() << "Physical min: " << sig.physical_minimum << " max: " << sig.physical_maximum;
        qDebug() << "Digital  min: " << sig.digital_minimum << " max: " << sig.digital_maximum;
        qDebug() << "Gain: " << sig.gain << " Offset: " << sig.offset;
        ifprint( "Pre-filter", sig.prefiltering );
        qDebug() << "Sample Count: " << sig.sampleCnt;
        qDebug() << "dataArray is at " << sig.dataArray << "\n";
    }
}

int main(int argc, char *argv[]) {

//    QString homeDocs = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)+"/";
//    QCoreApplication::setApplicationName(getAppName());
//    QCoreApplication::setOrganizationName(getDeveloperName());
//    QCoreApplication::setOrganizationDomain(getDeveloperDomain());

//  int first = 0, last = 0;
//  int firstSig = 1, lastSig = 0;

    QApplication a(argc, argv);
    QStringList args = a.arguments();
    
	if ( args.size() < 2 ) {
		qDebug() << args[0] << " needs a filename" ;
		exit(1);
	}
	
    QString filename = args[args.size()-1];
    bool showall = false;   //   brief = false;

    for (int i = 1; i < args.size()-1; i++) {
//      if (args[i] == "-f")
//          first = args[++i].toInt();
//      else if (args[i] == "-g")
//          firstSig = args[++i].toInt();
//      else if (args[i] == "-l")
//          last = args[++i].toInt();
//      else if (args[i] == "-m")
//          lastSig = args[++i].toInt();
        if (args[i] == "-a")
            showall = true;
//      else if (args[i] == "-b")
//          brief = true;
    }

    EDFInfo edf;
    if ( ! edf.Open(filename) ) {
        qDebug() << "Failed to open" << filename;
        exit(-1);
    }
    if ( ! edf.Parse() ) {
        qDebug() << "Parsing failed on" << filename;
        exit(-1);
    }

    QDate d2 = edf.edfHdr.startdate_orig.date();
    if (d2.year() < 2000) {
        d2.setDate(d2.year() + 100, d2.month(), d2.day());
        edf.edfHdr.startdate_orig.setDate(d2);
    }



    QDateTime date = edf.edfHdr.startdate_orig;

    qDebug() << edf.filename << " starts at " << date.toString() << " with " << edf.GetNumSignals() << " signals" <<
                " and " << edf.GetNumDataRecords() << " records";

//  if (args.size() == 2) {
//      exit(0);
//  }

    if (showall) {
    	dumpHeader( (edf.edfHdr) );
	
	    dumpSignals( (edf.edfsignals) );
    }

//  if ( brief )
//      exit(0);
	
    qDebug() << "File has " << edf.annotations.size() << "annotation vectors";
    int vec = 1;
    for (auto annoVec = edf.annotations.begin(); annoVec != edf.annotations.end(); annoVec++ ) {
        qDebug() << "Vector " << vec++ << " has " << annoVec->size() << " annotations";
        for (auto anno = annoVec->begin(); anno != annoVec->end(); anno++ ) {
            qDebug() << "Offset: " << anno->offset << " Duration: " << anno->duration << " Text: " << anno->text;
        }
    }

    exit(0);
}
