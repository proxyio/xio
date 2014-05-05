#!/bin/bash

find . -name '*.txt' | xargs -i{} asciidoc.py -f asciidoc.conf {}
rm html/*.html -rf
mv *.html html/
