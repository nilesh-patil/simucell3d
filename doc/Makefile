# Makefile for the SimuCell3D documentation.
#
#   make html             build the site into _build/html
#   make doxygen          write the Doxygen XML used by the C++ API pages
#   make doxygen html     both, in that order
#   make html-strict      as html, but treat warnings as errors (as CI does)
#   make clean            remove _build/

SPHINXBUILD   ?= sphinx-build
SPHINXOPTS    ?=
DOXYGEN       ?= doxygen
SOURCEDIR     = .
BUILDDIR      = _build

.PHONY: help html html-strict doxygen clean

help:
	@echo "Targets: html, html-strict, doxygen, clean"

html:
	$(SPHINXBUILD) -b html $(SPHINXOPTS) "$(SOURCEDIR)" "$(BUILDDIR)/html"

html-strict:
	$(SPHINXBUILD) -b html -W --keep-going $(SPHINXOPTS) "$(SOURCEDIR)" "$(BUILDDIR)/html"

doxygen:
	mkdir -p "$(BUILDDIR)/doxygen"
	$(DOXYGEN) Doxyfile

clean:
	rm -rf "$(BUILDDIR)"
