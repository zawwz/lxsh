#!/bin/sh

__for_fct() {
  for I in ; do
    echo $I
  done
}

for N
do
  echo $N
done

for I in $(seq 1 10); do
  echo "toto $I"
done

__for_fct toto tata
