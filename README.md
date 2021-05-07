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

    dig @224.0.0.251 -p 5353 device_name


