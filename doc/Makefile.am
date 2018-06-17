DOC = main
SECTS = $(DOC).sgml intro.sgml
SRC = $(SECTS) builddate

EXTRA_DIST = $(SECTS)

builddate:
	date "+%e %B %Y" > $@

$(DOC).dvi:	$(SRC)
		db2dvi $<

$(DOC).ps:	$(DOC).dvi
		dvips $< -o $@

$(DOC).pdf:	$(SRC)
		db2pdf $<

$(DOC):		$(SRC)
		-rm -rf $(DOC)
		db2html $<

