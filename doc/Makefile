.PHONY: all build vim count clean

all: build count copy

build: 
	mkdir -p obj
	mkdir -p obj/src
	if [ -e obj/scrutester.aux ];\
	then\
		cp obj/scrutester.aux obj/scrutester_old.aux;\
	fi
	pdflatex -jobname scrutester -output-directory obj Scrutester.tex
	cd obj && bibtex scrutester.aux
	latex_count=5 ; \
	while ! cmp -s obj/scrutester.aux obj/scrutester_old.aux && [ $$latex_count -gt 0 ] ;\
	do \
		echo "Rerunning latex....." ;\
		cp obj/scrutester.aux obj/scrutester_old.aux;\
		pdflatex -jobname scrutester -output-directory obj Scrutester.tex ;\
		latex_count=`expr $$latex_count - 1` ;\
	done

vim: 
	mkdir -p obj
	mkdir -p obj/src
	if [ -e obj/scrutester.aux ];\
	then\
		cp obj/scrutester.aux obj/scrutester_old.aux;\
	fi
	pdflatex -jobname scrutester -output-directory obj -halt-on-error Scrutester.tex
	cd obj && bibtex scrutester.aux
	latex_count=5 ; \
	while ! cmp -s obj/scrutester.aux obj/scrutester_old.aux && [ $$latex_count -gt 0 ] ;\
	do \
		echo "Rerunning latex....." ;\
		cp obj/scrutester.aux obj/scrutester_old.aux;\
		pdflatex -jobname scrutester -output-directory obj -halt-on-error Scrutester.tex ;\
		latex_count=`expr $$latex_count - 1` ;\
	done

count:
	@echo
	@echo Counting words in abstract:
	@wc src/abstract_de.tex src/abstract_en.tex | grep abstract
copy: 
	cp obj/scrutester.pdf obj/scrutester_cp.pdf

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
