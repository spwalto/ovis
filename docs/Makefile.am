EXTRA_DIST = conf.py index.rst README.rst requirements.txt

if HAVE_RST2MAN

EXTENSIONS = 1 7 8
RST_FILES = $(shell find $(srcdir)/man -type f -name '*.rst*' ! -name 'index.rst')
MAN_FILES = $(foreach rst, $(RST_FILES), $(shell echo $(rst) | sed -E 's|$(srcdir)/man/(.*)\.rst(\.[0-9]+)?|$(builddir)/man/\1.man\2|'))
MAN_IDX = $(builddir)/man/ldms-idx.man

# Function to categorize man pages by section. Default to 7 if filename does not have .rst.<section>.
categorize_man_pages = $(shell for man in $(MAN_FILES); do \
	section=$$(echo $$man | sed -n 's/.*\.\([0-9]\+\)$$/\1/p'); \
	if [ "$(1)" = "7" ] && [ -z "$$section" ]; then \
		echo "$$man"; \
	elif [ "$$section" = "$(1)" ]; then \
		echo "$$man"; \
	fi; \
done)

# convert to man, fix generated "/" after every special characer and remove any unnecessary rst2man output.
rst2man_convert = \
	sed 's/:ref:`\([^`]*\)<[^`]*>`/\1/g' $< | \
	rst2man | sed -e 's/\\\([`'\''\-]\)/\1/g' \
		-e '/rst2man/d' \
		-e '/rstR/d' \
		-e '/. RS/d' \
		-e '/. RE/d' \
		-e '/an-margin/d' \
		-e '/INDENT/d' > $@

# generate rules for each man page section (i.e. %.man.1:%.rst.1, etc.)
$(foreach ext, $(EXTENSIONS), \
	$(eval %.man.$(ext): %.rst.$(ext); \
	echo "Generating $$@..."; \
	mkdir -p $$(dir $$@); \
	$$(call rst2man_convert, $<)))

%.man: %.rst
	@echo "Generating $@..."
	@mkdir -p $(dir $@)
	@$(call rst2man_convert, $<)

# Create ldms-idx page with lsit of all available manpages.
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

EXTRA_DIST += $(RST_FILES)
dist_man1_MANS = $(call categorize_man_pages,1)
dist_man8_MANS = $(call categorize_man_pages,8)
dist_man7_MANS = $(call categorize_man_pages,7) $(MAN_IDX)

endif
