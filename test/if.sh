#!/bin/sh

if [ -n "$DEBUG" ]
then
  echo "set"
elif [ -n "$TOTO" ]
then
  echo "toto lol"
else
  echo "not set"
fi
