#!/bin/sh

I=0
while [ $I -lt 10 ]
do
  I=$((I+1))
  echo "$I"
done

I=0
until [ $I -eq 10 ]
do
  I=$((I+1))
  echo "$I"
done

I=0
while
  I=$((I+1))
  echo "$I"
  [ $I -lt 10 ]
do true
done
