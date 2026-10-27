.PHONY: all build vim clean

all: build

build: 
	mkdir -p obj
	mkdir -p obj/src
	if [ -e obj/Protocol_PS1-1.aux ];\
	then\
		cp obj/Protocol_PS1-1.aux obj/Protocol_PS1-1_old.aux;\
	fi
	xelatex -jobname Protocol_PS1-1 -output-directory obj Protocol_PS1-1.tex
	#cd obj && bibtex Protocol_PS1-1.aux
	latex_count=5 ; \
	while ! cmp -s obj/Protocol_PS1-1.aux obj/Protocol_PS1-1_old.aux && [ $$latex_count -gt 0 ] ;\
	do \
		echo "Rerunning latex....." ;\
		cp obj/Protocol_PS1-1.aux obj/Protocol_PS1-1_old.aux;\
		xelatex -jobname Protocol_PS1-1 -output-directory obj Protocol_PS1-1.tex ;\
		latex_count=`expr $$latex_count - 1` ;\
	done

vim: 
	mkdir -p obj
	mkdir -p obj/src
	if [ -e obj/Protocol_PS1-1.aux ];\
	then\
		cp obj/Protocol_PS1-1.aux obj/Protocol_PS1-1_old.aux;\
	fi
	xelatex -jobname Protocol_PS1-1 -output-directory obj -halt-on-error Protocol_PS1-1.tex
	#cd obj && bibtex Protocol_PS1-1.aux
	latex_count=5 ; \
	while ! cmp -s obj/Protocol_PS1-1.aux obj/Protocol_PS1-1_old.aux && [ $$latex_count -gt 0 ] ;\
	do \
		echo "Rerunning latex....." ;\
		cp obj/Protocol_PS1-1.aux obj/Protocol_PS1-1_old.aux;\
		xelatex -jobname Protocol_PS1-1 -output-directory obj -halt-on-error Protocol_PS1-1.tex ;\
		latex_count=`expr $$latex_count - 1` ;\
	done

clean:
	rm -f obj/*.aux
	rm -f obj/src/*.aux
	rm -f obj/*.bbl
	rm -f obj/*.blg
	rm -f obj/*.lof
	rm -f obj/*.log
	rm -f obj/*.lot
	rm -f obj/*.out
	rm -f obj/*.tdo
	rm -f obj/*.toc

cleanpdf:
	rm -f obj/*.aux
	rm -f obj/src/*.aux
	rm -f obj/*.bbl
	rm -f obj/*.blg
	rm -f obj/*.lof
	rm -f obj/*.log
	rm -f obj/*.lot
	rm -f obj/*.out
	rm -f obj/*.tdo
	rm -f obj/*.toc
	rm -f obj/*.pdf
