# Simple Traffic Generator
## Brief Introduction
A simple traffic generator for network experiments.

The **server** listens for incoming requests, and replies with a *flow* with the requested size (using the requested DSCP value & sending at the requested rate) for each request.

The **client** establishes *persistent TCP connections* to a list of servers and randomly generates requests over TCP connections according to the client configuration file. *If no available TCP connection, the client will establish a new one*. A request is completed only when all its flows are completed.  

In the **client configuration file**, the user can specify the list of destination servers, the request size distribution, the Differentiated Services Code Point (DSCP) value distribution, the sending rate distribution and the request fanout distribution, . 

## Build
In the main directory, run ```make```, then you will see **client**, **server**.    

## Quick Start
In the main directory, do following operations:
- Start server 
```
./bin/server -p 5001 
```

- Start client
```
./bin/client -b 900 -c conf/client_config.txt -n 5000 -l flows.txt -s 123 
```


## Command Line Arguments
### Server
Example:
```
./bin/server -p 5001 -d  
```
* **-p** : the TCP **port** that the server listens on (default 5001)

* **-v** : give more detailed output (**verbose**)

* **-d** : run the server as a **daemon**

* **-h** : display help information

### Client
Example:
```
./bin/client -b 900 -c conf/client_config.txt -n 5000 -l flows.txt -s 123 -r bin/result.py
```
* **-b** : expected average RX **bandwidth** in Mbits/sec
 
* **-c** : **configuration** file which specifies workload characteristics (required)

* **-n** : **number** of requests (instead of -t)

* **-t** : **time** in seconds to generate requests (instead of -n)
 
* **-l** : **log** file with flow completion times (default flows.txt)

* **-s** : **seed** to generate random numbers (default current system time)

* **-v** : give more detailed output (**verbose**)

* **-h** : display **help** information

Note that you need to specify either the number of requests (-n) or the time to generate requests (-t). But you cannot specify both of them.


## Client Configuration File
The client configuration file specifies the list of servers, the request size distribution.  

The format is a sequence of key and value(s), one key per line. The permitted keys are:

* **server:** IP address and TCP port of a server.
```
server 192.168.1.51 5001
```

* **req_size_dist:** request size distribution file path and name.
```
req_size_dist conf/DCTCP_CDF.txt
```
There must be one request size distribution file, present at the given path, 
which specifies the CDF of the request size distribution. See "DCTCP_CDF.txt" in ./conf directory 
for an example with proper formatting.



##Output
A successful run of **client** creates a file with flow completion time results. 

In files with flow completion times, each line gives flow size (in bytes), flow completion time (in microseconds) and actual per-flow goodput (in Mbps). 


