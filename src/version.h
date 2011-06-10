#ifndef VERSION_H
#define VERSION_H

namespace AutoVersion{
	
	//Date Version Types
	static const char DATE[] = "11";
	static const char MONTH[] = "06";
	static const char YEAR[] = "2011";
	static const char UBUNTU_VERSION_STYLE[] = "11.06";
	
	//Software Status
	static const char STATUS[] = "Alpha";
	static const char STATUS_SHORT[] = "a";
	
	//Standard Version Type
	static const long MAJOR = 0;
	static const long MINOR = 7;
	static const long BUILD = 4542;
	static const long REVISION = 7991;
	
	//Miscellaneous Version Types
	static const long BUILDS_COUNT = 3346;
	#define RC_FILEVERSION 0,7,4542,7991
	#define RC_FILEVERSION_STRING "0, 7, 4542, 7991\0"
	static const char FULLVERSION_STRING[] = "0.7.4542.7991";
	
	//These values are to keep track of your versioning state, don't modify them.
	static const long BUILD_HISTORY = 0;
	

}
#endif //VERSION_H
