## Pre-requisites
The system must have following tools available on the default path: 
- clang-format  (available via apt install clang-format)
- cpplint (available via python pip3 install cpplint)


## One time setup
Add tools.git as a submodule to the main project:

`git submodule add git@gitlab1:AsyncTasking/tools.git`

Run setup.sh as follows (must be in the root of outer repo):

`./tools/githooks/setup.sh`

