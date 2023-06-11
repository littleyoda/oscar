Creating OSCAR development environment on Windows, compiling, and building installers
=====================================================================================

This document is intended to be a brief description of how to install the necessary components to build OSCAR and create installers for Windows 32-bit and 64-bit versions.

On my computers, I have QT installed in E:\\QT and the OSCAR code base in E:\\oscar\\oscar-code. On another computer, they are on the F: drive. All references in the deploy.bat file are relative, so it should run with Oscar-code installed at any location.

The deploy.bat file is required to build release and install versions of Oscar using QtCreator or with buildall.bat. The buildall.bat compiles , builds Oscar and calls deploy.bat to create release and install version of Oscar.

There are three sections to this documentation.

1) Required Programs
2) Start Developing using batch files
3) Start Developing using QtCreator

## Required Programs

The following programs and files are required to create Windows installers:
-   Inno Setup 6.0.3 from <http://www.jrsoftware.org/isdl.php>.
    Download and install innosetup-qsp-6.0.3.exe.
-   GIT for windows, from <https://gitforwindows.org/>.
    GIT for Windows adds itself to your path.
    -   Gawk is required.
        You can use the version included with Git for Windows or install Gawk for Windows from <http://gnuwin32.sourceforge.net/packages/gawk.htm>. The deployment batch file will use the Git for Windows version if gawk.exe is not in your PATH.
-   QT Open Source edition from <https://www.qt.io/download>.
    I use the latest patch version in the 5.12 LTS series -- version 5.12.8 at the date this was last updated. More recent versions in the 5.12 series should also work.

**Installing Inno Setup 6**

Inno Setup 6.0.3 is found on <http://www.jrsoftware.org/isdl.php>. Download and install innosetup-qsp-6.0.3.exe.

The deployment batch file assumes that Inno Setup is installed into its default location: C:\\Program Files (x86)\\Inno Setup 6. If you put it somewhere else, you will have to change the batch file.

Run the installer, accepting options to install inno script studio (for possible future use) and install Inno Setup Preprocessor. Encryption support is not needed, so do not select it.

**Installing GIT for Windows**

Go to <https://gitforwindows.org/> and click on the Download button. Run the installer, which presents lots of options:

-   Select whichever editor you desire.
-   Select “Use Git from the command line and also from 3rd-party software.”
-   Select “Use the OpenSSL library.”
-   Select “Checkout Windows-style, commit Unix-style line endings.”
-   Select “Use Windows’ default console window.” I find the Windows default console to be satisfactory on Windows 10.
-   Select "Enable symbolic links"
-   Leave other extra options as they default (enable file system caching, enable Git credential manager).

GIT for Windows adds itself to your path.

Create SSH key and upload to GitLab--See https://docs.gitlab.com/ce/ssh/README.html.

**Installing Gawk (if Git for Windows’ gawk is not used)**

From <http://gnuwin32.sourceforge.net/packages/gawk.htm>, download setup for “Complete package, except sources”. When downloaded, run the setup program. Accept default options and location. The deployment batch file assumes that gawk.exe is in your PATH, so either add c:\\Program Files (x86)\\gnuwin32\\bin to your PATH or copy the executables to some other directory already in your PATH.

**Installing QT**

Go to QT at <https://www.qt.io/download> and download the Open Source edition of the Windows online installer, qt-unified-windows-x86-3.1.1-online.exe. Run the installer:

-   Logon with your QT account or create an account if needed.

-   Click Next to download meta information (this takes a while).

-   Choose your installation directory (I picked E:\\Qt, but there are no dependencies on where QT is located)

-   Select components:

   -   In QT 5.12.*x*:

   -   MinGW 7.3.0 32-bit
-   MinGW 7.3.0 64-bit
   -   Sources
   -   QT Debug Information Files

-   In Developer and Designer Tools:

   -   QT Creator 4.11.2 CDB Debug
   -   Debugging Tools for Windows
   -   MinGW 7.3.0 32-bit
   -   MinGW 7.3.0 64-bit

And complete the installation (this also takes a while).

## Start Developing using batch files

-   Requires Qt , Git and Inno setup described in above section.
-   Batch files buildall.bat and deploy.bat are used.
-   buildall.bat creates a build folder, compiles and executes deploy.bat
-   Supports both 32 and 64 bit version with an option to build brokenGl
-   Supports Qt 5.9.x 5.12.x 5.15.x
-   Auto detection for which compiler to use.
-   buildall.bat supports command Line options
-   deploy.bat creates a release version and an install version.
-   deploy.bat is also used by QtCreator
-   The release folder contains OSCAR.exe and all other files necessary to run OSCAR
-   The install folder contains the installable version of OSCAR...exe
-   Lists the release and install versions of OSCAR in the build folder.

#### Validate the installed software.

-   Verify Qt
   -   \<QtVersion\> will be in the form N.N.N  example 5.15.2
   -   For example: if Qt is installed at D:\\Qt then
   -   The \<QtFolder\> must contain the following folders: \<QtVersion\> Tools
   -   -   \<QtFolder\> is D:\\Qt
-   Verify Git and Inno are installed
   -   Note: Inno is used to create the Install version of OSCAR
   -   Git for windows provides GAWK.

#### Examples use the following assumptions

Except for Inno, Git, Qt, and OSCAR may be located in different locations.

-   Inno installed:
   -   "C:\\Program Files (x86)\\Inno Setup 6"
-   Git installed:
   -   "C:\\Program Files\\Git"
-   Qtinstalled:
   -   "D:\\Qt"
-   OSCAR installed:
   -   "D:\\OSCAR"

#### Building Commands

-  Build install version for OSCAR for 32 and 64 bit versions
    -   D:\\OSCAR\OSCAR-code\\Building\\Windows\buildall.bat D:\\Qt
