Simple P2P network on C++ based on a standalone version of the famous ASIO
library

### How to run this project

There are 2 versions of the project currently on GitHub.

The simple and pretty version of the code is available on master branch.
However, this program doesn't do "Peer-to-peer" in a sense that it is
impossible for peers to find each other in the network.

The cumbersome and probably buggy code that works in peer-to-peer is available
on wip branch. Peers in this program can ask for IP data of other peers and so
can propose ommunication to each other.

 1. Install modern compiler for C++ that supports the standard C++17
 2. Install `cmake`
 3. Run provided script `CMakeLists.txt` in command line. For example, Linux
users can type:
 
 ~~~
 > mkdir build
 > cd build
 > cmake -DCMAKE_BUILD_TYPE=Debug ..
 ~~~
 or
 ~~~
 > cmake -DCMAKE_BUILD_TYPE=Release ..
 ~~~
 4. Run `make all`
 5. In your build directory, you'll get 2 executable files.

File `peer` is a user of the network. Run this program. In case of difficulty
type `h` to get help. Follow the instructions on the screen.

File `server` is a server. Run this program in order to enable the
communication of peers with each other.
