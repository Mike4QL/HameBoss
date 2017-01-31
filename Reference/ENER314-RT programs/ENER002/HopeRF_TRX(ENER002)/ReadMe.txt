Running R1 and Legacy socket monitor&control software
0 - Copy all files onto the PI
1 - Enter into 'HopeRF_TRX' directory
2 - chmod +x hoperf_trx
3 - sudo ./hoperf_trx
Ctrl + C to break anytime
By default messages are send to toggle to the Legacy and R1 sockets every 10 seconds.
Any messages received from R1/C1 are displayed.


Compile R1 and Legacy socket monitor&control software
1 - Install bcm2835 drivers if not already installed
2 - Run 'make' from 'HopeRF_TRX' directory 


How to install bcm2835 drivers
1 - Enter into bcm2835 directory
2 - tar zxvf bcm2835-1.37.tar.gz
3 - cd bcm2835-1.37
4 - ./configure
5 - make
6 - sudo make check				// test should pass
7 - sudo make install


Hot plugging PI accessories MAY cause operating system to crash. Accessories should be plugged in before power up.
