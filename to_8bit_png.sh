#!/bin/bash
#requies ImageMagick for png options
convert $1 \
  -adaptive-resize '144x168>' \
  -fill '#FFFFFF00' -opaque none \
  -type palette -colorspace RGB \
  -colors 64 -depth 2 \
  -define png:compression-level=9 -define png:compression-strategy=0 \
  -define png:color-type=3 -define png:bit-depth=8 \
  -define png:exclude-chunk=all \
  PNG8:${1%.png}.mod.png

#fill #FFFFFF00 -opaque none makes the transparency white
#adaptive resize with > at end means resize only if larger, and maintains aspect ration
#we exclude png chunks to reduce size (like when image was made, author)
#depth is per color channel
