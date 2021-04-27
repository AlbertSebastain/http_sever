# Toy HTTP server
an HTTP server that supports basic headers and some basic methods.
# Purpose
According to the toy HTTP server, the user can learn how to build a web server and get some tricks from it. although some open-source HTTP server has good performance it is not friendly for the learner because of the complex structure. The toy HTTP server aims to achieve an HTTP server that can satisfy basic functions with simple structure and code. The toy HTTP server uses some interfaces and classes from c++11, the users can also befit some from it.  
## Support Features
Parse and add basic headers.  
Thread pool  
Using config file to change the parameters  
Non-blocking IO  
Reactor  
Finite state machine  
TImer with min-heap 
## Usage
```
mkdir build
cd ./build
cmake
make ..
./http_server
```
The parameters are saved in ./config.yaml  
## Drawbacks
The server does not parse the request body.  
The server always uses new and delete to apply and release memory.  
