#!/bin/csh

# setenv http_proxy http://proxy.se.shibaura-it.ac.jp:10080
set files = (codegen.c codegen.h compile.c getSource.c getSource.h main.c table.c table.h Makefile fact.pl0 ex1.pl0)

foreach file ($files)
    rm $file
    wget http://www.k.hosei.ac.jp/~nakata/oCompiler/PL0compiler/$file
    nkf -e -Lu --overwrite $file
end

patch < pl0d.diff

make
