juise
=====

JUNOS User Interface Scripting Environment


Building juise
----------------

To build juise from the git repo:

    git clone https://github.com/Juniper/juise.git
    cd juise
    mkdir build
    autoreconf --install
    cd build
    ../configure

We use "build" to keep object files and generated files away from the
source tree.
