#!/bin/bash
BUS=1
ID=0x14

GET_POW_BUT_CMD="POWBUT=\$(i2cget -y $BUS $ID \0x00  b |sed -e "s/^0x//" |tr "a-z" "A-Z")"
GET_RSOC_CMD="RSOC=\$(i2cget -y $BUS $ID \0x01  b |sed -e "s/^0x//" |tr "a-z" "A-Z")"
GET_VER_CMD="VER=\$(i2cget -y $BUS $ID \0xFD  b |sed -e "s/^0x//" |tr "a-z" "A-Z")"
GET_CUR_LOW_CMD="CUR_LOW=\$(i2cget -y $BUS $ID \0x0a  b |sed -e "s/^0x//" |tr "a-z" "A-Z")"
GET_CUR_HIGH_CMD="CUR_HIGH=\$(i2cget -y $BUS $ID \0x0b  b |sed -e "s/^0x//" |tr "a-z" "A-Z")"
GET_TEMP_LOW_CMD="TEMP_LOW=\$(i2cget -y $BUS $ID \0x02  b |sed -e "s/^0x//" |tr "a-z" "A-Z")"
GET_TEMP_HIGH_CMD="TEMP_HIGH=\$(i2cget -y $BUS $ID \0x03  b |sed -e "s/^0x//" |tr "a-z" "A-Z")"
GET_BATVOLT_LOW_CMD="BATVOLT_LOW=\$(i2cget -y $BUS $ID \0x04  b |sed -e "s/^0x//" |tr "a-z" "A-Z")"
GET_BATVOLT_HIGH_CMD="BATVOLT_HIGH=\$(i2cget -y $BUS $ID \0x05  b |sed -e "s/^0x//" |tr "a-z" "A-Z")"

eval $GET_VER_CMD
echo Firmvare version: V$VER

while true; do

eval $GET_POW_BUT_CMD
eval $GET_RSOC_CMD
eval $GET_CUR_LOW_CMD
eval $GET_CUR_HIGH_CMD
eval $GET_TEMP_LOW_CMD
eval $GET_TEMP_HIGH_CMD
eval $GET_BATVOLT_LOW_CMD
eval $GET_BATVOLT_HIGH_CMD

echo CHARGE = $((0x${RSOC}))%, Temp = $((0x${TEMP_HIGH}${TEMP_LOW}))[10K], VBAT = $((0x${BATVOLT_HIGH}${BATVOLT_LOW})) mV, #Current = $((0x${CUR_HIGH}${CUR_LOW})) mA,

if [ $POWBUT = "01" ]; then
    #echo $POWBUT
	sudo halt
fi

sleep 1
done 