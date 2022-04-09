# mDNSd
Minimal Multicast DNS daemon

## Install and start
```
git clone git@github.com:miguelleitao/mDNSd.git
cd mDNSd
make
sudo make install
sudo systemctl start mdnsd
```



mDNSd multicast DNS daemon can be tested using dig command:

    dig -p 5353 +unexpected @224.0.0.251 hostname.local

Implementation Issues


Announcements
 1. "As part of the mDNS protocol mDNS devices will make announcements containing their mDNS records
     on start and in response to network changes on the  host machine.
     These announcements will be received by all mDNS clients on the local network and used to update their own records."
 
    mdnsd is a response-only daemon. It does not implement announcements.
  
 2. 
