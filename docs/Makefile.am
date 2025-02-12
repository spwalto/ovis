EXTRA_DIST= \
	conf.py \
	index.rst \
	README.rst \
	requirements.txt

man7_MANS =

if HAVE_RST2MAN

# Find all .rst files under docs/man, excluding index.rst in any subdirectory
RST_FILES = $(shell find $(srcdir)/man -type f -name '*.rst' ! -name 'index.rst')

# Automatically generate corresponding .man filenames
MAN_FILES = $(RST_FILES:$(srcdir)/man/%.rst=$(builddir)/man/%.man)
MAN_FILES += $(builddir)/man/ldms-all.man

# Rule to convert .rst to .man
$(builddir)/man/%.man: man/%.rst
	mkdir -p $(dir $@)
	rst2man $< $@

# Rule to generate ldms-all.man directly
$(builddir)/man/ldms-all.man: $(MAN_FILES)
	@echo "Generating ldms-all.man..."
	@mkdir -p $(dir $@)
	@echo ".TH LDMS-ALL 7" > $@
	@echo ".SH NAME" >> $@
	@echo "ldms-all - List of LDMS man pages available after installation" >> $@
	@echo ".SH DESCRIPTION" >> $@
	@echo "The following LDMS-related man pages are available:" >> $@
	for man in $(filter-out $(builddir)/man/ldms-all.man, $(MAN_FILES)); do \
	    name=$$(basename $$man .man); \
	    section=$$(echo $$man | sed -n 's/.*\.\([0-9]\)$/\1/p'); \
	    echo ".TP" >> $@; \
	    echo "\\fB$$name($$section)\\fR" >> $@; \
	done

# Add the .rst files to EXTRA_DIST and the generated .man files to man7_MANS
EXTRA_DIST += $(RST_FILES)
man7_MANS += $(MAN_FILES)

endif
