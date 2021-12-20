#!/bin/sh

echo $(echo toto)
echo $(printf %s\\n tata)
echo `echo titi`
echo `printf '%s\\n' tutu`

