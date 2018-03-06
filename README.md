# Sedcam

Software for the sedcam.

#### Installation Instructions

On Mac:

1. wget https://debian.beagleboard.org/images/bone-debian-8.7-iot-armhf-2017-03-19-4gb.img.xz
2. shasum -a 256 bone-debian-8.7-iot-armhf-2017-03-19-4gb.img.xz
3. compare to: f709035e9d9470fa0aa9b778e4d369ba914f5acc7a5fecd6713facbe52cc9285
4. xz -d bone-debian-8.7-iot-armhf-2017-03-19-4gb.img.xz
5. insert sdcard and **verify that it is rdisk2. if it is notrdisk2, modify the following two commands.**
6. sudo chmod 666 /dev/rdisk2
7. pv -ptearb bone-debian-8.7-iot-armhf-2017-03-19-4gb.img | dd of=/dev/rdisk2 bs=1m
8. insert this sdcard into the beaglebone, and boot, making sure ethernet is attached to the usb port

On Beaglebone:

1. login as debian:temppwd
2. cd ~
3. git clone https://github.com/tjcrone/sedcam.git
4. cd sedcam/beaglebone
5. chmod 744 setup
6. sudo ./setup

7. wget http://repo.continuum.io/miniconda/Miniconda3-latest-Linux-armv7l.sh
8. chmod 744 Miniconda3-latest-Linux-armv7l.sh
9. ./Miniconda3-latest-Linux-armv7l.sh
10. conda create --name sedcam python=3.4
11. source activate sedcam
12. conda install numpy
13. pip install --upgrade pip
14. pip install numpngw
