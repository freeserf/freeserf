for i in `ls winter_tree*.png`; do echo $i; convert -bordercolor none -border 1x1 -background black -alpha background -channel A -blur 1x1 -level 0,70% $i ${i}_BORD; done
rename .png_BORD _bord.png *BORD*
