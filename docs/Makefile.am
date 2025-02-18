EXTRA_DIST= \
	conf.py \
	index.rst \
	README.rst \
	requirements.txt

man7_MANS =
man1_MANS =
man3_MANS =
man5_MANS =

if HAVE_RST2MAN

# Find all .rst files under docs/man, excluding index.rst in any subdirectory
RST_FILES=$(shell find $(srcdir)/man -type f -name '*.rst' ! -name 'index.rst')

# Automatically generate corresponding .man filenames
MAN_FILES = $(RST_FILES:$(srcdir)/man/%.rst=$(builddir)/man/%.man)
MAN_FILES += $(builddir)/man/ldms-idx.man

%.man: %.rst
	@echo "Generating $@..."
	@mkdir -p $(dir $@)
	@sed 's/:ref:`\([^`]*\)<[^`]*>`/\1/g' $< | rst2man | sed 's/\\\([`'\''\-]\)/\1/g' > $@

$(builddir)/man/ldms-idx.man: 
	@echo "Generating ldms-idx.man..."
	@echo ".TH ldms-idx" > $@
	@echo ".SH NAME" >> $@
	@echo "ldms-idx - Index of LDMS man pages available after installation" >> $@
	@echo ".SH DESCRIPTION" >> $@
	@echo "The following LDMS-related man pages are available:" >> $@
	@for man in $(filter-out $(builddir)/man/ldms-idx.man, $(MAN_FILES)); do \
		name=$$(basename $$man .man);\
		section=$$(grep -oE "$$(basename $$man .man)\([0-9]\)" $(builddir)/man/*.man $(builddir)/man/*/*.man $(builddir)/man/*/*/*.man | head -n 1 | sed -n 's/.*(\([0-9]\)).*/\1/p'); \
		echo ".TP" >> $@; \
		echo $$([ -n "$$section" ] && echo "\\fB$$name($$section)\\fR" || echo "\\fB$$name\\fR") >> $@; \
	done

EXTRA_DIST += $(RST_FILES)
man7_MANS += $(MAN_FILES)

endif
