#!/bin/bash

NOVNC_TMP=/tmp/one/novnc-$(date "+%Y%m%d%H%M%S")
PROXY_PATH=websockify/websocketproxy.py
NOVNC_TAR=https://github.com/downloads/kanaka/noVNC/novnc-0.4.tar.gz
WEBSOCKIFY_RAW_URL=https://raw.github.com/kanaka/websockify/ee2f269c067c27ef49d63ad11d9efec499423500/websockify

if [ -z "$ONE_LOCATION" ]; then
    ONE_SHARE=/usr/share/one
    ONE_PUBLIC_SUNSTONE=/usr/lib/one/sunstone/public
    SUNSTONE_CONF=/etc/one/sunstone-server.conf
else
    ONE_SHARE=$ONE_LOCATION/share
    ONE_PUBLIC_SUNSTONE=$ONE_LOCATION/lib/sunstone/public
    SUNSTONE_CONF=$ONE_LOCATION/etc/sunstone-server.conf
fi

echo "Downloading noVNC latest version..."
mkdir -p $NOVNC_TMP
cd $NOVNC_TMP
curl -O -# -L $NOVNC_TAR
if [ $? -ne 0 ]; then
  echo "\nError downloading noVNC"
  exit 1
fi

echo "Extracting files to temporary folder..."
tar=`ls -rt $NOVNC_TMP|tail -n1`
tar -mxzf $NOVNC_TMP/$tar

if [ $? -ne 0 ]; then
  echo "Error untaring noVNC"
  exit 1
fi

echo "Installing Sunstone client libraries in $ONE_PUBLIC_SUNSTONE..."
rm -rf $ONE_PUBLIC_SUNSTONE/vendor/noVNC/
mkdir -p $ONE_PUBLIC_SUNSTONE/vendor/noVNC
cp -r $NOVNC_TMP/*novnc*/include/* $ONE_PUBLIC_SUNSTONE/vendor/noVNC/

cd $ONE_SHARE
rm -rf $NOVNC_TMP

echo "Downloading Websockify VNC proxy files"
rm -rf $ONE_SHARE/websockify
mkdir -p $ONE_SHARE/websockify
cd $ONE_SHARE/websockify
curl -O -# -L $WEBSOCKIFY_RAW_URL/websocket.py
if [ $? -ne 0 ]; then
  echo "\nError downloading websockify"
  exit 1
fi
curl -O -# -L $WEBSOCKIFY_RAW_URL/websocketproxy.py
if [ $? -ne 0 ]; then
  echo "\nError downloading websocket.py"
  exit 1
fi
ln -s websocketproxy.py websockify

echo "Backing up and updating $SUNSTONE_CONF with new VNC proxy path..."
sed -i.bck "s%^\(:vnc_proxy_path:\).*$%\1 $ONE_SHARE/$PROXY_PATH%" $SUNSTONE_CONF

echo "Installation successful"
