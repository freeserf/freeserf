for i in `ls flower_groupa*_bright.png`; do echo $i; convert -modulate 100,100,-30 $i ${i}_PURP; done
rename groupa groupc *PURP*
rename _PURP '' *PURP*

