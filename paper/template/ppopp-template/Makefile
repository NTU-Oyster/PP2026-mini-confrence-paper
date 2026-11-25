# 本機編譯：make       清除：make clean
# 需要 TeX Live / MacTeX（含 latexmk）
MAIN = main

all:
	latexmk -pdf $(MAIN).tex

clean:
	latexmk -C $(MAIN).tex
	rm -f *.bbl *.run.xml

.PHONY: all clean
