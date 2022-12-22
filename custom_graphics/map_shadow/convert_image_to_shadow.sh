for i in `ls *png`; do echo $i; convert -density 150 -threshold 99% $i ${i}_MASK; done
for i in `ls *png_MASK`; do echo $i; convert -alpha set -background none -channel A -evaluate multiply 0.5 +channel $i ${i}_TRANSP; done
for i in `ls *png_MASK_TRANSP`; do echo $i; convert -matte -interpolate Integer -filter point -virtual-pixel transparent +distort Perspective '0,0,0,0  0,90,-40,40  90,90,50,40  90,0,90,0' $i ${i}_SKEW ; done
rm -f ./*.png
rm -f ./*_MASK
rm -f ./*_MASK_TRANSP
rename _MASK_TRANSP_SKEW '' *png*
