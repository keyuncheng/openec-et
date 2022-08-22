#!/bin/bash

cd ~
wget https://www.openssl.org/source/openssl-1.1.1q.tar.gz
tar -zxvf openssl-1.1.1q.tar.gz
cd openssl-1.1.1q
sudo ./config --prefix=/usr/local/ssl --openssldir=/usr/local/ssl shared zlib
sudo make
sudo make install

# Do the following things
# 1.:
