#!/bin/sh

var="marijuana"
echo ${#var}

echo ${var-foo}
echo ${var+foo}
echo ${foo-foo}
echo ${foo+foo}

echo ${var#*a}
echo ${var##*a}
echo ${var%a*}
echo ${var%%a*}

