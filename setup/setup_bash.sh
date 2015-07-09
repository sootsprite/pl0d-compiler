#!/bin/bash

# export http_proxy=http://proxy.se.shibaura-it.ac.jp:10080
files=(codegen.c codegen.h compile.c getSource.c getSource.h main.c table.c table.h Makefile fact.pl0 ex1.pl0)

for file in ${files[@]}; do
	wget -O $file http://www.k.hosei.ac.jp/~nakata/oCompiler/PL0compiler/$file
	nkf -e -Lu --overwrite $file
done
patch < pl0d.diff
make
