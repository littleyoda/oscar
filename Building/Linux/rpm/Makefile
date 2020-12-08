# Front end for second-stage Makefile


include Makefile.common


# Second-stage make, which does the actual work.
MAKE2 := $(MAKE) -f Makefile-stage2


default: build

%: $(SPEC)
	$(MAKE2) $@

# Build the spec with a valid version.

VERSION_FILE := VERSION
VERSION_SOURCE := $(TOP)/oscar/VERSION
VERSION := $(shell egrep -e '\#define\s+VERSION\s+' '$(VERSION_SOURCE)' \
	| sed -e 's/^.* //g' \
	| tr -d '"' \
	| tr -- - . \
	)

SPEC=$(NAME).spec
$(SPEC): $(SPEC).raw Makefile
	sed -e 's/__VERSION__/$(VERSION)/g' $< > $@
TO_CLEAN += $(SPEC)


build: $(SPEC)
	$(MAKE2)


clean:: $(SPEC)
	$(MAKE2) $@
	rm -f $(TO_CLEAN)
