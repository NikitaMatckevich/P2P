Simple P2P network on C++ based on a standalone version of the famous ASIO library

### How to run this project

1. Install modern compiler for C++ that supports the standard C++17
2. Install `cmake`
3. Run provided script `CMakeLists.txt` in command line. For Linux users:
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

File `peer` is a user of the network. Run this program. In case of difficulty type `h` to get
help. Follow the instructions on the screen.

File `server` is a server. Run this program in order to enable the
communication of peers with each other.
