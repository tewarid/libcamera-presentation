#!/bin/sh

# create standalone revealjs presentation
pandoc \
    --slide-level=2 \
    -o slides.html \
    -t revealjs -s \
    -V minScale=0.2 -V maxScale=1.5 \
    -V width=\"100%\" -V height=\"100%\" \
    -V margin=0.05 \
    front-matter.md slides.md

# create PDF slides using beamer
pandoc \
    --slide-level=2 \
    -o slides.pdf \
    -t beamer \
    -V theme:default \
    -V aspectratio:169 \
    front-matter.md slides.md
