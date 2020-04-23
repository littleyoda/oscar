
Shadow building is recommneded to avoid cluttering up the git source code folder.

The following does not use the QT creator package, and assumes the directoy structure shown.
After installing the required packages:

$ mkdir OSCAR
$ cd OSCAR
$ git clone https://gitlab.com/pholy/OSCAR-code.git
$ mkdir build
$ cd build
$ qmake ../OSCAR-code/OSCAR_QT.pro.
$ make

After successful compilation, you can execute in place with ./oscar/OSCAR,
or build an installable package with one of the mkXXXX.sh scripts. It is recommended to create a
Packages folder within OSCAR and copy the scripts and associated files to it to avoid cluttering up the git source tree.

