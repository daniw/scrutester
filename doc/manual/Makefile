.PHONY: all en de copy clean cleanpdf

# Builds the user manual in English and German.
all: en de copy

en: Manual_EN.tex
	$(call build,Manual_EN)

de: Manual_DE.tex
	$(call build,Manual_DE)

# Copy for viewing, so the viewer keeps a valid PDF during the next build.
copy:
	cp obj/Manual_EN.pdf obj/Manual_EN_cp.pdf
	cp obj/Manual_DE.pdf obj/Manual_DE_cp.pdf

# Rerun pdflatex until the .aux file is stable (max. 5 reruns).
define build
	mkdir -p obj
	if [ -e obj/$(1).aux ]; then cp obj/$(1).aux obj/$(1)_old.aux; fi
	pdflatex -interaction=nonstopmode -halt-on-error -jobname $(1) -output-directory obj $(1).tex
	latex_count=5 ; \
	while ! cmp -s obj/$(1).aux obj/$(1)_old.aux && [ $$latex_count -gt 0 ] ; do \
		echo "Rerunning latex....." ; \
		cp obj/$(1).aux obj/$(1)_old.aux ; \
		pdflatex -interaction=nonstopmode -halt-on-error -jobname $(1) -output-directory obj $(1).tex ; \
		latex_count=`expr $$latex_count - 1` ; \
	done
endef

clean:
	rm -f obj/*.aux obj/*.log obj/*.out obj/*.toc obj/*.lof obj/*.lot obj/*.tdo

cleanpdf: clean
	rm -f obj/*.pdf
