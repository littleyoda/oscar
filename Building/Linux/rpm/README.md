# RPM Builder for Linux

The Makefile in this directory builds OSCAR as an RPM.


## Build Process

 * Clone this repository
 * `cd Building/Linux/rpm`
 * `sudo make`

Build by-products can be removed with `sudo make clean`.



## Assumptions

`git rev-parse` is used to determine the top-level directory, ergo
this tree must have been cloned from GitLab and `git` must be
installed.
