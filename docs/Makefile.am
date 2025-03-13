EXTRA_DIST = conf.py index.rst README.rst requirements.txt

if HAVE_RST2MAN

MAN_FILES = $(shell find $(prefix)/share/man/ -type f -name '*.man')
#MAN_FILES = $(patsubst $(topdir)/ldms/%.rst, $(builddir)/man/%.man, $(RST_FILES))
MAN_IDX = $(prefix)/share/man/ldms-idx.man

# Create ldms-idx page listing all available manpages.
$(MAN_IDX):
	@echo "Generating $@..."
	@echo ".TH ldms-idx" > $@
	@echo ".SH NAME" >> $@
	@echo "ldms-idx - Index of LDMS man pages available after installation" >> $@
	@echo ".SH DESCRIPTION" >> $@
	@echo "The following LDMS-related man pages are available:" >> $@
	@for man in $(MAN_FILES); do \
		section=$$(echo $$man | sed -n 's/.*\.\([0-9]\+\)$$/\1/p'); \
		name=$$(basename $$man .man | sed 's/\([^.]*\)\(\.[0-9]*\)\?$$/\1/'); \
		echo ".TP" >> $@; \
		echo $$([ -n "$$section" ] && echo "\\fB$$name($$section)\\fR" || echo "\\fB$$name(7)\\fR") >> $@; \
	done

endif

EXTRA_DIST += $(RST_FILES)
