for i in `ls flower*_bright.png`; do echo $i; convert -modulate 70 $i ${i}_DARK; done
rename _bright.png_DARK _dark.png *DARK*

