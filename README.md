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


