#!/bin/bash

TOTO=(toto tata)
TOTO[0]=titi
TOTO[1]+=tu
echo ${TOTO[0]}
echo ${TOTO[1]}
echo ${TOTO[*]}

TOTO+=(2)
echo $((TOTO[2]+1))
echo $((${TOTO[2]}+2))

declare -a TUTU
TUTU=(titi "tu tu")
echo ${TUTU[0]}
echo ${TUTU[1]}
echo ${TUTU[*]}
echo "${TUTU[*]}"

declare -A A
A[to]=ta
A[ti]=tu
echo ${A[to]}
echo ${A[ti]}
echo ${A[*]}

declare -A B
B=([to]=ta [ti]=tu)
echo ${B[to]}
echo ${B[ti]}
echo ${B[*]}
echo "${B[*]}"

toto=tata
C=()
C+=($toto)
echo ${C[@]}
