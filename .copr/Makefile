# COPR SCM contract:
# https://docs.copr.fedorainfracloud.org/user_documentation.html#scm
SHELL := /bin/bash
.SHELLFLAGS := -eu -o pipefail -c
.ONESHELL:

repo_root := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/..)
outdir ?= $(repo_root)/build/copr
spec ?= usbeekeeper.spec

# Pass paths through the environment so spaces are not interpreted as shell code.
export COPR_REPO_ROOT := $(repo_root)
export COPR_OUTDIR := $(outdir)
export COPR_SPEC := $(spec)

.PHONY: srpm
srpm:
	@missing=0
	for tool in git rpmbuild rpmspec tar gzip; do
		command -v "$$tool" >/dev/null 2>&1 || missing=1
	done
	if (( missing )); then
		if (( $$(id -u) != 0 )); then
			printf '%s\n' 'Source generation needs git, rpm-build, tar and gzip. Install these separately; do not run this target with sudo.' >&2
			exit 1
		fi
		# COPR runs this stage as root in its disposable source-generation chroot.
		dnf -y install git rpm-build tar gzip
	fi
	case "$$COPR_SPEC" in
		/*) spec_path=$$COPR_SPEC ;;
		*) spec_path="$$COPR_REPO_ROOT/$$COPR_SPEC" ;;
	esac
	archive_name=$$(rpmspec -q --srpm --qf '%{SOURCE}' "$$spec_path")
	case "$$archive_name" in
		usbeekeeper-*.tar.gz) ;;
		*) printf 'Unexpected Source0 filename: %s\n' "$$archive_name" >&2; exit 1 ;;
	esac
	upstream_version=$${archive_name#usbeekeeper-}
	upstream_version=$${upstream_version%.tar.gz}
	# The archive root must match the spec's autosetup directory.
	archive_root="usbeekeeper-plasma-$$upstream_version"
	mkdir -p "$$COPR_OUTDIR"
	output=$$(cd "$$COPR_OUTDIR" && pwd)
	work=$$(mktemp -d)
	trap 'rm -rf -- "$$work"' EXIT
	mkdir -p "$$work/SOURCES" "$$work/tmp"
	git -C "$$COPR_REPO_ROOT" archive --format=tar --prefix="$$archive_root/" HEAD \
		| gzip -n > "$$work/SOURCES/$$archive_name"
	rpmbuild -bs \
		--define "_topdir $$work" \
		--define "_tmppath $$work/tmp" \
		--define "_srcrpmdir $$output" \
		--define 'dist %{nil}' \
		"$$spec_path"
