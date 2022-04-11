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

## Using
mDNSd multicast DNS daemon can be tested using dig command:

    dig -p 5353 +unexpected @224.0.0.251 hostname.local

## Implementation Issues

### Announcements
    "As part of the mDNS protocol mDNS devices will make announcements containing their mDNS records
     on start and in response to network changes on the  host machine.
     These announcements will be received by all mDNS clients on the local network 
     and used to update their own records."
 
    mdnsd is a response-only daemon. It does not implement periodic or "on start" announcements.
  
### Unicast responses
    "The destination UDP port in all Multicast DNS responses MUST be 5353,
     and the destination address MUST be the mDNS IPv4 link-local
     multicast address 224.0.0.251 or its IPv6 equivalent FF02::FB, except
     when generating a reply to a query that explicitly requested a
     unicast response:

      * via the unicast-response bit,
      * by virtue of being a legacy query (Section 6.7), or
      * by virtue of being a direct unicast query."
      
    mDNSd responses are sent using to the port and unicast address that originated the query,
    as sugested in "6.7.  Legacy Unicast Responses"
    
## Documentation
https://datatracker.ietf.org/doc/rfc6762/