-  Build install version for OSCAR for 64 bit version
    -   D:\\OSCAR\OSCAR-code\\Building\\Windows\buildall.bat D:\\Qt 64
-  Build Just release version for OSCAR for 64 bit version
    -   D:\\OSCAR\OSCAR-code\\Building\\Windows\buildall.bat D:\\Qt 64 skipInstall
-  Build release version for OSCAR for 64 bit version - without deleting build folder first
    -   D:\\OSCAR\OSCAR-code\\Building\\Windows\buildall.bat D:\\Qt 64 skipInstall remake
-  The current folder is not used by the buildall.bat
-  There is a pause when the build completes.
-  This insure that the user has a chance to read the build results.
-  Allows using windows shortcuts to build OSCAR and see results.

Note: The default folder of Qt is C:\\Qt
If the Qt is located at the default folder then the \<QtFolder\> is not required as a command line option.

#### Windows Shortcuts
-   Windows shortcuts can be used to execute the build commands or run OSCAR.
-   Create shortcut to buildall.bat
-   rename shortcut to indicate its function
-   edit the short cut property and add options to the Target
-   Create a shortcut to release version of OSCAR.exe
-   For offical OSCAR release should not use remake or skipInsall options
-   Should add skipInstall options for developement, testing, verification
-   Suggestion is to create the following shortcut example.
   -   use options  \<qtfolder\> 64 skipInstall
   -   - shortcut name: "OSCAR Fresh build"
   -   use options  \<qtfolder\> 64 skipInstall remake
   -   - shortcut name: "OSCAR quick rebuild"
   -   Create Shortcut to release version of OSCAR.exe   (not the install version)
   -   - shortcut name: "RUN OSCAR"


#### Buildall.bat options.
-   A full list of options can be displayed using the help option.
    **32**   Build 32 bit versions

    **64**   Build 64 bit versions

    32 and 64 bit version are built if both 32 and 64 are used or both not used
    **brokenGL** (special option) to build brokenGL version

    **make**   The default option. removes and re-creates the build folder.

    **remake**   Execute Make in the existing build folder. Does not re-create the Makefile. Should not be used for Offical OSCAR release

    **skipInstall** skips creating the Install version saving both time and disk space

    **skipDeploy** skips executing the deploy.bat script. just compiles oscar.

There is a pause when the build completes. This insure that the user has a chance to read the build results.
This also allows building using windows shortcuts.
1) create a shortcut to buildall.bat
   edit shortcut's property add the necessary options: D:\\Qt 64 remake skipInstall
2) create and shortcut for the make option.
3) create a shortcut to the **release** version of OSCAR.exe

## Start Developing Oscar in Qt Creator

In browser, log into your account at gitlab.com. Select the Oscar project at https://gitlab.com/pholy/OSCAR-code. Clone a copy of the repository to a location on your computer.

Start QT. There are two QT Oscar project files: OSCAR_QT.pro in the Oscar-code directory, and Oscar.pro in the Oscar-code\\oscar directory. You may use *either* project file. Both will create a running version of Oscar. I find building with Oscar.pro in the Oscar-code\\oscar directory to be very slightly faster, but the difference is negligible.

QT it will ask you to select your kits and configure them. Select both MinGW 7.3.0 32-bit and 64-bit kits.

Click on Projects in the left panel to show your active project (“oscar”) and **Build & Run** settings. Click on the **Build** line for either 32-bit or 64-bit.

In the Build settings in the center panel, select “Release” rather than the default “Debug” in the pull-down at the top of the Build Settings. Click on Details for the qmake build step. By default, “Enable Qt Quick Compiler” is checked. Remove that check – errors result if it is on. QT will ask if you want to recompile everything now. Don’t, as there is more to do before compiling. Make this same change for the Release build for both 32-bit and 64-bit kits. If you want to use the QT Creator Debug tools, select the Build Debug pull-down and disable the QT Quick Compiler there as well.

With these changes, you can build, debug, and run Oscar from QT Creator.  However, to run Oscar from a Windows shortcut, not in the QT environment, you must create a deployment directory that contains all files required by Oscar.  Creating an installer also requires an additional step.

A deploy.bat file performs both functions.  It creates a release directory and an installer.  You can include this deployment file in QT Creator in one of two ways.  You can include it as part of QT Creator's build process, or you can do this as a separate deployment step.

To include deployment as part of the Release build process, add a custom process step to the configuration. Be sure to select “Release” rather than the default “Debug” in the pull-down at the top of the Build Settings.

Create a custom process step as the final build step. Put the fully qualified path for deploy.bat in the command field. Don’t touch “working directory.” Any string you care to place in the “arguments” field will be appended to the installer executable file name.

Do the same for both 32-bit and 64-bit Build settings.

Now you should be able to build the OSCAR project from the QT Build menu.

To make 32-bit or 64-bit builds, just make sure the correct Build item is selected in the Build & Run section on the left.

If you prefer to run deploy.bat as a separate deployment step, select Run under the kit name in the Build & Run section.  Under Run Settings, select Add Deploy Step.  Now create a custom process step just as described earlier.  Menu item Build/Deploy will now run this deployment script.

**Compiling and building from the command line**

If you prefer to build from the command line and not use QT Creator, a batch script buildall.bat will build and create installers for both 32-bit and 64-bit versions of Windows. This script has some hard-coded paths, so will need to be modified for your system configuration.  This batch file is not well tested, as I prefer to build from QT Creator.

**The Deploy.BAT file**

The deployment batch file creates two folders inside the shadow build folder:

Release – everything needed to run OSCAR. You can run OSCAR from this directory just by clicking on it.

Installer – contains an installer exe file that will install this product.
