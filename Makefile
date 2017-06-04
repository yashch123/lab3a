default:
	gcc -o lab3a -g lab3a.c

dist:
	tar -czvf lab3a-804653078.tar.gz ext2_fs.h lab3a.c README Makefile

clean:
	rm -rf lab3a *.csv 
